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

#include "DigitalInput.h"
namespace hw_interface
{

    void DigitalInput::Init()
    {
        gpio_init(gpio_);
        gpio_set_dir(gpio_, GPIO_IN);
    }

    // Input is inverted by the npn buffer in front of the GPIO
    void DigitalInput::Process()
    {
        bool res = gpio_get(gpio_);
        switch (state_)
        {
        case DigitalInputState::kGate:
        {
            if (!res)
            {
                state_ = DigitalInputState::kRising;
            }
            break;
        }
        case DigitalInputState::kRising:
        {
            if (res)
            {
                state_ = DigitalInputState::kFalling;
            }
            break;
        }
        case DigitalInputState::kFalling:
        {
            if (res)
            {
                state_ = DigitalInputState::kGate;
            }
            break;
        }
        }
    }

    DigitalInputState DigitalInput::GetState() const
    {
        return state_;
    }

} // namespace hw_interface
