// Common interface implemented by every modulation-source processor
// (BassDrum, SnareDrum, HighHat, FmDrum, NumberStation), so that Processors
// can dispatch to whichever one is currently selected through a single
// virtual call instead of a hand-built function-pointer table.

#pragma once

#include <cstddef>
#include <cstdint>

#include "peaks_ressources/gate_processor.h"

namespace peaks {

class IProcessor {
 public:
  virtual ~IProcessor() = default;

  virtual void Init() = 0;
  virtual void Process(const GateFlags* gate_flags, int16_t* out, size_t size) = 0;
  virtual void Configure(uint16_t* parameter) = 0;
};

}  // namespace peaks

