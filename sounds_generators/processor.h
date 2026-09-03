// Copyright 2013 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// This is the common entry points for all types of modulation sources!

#pragma once

#include "../lib/mu_stmlib.h"

#include <algorithm>

#include "drums/bass_drum.h"
#include "drums/fm_drum.h"
#include "drums/snare_drum.h"
#include "drums/high_hat.h"
#include "number_station/number_station.h"

#include "i_processor.h"
#include "peaks_ressources/gate_processor.h"

namespace peaks
{

    enum class ProcessorFunction: uint8_t
    {
        kBassDrum,
        kSnareDrum,
        kHighHat,
        kFmDrum,
        kNumberStation,
        kLast
    };

    class Processors
    {
    public:
        Processors() {}
        ~Processors() {}

        void Init(uint8_t index);

        inline void set_parameter(uint8_t index, uint16_t parameter)
        {
            parameter_[index] = parameter;
            Configure();
        }

        inline void CopyParameters(uint16_t *parameters, uint16_t size)
        {
            std::copy(&parameters[0], &parameters[size], &parameter_[0]);
        }

        inline void set_function(ProcessorFunction function)
        {
            function_ = function;
            current_processor_ = processors_table_[GetProcessorIndexFromFunction(function_)];
            Configure();
        }

        inline ProcessorFunction function() const { return function_; }

        inline void Process(const GateFlags *gate_flags, int16_t *output, size_t size)
        {
            current_processor_->Process(gate_flags, output, size);
        }


    private:
        void Configure()
        {
            current_processor_->Configure(&parameter_[0]);
        }

        inline uint8_t GetProcessorIndexFromFunction(ProcessorFunction processor_function)
        {
            return static_cast<uint8_t>(processor_function);
        }


        ProcessorFunction function_;
        uint16_t parameter_[4];

        // Currently-selected processor, dispatched to via a single virtual call
        // (IProcessor::Process()/Configure()) instead of a hand-built
        // function-pointer table. All concrete processors below are still
        // instantiated as plain members (no heap allocation) so switching
        // function_ is just repointing current_processor_ -- no lifetime or
        // allocation concerns.
        IProcessor *current_processor_ = nullptr;
        IProcessor *processors_table_[static_cast<uint8_t>(ProcessorFunction::kLast)];

        BassDrum bass_drum_;
        SnareDrum snare_drum_;
        HighHat high_hat_;
        FmDrum fm_drum_;
        NumberStation number_station_;
    };


} // namespace peaks

