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

#include <cstddef>

#include "../../../lib/mu_stmlib.h"
#include "../samples/samples.h"

namespace peaks
{

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
  class SampleTable
  {
  public:
    SampleTable() : active_mask_(0) {}

    // Builds a SampleTable whose active set is given by active_mask: bit i
    // means kSamples[i] is one of the samples available to be selected for
    // a grain. Bits beyond kNsamples are ignored.
    explicit SampleTable(uint16_t active_mask) { Init(active_mask); }

    inline void Init(uint16_t active_mask)
    {
      active_mask_ = active_mask & kAllSamplesMask;
    }

    inline uint16_t active_mask() const { return active_mask_; }

    // True as long as at least one sample is active in this bank.
    inline bool valid() const { return active_mask_ != 0; }

    // True if kSamples[index] is part of this bank's active set.
    inline bool IsActive(size_t index) const
    {
      return index < kNsamples && ((active_mask_ >> index) & 1u) != 0;
    }

    // Number of samples currently active in this bank (population count of
    // active_mask()).
    inline size_t active_count() const
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
    inline size_t NthActiveIndex(size_t n) const
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
    inline const int16_t *data(size_t index) const
    {
      return index < kNsamples ? kSamples[index].samples : nullptr;
    }

    inline size_t size(size_t index) const
    {
      return index < kNsamples ? kSamples[index].lenght : 0;
    }

  private:
    static constexpr uint16_t kAllSamplesMask =
        static_cast<uint16_t>((1u << kNsamples) - 1);

    uint16_t active_mask_;
  };

  // Synthesizes the waveform for a single grain by reading (with linear
  // interpolation) from a shared SampleTable.
  class GrainSource
  {
  public:
    // Stores the initialization parameters needed to (re)start a
    // GrainSource when a new grain is activated: which SampleTable to read
    // from, where in it to start, at what rate, and in which direction.
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
      inline void set_sample_table(const SampleTable *sample_table)
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

      inline const SampleTable *sample_table() const { return sample_table_; }
      inline uint16_t sample_index() const { return sample_index_; }
      inline uint16_t start_position() const { return start_position_; }
      inline uint32_t phase_increment() const { return phase_increment_; }
      inline bool reverse() const { return reverse_; }
      inline uint32_t duration_samples() const { return duration_samples_; }

    private:
      const SampleTable *sample_table_;
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