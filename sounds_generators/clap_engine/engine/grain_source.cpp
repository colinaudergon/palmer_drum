// GrainSource implementation. See grain_source.h for design notes.

#include "grain_source.h"

namespace peaks
{

  // Defined out-of-line so this is SampleTable's "key function" (see the
  // comment on data()'s declaration in grain_source.h) -- pins the vtable,
  // and the one real copy of kSamples-referencing code, to this single
  // translation unit.
  const int16_t *SampleTable::data(size_t index) const
  {
    return index < kNsamples ? kSamples[index].samples : nullptr;
  }

  size_t SampleTable::size(size_t index) const
  {
    return index < kNsamples ? kSamples[index].lenght : 0;
  }

  uint32_t SampleTable::MapPotToActiveMask(uint16_t pot_value) const
  {
    if (pot_value < kSourceRangeHalf)
    {
      uint32_t index =
          (static_cast<uint32_t>(pot_value) * kNsamples) / kSourceRangeHalf;
      if (index >= kNsamples)
      {
        index = kNsamples - 1;
      }
      return 1u << index;
    }

    const uint32_t relative = pot_value - kSourceRangeHalf;
    // kSourceRangeHalf == 65536 - kSourceRangeHalf, i.e. both halves of
    // the uint16_t range are the same size.
    uint32_t combo_index = (relative * kNumOtherCombos) / kSourceRangeHalf;
    if (combo_index >= kNumOtherCombos)
    {
      combo_index = kNumOtherCombos - 1;
    }
    // O(1) table lookup -- see kMultiSampleMasks -- so turning the pot
    // rapidly (or a noisy/jittery ADC re-sending the same reading many
    // times) never costs more than a single array access, however far
    // into the "combinations" half of the range it lands.
    return kMultiSampleMasks[combo_index];
  }

  void GrainSource::Reset()
  {
    data_ = nullptr;
    size_ = 0;
    phase_ = 0;
    phase_increment_ = 1UL << kFractionalBits;
    remaining_duration_ = 0;
    reverse_ = false;
    done_ = true;
  }

  void GrainSource::Init(const Essence &essence)
  {
    phase_increment_ = essence.phase_increment();
    reverse_ = essence.reverse();
    remaining_duration_ = essence.duration_samples();

    const ISampleTable *sample_table = essence.sample_table();
    const uint16_t sample_index = essence.sample_index();

    // Resolve (and cache) which underlying buffer this grain reads from.
    // Only an index that the bank actually marks active is accepted --
    // this is what lets several samples be "in rotation" as sources
    // without ever reading stale/unselected data.
    const bool sample_selected =
        sample_table != nullptr && sample_table->IsActive(sample_index);

    data_ = sample_selected ? sample_table->data(sample_index) : nullptr;
    size_ = sample_selected ? static_cast<uint32_t>(sample_table->size(sample_index)) : 0;

    done_ = data_ == nullptr || size_ <= 1 || remaining_duration_ == 0;

    if (done_)
    {
      phase_ = 0;
      return;
    }

    // Map the Essence's length-independent start_position (0..65535) onto
    // an absolute Q24.8 read position within the selected sample.
    const uint32_t last_sample = size_ - 1;
    const uint64_t start_sample =
        (static_cast<uint64_t>(essence.start_position()) *
         static_cast<uint64_t>(last_sample)) >>
        16;

    phase_ = static_cast<uint32_t>(start_sample) << kFractionalBits;
  }

  int16_t GrainSource::Synthesize()
  {
    if (done_)
    {
      return 0;
    }

    const uint32_t index = phase_ >> kFractionalBits;
    const uint32_t frac = phase_ & ((1UL << kFractionalBits) - 1);

    // Linearly interpolate between the sample at the current (fractional)
    // read position and the next one.
    int32_t a = data_[index];
    int32_t b = (index + 1 < size_) ? data_[index + 1] : data_[index];
    int16_t sample = static_cast<int16_t>(
        a + ((b - a) * static_cast<int32_t>(frac) >> kFractionalBits));

    // A grain's duration is counted in output samples, independent of how
    // fast phase_ is advancing through the source material (i.e.
    // independent of phase_increment_/pitch) -- this is what lets a grain
    // play only a short burst of a sample rather than the whole thing.
    --remaining_duration_;
    if (remaining_duration_ == 0)
    {
      done_ = true;
      return sample;
    }

    // Advance the read position and detect exhaustion of the source
    // material, in either playback direction.
    if (reverse_)
    {
      if (phase_ < phase_increment_)
      {
        done_ = true;
      }
      else
      {
        phase_ -= phase_increment_;
      }
    }
    else
    {
      phase_ += phase_increment_;
      if ((phase_ >> kFractionalBits) + 1 >= size_)
      {
        done_ = true;
      }
    }

    return sample;
  }

} // namespace peaks
