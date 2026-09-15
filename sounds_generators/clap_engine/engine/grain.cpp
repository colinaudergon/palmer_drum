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

#include "grain.h"

namespace peaks
{

    void Grain::Activate(const GrainSource::Essence &source_essence,
                          const Envelope::Essence &envelope_essence)
    {
        source_.Init(source_essence);
        envelope_.Init(envelope_essence);
        is_active_ = true;
    }

    int16_t Grain::Synthesize()
    {
        int16_t sample = source_.Synthesize();
        uint32_t gain = envelope_.Synthesize();
        int16_t enveloped = static_cast<int16_t>(
            (static_cast<int32_t>(sample) * static_cast<int32_t>(gain)) >> 16);

        if (source_.Done() || envelope_.Done())
        {
            is_active_ = false;
        }
        return enveloped;
    }

    bool Grain::IsGrainActive()
    {
        return is_active_;
    }

} // namespace peaks