// from RunnigMedian
//     URL: https://github.com/RobTillaart/RunningMedian

#include "running_median.h"

namespace hw_interface
{
    RunningMedian::RunningMedian()
    {
        clear();
    }

    RunningMedian::~RunningMedian()
    {
    }

    //  resets all internal counters
    void RunningMedian::clear()
    {
        _count = 0;
        _index = 0;
        _sorted = false;
        for (uint8_t i = 0; i < kMaxNumberOfValues; i++)
        {
            _sortIdx[i] = i;
        }
    }

    //  adds a new value to the data-set
    //  or overwrites the oldest if full.
    void RunningMedian::add(float value)
    {
        _values[_index++] = value;
        if (_index >= kMaxNumberOfValues)
            _index = 0; //  wrap around
        if (_count < kMaxNumberOfValues)
            _count++;
        _sorted = false;
    }

    float RunningMedian::getMedian()
    {
        if (_count == 0)
            return -1;

        if (_sorted == false)
            sort();

        if (_count & 0x01) //  is it odd sized?
        {
            return _values[_sortIdx[_count / 2]];
        }
        return (_values[_sortIdx[_count / 2]] + _values[_sortIdx[_count / 2 - 1]]) / 2;
    }

    void RunningMedian::setSearchMode(uint8_t searchMode)
    {
        if (searchMode == 1)
            _searchMode = 1;
        else
            _searchMode = 0;
    }

    uint8_t RunningMedian::getSearchMode()
    {
        return _searchMode;
    }

    ////////////////////////////////////////////////////////////
    //
    //  PRIVATE
    //

    //  insertion sort - _searchMode = linear or binary.

    void RunningMedian::sort()
    {
        uint16_t lo = 0;
        uint16_t hi = 0;
        uint16_t mi = 0;
        uint16_t temp = 0;

        for (uint16_t i = 1; i < _count; i++)
        {
            temp = _sortIdx[i];
            float f = _values[temp];

            //  handle special case f is smaller than all elements first.
            //  only one compare needed, improves linear search too.
            if (f <= _values[_sortIdx[0]])
            {
                hi = 0;
            }
            else
            {
                if (_searchMode == 0)
                {
                    hi = i;
                    //  find insertion point with linear search
                    while ((hi > 0) && (f < _values[_sortIdx[hi - 1]]))
                    {
                        hi--;
                    }
                }
                else if (_searchMode == 1)
                {
                    //  find insertion point with binary search
                    lo = 0;
                    hi = i;
                    //  be aware there might be duplicates
                    while (hi - lo > 1)
                    {
                        mi = (lo + hi) / 2;
                        if (f < _values[_sortIdx[mi]])
                        {
                            hi = mi;
                        }
                        else
                        {
                            lo = mi;
                        }
                    }
                }
            }

            //  move elements to make space
            uint16_t k = i;
            while (k > hi)
            {
                _sortIdx[k] = _sortIdx[k - 1];
                k--;
            }

            //  insert at right spot.
            _sortIdx[k] = temp;
        }
        _sorted = true;
    }
} // namespace hw_interface
