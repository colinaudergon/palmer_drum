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
    enum class GateInputState : uint8_t
    {
        kGate = 0x01,
        kRising = 0x02,
        kFalling = 0x04,
    };

    class GateInput
    {
    public:
        GateInput(uint gpio) : gpio_(gpio) {};
        ~GateInput();
        void Init();
        void Process();
        GateInputState GetGateState();

    private:
        GateInputState state_ = GateInputState::kGate;
        uint gpio_;
        
    };
} // namespace hw_interface