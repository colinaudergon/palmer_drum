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

/**
 * @file pico_adc.h
 * @brief
 */

#pragma once

#include <cstdint>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "running_median.h"
namespace hw_interface
{

    static constexpr size_t kMaxAdcBufferSize = 19; // value from RunningMedian.h

    struct PicoAdcConfig
    {
        uint internal_adc_gpio;
        size_t number_of_adc;
        uint gpio_ctrl_a;
        uint gpio_ctrl_b;
        uint gpio_ctrl_c;
    };

    class PicoAdcController
    {
    public:
        PicoAdcController() {
        };
        ~PicoAdcController() {};
        int Init(const PicoAdcConfig &config);
        void Process();
        float GetLastReading(size_t adc_index);

    private:
        static constexpr uint8_t kNbrMaxAdc = 8;
        static constexpr uint16_t kAdcMaxValue = 4095;
        static constexpr int32_t kBackgroundReadingPeriodUs = -1000;
        static constexpr uint kAdcNumber = 0;
        //  time to let the external mux output and ADC input settle after switching channels
        static constexpr uint32_t kMuxSettleUs = 10;
        //  minimum change (raw ADC counts) required before a new reading is reported
        static constexpr float kDeadband = 4.0f;
        // static constexpr float kDeadband = 8.0f;

        RunningMedian medians_[kNbrMaxAdc];
        float last_reported_[kNbrMaxAdc] = {0};
        bool has_reported_[kNbrMaxAdc] = {false};

        uint8_t next_adc_position_ = 0;
        PicoAdcConfig config_;
        bool initialized_{false};
        void SelectAdcInput(size_t adc_number);
        static constexpr int kAdcControllerSuccess = 0;
        static constexpr int kAdcControllerErr = -1;
        static constexpr int kAdcControllerAlreadyReading = -2;
        static constexpr int kAdcControllerAccessOutofBound = -3;
    };

} // namespace hw_interface
