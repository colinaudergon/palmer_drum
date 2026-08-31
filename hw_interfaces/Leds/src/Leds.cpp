#include "Leds.h"

void hw_interface::Leds::Init()
{

    for (int i = 0; i < kNumbersLed; i++)
    {
        gpio_init(kLedArray[i]);
        gpio_set_dir(kLedArray[i], GPIO_OUT);
    }
}

void hw_interface::Leds::ShowValue(uint8_t value)
{
    if(value >  kMaxValueToDisplay)
    {
        return;
    }

    for (int i = 0; i < kNumbersLed; i++)
    {
        gpio_put(kLedArray[i],(value & 1<<i)>>i);
    }
}

// void hw_interface::Leds::ShowLed(AvailableLed led)
// {
//     const uint led_gpio_index = static_cast<uint>(led);
//     if(led_gpio_index >= kNumbersLed)
//     {
//         return;
//     }

//     const uint led_gpio = kLedArray[led_gpio_index];
//     gpio_put(led_gpio,kLedOn);
// }

void hw_interface::Leds::ClearAllLeds()
{
        for (int i = 0; i < kNumbersLed; i++)
    {
        gpio_put(kLedArray[i],kLedOff);
    }
}
