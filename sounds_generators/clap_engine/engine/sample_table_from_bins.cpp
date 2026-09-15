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
