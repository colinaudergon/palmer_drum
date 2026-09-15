// Stored-sample Grain Source, following the architecture described in Ross
// Bencina's "Implementing Real-Time Granular Synthesis" (Audio Anecdotes
// III, 2001-2002).
//
// GrainSource plays the role of "Source" in that article: given its current
// per-grain playback state (read position and rate), it synthesizes the
// waveform for a single grain by reading from a sample buffer shared by
// every other GrainSource that happens to be reading from the same sample.
//
// Initialization parameters for a GrainSource (which sample to read, where
// to start reading, at what rate, ...) are grouped into a nested
// GrainSource::Essence object rather than being passed individually,
// following the "Essence" design pattern (Andy Carlson, "Essence," in
// Pattern Languages of Program Design 4, eds. N. Harrison, B. Foote and
// H. Rohnert, Addison-Wesley, 2000, 33-40) as applied to Grain
// initialization in Bencina's article. This keeps GrainSource itself a
// small, cheaply-recycled object holding only live per-grain state, while
// whichever component decides how new grains should be parameterized (e.g.
// GrainScheduler, driven by the "source"/"spread" parameters) owns and
// mutates a single reusable Essence instance that is handed to
// GrainSource::Init() each time a new grain is activated.

#pragma once

#include <array>
#include <cstddef>

#include "../../../lib/mu_stmlib.h"
#include "../samples/samples.h"
#include "i_sample_table.h"

namespace peaks
{

  namespace sample_table_internal
  {
    // Builds, at compile time, the table mapping each 0-based index in
    // [0, kNumMultiSampleMasks) to the index-th bitmask (in ascending
    // numeric order) among all masks over kNsamples bits that have two or
    // more bits set -- i.e. every sample combination not reachable as a
    // single sample (see SampleTable::MapPotToActiveMask()). Computing
    // this once at compile time (rather than scanning candidate masks
    // every time MapPotToActiveMask() is called) means a rapid pot
    // sweep, or a noisy ADC re-sending readings, can never cost more
    // than a single array lookup at runtime -- important since it runs
    // on the same core that also keeps the audio codec's DMA buffer fed.
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
  } // namespace sample_table_internal

  // Shared, read-only sample data that any number of GrainSource instances
  // may read from concurrently, each at its own independent position and
  // rate. This is the "SourceData" role from Bencina's architecture (a
  // wavetable, in the Stored Sample Granulator case) -- it is registered
  // once with a GrainSource::Essence and then referenced (never copied) by
  // every Grain that reads from it.
  //
  // Unlike a single fixed wavetable, SampleTable represents a *bank* of
  // zero or more entries of kSamples (clap_engine/samples/samples.h),
  // selected via a bitmask -- mirroring ClapEngine's set_source(), where
  // any combination of the 12 available samples may be active at once.
  // Each individual grain still only ever reads from exactly one of those
  // entries at a time (see GrainSource::Essence::set_sample_index()); the
  // bank simply lets whichever component activates grains (e.g.
  // GrainScheduler) pick a different active sample per grain, so several
  // samples can be "in rotation" as sources concurrently.
  class SampleTable : public ISampleTable
  {
  public:
    SampleTable() : active_mask_(0) {}

    // Builds a SampleTable whose active set is given by active_mask: bit i
    // means kSamples[i] is one of the samples available to be selected for
    // a grain. Bits beyond kNsamples are ignored.
    explicit SampleTable(uint16_t active_mask) { Init(active_mask); }

    inline void Init(uint32_t active_mask) override
    {
      active_mask_ = static_cast<uint16_t>(active_mask) & kAllSamplesMask;
    }

    inline uint16_t active_mask() const { return active_mask_; }

    // True as long as at least one sample is active in this bank.
    inline bool valid() const override { return active_mask_ != 0; }

    // True if kSamples[index] is part of this bank's active set.
    inline bool IsActive(size_t index) const override
    {
      return index < kNsamples && ((active_mask_ >> index) & 1u) != 0;
    }

    // Number of samples currently active in this bank (population count of
    // active_mask()).
    inline size_t active_count() const override
    {
      size_t count = 0;
      for (uint16_t mask = active_mask_; mask != 0; mask >>= 1)
      {
        count += mask & 1u;
      }
      return count;
    }

    // Maps n (0 .. active_count()-1) to the actual kSamples[] index of the
    // n-th active sample, in ascending index order. Returns kNsamples if n
    // is out of range (e.g. the bank is empty).
    inline size_t NthActiveIndex(size_t n) const override
    {
      for (size_t index = 0; index < kNsamples; ++index)
      {
        if (IsActive(index))
        {
          if (n == 0)
          {
            return index;
          }
          --n;
        }
      }
      return kNsamples;
    }

    // Direct, bounds-checked access to one of kSamples by its raw index --
    // regardless of whether that index is currently marked active. Used by
    // GrainSource once a specific index has already been chosen (e.g. via
    // NthActiveIndex()).
    //
    // Deliberately *not* defined inline here: per the Itanium C++ ABI (used
    // by GCC), a polymorphic class with every virtual function defined
    // inline in its header has no single "key function" to anchor its
    // vtable to, so the vtable (and every inline virtual function's body)
    // gets emitted as a weak/COMDAT symbol in *every* translation unit that
    // constructs a SampleTable. Since data()'s body references kSamples --
    // itself only reachable via samples.h's internal-linkage (static)
    // arrays -- each of those duplicate emissions would drag its own
    // private copy of every sample's audio data into that TU's object
    // file, ballooning flash usage far past a single copy. Defining data()
    // out-of-line (in grain_source.cpp) makes it this class's key
    // function, pinning the vtable -- and the one real copy of this
    // function's body -- to that single translation unit instead.
    const int16_t *data(size_t index) const override;

    size_t size(size_t index) const override;

    // `pot_value` arrives as a uint16_t but in practice originates from a
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
    uint32_t MapPotToActiveMask(uint16_t pot_value) const override;

  private:
    static constexpr uint16_t kAllSamplesMask =
        static_cast<uint16_t>((1u << kNsamples) - 1);

    // Midpoint of the uint16_t parameter range, splitting
    // MapPotToActiveMask()'s input into the "single sample" first half
    // and the "combination" second half. Both halves are the same size
    // (65536 - kSourceRangeHalf == kSourceRangeHalf).
    static constexpr uint32_t kSourceRangeHalf = 1UL << 15;

    // Number of distinct multi-sample combinations available in the
    // second half of the pot's range: every non-empty bitmask over the
    // kNsamples samples (kAllSamplesMask of them), minus the kNsamples
    // single-bit masks already covered by the first half. Must match
    // sample_table_internal::kNumMultiSampleMasks (computed the same
    // way).
    static constexpr uint32_t kNumOtherCombos =
        sample_table_internal::kNumMultiSampleMasks;

    // See sample_table_internal::BuildMultiSampleMasks().
    static constexpr std::array<uint16_t, kNumOtherCombos> kMultiSampleMasks =
        sample_table_internal::BuildMultiSampleMasks();

    uint16_t active_mask_;
  };

  // Synthesizes the waveform for a single grain by reading (with linear
  // interpolation) from a shared ISampleTable (SampleTable, SampleTable
  // FromBins, ...).
  class GrainSource
  {
  public:
    // Stores the initialization parameters needed to (re)start a
    // GrainSource when a new grain is activated: which ISampleTable to
    // read from, where in it to start, at what rate, and in which
    // direction.
    //
    // A single Essence instance is typically owned and mutated by whatever
    // object decides how successive grains should be parameterized (e.g.
    // GrainScheduler, following the Scheduler -> Grain::Activate(essence)
    // -> Source::Init(essence) collaboration described in the article),
    // then handed by reference to GrainSource::Init(). This keeps
    // GrainSource free of any knowledge of *how* its parameters are chosen
    // (fixed, random within a "spread" range, sequenced, ...) -- it only
    // ever consumes the resulting Essence.
    class Essence
    {
    public:
      Essence()
          : sample_table_(nullptr),
            sample_index_(0),
            start_position_(0),
            phase_increment_(1UL << kFractionalBits),
            duration_samples_(0xFFFFFFFFUL),
            reverse_(false) {}

      // Registers the shared bank of samples grains initialized from this
      // Essence may read from. Mirrors registering a shared DelayLine or
      // StoredSample with a Scheduler's sourceEssence in the article.
      // Accepts any ISampleTable implementation (SampleTable,
      // SampleTableFromBins, ...).
      inline void set_sample_table(const ISampleTable *sample_table)
      {
        sample_table_ = sample_table;
      }

      // Selects which entry of the bank (a raw kSamples index) the next
      // grain initialized from this Essence should read from. The caller
      // (e.g. GrainScheduler) is responsible for choosing this from among
      // sample_table()'s currently active indices -- for example via
      // sample_table()->NthActiveIndex(random(0, sample_table()->
      // active_count())).
      inline void set_sample_index(uint16_t sample_index)
      {
        sample_index_ = sample_index;
      }

      // Start position expressed as a fraction of the sample table's
      // length: 0 = first sample, 65535 = last sample. Using a
      // length-independent fraction lets a caller (e.g. a "spread"
      // parameter) scan across any registered sample without needing to
      // know its actual length in samples.
      inline void set_start_position(uint16_t start_position)
      {
        start_position_ = start_position;
      }

      // Playback rate, as a fixed-point Q24.8 ratio (1 << 8 == 256 plays
      // back at the sample's native rate; smaller/larger values transpose
      // the grain down/up).
      inline void set_phase_increment(uint32_t phase_increment)
      {
        phase_increment_ = phase_increment;
      }

      inline void set_reverse(bool reverse) { reverse_ = reverse; }

      // How many output samples this grain should play for, regardless of
      // how much of the underlying sample buffer would otherwise be
      // available -- this is what turns "play the whole sample" into "play
      // a short burst/grain" as described in Bencina's article (a grain's
      // duration is independent of its source material's length). Defaults
      // to 0xFFFFFFFF (effectively unbounded), so a grain that's never had
      // this set behaves exactly as before: it plays until the underlying
      // sample data itself is exhausted.
      inline void set_duration_samples(uint32_t duration_samples)
      {
        duration_samples_ = duration_samples;
      }

      inline const ISampleTable *sample_table() const { return sample_table_; }
      inline uint16_t sample_index() const { return sample_index_; }
      inline uint16_t start_position() const { return start_position_; }
      inline uint32_t phase_increment() const { return phase_increment_; }
      inline bool reverse() const { return reverse_; }
      inline uint32_t duration_samples() const { return duration_samples_; }

    private:
      const ISampleTable *sample_table_;
      uint16_t sample_index_;
      uint16_t start_position_;
      uint32_t phase_increment_;
      uint32_t duration_samples_;
      bool reverse_;
    };

    // Number of fractional bits used by the internal Q24.8 read-position
    // phase (24 integer bits => sample tables of up to 2^24 samples are
    // addressable, ~380 seconds at 44.1kHz; 8 fractional bits give the
    // linear interpolator 1/256-sample precision).
    static constexpr uint32_t kFractionalBits = 8;

    GrainSource() { Reset(); }

    // Initializes this GrainSource's per-grain playback state (read
    // position, rate, direction) from an Essence. Called when a grain is
    // (re)activated -- mirrors Source::init(essence) in the article's
    // sequence diagram.
    void Init(const Essence &essence);

    // Produces the next audio sample for this grain and advances its read
    // position accordingly. Should be called once per audio sample for the
    // lifetime of the grain.
    int16_t Synthesize();

    // True once the underlying sample data has been exhausted (the read
    // position has reached the start/end of the selected sample), or no
    // valid sample was selected (no SampleTable registered, or its
    // sample_index() is not one of the bank's active entries). The owning
    // Grain should terminate itself once this returns true, regardless of
    // its own envelope/duration state, since there is no more source
    // material left to read.
    inline bool Done() const { return done_; }

  private:
    void Reset();

    const int16_t *data_; // Resolved sample buffer for this grain (cached
                           // from sample_table + sample_index at Init()
                           // time so Synthesize()'s hot per-sample loop
                           // doesn't need to re-resolve or re-bounds-check
                           // it on every call).
    uint32_t size_;        // Sample count of data_.
    uint32_t phase_;           // Q24.8 fixed-point read position, in samples.
    uint32_t phase_increment_; // Q24.8 fixed-point playback rate.
    uint32_t remaining_duration_; // Output samples left to play before this
                                   // grain self-retires, independent of how
                                   // much source material remains -- this is
                                   // what bounds a grain to a short burst
                                   // instead of playing the whole sample.
    bool reverse_;
    bool done_;

    GrainSource(const GrainSource &) = delete;
    const GrainSource &operator=(const GrainSource &) = delete;
  };

} // namespace peaks