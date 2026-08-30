
#include "utils/cross_core_queues.h"
#include "../hw_interfaces/include/InputHandler.h"
#include "core1_main.h"
#include "pico_adc.h"
#include <algorithm>
hw_interface::PicoAdcController adc_controller;

namespace
{
    constexpr size_t kNumAdcParameters = 4;
    constexpr float kAdcMaxValue = 4095.0f;        // 12-bit raw ADC counts
    constexpr float kParameterMaxValue = 65535.0f; // Processors::set_parameter's uint16_t range

    // Rescales a raw ADC reading (0..kAdcMaxValue) to the processor's parameter range.
    uint16_t AdcToParameter(float adc_value)
    {
        float clamped = std::clamp(adc_value, 0.0f, kAdcMaxValue);
        return static_cast<uint16_t>(clamped * (kParameterMaxValue / kAdcMaxValue) + 0.5f);
    }

}

void Core1Main()
{
    hw_interface::PicoAdcConfig adc_config = {
        .internal_adc_gpio = 26,
        .number_of_adc = 4,
        .gpio_ctrl_a = 18,
        .gpio_ctrl_b = 17,
        .gpio_ctrl_c = 16};

    adc_controller.Init(adc_config);
    while (true)
    {
        hw_interface::InputEvent event;

        adc_controller.Process();
        for (size_t i = 0; i < kNumAdcParameters; ++i)
        {   event.type = hw_interface::ControlType::CONTROL_POT;
            event.control_id = static_cast<uint8_t>(i);
            event.data = AdcToParameter(adc_controller.GetLastReading(i));
            // processor.set_parameter(i, AdcToParameter(adc_controller.GetLastReading(i)));
            input_event_queue_.TryPush(event);
        }
    }
}