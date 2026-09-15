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

// SampleTableFromBins: an ISampleTable that aggregates the AudioBin energy-
// bin tables embedded directly in samples.h's kSamples[] entries (see
// sample.bins/sample.n_bins, produced by Scripts/wav_to_header.py) into a
// single flattened, selectable pool of grain sources -- one ISampleTable
// "entry" per bin, rather than per whole sample (contrast with SampleTable,
// in grain_source.h, which selects whole samples).
//
// Reading bins straight out of kSamples[] (rather than a separately
// registered/aggregated bin table) means a sample's audio is stored
// exactly once regardless of whether it's used for whole-sample playback
// (SampleTable) or bin-based grain selection (SampleTableFromBins) -- both
// simply index into the same kSamples[i].samples array via each AudioBin's
// `index`/`size` window.
//
// This is a first version: exactly one bin is "active" (selectable by
// GrainSource) at a time, chosen directly by its flattened index (see
// Init()/MapPotToActiveMask()) rather than by an arbitrary combination.
// A real per-sample bin count can easily exceed 32 (e.g. 12 samples x 16
// bins each == 192), which rules out SampleTable's bitmask-based approach
// outright -- there is no fixed-width integer type wide enough to give
// every bin its own selectable bit. A later change is expected to
// replace/augment single-bin selection with power/envelope-driven
// automatic bin selection (e.g. biasing towards loud/transient bins early
// in a hit and quieter tail bins as its decay envelope falls) -- Init()
// and the active-bin state are kept private to this class so that later
// change shouldn't need to touch GrainSource or ClapEngine's use of the
// ISampleTable interface.
#pragma once

#include <cstdint>
#include <cstddef>

#include "i_sample_table.h"
#include "../samples/samples.h"

namespace peaks
{

  namespace sample_table_from_bins_internal
  {
    // Total number of bins reachable across every entry of kSamples[],
    // computed once at compile time (kSamples/its .bins/.n_bins fields
    // are all constexpr) -- capped at kMaxTotalBins as a generous sanity
    // bound only (not a hard architectural limit).
    constexpr size_t kMaxTotalBins = 512;

    constexpr size_t ComputeTotalBins()
    {
      size_t total = 0;
      for (size_t i = 0; i < kNsamples; ++i)
      {
        total += kSamples[i].n_bins;
      }
      return total > kMaxTotalBins ? kMaxTotalBins : total;
    }
  } // namespace sample_table_from_bins_internal

  class SampleTableFromBins : public ISampleTable
  {
  public:
    SampleTableFromBins() : active_index_(0), has_active_(false) {}

    // active_mask is not a bitmask here (see the class comment) -- it is
    // instead the flattened index of the single bin to make active,
    // exactly as returned by MapPotToActiveMask(). Values >= total_bins()
    // are clamped to the last valid bin; if total_bins() == 0 (no bins
    // available), no bin becomes active.
    inline void Init(uint32_t active_mask) override
    {
      if (kTotalBins == 0)
      {
        has_active_ = false;
        active_index_ = 0;
        return;
      }
      active_index_ = active_mask < kTotalBins
                          ? static_cast<size_t>(active_mask)
                          : kTotalBins - 1;
      has_active_ = true;
    }

    inline bool valid() const override { return has_active_; }

    inline bool IsActive(size_t index) const override
    {
      return has_active_ && index == active_index_;
    }

    inline size_t active_count() const override
    {
      return has_active_ ? 1 : 0;
    }

    inline size_t NthActiveIndex(size_t n) const override
    {
      return (has_active_ && n == 0) ? active_index_ : kTotalBins;
    }

    // Deliberately declared (not defined inline) here and defined
    // out-of-line in sample_table_from_bins.cpp instead: see the identical
    // rationale on SampleTable::data() in grain_source.h/.cpp -- this is
    // what pins SampleTableFromBins's vtable to a single translation unit
    // instead of it (and a private copy of every sample's audio data)
    // being duplicated into every TU that constructs one.
    const int16_t *data(size_t index) const override;
    size_t size(size_t index) const override;

    // A simple linear mapping: pot_value == 0 selects flattened bin 0,
    // pot_value == 65535 selects the last bin, everything in between
    // sweeps across total_bins() one at a time. Unlike SampleTable's
    // two-halves/combinations scheme, only a single bin is ever active
    // at once here.
    uint32_t MapPotToActiveMask(uint16_t pot_value) const override;

    // Direct, bounds-checked access to a flattened bin's full AudioBin
    // entry (including its power/raw_power), for callers that want to
    // pick bins based on their energy (e.g. a future power/envelope-
    // driven selection strategy) rather than just play one already
    // chosen by index.
    inline const AudioBin *bin_at(size_t index) const { return ResolveBin(index); }

    // Total number of bins reachable across every entry of kSamples[].
    static inline size_t total_bins() { return kTotalBins; }

  private:
    static constexpr size_t kTotalBins =
        sample_table_from_bins_internal::ComputeTotalBins();

    // Resolves a flattened bin index to the kSamples[] entry it belongs
    // to (for its base sample pointer) and its AudioBin descriptor
    // (index/size window into that base pointer, plus power/raw_power).
    // Returns a null AudioBin pointer / nullptr base pointer if
    // flat_index is out of range.
    struct ResolvedBin
    {
      const int16_t *base;
      const AudioBin *bin;
    };

    inline ResolvedBin ResolveFlat(size_t flat_index) const
    {
      for (size_t i = 0; i < kNsamples; ++i)
      {
        if (flat_index < kSamples[i].n_bins)
        {
          return {kSamples[i].samples, &kSamples[i].bins[flat_index]};
        }
        flat_index -= kSamples[i].n_bins;
      }
      return {nullptr, nullptr};
    }

    inline const AudioBin *ResolveBin(size_t flat_index) const
    {
      return ResolveFlat(flat_index).bin;
    }

    size_t active_index_;
    bool has_active_;

    SampleTableFromBins(const SampleTableFromBins &) = delete;
    const SampleTableFromBins &operator=(const SampleTableFromBins &) = delete;
  };

} // namespace peaks

