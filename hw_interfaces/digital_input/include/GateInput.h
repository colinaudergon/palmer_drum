/*
 * @file GateInput.h
 * @brief Plain gate/trigger digital input -- no added behavior over DigitalInput.
 */

#pragma once

#include "DigitalInput.h"

namespace hw_interface
{
    // Thin alias for the plain gate-input use case: shares all behavior with DigitalInput,
    // kept as its own type for call-site clarity (as opposed to ButtonInput, which adds
    // long-press detection).
    class GateInput : public DigitalInput
    {
    public:
        using DigitalInput::DigitalInput;
    };
} // namespace hw_interface