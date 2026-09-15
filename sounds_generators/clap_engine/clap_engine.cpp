// Copyright 2026 colinaudergon.
//
// Author: colinaudergon
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// -----------------------------------------------------------------------------

#include "clap_engine.h"

#include <algorithm>

namespace peaks
{

  void ClapEngine::Init()
  {
    // pot_value == 0 gives each ISampleTable implementation's own sane
    // default active set (e.g. SampleTable selects just kSamples[0];
    // SampleTableFromBins selects just flattened bin 0) -- see
    // ISampleTable::MapPotToActiveMask().
    source_ = sample_table_->MapPotToActiveMask(0);
    sample_table_->Init(source_);
    density_ = 0;
    mean_interonset_samples_ = kSampleRate; // matches set_density(0)'s result
    spread_ = 0;
    interonset_time_ = 0;
    samples_until_next_grain_ = 0; // fire the first grain immediately
    envelope_ = 0;                 // silent until the first gate trigger
    set_decay(0);                  // initializes envelope_decrement_
    grain_essence_.set_sample_table(sample_table_);

  }

  void ClapEngine::Process(const GateFlags *gate_flags, int16_t *out, size_t size)
  {
    GateFlags gate_flag = *gate_flags++;
    if (gate_flag & GATE_FLAG_RISING)
    {
      // Force the very first sample of this block to activate a grain,
      // instead of waiting out whatever remained of the previous
      // interonset interval -- this is what makes a gate hit start a new
      // grain cloud immediately rather than at some arbitrary point
      // in-between onsets.
      samples_until_next_grain_ = 0;
      // Restart the decay line at full scale on every new trigger.
      envelope_ = kEnvelopeFullScale;
    }

    for (size_t i = 0; i < size; i++)
    {
      if (samples_until_next_grain_ <= 0 && envelope_ > 0)
      {
        ActivateGrains();
        samples_until_next_grain_ += static_cast<int32_t>(interonset_time_);
      }
      samples_until_next_grain_--;

      int32_t accumulator = 0;
      for (auto &grain : grain_pool_)
      {
        if (grain.IsGrainActive())
          accumulator += grain.Synthesize();
      }

      // Apply the decay envelope as a plain amplitude scaler (Q16 gain,
      // always <= 1.0, so this can never push accumulator further out of
      // range -- SoftLimit() below still guards against grain-summing
      // overflow, not this multiply).
      accumulator = (accumulator * static_cast<int32_t>(envelope_)) >> 16;

      *out++ = static_cast<int16_t>(SoftLimit(accumulator));

      envelope_ = (envelope_ > envelope_decrement_) ? envelope_ - envelope_decrement_ : 0;
    }
  }

  int32_t ClapEngine::SoftLimit(int32_t x)
  {
    // Below kLimiterThreshold (most of the time -- a handful of grains
    // overlapping, none of them clipping on their own), this is a
    // transparent pass-through: exactly x, no coloration at all. Only
    // once several loud grains stack up and the raw sum pushes past the
    // threshold does this curve start bending it over, asymptotically
    // approaching (but never reaching or exceeding) kLimiterCeiling --
    // trading a hard, buzzy flat-top clip (the old CLIP() macro) for a
    // smooth, continuous roll-off with no audible "kink": both the
    // curve's value and its slope match the identity line exactly at
    // x == +/-kLimiterThreshold.
    //
    //   y = L - R^2 / (|x| - T + R),  where R = L - T
    //
    // (a standard soft-knee/rational limiter shape). As |x| -> infinity,
    // R^2 / (|x| - T + R) -> 0, so y -> L; at |x| == T it reduces to
    // exactly T (continuous), and its derivative there is exactly 1
    // (matching the identity line's slope, so the join is smooth, not
    // just continuous).
    const int32_t magnitude = (x < 0) ? -x : x;
    if (magnitude <= kLimiterThreshold)
    {
      return x;
    }
    const int32_t sign = (x < 0) ? -1 : 1;
    const int64_t range_squared =
        static_cast<int64_t>(kLimiterRange) * kLimiterRange;
    const int64_t denominator = magnitude - kLimiterThreshold + kLimiterRange;
    const int32_t y = kLimiterCeiling -
                       static_cast<int32_t>(range_squared / denominator);
    return sign * y;
  }

  void ClapEngine::ActivateGrains()
  {

    // Both the effective onset rate and each new grain's duration are
    // scaled down by the current envelope_ value (a Q16 fraction of
    // "full" density/duration), so the whole grain cloud thins out and
    // shortens as the decay line falls, not just its final gain.
    const uint64_t effective_mean_interonset_samples =
        (static_cast<uint64_t>(mean_interonset_samples_) << 16) /
        envelope_;

    uint32_t neg_log_q88 = mu_stmlib::Interpolate824(lut_neg_log, mu_stmlib::Random::GetWord());
    interonset_time_ = static_cast<uint32_t>(
        (static_cast<uint64_t>(neg_log_q88) * effective_mean_interonset_samples) >>
        kNegLogFractionalBits);

    uint16_t start_pos = RandomizedStartPosition();
    uint32_t rate = RandomizedPhaseIncrement();
    const uint32_t grain_duration_samples = RandomizedGrainDuration();
    const bool reverse = RandomizedReverse();

    const size_t active_count = sample_table_->active_count();
    if (active_count == 0)
    {
      // Nothing is currently selectable (e.g. a SampleTableFromBins that
      // hasn't had SetSampleBins() called yet) -- drop this onset rather
      // than divide by zero below.
      return;
    }
    size_t chosen_n = mu_stmlib::Random::GetWord() % active_count;
    grain_essence_.set_sample_index(sample_table_->NthActiveIndex(chosen_n));
    grain_essence_.set_start_position(start_pos);
    grain_essence_.set_phase_increment(rate);
    grain_essence_.set_duration_samples(grain_duration_samples);
    grain_essence_.set_reverse(reverse);

    // The per-grain trapezoidal envelope only shapes that individual
    // grain's attack/release (click-free fade in/out); it always ramps
    // to full scale (1.0) -- the overall "cloud" shaping (density,
    // duration and gain all tapering off together) is handled separately
    // by ClapEngine's own decay envelope_, applied on top of this one in
    // Process().
    envelope_essence_.set_duration_samples(grain_duration_samples);
    envelope_essence_.set_attack_samples(kGrainAttackSamples);
    envelope_essence_.set_release_samples(kGrainReleaseSamples);
    envelope_essence_.set_amplitude(Envelope::kFullScale);

    Grain *free_slot = FindFreeGrainSlot();
    if (free_slot != nullptr)
    {
      free_slot->Activate(grain_essence_, envelope_essence_);
    }
    // If every grain in the pool is already active, this onset is simply
    // dropped -- see FindFreeGrainSlot()'s comment in clap_engine.h.
  }

  Grain *ClapEngine::FindFreeGrainSlot()
  {
    for (auto &grain : grain_pool_)
    {
      if (!grain.IsGrainActive())
      {
        return &grain;
      }
    }
    return nullptr;
  }

  uint16_t ClapEngine::RandomizedStartPosition() const
  {
    uint16_t random_u16 = static_cast<uint16_t>(mu_stmlib::Random::GetWord() >> 16);
    return static_cast<uint16_t>((static_cast<uint32_t>(random_u16) * spread_) >> 16);
  }

  uint32_t ClapEngine::RandomizedPhaseIncrement() const
  {
    int16_t random_bipolar = mu_stmlib::Random::GetSample();
    int32_t deviation = (static_cast<int32_t>(random_bipolar) *
                         static_cast<int32_t>(spread_)) >>
                        kRateDeviationShift;
    int32_t rate = static_cast<int32_t>(kNominalPhaseIncrement) + deviation;
    if (rate < static_cast<int32_t>(kMinPhaseIncrement))
    {
      rate = static_cast<int32_t>(kMinPhaseIncrement);
    }
    if (rate > static_cast<int32_t>(kMaxPhaseIncrement))
    {
      rate = static_cast<int32_t>(kMaxPhaseIncrement);
    }
    return static_cast<uint32_t>(rate);
  }

  uint32_t ClapEngine::RandomizedGrainDuration() const
  {
    int16_t random_bipolar = mu_stmlib::Random::GetSample();
    int32_t deviation = (static_cast<int32_t>(random_bipolar) *
                         static_cast<int32_t>(spread_)) >>
                        kDurationDeviationShift;
    int32_t duration = static_cast<int32_t>(kGrainDurationSamples) + deviation;
    const int32_t kMinDuration = static_cast<int32_t>(
        kGrainDurationSamples - kMaxDurationDeviationSamples);
    const int32_t kMaxDuration = static_cast<int32_t>(
        kGrainDurationSamples + kMaxDurationDeviationSamples);
    if (duration < kMinDuration)
    {
      duration = kMinDuration;
    }
    if (duration > kMaxDuration)
    {
      duration = kMaxDuration;
    }
    return static_cast<uint32_t>(duration);
  }

  bool ClapEngine::RandomizedReverse() const
  {
    // Squaring spread_ before using it as a probability threshold gives a
    // "log taper" feel (as in an audio-taper potentiometer): across most
    // of the lower/middle range very few grains reverse, and the
    // proportion only ramps up quickly near the top of the range. Linear
    // (threshold == spread_) would instead make the reverse probability
    // grow in lockstep with the knob the whole way, which is too eager
    // early on.
    //
    // spread_ * spread_ fits comfortably in 32 bits (max ~65535^2 =~
    // 4.29e9), and dividing by 65535 rescales the result back down to a
    // 0..65535 threshold, comparable against GetWord()'s uniformly
    // distributed low 16 bits.
    const uint32_t threshold =
        (static_cast<uint32_t>(spread_) * spread_) / 65535u;
    uint16_t random_word = static_cast<uint16_t>(mu_stmlib::Random::GetWord());
    return random_word < threshold;
  }

} // namespace peaks
