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

#include "processor.h"

#include <algorithm>

namespace peaks
{

  using namespace mu_stmlib;
  using namespace std;

  void Processors::Init(uint8_t index)
  {
    processors_table_[GetProcessorIndexFromFunction(ProcessorFunction::kBassDrum)] = &bass_drum_;
    processors_table_[GetProcessorIndexFromFunction(ProcessorFunction::kSnareDrum)] = &snare_drum_;
    processors_table_[GetProcessorIndexFromFunction(ProcessorFunction::kHighHat)] = &high_hat_;
    processors_table_[GetProcessorIndexFromFunction(ProcessorFunction::kFmDrum)] = &fm_drum_;
    processors_table_[GetProcessorIndexFromFunction(ProcessorFunction::kNumberStation)] = &number_station_;

    for (uint16_t i = 0; i < GetProcessorIndexFromFunction(ProcessorFunction::kLast); ++i)
    {
      processors_table_[i]->Init();
    }

    fm_drum_.set_sd_range(index == 1);
    number_station_.set_voice(index == 1);

    std::fill(&parameter_[0], &parameter_[4], 32768);
    set_function(ProcessorFunction::kBassDrum);
    // set_function(kNumberStation);
  }
} // namespace peaks
