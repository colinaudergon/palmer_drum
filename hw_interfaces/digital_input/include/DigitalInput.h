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
 * @file DigitalInput.h
 * @brief Base class for simple debounced-by-polling digital inputs (gate/switch/button).
 */

#pragma once

#include <cstdint>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

namespace hw_interface
{
    enum class DigitalInputState : uint8_t
    {
        kGate = 0x01,
        kRising = 0x02,
        kFalling = 0x04,
    };

    // Shared behavior for polled digital inputs: tracks a simple edge state machine
    // (kGate -> kRising -> kFalling -> kGate) driven by repeated calls to Process(). kRising
    // persists for the entire time the input is held active, which is what lets subclasses
    // (e.g. ButtonInput) layer timing-based behavior like long-press detection on top.
    class DigitalInput
    {
    public:
        DigitalInput(uint gpio) : gpio_(gpio) {};
        virtual ~DigitalInput() = default;
        virtual void Init();
        virtual void Process();
        DigitalInputState GetState() const;

    protected:
        DigitalInputState state_ = DigitalInputState::kGate;
        uint gpio_;
    };
} // namespace hw_interface
