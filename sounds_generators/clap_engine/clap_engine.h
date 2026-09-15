
// sample based clap engine
#pragma once

#include <array>

#include "../../lib/mu_stmlib.h"
#include "../../lib/dsp.h"

#include "../i_processor.h"
#include "../peaks_ressources/gate_processor.h"
#include "samples/samples.h"
#include "engine/grain.h"
#include "engine/grain_source.h"
#include "engine/envelope.h"
#include "../peaks_ressources/random.h"
#include "engine/lut_neg_log.h"

namespace peaks
{

  namespace clap_engine_internal
  {
    // Builds, at compile time, the table mapping each 0-based index in
    // [0, kNumMultiSampleMasks) to the index-th bitmask (in ascending
    // numeric order) among all masks over kNsamples bits that have two or
    // more bits set -- i.e. every sample combination not reachable as a
    // single sample (see ClapEngine::set_source()). Computing this once
    // at compile time (rather than scanning candidate masks every time
    // set_source() is called) means a rapid pot sweep, or a noisy ADC
    // re-sending readings, can never cost more than a single array
    // lookup at runtime -- important since set_source() runs on the same
    // core that also keeps the audio codec's DMA buffer fed.
    constexpr uint32_t kAllSamplesMask =
        static_cast<uint32_t>((1u << kNsamples) - 1);
    constexpr uint32_t kNumMultiSampleMasks = kAllSamplesMask - kNsamples;

    constexpr std::array<uint16_t, kNumMultiSampleMasks> BuildMultiSampleMasks()
    {
      std::array<uint16_t, kNumMultiSampleMasks> masks{};
      size_t count = 0;
      for (uint32_t mask = 1; mask <= kAllSamplesMask; ++mask)
      {
        if (__builtin_popcount(mask) >= 2)
        {
          masks[count++] = static_cast<uint16_t>(mask);
        }
      }
      return masks;
    }
  } // namespace clap_engine_internal

  class ClapEngine : public IProcessor
  {
  public:
    ClapEngine() {}
    ~ClapEngine() override {}

    void Init() override;
    void Process(const GateFlags *gate_flags, int16_t *out, size_t size) override;

    void Configure(uint16_t *parameter) override
    {
      set_source(parameter[0]);
      set_density(parameter[1]);
      set_spread(parameter[2]);
      set_decay(parameter[3]);
    }

  private:
    // `source` arrives as a uint16_t but in practice originates from a
    // 12-bit ADC reading scaled up to the full uint16_t range (see
    // AdcToParameter() in core1_main.cpp), so only ~4096 distinct raw
    // values are ever actually seen. The mapping below is designed to
    // degrade gracefully with that coarser resolution rather than assume
    // every one of the 65536 possible values is reachable.
    //
    // The pot's travel is split into two halves:
    //  - The first half ([0, kSourceRangeHalf)) sweeps through the
    //    kNsamples (12) original samples one at a time -- each position
    //    selects exactly one of kSamples[0..11] as the sole active
    //    source.
    //  - The second half ([kSourceRangeHalf, 65535]) sweeps through
    //    every other possible combination of two or more samples played
    //    together (i.e. every bitmask over the 12 samples except the
    //    empty set and the 12 singles already covered by the first
    //    half).
    void set_source(uint16_t source)
    {
      if (source < kSourceRangeHalf)
      {
        uint32_t index =
            (static_cast<uint32_t>(source) * kNsamples) / kSourceRangeHalf;
        if (index >= kNsamples)
        {
          index = kNsamples - 1;
        }
        source_ = static_cast<uint16_t>(1u << index);
      }
      else
      {
        const uint32_t relative = source - kSourceRangeHalf;
        // kSourceRangeHalf == 65536 - kSourceRangeHalf, i.e. both halves
        // of the uint16_t range are the same size.
        uint32_t combo_index =
            (relative * kNumOtherCombos) / kSourceRangeHalf;
        if (combo_index >= kNumOtherCombos)
        {
          combo_index = kNumOtherCombos - 1;
        }
        // O(1) table lookup -- see kMultiSampleMasks -- so turning the
        // pot rapidly (or a noisy/jittery ADC re-sending the same
        // reading many times) never costs more than a single array
        // access, however far into the "combinations" half of the range
        // it lands.
        source_ = kMultiSampleMasks[combo_index];
      }
      sample_table_.Init(source_);
    }
    
    void set_density(uint16_t density)
    {
      density_ = density;
      mean_interonset_samples_ = kSampleRate / (density_ == 0 ? 1 : density_);
    }


    // `spread` (0..65535) controls how much randomness is injected into
    // each new grain: start position and playback rate (see
    // RandomizedStartPosition()/RandomizedPhaseIncrement()), duration
    // (see RandomizedGrainDuration()), and now also the probability that
    // a grain reads its source backwards (see RandomizedReverse()) -- 0%
    // of grains reversed at spread == 0, up to effectively all of them at
    // spread == 65535.
    void set_spread(uint16_t spread)
    {
      spread_ = spread;
    }

    // `decay` is a straight line with a negative slope: it starts every
    // gate trigger at full scale (kEnvelopeFullScale) and ramps linearly
    // down to 0 over decay_time_samples, which decay itself controls --
    // the higher decay is, the longer that line takes to reach 0 (a
    // slower, gentler slope). The line's current value (envelope_) is
    // read once per output sample in Process() and used two ways: (1) it
    // directly scales the mixed grain output (a simple amplitude
    // envelope over the whole burst), and (2) it lengthens the effective
    // mean interonset time in ActivateGrains() as it falls, so grains
    // fire less often as the sound decays. It does not affect individual
    // grain duration -- that's controlled by spread_ instead, see
    // RandomizedGrainDuration().
    void set_decay(uint16_t decay)
    {
      decay_ = decay;
      const uint32_t decay_time_samples =
          kMinDecaySamples +
          ((static_cast<uint64_t>(decay_) *
            static_cast<uint64_t>(kMaxDecaySamples - kMinDecaySamples)) >>
           16);
      // envelope_decrement_ is how much envelope_ (a Q16 value, kEnvelope
      // FullScale == 1.0) drops every output sample so that it reaches 0
      // after exactly decay_time_samples samples. Clamped to at least 1
      // so the line always eventually reaches 0 rather than stalling.
      envelope_decrement_ = kEnvelopeFullScale / decay_time_samples;
      if (envelope_decrement_ == 0)
      {
        envelope_decrement_ = 1;
      }
    }

    void ActivateGrains();

    // Returns a pointer to the first currently-inactive Grain in
    // grain_pool_, or nullptr if every slot is already active. kNGrains
    // therefore acts as a hard ceiling on concurrent grains: if density_
    // pushes onsets faster than grains naturally finish, new onsets are
    // simply dropped once the pool is full rather than stealing an
    // in-progress grain's voice.
    Grain *FindFreeGrainSlot();

    // Picks a start position within [0, spread_] (as a 0..65535 fraction of
    // the selected sample's length -- see GrainSource::Essence::
    // set_start_position()). spread_ == 0 always starts at the very first
    // sample; spread_ == 65535 allows the grain to start anywhere across
    // the whole sample.
    uint16_t RandomizedStartPosition() const;

    // Picks a playback rate (Q24.8, see GrainSource::kFractionalBits)
    // randomly deviating from the nominal (native pitch) rate by up to one
    // octave in either direction at spread_ == 65535, and exactly the
    // nominal rate at spread_ == 0.
    uint32_t RandomizedPhaseIncrement() const;

    // Picks a grain duration (in samples), randomly deviating from the
    // nominal kGrainDurationSamples (20ms) by up to
    // kMaxDurationDeviationSamples in either direction at spread_ ==
    // 65535, and exactly the nominal duration at spread_ == 0. Unlike the
    // overall decay envelope (see set_decay()), spread_ is what controls
    // per-grain duration variance.
    uint32_t RandomizedGrainDuration() const;

    // Decides whether the next grain should read its source in reverse.
    // spread_ acts as a probability (0 == never, 65535 == almost always),
    // but mapped through a squared ("log taper") curve rather than
    // linearly: reverse grains stay rare across most of spread_'s range
    // and only become common as it approaches its maximum. See the .cpp
    // for the exact curve.
    bool RandomizedReverse() const;

    // Smoothly bends the summed/enveloped grain output (which, with many
    // grains active at once, easily exceeds the +/-32767 range a single
    // int16_t output sample can hold) back down towards that range,
    // instead of hard-clipping it flat the instant it crosses the
    // boundary. See the .cpp for the exact curve and rationale.
    static int32_t SoftLimit(int32_t x);

    // Below this magnitude, SoftLimit() is a transparent no-op (this is
    // where the vast majority of typical grain overlap -- one to a few
    // grains -- always lands). 80% of full scale leaves enough headroom
    // above it for the limiter curve to do meaningful, audible work
    // before output would otherwise have clipped.
    static constexpr int32_t kLimiterCeiling = 32767;
    static constexpr int32_t kLimiterThreshold = kLimiterCeiling * 4 / 5;
    static constexpr int32_t kLimiterRange =
        kLimiterCeiling - kLimiterThreshold;

    // Nominal (native-pitch) playback rate: GrainSource reads one source
    // sample per output sample.
    static constexpr uint32_t kNominalPhaseIncrement =
        1UL << GrainSource::kFractionalBits;

    // Largest allowed deviation from kNominalPhaseIncrement (at maximum
    // spread_): +/- one octave, i.e. the rate may range from half to
    // double the nominal rate.
    static constexpr uint32_t kMaxRateDeviation = kNominalPhaseIncrement;
    static constexpr uint32_t kMinPhaseIncrement =
        kNominalPhaseIncrement - kMaxRateDeviation;
    static constexpr uint32_t kMaxPhaseIncrement =
        kNominalPhaseIncrement + kMaxRateDeviation;

    // Random::GetSample() spans [-32768, 32767] and spread_ spans
    // [0, 65535]; their product needs a 16-bit shift to normalize spread_
    // back to a [0, 1] fraction, plus a further 7-bit shift to map the
    // resulting +/-32768 range down to +/-kMaxRateDeviation (32768 >> 7 ==
    // 256 == kNominalPhaseIncrement).
    static constexpr int kRateDeviationShift = 16 + 7;

    static constexpr uint16_t kSampleRate = 44100;

    // Nominal length of a single grain, in output samples, independent of
    // how long the underlying sample is or what rate it's played back at
    // -- this is what makes each grain a short burst of the sample (per
    // Ross Bencina's granular synthesis model) instead of playing the
    // whole thing. Fixed at a standard 20ms; not itself controlled by
    // decay_ or exposed as its own knob/parameter. Actual per-grain
    // duration varies around this nominal value according to spread_ --
    // see RandomizedGrainDuration().
    static constexpr uint32_t kGrainDurationSamples =
        static_cast<uint32_t>(kSampleRate * 20 / 1000);

    // Largest allowed deviation from kGrainDurationSamples (at maximum
    // spread_): +/- half of the nominal duration, i.e. actual grain
    // duration may range from half to 1.5x the nominal 20ms value.
    static constexpr uint32_t kMaxDurationDeviationSamples =
        kGrainDurationSamples / 2;

    // Random::GetSample() spans [-32768, 32767] and spread_ spans
    // [0, 65535]; their product spans roughly +/-2^31. Shifting by 22
    // maps that down to roughly +/-2^9 (in the low hundreds of samples at
    // 44.1kHz), which approximates kMaxDurationDeviationSamples closely
    // enough for a randomized deviation -- exact precision doesn't matter
    // here since RandomizedGrainDuration() clamps the result afterwards
    // to a sane range anyway.
    static constexpr int kDurationDeviationShift = 22;

    // Length of each grain's attack/release ramps (see Envelope), in
    // samples -- short enough to avoid audible clicks at grain
    // boundaries without eating too much of a typical (20ms)
    // kGrainDurationSamples grain into fades. If a grain's actual
    // duration (see RandomizedGrainDuration()) ends up shorter than
    // attack+release, Envelope::Init() scales both down proportionally
    // so they still exactly fill the grain with no clicks, just a
    // steeper ramp.
    static constexpr uint32_t kGrainAttackSamples =
        static_cast<uint32_t>(kSampleRate * 5 / 1000);
    static constexpr uint32_t kGrainReleaseSamples =
        static_cast<uint32_t>(kSampleRate * 5 / 1000);

    // Fixed-point (Q16) representation of the decay envelope's full-scale
    // value (gain 1.0 / 100% density-and-duration).
    static constexpr uint32_t kEnvelopeFullScale = 1UL << 16;

    // Range of decay times (in samples) that decay_ (0..65535) is mapped
    // to, linearly: decay_ == 0 gives the shortest/fastest decay,
    // decay_ == 65535 the longest/slowest one.
    static constexpr uint32_t kMinDecaySamples = kSampleRate / 20;      // 50ms
    static constexpr uint32_t kMaxDecaySamples = kSampleRate * 3;       // 3s

    static constexpr uint16_t kSourceMask =
        static_cast<uint16_t>((1u << kNsamples) - 1);

    // Midpoint of the uint16_t parameter range, splitting set_source()'s
    // input into the "single sample" first half and the "combination"
    // second half. Both halves are the same size (65536 - kSourceRangeHalf
    // == kSourceRangeHalf).
    static constexpr uint32_t kSourceRangeHalf = 1UL << 15;

    // Number of distinct multi-sample combinations available in the
    // second half of the source pot's range: every non-empty bitmask over
    // the kNsamples samples (kSourceMask of them), minus the kNsamples
    // single-bit masks already covered by the first half. Must match
    // clap_engine_internal::kNumMultiSampleMasks (computed the same way).
    static constexpr uint32_t kNumOtherCombos =
        clap_engine_internal::kNumMultiSampleMasks;

    // See clap_engine_internal::BuildMultiSampleMasks().
    static constexpr std::array<uint16_t, kNumOtherCombos> kMultiSampleMasks =
        clap_engine_internal::BuildMultiSampleMasks();

    // Bitmask selecting only kSamples[0] -- the fallback when set_source()
    // is called with 0 (i.e. no bits explicitly set).
    static constexpr uint16_t kDefaultSourceMask = 1u;
    uint16_t source_;
    static constexpr uint32_t kNegLogFractionalBits = 12;
    static constexpr size_t kNGrains = 12;

    SampleTable sample_table_;
    GrainSource::Essence grain_essence_;
    Envelope::Essence envelope_essence_;
    Grain grain_pool_[kNGrains];

    uint16_t density_;
    uint32_t interonset_time_;
    uint32_t mean_interonset_samples_;
    uint16_t spread_;
    uint16_t decay_;

    // Current value of the decay envelope line (Q16, kEnvelopeFullScale ==
    // 1.0 / full density-duration-gain). Reset to kEnvelopeFullScale on
    // every gate trigger and ramped down by envelope_decrement_ every
    // output sample until it reaches 0.
    uint32_t envelope_;

    // Per-sample decrement applied to envelope_ -- see set_decay().
    uint32_t envelope_decrement_;

    // Sample-accurate countdown to the next grain onset: decremented once
    // per output sample in Process(); a new grain is activated (and this
    // reloaded from interonset_time_) whenever it reaches zero. Signed so
    // it can go slightly negative (absorbed via += rather than = on
    // reload) without losing timing accuracy when a block boundary falls
    // mid-interval.
    int32_t samples_until_next_grain_;

    ClapEngine(const ClapEngine &) = delete;
    const ClapEngine &operator=(const ClapEngine &) = delete;
  };

} // namespace peaks
