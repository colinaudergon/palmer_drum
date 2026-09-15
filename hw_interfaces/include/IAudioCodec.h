// Copyright 2026 colinaudergon.
//
// Author: colinaudergon
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// -----------------------------------------------------------------------------

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
