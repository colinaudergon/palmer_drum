/**
 * @file IAudioCodec.h
 * @brief Audio codec hardware interface.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace hw_interface
{
    typedef struct
    {
        int16_t *buffer_left;
        int16_t *buffer_right;
        size_t buffer_len;
    } audio_buffer_t;

    typedef void (*buffer_fill_cb)(audio_buffer_t *buffer_0, audio_buffer_t *buffer_1);

    class IAudioCodec
    {
    public:
        virtual ~IAudioCodec() = default;

        virtual int Init() = 0;
        virtual int RegisterFillCallback(buffer_fill_cb cb) = 0;
    };

} // namespace hw_interface
