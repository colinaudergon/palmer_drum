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

/*
 * @file pico_adc.h
 * @brief
 */

#pragma once

#include <cstdint>
#include <optional>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

namespace hw_interface
{
    // Maximum number of LEDs a single Leds instance can drive; LedsConfiguration::led_gpio_map
    // is sized to this so the configuration stays a simple, fully value-typed aggregate (no
    // dynamic/heap storage).
    static constexpr uint8_t kMaxLeds = 8;

    // Whether the GPIO must be driven high or low to light the LED. Must be paired with
    // LedsConfiguration::is_sink correctly (see IsPolarityValid()): a sinking LED (cathode on
    // the GPIO) needs the GPIO to pull low to light, while a sourcing LED (anode on the GPIO)
    // needs the GPIO to drive high.
    enum class OutputPolarity : uint8_t
    {
        kActiveHigh,
        kActiveLow,
    };

    // Fully describes how a Leds instance is wired. Instances of this struct are only ever
    // constructed in Leds.cpp (never by external callers), which is what lets the
    // static_assert(IsPolarityValid(...)) placed there catch an electrically invalid
    // polarity/sink-source combination at compile time.
    struct LedsConfiguration
    {
        OutputPolarity polarity;
        uint num_leds;
        uint led_gpio_map[kMaxLeds];
        // GPIO driving a PWM slice for brightness control, if any. When unset,
        // SetLedBrightness() is unavailable.
        std::optional<uint> pwm_gpio;
        // true: LED sinks current through the GPIO (cathode on GPIO, needs kActiveLow).
        // false: LED sources current through the GPIO (anode on GPIO, needs kActiveHigh).
        bool is_sink;
    };

    // A sinking LED can only be lit by driving its GPIO low, and a sourcing LED only by driving
    // it high; any other pairing can never light the LED and indicates a configuration mistake.
    constexpr bool IsPolarityValid(const LedsConfiguration &config)
    {
        return (config.is_sink && config.polarity == OutputPolarity::kActiveLow) ||
               (!config.is_sink && config.polarity == OutputPolarity::kActiveHigh);
    }

    class Leds
    {
    public:
        enum class LedsError : int
        {
            kSuccess = 0,
            kBrightnessControlUnavailable = -1,
        };

        void Init();
        void ShowValue(uint8_t value);
        void ClearAllLeds();

        // Sets the PWM-driven LED brightness as a 0-100 percentage. Only available when the
        // configuration's pwm_gpio is set; otherwise returns kBrightnessControlUnavailable
        // without touching any GPIO.
        int SetLedBrightness(uint8_t brightness_percent);

    private:
        static constexpr uint kLedOn = 1;
        static constexpr uint kLedOff = 0;
        static constexpr uint8_t kBrightnessPercentMax = 100;
        static constexpr uint8_t kBrightnessMax = 255;

        // Scales a clamped 0-100 brightness percentage to the 0-kBrightnessMax duty cycle
        // range used by pwm_set_gpio_level().
        uint8_t ScaleBrightnessValue(uint8_t brightness_percent);

        LedsConfiguration config_{};
    };
} // namespace hw_interface