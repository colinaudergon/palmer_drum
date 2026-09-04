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
