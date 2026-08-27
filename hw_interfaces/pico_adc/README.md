# pico_adc

`PicoAdcController` drives the RP2040's on-board ADC through an external analog multiplexer,
letting a single ADC input read up to 8 channels. Each channel's readings are smoothed with a
`RunningMedian` filter (see `running_median.h`) to reduce noise.

The controller is configured directly with a `PicoAdcConfig` struct (no generic interface layer
or config blob):

```cpp
struct PicoAdcConfig
{
    uint internal_adc_gpio; // Pico GPIO wired to the mux's shared output (must be 26-29)
    size_t number_of_adc;   // number of mux channels actually in use (max 8)
    uint gpio_ctrl_a;       // mux select line A (LSB)
    uint gpio_ctrl_b;       // mux select line B
    uint gpio_ctrl_c;       // mux select line C (MSB)
};
```

`gpio_ctrl_a/b/c` select which of the up to 8 mux channels is currently routed to
`internal_adc_gpio`; `PicoAdcController` cycles through channels `0..number_of_adc-1`
round-robin each time `Process()` is called.

## Usage

```cpp
#include "pico_adc.h"

using hw_interface::PicoAdcConfig;
using hw_interface::PicoAdcController;

PicoAdcController adc_controller;

// GPIO 26 is wired to the mux output; GPIOs 10/11/12 drive the mux select lines A/B/C;
// 4 of the mux's channels are wired up.
PicoAdcConfig config{26, 4, 10, 11, 12};
if (adc_controller.Init(config) != 0)
{
    // handle init failure (invalid ADC GPIO, etc.)
}

// Call repeatedly (e.g. from the main loop) to advance the round-robin sampling and
// keep each channel's running median up to date.
adc_controller.Process();

// Read the latest median-filtered value (raw 12-bit ADC counts, 0-4095) for a channel.
float value = adc_controller.GetLastReading(/*adc_index=*/0);
```
