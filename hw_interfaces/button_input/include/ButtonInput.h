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
 * @file ButtonInput.h
 * @brief Digital input with long-press detection, layered on top of DigitalInput.
 */

#pragma once

#include "DigitalInput.h"

namespace hw_interface
{
    // Adds long-press timing on top of DigitalInput's edge state machine. The base class's
    // kRising state persists for the whole time the button is held down, so ButtonInput times
    // that held duration and flips IsLongPress() true as soon as the configured threshold
    // elapses -- while the button is still held, not only on release.
    class ButtonInput : public DigitalInput
    {
    public:
        static constexpr uint32_t kDefaultLongPressMs = 800;

        // long_press_ms: minimum time (ms) the button must be held for IsLongPress() to
        // report true.
        explicit ButtonInput(uint gpio, uint32_t long_press_ms = kDefaultLongPressMs);

        void Process() override;

        // True once the button has been held continuously for at least long_press_ms_.
        // Stays true until the button is released (back to DigitalInputState::kGate), at
        // which point it is re-armed for the next press.
        bool IsLongPress() const;

    private:
        uint32_t long_press_ms_;
        absolute_time_t press_start_time_{};
        bool long_press_fired_ = false;
        // Cached previous state (as seen by ButtonInput), used to detect the kGate->kRising
        // and ->kGate edges that (re)start/re-arm the long-press timer.
        DigitalInputState previous_state_ = DigitalInputState::kGate;
    };
} // namespace hw_interface
