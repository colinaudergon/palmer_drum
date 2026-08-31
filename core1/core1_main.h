#pragma once
#include "pico/stdlib.h"
#include "Leds.h"
#include "GateInput.h"

static constexpr uint kGateInputGpio = 14;
static constexpr uint kButtonGpio = 15;
void Core1Main();