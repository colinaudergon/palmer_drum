#include "GateInput.h"
namespace hw_interface
{

    void GateInput::Init()
    {
        gpio_init(gpio_);
        gpio_set_dir(gpio_, GPIO_IN);
    }

    // Gate input is inverted by the npn buffer in front of the GPIO
    void GateInput::Process()
    {
        bool res = gpio_get(gpio_);
        switch (state_)
        {
        case GateInputState::kGate:
        {
            if (!res)
            {
                state_ = GateInputState::kRising;
            }
            break;
        }
        case GateInputState::kRising:
        {
            if (res)
            {
                state_ = GateInputState::kFalling;
            }
            break;
        }
        case GateInputState::kFalling:
        {
            if (res)
            {
                state_ = GateInputState::kGate;
            }
            break;
        }
        }
    }

    GateInputState GateInput::GetGateState()
    {
        return state_;
    }

} // namespace hw_interface