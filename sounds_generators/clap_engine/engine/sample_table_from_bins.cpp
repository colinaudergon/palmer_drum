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

// SampleTableFromBins implementation. See sample_table_from_bins.h for
// design notes, including why data()/size() are defined out-of-line here
// rather than inline in the header.

#include "sample_table_from_bins.h"

namespace peaks
{

  const int16_t *SampleTableFromBins::data(size_t index) const
  {
    ResolvedBin resolved = ResolveFlat(index);
    if (resolved.base == nullptr || resolved.bin == nullptr)
    {
      return nullptr;
    }
    return resolved.base + resolved.bin->index;
  }

  size_t SampleTableFromBins::size(size_t index) const
  {
    const AudioBin *bin = ResolveBin(index);
    return bin != nullptr ? bin->size : 0;
  }

  uint32_t SampleTableFromBins::MapPotToActiveMask(uint16_t pot_value) const
  {
    if (kTotalBins == 0)
    {
      return 0;
    }
    uint32_t index =
        (static_cast<uint32_t>(pot_value) * static_cast<uint32_t>(kTotalBins)) /
        65536u;
    if (index >= kTotalBins)
    {
      index = static_cast<uint32_t>(kTotalBins) - 1;
    }
    return index;
  }

} // namespace peaks
