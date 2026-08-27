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
        explicit RunningMedian(const uint8_t size = MEDIAN_MAX_SIZE);
        ~RunningMedian();

        //  resets internal buffer and variables
        void clear();
        //  adds a new value to internal buffer, optionally replacing the oldest element.
        void add(const float value);
        //  returns the median == middle element
        float getMedian();

        //  returns the Quantile
        float getQuantile(const float quantile);

        //  returns average of the values in the internal buffer
        float getAverage();
        //  returns average of the middle nMedian values, removes noise from outliers
        float getAverage(uint8_t nMedian);
        //  returns average of the middle nMedian values, removes noise from outliers
        //  Bias compensated see #22.
        float getMedianAverage(uint8_t nMedian);

        float getHighest() { return getSortedElement(_count - 1); };
        float getLowest() { return getSortedElement(0); };

        //  get n-th element from the values in time order
        float getElement(const uint8_t n);
        //  get n-th element from the values in size order
        float getSortedElement(const uint8_t n);
        //  predict the max change of median after n additions
        float predict(const uint8_t n);

        uint8_t getSize() { return _size; };
        //  returns current used elements, getCount() <= getSize()
        uint8_t getCount() { return _count; };
        bool isFull() { return (_count == _size); }

        //  EXPERIMENTAL  (might change in the future)
        //  searchMode defines how the internal insertionSort works
        //  can be used to optimize performance.
        //  0 = LINEAR_SEARCH   1 = BINARY_SEARCH
        void setSearchMode(uint8_t searchMode = 0);
        uint8_t getSearchMode();

    protected:
        boolean _sorted; //  _sortIdx{} is up to date
        uint8_t _size;   //  max number of values
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