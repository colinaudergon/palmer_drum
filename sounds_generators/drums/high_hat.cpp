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
//
// 808-style HH.

#include "high_hat.h"

#include <algorithm>
#include <cstdio>

#include "../../lib/mu_dsp.h"

#include "../peaks_ressources/resources.h"
#include "../peaks_ressources/random.h"

namespace peaks
{

  using namespace mu_stmlib;

  void HighHat::Init()
  {
    noise_.Init();
    noise_.set_frequency(105 << 7); // 8kHz
    noise_.set_resonance(24000);

    vca_coloration_.Init();
    vca_coloration_.set_frequency(110 << 7); // 13kHz
    vca_coloration_.set_resonance(0);

    vca_envelope_.Init();
    vca_envelope_.set_delay(0);
    vca_envelope_.set_decay(kDefaultDecay);
    envelope_value_ = 0;
    envelope_update_period_ = 1;
    envelope_update_counter_ = 0;
  }

  void HighHat::Process(const GateFlags *gate_flags, int16_t *out, size_t size)
  {
    while (size--)
    {
      GateFlags gate_flag = *gate_flags++;
      if (gate_flag & GATE_FLAG_RISING)
      {
        vca_envelope_.Trigger(32768 * 15);
        envelope_update_counter_ = 0;
      }

      HandleOsc();

      int32_t filtered_noise = HandleNoise();

      if (envelope_update_counter_ == 0)
      {
        envelope_value_ = vca_envelope_.Process() >> 4;
        envelope_update_counter_ = envelope_update_period_ - 1;
      }
      else
      {
        --envelope_update_counter_;
      }

      int32_t vca_noise = envelope_value_ * filtered_noise >> 14;
      CLIP(vca_noise);
      int32_t hh = 0;
      hh += vca_coloration_.Process<SVF_MODE_HP>(vca_noise);
      hh += vca_coloration_.Process<SVF_MODE_HP>(vca_noise);
      hh <<= 1;
      CLIP(hh);
      *out++ = hh;
    }
  }

  void HighHat::Configure(uint16_t *parameter)
  {
    envelope_update_period_ = Map(parameter[0], 0, 65535, 1, kMaxEnvelopeUpdatePeriod);
    tone_repeat_processing_ = Map(parameter[1], 0, 65535, 1, 16);
    noise_.set_resonance(parameter[2]);
    noise_mixer_ = parameter[3];
  }

  int32_t HighHat::HandleNoise()
  {
    int16_t noise = 0;
    // 
    noise += phase_[0] >> 31;
    noise += phase_[1] >> 31;
    noise += phase_[2] >> 31;
    noise += phase_[3] >> 31;
    noise += phase_[4] >> 31;
    noise += phase_[5] >> 31;
    noise <<= 12;
    // Run the SVF at the double of the original sample rate for stability.
    int32_t filtered_noise = 0;
    
    for(size_t i = 0; i <tone_repeat_processing_; i++)
    {
      filtered_noise += noise_.Process<SVF_MODE_BP>(noise);
    }
    
    constexpr int32_t kMinimumNoise = -32768;
    constexpr int32_t kMaximumNoise = 32767;
    int32_t unrectified_noise = std::clamp(filtered_noise, kMinimumNoise, kMaximumNoise);
    int32_t half_wave_noise = std::max(unrectified_noise, int32_t{0});
    int64_t difference = unrectified_noise - half_wave_noise;
    return half_wave_noise +
           static_cast<int32_t>((difference * noise_mixer_ + 32767) / 65535);
  }

  void HighHat::HandleOsc()
  {

    phase_[0] += 48318382;
    phase_[1] += 71582788;
    phase_[2] += 37044092;
    phase_[3] += 54313440;
    phase_[4] += 66214079;
    phase_[5] += 93952409;
  }

  uint16_t HighHat::Map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max)
  {
    uint32_t input_range = in_max - in_min;
    uint32_t output_range = out_max - out_min;
    uint32_t scaled = static_cast<uint32_t>(x - in_min) * output_range;
    return static_cast<uint16_t>(scaled / input_range + out_min);
  }

} // namespace peaks
