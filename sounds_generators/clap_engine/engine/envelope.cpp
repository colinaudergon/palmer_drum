// Envelope implementation. See envelope.h for design notes.

#include "envelope.h"

namespace peaks
{

  void Envelope::Reset()
  {
    segment_ = Segment::kDone;
    amplitude_ = 0;
    increment_ = 0;
    samples_left_in_segment_ = 0;
    attack_samples_ = 0;
    sustain_samples_ = 0;
    release_samples_ = 0;
    peak_amplitude_ = 0;
    done_ = true;
  }

  void Envelope::EnterSegment(Segment segment)
  {
    segment_ = segment;
    switch (segment_)
    {
    case Segment::kAttack:
      samples_left_in_segment_ = attack_samples_;
      amplitude_ = 0;
      increment_ = static_cast<int32_t>(peak_amplitude_ / attack_samples_);
      break;

    case Segment::kSustain:
      samples_left_in_segment_ = sustain_samples_;
      amplitude_ = static_cast<int32_t>(peak_amplitude_);
      increment_ = 0;
      break;

    case Segment::kRelease:
      samples_left_in_segment_ = release_samples_;
      amplitude_ = static_cast<int32_t>(peak_amplitude_);
      increment_ = -static_cast<int32_t>(peak_amplitude_ / release_samples_);
      break;

    case Segment::kDone:
    default:
      samples_left_in_segment_ = 0;
      amplitude_ = 0;
      increment_ = 0;
      done_ = true;
      break;
    }
  }

  void Envelope::Init(const Essence &essence)
  {
    const uint32_t duration = essence.duration_samples();
    uint32_t attack = essence.attack_samples();
    uint32_t release = essence.release_samples();

    // If the requested attack+release would overrun the grain's total
    // duration (e.g. a short grain with comparatively long fixed
    // attack/release times), scale both down proportionally so they
    // exactly fill the grain instead, with no sustain segment.
    const uint64_t requested =
        static_cast<uint64_t>(attack) + static_cast<uint64_t>(release);
    if (requested > duration && requested > 0)
    {
      attack = static_cast<uint32_t>(
          (static_cast<uint64_t>(attack) * duration) / requested);
      release = duration - attack;
    }

    attack_samples_ = attack;
    release_samples_ = release;
    sustain_samples_ = duration - attack_samples_ - release_samples_;
    peak_amplitude_ = essence.amplitude();

    done_ = duration == 0;

    if (done_)
    {
      EnterSegment(Segment::kDone);
      return;
    }

    if (attack_samples_ > 0)
    {
      EnterSegment(Segment::kAttack);
    }
    else if (sustain_samples_ > 0)
    {
      EnterSegment(Segment::kSustain);
    }
    else if (release_samples_ > 0)
    {
      EnterSegment(Segment::kRelease);
    }
    else
    {
      EnterSegment(Segment::kDone);
    }
  }

  uint32_t Envelope::Synthesize()
  {
    if (done_)
    {
      return 0;
    }

    const uint32_t output =
        static_cast<uint32_t>(amplitude_ < 0 ? 0 : amplitude_);

    amplitude_ += increment_;
    --samples_left_in_segment_;

    if (samples_left_in_segment_ == 0)
    {
      switch (segment_)
      {
      case Segment::kAttack:
        if (sustain_samples_ > 0)
        {
          EnterSegment(Segment::kSustain);
        }
        else if (release_samples_ > 0)
        {
          EnterSegment(Segment::kRelease);
        }
        else
        {
          EnterSegment(Segment::kDone);
        }
        break;

      case Segment::kSustain:
        if (release_samples_ > 0)
        {
          EnterSegment(Segment::kRelease);
        }
        else
        {
          EnterSegment(Segment::kDone);
        }
        break;

      case Segment::kRelease:
      default:
        EnterSegment(Segment::kDone);
        break;
      }
    }

    return output;
  }

} // namespace peaks
