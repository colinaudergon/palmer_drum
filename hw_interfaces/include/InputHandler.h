#pragma once
#include "pico/stdlib.h"

namespace hw_interface
{

    enum ControlType
    {
        CONTROL_POT = 0,
        CONTROL_ENCODER = 1,
        CONTROL_ENCODER_CLICK = 2,
        CONTROL_ENCODER_LONG_CLICK = 3,
        CONTROL_SWITCH = 4,
        CONTROL_SWITCH_HOLD = 5,
        CONTROL_REFRESH = 0xff
    };

    struct InputEvent
    {
        ControlType type;
        uint8_t control_id;
        int32_t data;
    };

} // namespace hw_interface
