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

    // Hardware-specific configuration consumed by PicoAdcController::Init(), packed by the
    // caller into an AdcConfig{data, size} pair (see IAdcController::Init()):
    //   PicoAdcConfig cfg{26, 27, 28, 29};
    //   adc_controller.Init(hw_interface::AdcConfig{&cfg, sizeof(cfg)});
    namespace
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
    }

    class PicoAdcController
    {
    public:
        int Init(const PicoAdcConfig &config);
        void Process();
        float GetLastReading(size_t adc_index);

    private:
        static constexpr uint8_t kNbrMaxAdc = 8;
        static constexpr uint16_t kAdcMaxValue = 4095;
        static constexpr int32_t kBackgroundReadingPeriodUs = -1000;
        static constexpr uint kAdcNumber = 0;

        RunningMedian medians_[kNbrMaxAdc];

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
