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

#include "ButtonInput.h"

namespace hw_interface
{

    ButtonInput::ButtonInput(uint gpio, uint32_t long_press_ms)
        : DigitalInput(gpio), long_press_ms_(long_press_ms)
    {
    }

    void ButtonInput::Process()
    {
        DigitalInput::Process();
        DigitalInputState current_state = GetState();

        if (previous_state_ != DigitalInputState::kRising && current_state == DigitalInputState::kRising)
        {
            // Button just pressed: (re)start the long-press timer.
            press_start_time_ = get_absolute_time();
            long_press_fired_ = false;
        }
        else if (current_state == DigitalInputState::kGate)
        {
            // Fully released: re-arm for the next press.
            long_press_fired_ = false;
        }
        else if (current_state == DigitalInputState::kRising && !long_press_fired_)
        {
            int64_t held_us = absolute_time_diff_us(press_start_time_, get_absolute_time());
            if (held_us >= static_cast<int64_t>(long_press_ms_) * 1000)
            {
                long_press_fired_ = true;
            }
        }

        previous_state_ = current_state;
    }

    bool ButtonInput::IsLongPress() const
    {
        return long_press_fired_;
    }

} // namespace hw_interface
