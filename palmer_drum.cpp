#include <stdio.h>
#include "pico/stdlib.h"
#include "pwm_audio_codec.h"
#include "hardware/clocks.h"

hw_interface::PicoAudioCodec audio_codec;

void buffer_callback(hw_interface::audio_buffer_t *buffer_0, hw_interface::audio_buffer_t *buffer_1)
{
    // buffer_0 is always the buffer to be consumed for this callback ("current"); buffer_1 is
    // read-ahead scratch space managed internally by the codec and isn't touched here.
    (void)buffer_1;

}

int main()
{

    stdio_init_all();
    set_sys_clock_khz(176000, true);

    int codec_init_result = audio_codec.Init();
    if (codec_init_result != 0)
    {
        printf("Audio codec Init result: %d\n", codec_init_result);
        return -1;
    }

    audio_codec.ServiceRefill();
    audio_codec.RegisterFillCallback(buffer_callback);
    audio_codec.Start();
    while (true)
    {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
