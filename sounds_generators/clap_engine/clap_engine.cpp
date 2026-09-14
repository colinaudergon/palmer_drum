// Copyright 2013 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------

#include "clap_engine.h"

#include <algorithm>

namespace peaks
{

  void ClapEngine::Init()
  {
    source_ = kDefaultSourceMask;
    sample_table_.Init(source_);
    density_ = 0;
    mean_interonset_samples_ = kSampleRate; // matches set_density(0)'s result
    spread_ = 0;
    interonset_time_ = 0;
    samples_until_next_grain_ = 0; // fire the first grain immediately
    grain_essence_.set_sample_table(&sample_table_);

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
    }

    for (size_t i = 0; i < size; i++)
    {
      if (samples_until_next_grain_ <= 0)
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
      CLIP(accumulator);
      *out++ = accumulator;
    }
  }

  void ClapEngine::ActivateGrains()
  {

    uint32_t neg_log_q88 = mu_stmlib::Interpolate824(lut_neg_log, mu_stmlib::Random::GetWord());
    interonset_time_ = (neg_log_q88 * mean_interonset_samples_) >> kNegLogFractionalBits;

    uint16_t start_pos = RandomizedStartPosition();
    uint32_t rate = RandomizedPhaseIncrement();

    size_t chosen_n = mu_stmlib::Random::GetWord() % sample_table_.active_count();
    grain_essence_.set_sample_index(sample_table_.NthActiveIndex(chosen_n));
    grain_essence_.set_start_position(start_pos);
    grain_essence_.set_phase_increment(rate);

    Grain *free_slot = FindFreeGrainSlot();
    if (free_slot != nullptr)
    {
      free_slot->Activate(grain_essence_);
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

} // namespace peaks
