#include "pico_adc.h"

#include <cmath>
#include <cstring>

namespace hw_interface
{

    namespace
    {
        constexpr uint8_t kAdcGpioMin = 26;
        constexpr uint8_t kAdcGpioMax = 29;
    }

    int PicoAdcController::Init(const PicoAdcConfig &config)
    {

        if ((config.internal_adc_gpio < kAdcGpioMin) || (config.internal_adc_gpio > kAdcGpioMax))
        {
            return kAdcControllerErr;
        }

        config_.internal_adc_gpio = config.internal_adc_gpio;
        
        config_.number_of_adc = config.number_of_adc;
        if(config_.number_of_adc >= kNbrMaxAdc)
        {
            config_.number_of_adc = kNbrMaxAdc -1;
        }

        config_.gpio_ctrl_a = config.gpio_ctrl_a;
        config_.gpio_ctrl_b = config.gpio_ctrl_b;
        config_.gpio_ctrl_c = config.gpio_ctrl_c;

        gpio_init(config_.gpio_ctrl_a);
        gpio_set_dir(config_.gpio_ctrl_a, GPIO_OUT);

        gpio_init(config_.gpio_ctrl_b);
        gpio_set_dir(config_.gpio_ctrl_b, GPIO_OUT);

        gpio_init(config_.gpio_ctrl_c);
        gpio_set_dir(config_.gpio_ctrl_c, GPIO_OUT);

        adc_init();
        adc_gpio_init(config_.internal_adc_gpio);
        adc_select_input(kAdcNumber);

        initialized_ = true;
        next_adc_position_ = 0;

        return kAdcControllerSuccess;
    }
    void PicoAdcController::Process()
    {
        if (!initialized_)
        {
            return;
        }

        SelectAdcInput(next_adc_position_);
        //  let the mux output / ADC sample-and-hold settle before converting,
        //  otherwise the reading is a blend with the previously selected channel.
        sleep_us(kMuxSettleUs);

        medians_[next_adc_position_].add(adc_read());

        next_adc_position_ = static_cast<uint8_t>((next_adc_position_ + 1) % config_.number_of_adc);
    }

    float PicoAdcController::GetLastReading(size_t adc_index)
    {
        if (adc_index >= kNbrMaxAdc)
        {
            adc_index = kNbrMaxAdc -1;
        }

        float value = medians_[adc_index].getMedian();

        //  only move the reported value once it clears the deadband, so small
        //  jitter is ignored but larger/intentional moves stay immediately responsive.
        if (!has_reported_[adc_index] || std::abs(value - last_reported_[adc_index]) >= kDeadband)
        {
            last_reported_[adc_index] = value;
            has_reported_[adc_index] = true;
        }

        return last_reported_[adc_index];
    }

    void PicoAdcController::SelectAdcInput(size_t adc_number)
    {
        static constexpr size_t kMaskGpioCtrlA = 0x01;
        static constexpr size_t kMaskGpioCtrlB = 0x02;
        static constexpr size_t kMaskGpioCtrlC = 0x04;
        static constexpr size_t kShiftGpioCtrlB = 1;
        static constexpr size_t kShiftGpioCtrlC = 2;

        if (adc_number >= kNbrMaxAdc)
        {
            adc_number = kNbrMaxAdc -1;
        }

        gpio_put(config_.gpio_ctrl_a, (adc_number & kMaskGpioCtrlA));
        gpio_put(config_.gpio_ctrl_b, ((adc_number & kMaskGpioCtrlB) >> kShiftGpioCtrlB));
        gpio_put(config_.gpio_ctrl_c, ((adc_number & kMaskGpioCtrlC)) >> kShiftGpioCtrlC);
    }
} // namespace hw_interface
