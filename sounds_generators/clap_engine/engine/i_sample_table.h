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

// Shared, read-only interface for a bank ("table") of grain source
// material a GrainSource may read from.
//
// See grain_source.h's SampleTable for the original whole-sample
// implementation (a bank of up to kNsamples full samples, selected via a
// bitmask), and sample_table_from_bins.h's SampleTableFromBins for a
// bin-based implementation (a bank of individual energy bins, aggregated
// across one or more samples, as produced by Scripts/wav_to_bins.py).
//
// GrainSource/GrainSource::Essence and ClapEngine::ActivateGrains() only
// ever interact with a sample table through this interface, so a
// concrete implementation can be swapped in (or added alongside the
// existing one) without either of them needing to change.
#pragma once

#include <cstdint>
#include <cstddef>

namespace peaks
{

  class ISampleTable
  {
  public:
    virtual ~ISampleTable() {}

    // Replaces which entries are active. The meaning of active_mask is
    // entirely up to the implementation (e.g. SampleTable treats it as a
    // literal per-entry bitmask; SampleTableFromBins treats it as a
    // single flattened bin index, since its entry count can exceed 32
    // and so can't be addressed bit-by-bit) -- always pass a value
    // previously obtained from this same instance's
    // MapPotToActiveMask(), never a raw/assumed-bitmask literal.
    virtual void Init(uint32_t active_mask) = 0;

    // True as long as at least one entry is active.
    virtual bool valid() const = 0;

    // True if entry `index` is part of the active set.
    virtual bool IsActive(size_t index) const = 0;

    // Number of currently active entries (population count of the active
    // mask).
    virtual size_t active_count() const = 0;

    // Maps n (0 .. active_count()-1) to the actual entry index of the
    // n-th active entry, in ascending index order. Returns an
    // out-of-range index (>= however many entries this table has) if n
    // is out of range (e.g. no entries are active).
    virtual size_t NthActiveIndex(size_t n) const = 0;

    // Direct, bounds-checked access to entry `index`'s sample buffer and
    // length, regardless of whether that index is currently active. Used
    // by GrainSource once a specific index has already been chosen (e.g.
    // via NthActiveIndex()). Returns nullptr/0 for an out-of-range index.
    virtual const int16_t *data(size_t index) const = 0;
    virtual size_t size(size_t index) const = 0;

    // Maps a raw pot/ADC reading (0..65535) onto an active_mask suitable
    // for Init(), entirely according to this implementation's own notion
    // of what its selectable entries are (whole samples, bins, ...) --
    // this is what lets ClapEngine's set_source() stay generic across
    // every ISampleTable implementation instead of hard-coding a mapping
    // tied to one particular layout (e.g. SampleTable's kNsamples-sized
    // bitmask). Called with pot_value == 0, this should return a sane,
    // non-empty default mask (used by ClapEngine::Init()).
    virtual uint32_t MapPotToActiveMask(uint16_t pot_value) const = 0;
  };

} // namespace peaks
