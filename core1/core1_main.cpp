
#include "utils/cross_core_queues.h"
#include "../hw_interfaces/include/InputHandler.h"
#include "core1_main.h"
#include "pico_adc.h"

#include <algorithm>
hw_interface::PicoAdcController adc_controller;
hw_interface::Leds leds;
hw_interface::GateInput gate_input(kGateInputGpio);
hw_interface::ButtonInput button_input(kButtonGpio, kButtonLongPressMs);

namespace
{
    constexpr size_t kNumAdcParameters = 4;
    constexpr float kAdcMaxValue = 4095.0f;        // 12-bit raw ADC counts
    constexpr float kParameterMaxValue = 65535.0f; // Processors::set_parameter's uint16_t range
    constexpr uint8_t kNumberOfProcessors = 5;
    // Rescales a raw ADC reading (0..kAdcMaxValue) to the processor's parameter range.
    uint16_t AdcToParameter(float adc_value)
    {
        float clamped = std::clamp(adc_value, 0.0f, kAdcMaxValue);
        return static_cast<uint16_t>(clamped * (kParameterMaxValue / kAdcMaxValue) + 0.5f);
    }

}

void Core1Main()
{

    constexpr uint8_t kGateEventId = kNumAdcParameters + 1;
    constexpr uint8_t kButtonEventId = kGateEventId + 1;
    constexpr uint8_t kButtonHoldEventId = kButtonEventId + 1;
    uint8_t selected_processor = 0;
    
    hw_interface::PicoAdcConfig adc_config = {
        .internal_adc_gpio = 26,
        .number_of_adc = 4,
        .gpio_ctrl_a = 18,
        .gpio_ctrl_b = 17,
        .gpio_ctrl_c = 16};

    adc_controller.Init(adc_config);

    leds.Init();
    gate_input.Init();
    button_input.Init();
    leds.ShowValue(selected_processor + 1);
    hw_interface::DigitalInputState gate_state = hw_interface::DigitalInputState::kGate;
    hw_interface::DigitalInputState button_state = hw_interface::DigitalInputState::kGate;
    bool button_long_press_state = false;

    hw_interface::InputEvent adc_event;
    adc_event.type = hw_interface::ControlType::kControlPot;

    hw_interface::InputEvent gate_event;
    gate_event.type = hw_interface::ControlType::kControlGate;
    gate_event.control_id = kGateEventId;

    hw_interface::InputEvent button_event;
    button_event.type = hw_interface::ControlType::kControlSwitch;
    button_event.control_id = kButtonEventId;

    hw_interface::InputEvent button_hold_event;
    button_hold_event.type = hw_interface::ControlType::kControlSwitchHold;
    button_hold_event.control_id = kButtonHoldEventId;

    while (true)
    {

        adc_controller.Process();
        gate_input.Process();
        button_input.Process();

        hw_interface::DigitalInputState current_gate_state = gate_input.GetState();
        if (current_gate_state == hw_interface::DigitalInputState::kRising &&
            current_gate_state != gate_state)
        {
            gate_event.data = static_cast<int32_t>(current_gate_state);
            gate_event_queue_.TryPush(gate_event);
        }
        gate_state = current_gate_state;

        hw_interface::DigitalInputState current_button_state = button_input.GetState();
        if (current_button_state == hw_interface::DigitalInputState::kRising &&
            current_button_state != button_state)
        {
            selected_processor = (selected_processor + 1) % kNumberOfProcessors;
            leds.ClearAllLeds();
            leds.ShowValue(selected_processor + 1);
            button_event.data = static_cast<int32_t>(selected_processor);
            button_event_queue_.TryPush(button_event);
        }
        button_state = current_button_state;

        bool current_button_long_press = button_input.IsLongPress();
        if (current_button_long_press && !button_long_press_state)
        {
            button_hold_event.data = static_cast<int32_t>(selected_processor);
            button_event_queue_.TryPush(button_hold_event);
        }
        button_long_press_state = current_button_long_press;

        for (size_t i = 0; i < kNumAdcParameters; ++i)
        {

            adc_event.control_id = static_cast<uint8_t>(i);
            adc_event.data = AdcToParameter(adc_controller.GetLastReading(i));
            input_event_queue_.TryPush(adc_event);
        }
    }
}