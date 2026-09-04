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
