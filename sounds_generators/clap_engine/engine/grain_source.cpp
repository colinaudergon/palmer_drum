// GrainSource implementation. See grain_source.h for design notes.

#include "grain_source.h"

namespace peaks
{

  void GrainSource::Reset()
  {
    data_ = nullptr;
    size_ = 0;
    phase_ = 0;
    phase_increment_ = 1UL << kFractionalBits;
    reverse_ = false;
    done_ = true;
  }

  void GrainSource::Init(const Essence &essence)
  {
    phase_increment_ = essence.phase_increment();
    reverse_ = essence.reverse();

    const SampleTable *sample_table = essence.sample_table();
    const uint16_t sample_index = essence.sample_index();

    // Resolve (and cache) which underlying buffer this grain reads from.
    // Only an index that the bank actually marks active is accepted --
    // this is what lets several samples be "in rotation" as sources
    // without ever reading stale/unselected data.
    const bool sample_selected =
        sample_table != nullptr && sample_table->IsActive(sample_index);

    data_ = sample_selected ? sample_table->data(sample_index) : nullptr;
    size_ = sample_selected ? static_cast<uint32_t>(sample_table->size(sample_index)) : 0;

    done_ = data_ == nullptr || size_ <= 1;

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
