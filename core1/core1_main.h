#pragma once
#include "pico/stdlib.h"
#include "Leds.h"
#include "GateInput.h"
#include "ButtonInput.h"

static constexpr uint kGateInputGpio = 14;
static constexpr uint kButtonGpio = 15;
static constexpr uint32_t kButtonLongPressMs = 800;
void Core1Main();