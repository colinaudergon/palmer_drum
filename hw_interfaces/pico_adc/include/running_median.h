// Copyright (c) 2011-2026 Rob Tillaart.
//
// Author: Rob Tillaart (https://github.com/RobTillaart/RunningMedian)
// Adapted for this project (Pico SDK port) by: colinaudergon
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
 * @file running_median.h
 * @brief
 */

#pragma once

#include "pico/stdlib.h"

#define MEDIAN_MIN_SIZE 3
//  using fixed memory will be limited to 19 elements.
#define MEDIAN_MAX_SIZE 19

namespace hw_interface
{
    class RunningMedian
    {
    public:
        //  # elements in the internal buffer
        //  odd sizes results in a 'real' middle element and will be a bit faster.
        //  even sizes takes the average of the two middle elements as median
        explicit RunningMedian();
        ~RunningMedian();

        //  resets internal buffer and variables
        void clear();
        //  adds a new value to internal buffer, optionally replacing the oldest element.
        void add(const float value);
        //  returns the median == middle element
        float getMedian();

        //  EXPERIMENTAL  (might change in the future)
        //  searchMode defines how the internal insertionSort works
        //  can be used to optimize performance.
        //  0 = LINEAR_SEARCH   1 = BINARY_SEARCH
        void setSearchMode(uint8_t searchMode = 0);
        uint8_t getSearchMode();

    protected:
        bool _sorted; //  _sortIdx{} is up to date 
        static constexpr size_t kMaxNumberOfValues = 19;//  max number of values
        uint8_t _count;  //  current number of values <= size
        uint8_t _index;  //  next index to add

        //  _values holds the elements themself
        //  _sortIdx holds the index for sorted
        float _values[MEDIAN_MAX_SIZE];
        uint8_t _sortIdx[MEDIAN_MAX_SIZE];
        void sort();
        uint8_t _searchMode = 0;
    };

} // namespace hw_interface