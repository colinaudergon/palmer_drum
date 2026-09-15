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

#include "Leds.h"

namespace hw_interface
{
    // The only LedsConfiguration instance for this build. LedsConfiguration is only ever
    // constructed here, in source code, which is what lets IsPolarityValid() be checked at
    // compile time below.
    constexpr LedsConfiguration kLedsConfig = {
        .polarity = OutputPolarity::kActiveHigh,
        .num_leds = 4,
        .led_gpio_map = {2, 3, 4, 5},
        .pwm_gpio = std::nullopt,
        .is_sink = false,
    };

    static_assert(IsPolarityValid(kLedsConfig),
                  "LedsConfiguration: a sinking LED needs kActiveLow, a sourcing LED needs kActiveHigh");
} // namespace hw_interface

void hw_interface::Leds::Init()
{
    config_ = kLedsConfig;

    for (uint i = 0; i < config_.num_leds; i++)
    {
        gpio_init(config_.led_gpio_map[i]);
        gpio_set_dir(config_.led_gpio_map[i], GPIO_OUT);
        gpio_put(config_.led_gpio_map[i], config_.polarity == OutputPolarity::kActiveLow ? kLedOn : kLedOff);
    }

    if (config_.pwm_gpio.has_value())
    {
        const uint pwm_gpio = *config_.pwm_gpio;
        gpio_set_function(pwm_gpio, GPIO_FUNC_PWM);
        const uint slice = pwm_gpio_to_slice_num(pwm_gpio);
        pwm_set_wrap(slice, kBrightnessMax);
        pwm_set_gpio_level(pwm_gpio, 0);
        pwm_set_enabled(slice, true);
    }
}

void hw_interface::Leds::ShowValue(uint8_t value)
{
    const uint8_t max_value_to_display = (1 << config_.num_leds) - 1;
    if (value > max_value_to_display)
    {
        return;
    }

    for (uint i = 0; i < config_.num_leds; i++)
    {
        const bool bit_set = (value & (1 << i)) != 0;
        const uint level = (config_.polarity == OutputPolarity::kActiveLow) ? !bit_set : bit_set;
        gpio_put(config_.led_gpio_map[i], level);
    }
}

void hw_interface::Leds::ClearAllLeds()
{
    for (uint i = 0; i < config_.num_leds; i++)
    {
        gpio_put(config_.led_gpio_map[i], config_.polarity == OutputPolarity::kActiveLow ? kLedOn : kLedOff);
    }
}

int hw_interface::Leds::SetLedBrightness(uint8_t brightness_percent)
{
    if (!config_.pwm_gpio.has_value())
    {
        return static_cast<int>(LedsError::kBrightnessControlUnavailable);
    }

    if (brightness_percent > kBrightnessPercentMax)
    {
        brightness_percent = kBrightnessPercentMax;
    }

    pwm_set_gpio_level(*config_.pwm_gpio, ScaleBrightnessValue(brightness_percent));
    return static_cast<int>(LedsError::kSuccess);
}

uint8_t hw_interface::Leds::ScaleBrightnessValue(uint8_t brightness_percent)
{
    return static_cast<uint8_t>((static_cast<uint16_t>(brightness_percent) * kBrightnessMax) / kBrightnessPercentMax);
}
