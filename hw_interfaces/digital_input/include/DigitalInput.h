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
