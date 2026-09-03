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

#pragma once

#include "../../lib/mu_stmlib.h"

#include "svf.h"
#include "excitation.h"

#include "../i_processor.h"
#include "../peaks_ressources/gate_processor.h"

namespace peaks
{

  class HighHat : public IProcessor
  {
  public:
    HighHat() {}
    ~HighHat() override {}

    void Init() override;
    void Process(const GateFlags *gate_flags, int16_t *out, size_t size) override;
    void Configure(uint16_t *parameter) override;

  private:
    Svf noise_;
    Svf vca_coloration_;
    Excitation vca_envelope_;
    int32_t envelope_value_ = 0;
    uint16_t envelope_update_period_ = 1;
    uint16_t envelope_update_counter_ = 0;
    size_t tone_repeat_processing_ = 1;
    uint16_t noise_mixer_ = 0;
    int32_t HandleNoise();
    void HandleOsc();
    uint16_t Map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max);
    uint32_t phase_[6];
    static constexpr uint16_t kDefaultDecay = 4093;
    static constexpr uint16_t kMaxEnvelopeUpdatePeriod = 16;
    static constexpr size_t kNumberOfOsc = 6;
    static constexpr int kDefaultFilterIncrement[] = {48318382,
                                                      71582788,
                                                      37044092,
                                                      54313440,
                                                      66214079,
                                                      93952409};

    static constexpr uint16_t kDefaultResonance = 24000;
    // static constexpr uint16_t kDefaultResonance = 24000;

    HighHat(const HighHat &) = delete;
    const HighHat &operator=(const HighHat &) = delete;
  };

} // namespace peaks

