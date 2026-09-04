#pragma once
#include "pico/stdlib.h"

namespace hw_interface
{

    enum ControlType
    {
        kControlPot = 0,
        kControlEncoder = 1,
        kControlEncoderClick = 2,
        kControlEncoderLongClick = 3,
        kControlSwitch = 4,
        kControlSwitchHold = 5,
        kControlGate = 6
    };

    struct InputEvent
    {
        ControlType type;
        uint8_t control_id;
        int32_t data;
    };

} // namespace hw_interface
