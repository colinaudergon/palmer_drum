
// sample based clap engine
#pragma once

#include "../../lib/mu_stmlib.h"
#include "../../lib/dsp.h"

#include "../i_processor.h"
#include "../peaks_ressources/gate_processor.h"
#include "samples/samples.h"
#include "engine/grain.h"
#include "engine/grain_source.h"
#include "../peaks_ressources/random.h"
#include "engine/lut_neg_log.h"

namespace peaks
{

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
      // set_decay(parameter[3]);
    }

  private:
    void set_source(uint16_t source)
    {
      // `source` is treated as a bitmask over the kMaxInputSource (12)
      // entries of kSamples: bit i (source & (1 << i)) toggles whether
      // kSamples[i] is part of the currently active set of grain sources.
      // Several bits may be set at once -- e.g. source == 5 (0b101)
      // activates kSamples[0] and kSamples[2] together. Only the low
      // kMaxInputSource bits are meaningful, so the value is masked to
      // that range (0 .. 2^kMaxInputSource - 1).
      //
      // A raw value of 0 (no bits set) is special-cased to always select
      // kSamples[0], so the engine never ends up with zero active sources
      // -- kSamples[0] acts as the default sample.
      source_ = (source == 0) ? kDefaultSourceMask : (source & kSourceMask);
      sample_table_.Init(source_);
    }
    
    void set_density(uint16_t density)
    {
      density_ = density;
      mean_interonset_samples_ = kSampleRate / (density_ == 0 ? 1 : density_);
    }


    void set_spread(uint16_t spread)
    {
      spread_ = spread;
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
    static constexpr uint16_t kSourceMask =
        static_cast<uint16_t>((1u << kNsamples) - 1);

    // Bitmask selecting only kSamples[0] -- the fallback when set_source()
    // is called with 0 (i.e. no bits explicitly set).
    static constexpr uint16_t kDefaultSourceMask = 1u;
    uint16_t source_;
    static constexpr uint32_t kNegLogFractionalBits = 12;
    static constexpr size_t kNGrains = 10;

    SampleTable sample_table_;
    GrainSource::Essence grain_essence_;
    Grain grain_pool_[kNGrains];

    uint16_t density_;
    uint32_t interonset_time_;
    uint32_t mean_interonset_samples_;
    uint16_t spread_;

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
