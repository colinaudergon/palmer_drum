# palmer_drum

A port of the drum section from [Mutable Instruments Peaks](https://mutable-instruments.net/modules/peaks/) to the
Raspberry Pi RP2040 microcontroller (Raspberry Pi Pico).

Peaks is a versatile Eurorack module that, among other modes, includes three classic analog-modeled percussion
voices: a bass drum, a snare drum, and a hi-hat. `palmer_drum` extracts and reimplements this drum synthesis engine
(originally written for the STM32F Peaks firmware) as a standalone RP2040 project, driving audio out through the
RP2040's PWM peripheral instead of Peaks' original DAC/codec hardware. Peaks' "number station" generator function
is also ported here alongside the drum voices.

## Project layout

- `palmer_drum.cpp` — application entry point: clock/stdio setup and audio codec initialization.
- `sounds_generators/` — the ported Peaks drum synthesis engine:
  - `drums/` — bass drum, snare drum, hi-hat (FM drum) voice implementations.
  - `peaks_ressources/` — supporting resources (lookup tables, RNG, ring buffer) ported from Peaks.
  - `number_station/` — number station generator, ported from Peaks.
  - `processor.cpp/.h` — voice processing/sequencing glue.
- `hw_interfaces/` — RP2040 hardware drivers:
  - `pwm_audio_codec/` — PWM-based audio output driver used in place of Peaks' original codec.
  - `adc/`, `gate_input/` — analog input and gate/trigger input interfaces.
- `lib/` — shared DSP/STM-compatibility headers (`dsp.h`, `mu_dsp.h`, `mu_stmlib.h`) used to bridge the ported Peaks
  code onto the RP2040 SDK.

## Prerequisites

This is a standard [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) CMake project targeting the
RP2040 (`PICO_BOARD` defaults to `pico`). You'll need:

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) (this project was set up against SDK `2.3.0`)
- [Pico toolchain](https://github.com/raspberrypi/pico-sdk-tools) / GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`, tested with `15_2_Rel1`)
- CMake >= 3.13
- Ninja (or another CMake-supported build system)
- [picotool](https://github.com/raspberrypi/picotool) (optional, for flashing/inspecting the built UF2)

The easiest way to get all of the above is via the
[Raspberry Pi Pico VS Code extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico),
which this repo's `.vscode/` configuration is set up to use. Alternatively, install the SDK and toolchain manually and
set the `PICO_SDK_PATH` environment variable to point at your SDK checkout.

## Building

### Using the Pico VS Code extension

Open the project folder in VS Code with the Raspberry Pi Pico extension installed, then use the extension's
**Compile** / **Run Project** commands. Board type and SDK/toolchain versions are picked up automatically from
`CMakeLists.txt`.

### Manual command-line build (Windows / PowerShell)

```powershell
# Set once per environment if not already configured (adjust path to your SDK checkout)
$env:PICO_SDK_PATH = "C:\path\to\pico-sdk"

# Configure
cmake -S . -B build -G Ninja -DPICO_BOARD=pico

# Build
cmake --build build
```

### Manual command-line build (Linux / macOS)

```bash
export PICO_SDK_PATH=/path/to/pico-sdk

cmake -S . -B build -G Ninja -DPICO_BOARD=pico
cmake --build build
```

A successful build produces `build/palmer_drum.uf2` (along with `.elf`, `.bin`, and `.hex` outputs) which can be
flashed to the RP2040 by putting the board into BOOTSEL mode and copying the `.uf2` file to the mounted drive, or via
`picotool load build/palmer_drum.uf2`.

## Hardware notes

- Audio output is generated via the RP2040 PWM peripheral (`hw_interfaces/pwm_audio_codec`), not a dedicated DAC.
- Gate/trigger inputs and analog control inputs are handled by `hw_interfaces/gate_input` and `hw_interfaces/adc`
  respectively.
- The system clock is set to 176 MHz (`set_sys_clock_khz(176000, true)`) in `palmer_drum.cpp` to support the audio
  timing requirements of the PWM codec.
