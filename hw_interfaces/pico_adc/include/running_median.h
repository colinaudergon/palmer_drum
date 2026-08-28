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