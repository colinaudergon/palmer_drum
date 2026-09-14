// Simple trapezoidal amplitude envelope for a single grain, as described
// in Ross Bencina's "Implementing Real-Time Granular Synthesis" (Audio
// Anecdotes III, 2001-2002): successive samples are produced from an
// accumulator (nextAmplitude = previousAmplitude + amplitudeIncrement),
// where amplitudeIncrement depends on the current segment:
//
//   attack:  amplitudeIncrement =  grainAmplitude / attackSamples
//   sustain: amplitudeIncrement =  0
//   release: amplitudeIncrement = -grainAmplitude / releaseSamples
//
// Like GrainSource, Envelope's initialization parameters are grouped into
// a nested Essence object following the "Essence" design pattern, so
// whichever component decides how successive grains should be
// parameterized (ClapEngine, in this codebase) owns and mutates a single
// reusable Essence instance, handed to Envelope::Init() each time a new
// grain is activated.

#pragma once

#include <cstdint>

namespace peaks
{

  class Envelope
  {
  public:
    // Initialization parameters for a single grain's envelope: how long
    // the grain (and therefore the envelope) lasts in total, how many of
    // those samples are spent ramping up/down, and the peak amplitude
    // reached at the end of the attack segment.
    class Essence
    {
    public:
      Essence()
          : duration_samples_(0),
            attack_samples_(0),
            release_samples_(0),
            amplitude_(kFullScale) {}

      // Total envelope length, in samples -- should match the owning
      // grain's GrainSource::Essence::duration_samples() so the envelope
      // finishes exactly when the grain itself does.
      inline void set_duration_samples(uint32_t duration_samples)
      {
        duration_samples_ = duration_samples;
      }

      // Length of the attack (ramp up from 0) and release (ramp down to
      // 0) segments, in samples. Whatever remains of duration_samples()
      // after both is the sustain segment (held flat at amplitude()). If
      // attack_samples + release_samples would exceed duration_samples,
      // Init() scales both down proportionally so they exactly fill the
      // grain instead (no sustain segment) rather than overrunning it.
      inline void set_attack_samples(uint32_t attack_samples)
      {
        attack_samples_ = attack_samples;
      }
      inline void set_release_samples(uint32_t release_samples)
      {
        release_samples_ = release_samples;
      }

      // Peak amplitude reached at the end of the attack segment, as a Q16
      // fixed-point gain (kFullScale == 1.0). Grain::Synthesize() multiplies
      // its GrainSource's output sample by this envelope's current output,
      // so kFullScale here means "no additional attenuation" -- any overall
      // shaping across many grains (e.g. ClapEngine's own decay envelope)
      // is expected to be applied separately, on top of this one.
      inline void set_amplitude(uint32_t amplitude)
      {
        amplitude_ = amplitude;
      }

      inline uint32_t duration_samples() const { return duration_samples_; }
      inline uint32_t attack_samples() const { return attack_samples_; }
      inline uint32_t release_samples() const { return release_samples_; }
      inline uint32_t amplitude() const { return amplitude_; }

    private:
      uint32_t duration_samples_;
      uint32_t attack_samples_;
      uint32_t release_samples_;
      uint32_t amplitude_;
    };

    // Q16 fixed-point full scale (gain 1.0).
    static constexpr uint32_t kFullScale = 1UL << 16;

    Envelope() { Reset(); }

    // Initializes this Envelope's per-grain segment lengths and peak
    // amplitude from an Essence. Called when a grain is (re)activated.
    void Init(const Essence &essence);

    // Produces the envelope's current amplitude (Q16 gain, 0..kFullScale)
    // and advances to the next sample/segment. Should be called once per
    // audio sample for the lifetime of the grain, in lockstep with the
    // grain's GrainSource::Synthesize().
    uint32_t Synthesize();

    // True once the release segment (or, for a zero-length envelope, the
    // whole envelope) has completed. The owning Grain should terminate
    // itself once this returns true, regardless of its GrainSource's own
    // Done() state, since there is no more envelope shaping left to apply.
    inline bool Done() const { return done_; }

  private:
    enum class Segment
    {
      kAttack,
      kSustain,
      kRelease,
      kDone
    };

    void Reset();

    // Enters segment, setting up samples_left_in_segment_, amplitude_ and
    // increment_ accordingly. Segments with zero length are skipped by the
    // caller (Init()/Synthesize()) -- EnterSegment() itself assumes the
    // requested segment has at least one sample to produce, except kDone.
    void EnterSegment(Segment segment);

    Segment segment_;
    int32_t amplitude_;  // Current amplitude, Q16 (kept signed so a
                         // slightly-negative rounding error at the very
                         // end of the release ramp doesn't wrap instead of
                         // just clamping to 0 in Synthesize()).
    int32_t increment_;  // Per-sample delta for the current segment (0
                         // during sustain).
    uint32_t samples_left_in_segment_;
    uint32_t attack_samples_;
    uint32_t sustain_samples_;
    uint32_t release_samples_;
    uint32_t peak_amplitude_;
    bool done_;
  };

} // namespace peaks
