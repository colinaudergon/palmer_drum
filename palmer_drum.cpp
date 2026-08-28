#include <algorithm>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "pwm_audio_codec.h"

#include "core1/core1_main.h"
#include "pico/multicore.h"
#include "sounds_generators/processor.h"
#include "sounds_generators/peaks_ressources/gate_processor.h"
#include "utils/cross_core_queues.h"

static constexpr size_t buffer_size = 256;
int16_t output_buffer[buffer_size];

static constexpr uint kInputEventQueueCapacity = 8;
InputEventQueue input_event_queue_;

hw_interface::PicoAudioCodec audio_codec;

peaks::Processors processor;

namespace
{
    // Free-running internal trigger, standing in for an external gate/CV input: pulses high
    // for kGatePulseSamples out of every kGatePeriodSamples.
    constexpr size_t kGatePeriodSamples = 2 * 22050; // ~2 Hz at the codec's 44.1kHz sample rate
    constexpr size_t kGatePulseSamples = 32;

    size_t gate_phase_ = 0;
    peaks::GateFlags previous_gate_flag_ = peaks::GATE_FLAG_LOW;
    peaks::GateFlags gate_buffer[buffer_size];

    void GenerateInternalGate(peaks::GateFlags *gate_flags, size_t size)
    {
        for (size_t i = 0; i < size; ++i)
        {
            bool gate_high = gate_phase_ < kGatePulseSamples;
            gate_flags[i] = peaks::ExtractGateFlags(previous_gate_flag_, gate_high);
            previous_gate_flag_ = gate_flags[i];
            gate_phase_ = (gate_phase_ + 1) % kGatePeriodSamples;
        }
    }
} // namespace

void buffer_callback(hw_interface::audio_buffer_t *buffer_0, hw_interface::audio_buffer_t *buffer_1)
{
    // buffer_0 is always the buffer to be consumed for this callback ("current"); buffer_1 is
    // read-ahead scratch space managed internally by the codec and isn't touched here.
    (void)buffer_1;

    GenerateInternalGate(gate_buffer, buffer_0->buffer_len);
    processor.Process(gate_buffer, output_buffer, buffer_0->buffer_len);

    for (size_t i = 0; i < buffer_0->buffer_len; ++i)
    {
        buffer_0->buffer_left[i] = output_buffer[i];
        buffer_0->buffer_right[i] = output_buffer[i];
    }
}

int main()
{
    set_sys_clock_khz(176000, true);
    stdio_init_all();

    input_event_queue_.Init(kInputEventQueueCapacity);
    multicore_launch_core1(Core1Main);

    processor.Init(0);

    int codec_init_result = audio_codec.Init();

    if (codec_init_result != 0)
    {
        printf("Audio codec Init result: %d\n", codec_init_result);
        return -1;
    }

    audio_codec.RegisterFillCallback(buffer_callback);
    audio_codec.ServiceRefill();
    audio_codec.Start();
    hw_interface::InputEvent command;
    while (true)
    {

        audio_codec.ServiceRefill();
         while (input_event_queue_.TryPop(command))
        {
            if(command.type == hw_interface::ControlType::CONTROL_POT)
            {
                processor.set_parameter(command.control_id,command.data);
            }
        }
    }
}
