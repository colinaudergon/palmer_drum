/*
 * @file pico_adc.h
 * @brief
 */

#pragma once

#include <cstdint>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

namespace hw_interface
{

    class Leds
    {
    public:
        void Init();
        void ShowValue(uint8_t value);
        void ClearAllLeds();

    private:
        static constexpr uint kLedOn = 1;
        static constexpr uint kLedOff = 0;
        static constexpr uint kNumbersLed = 4;
        static constexpr uint kLed0Gpio = 2;
        static constexpr uint kLed1Gpio = 3;
        static constexpr uint kLed2Gpio = 4;
        static constexpr uint kLed3Gpio = 5;
        static constexpr uint8_t kMaxValueToDisplay = (1 << kNumbersLed) -1;
        static constexpr uint kLedArray[kNumbersLed] = {
            kLed0Gpio,
            kLed1Gpio,
            kLed2Gpio,
            kLed3Gpio,
        };
    };
} // namespace hw_interface