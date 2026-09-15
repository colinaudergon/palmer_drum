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
  - `clap_engine/` — granular "clap" sample-playback engine (see below).
  - `processor.cpp/.h` — voice processing/sequencing glue.
- `hw_interfaces/` — RP2040 hardware drivers:
  - `pwm_audio_codec/` — PWM-based audio output driver used in place of Peaks' original codec.
  - `adc/`, `gate_input/` — analog input and gate/trigger input interfaces.
- `lib/` — shared DSP/STM-compatibility headers (`dsp.h`, `mu_dsp.h`, `mu_stmlib.h`) used to bridge the ported Peaks
  code onto the RP2040 SDK.

## Processors and parameters

Each processor is configured by four control-rate parameters, `parameter[0]` through `parameter[3]`. These come
directly from four analog potentiometer inputs read by the RP2040's ADC — `parameter[0]` is ADC 1, `parameter[1]` is
ADC 2, `parameter[2]` is ADC 3, and `parameter[3]` is ADC 4 (see `AdcToParameter()` in `core1/core1_main.cpp`). Each
parameter is a `uint16_t` spanning the ADC's full `0..65535` scaled range, regardless of what it controls in a given
processor.

### Bass Drum (`kBassDrum`, default) — `sounds_generators/drums/bass_drum.h`

808-style bass drum.

| Parameter | ADC   | Function |
|-----------|-------|----------|
| `parameter[0]` | ADC 1 | Frequency (bipolar around the pot's center) — base pitch of the drum. |
| `parameter[1]` | ADC 2 | Punch — amount of transient "click"/attack energy. |
| `parameter[2]` | ADC 3 | Tone — brightness of the low-pass filter applied to the resonator. |
| `parameter[3]` | ADC 4 | Decay — resonator resonance/decay time (how long the tone rings out). |

### Snare Drum (`kSnareDrum`) — `sounds_generators/drums/snare_drum.h`

808-style snare drum.

| Parameter | ADC   | Function |
|-----------|-------|----------|
| `parameter[0]` | ADC 1 | Frequency (bipolar around the pot's center) — base pitch of the two body oscillators. |
| `parameter[1]` | ADC 2 | Tone — balance of gain between the two body oscillators. |
| `parameter[2]` | ADC 3 | Snappy — amount of noise ("snare snap") mixed into the body. |
| `parameter[3]` | ADC 4 | Decay — body and noise decay time. |

### Hi-Hat (`kHighHat`) — `sounds_generators/drums/high_hat.h`

808-style hi-hat.

| Parameter | ADC   | Function |
|-----------|-------|----------|
| `parameter[0]` | ADC 1 | Envelope update period — how often the amplitude envelope is refreshed (lower rates add a lo-fi/stepped character). |
| `parameter[1]` | ADC 2 | Tone repeat processing — oversampling factor of the metallic noise filter (affects brightness/character). |
| `parameter[2]` | ADC 3 | Noise filter resonance. |
| `parameter[3]` | ADC 4 | Noise mixer — blends between half-wave-rectified and full (bipolar) noise, affecting harshness. |

### FM Drum (`kFmDrum`) — `sounds_generators/drums/fm_drum.h`

Sine FM drum, similar to the BD/SD in the Anushri synthesizer.

| Parameter | ADC   | Function |
|-----------|-------|----------|
| `parameter[0]` | ADC 1 | Frequency — base pitch. |
| `parameter[1]` | ADC 2 | FM amount — amount of frequency modulation (metallic tone / pitch sweep). |
| `parameter[2]` | ADC 3 | Decay — amplitude and FM envelope decay time. |
| `parameter[3]` | ADC 4 | Noise — amount of noise/overdrive blended into the output. |

### Number Station (`kNumberStation`) — `sounds_generators/number_station/number_station.h`

Shortwave "numbers station" voice generator.

| Parameter | ADC   | Function |
|-----------|-------|----------|
| `parameter[0]` | ADC 1 | Tone — pitch and pitch-shift amount of the voice-like oscillator. |
| `parameter[1]` | ADC 2 | Transition probability — likelihood of the spoken "digit" changing. |
| `parameter[2]` | ADC 3 | Noise — amount of background/static noise. |
| `parameter[3]` | ADC 4 | Distortion — amount of waveshaping distortion applied to the voice. |

### Clap Engine (`kClapEngine`) — `sounds_generators/clap_engine/clap_engine.h`

Granular sample-playback ("clap") engine: on each gate trigger, short bursts ("grains") of one or more built-in
samples are triggered stochastically for as long as a decaying envelope stays open.

| Parameter | ADC   | Function |
|-----------|-------|----------|
| `parameter[0]` | ADC 1 | Source — first half of the pot's range selects one of the 12 built-in samples individually; the second half selects among every combination of two or more samples played together. |
| `parameter[1]` | ADC 2 | Density — average grain rate (how many grains are triggered per second). |
| `parameter[2]` | ADC 3 | Spread — amount of per-grain randomization: start position within the sample, playback rate/pitch, duration, and probability of reverse playback. |
| `parameter[3]` | ADC 4 | Decay — shapes the whole grain cloud after a gate trigger: how long grain density, grain duration, and output gain all taper off together. |

### Clap Engine From Bins (`kClapEngineFromBins`) — `sounds_generators/clap_engine/clap_engine.h`

Same `ClapEngine` engine/parameters as above, but backed by a `SampleTableFromBins` instead of a plain `SampleTable`:
its selectable grain sources are individual energy "bins" rather than whole samples. Bins are read directly from
`kSamples[]` (see `clap_engine/samples/samples.h`) -- each sample's `AudioBin` array (`index`/`size`/`power`/
`raw_power`, generated alongside its audio by `Scripts/wav_to_header.py --bins N`) references an offset window into
that *same* sample's embedded array, so bin-based and whole-sample playback share one copy of the audio data.
Currently 16 bins per sample across all 12 built-in samples. Exactly one bin is active (selectable by a grain) at a
time.

| Parameter | ADC   | Function |
|-----------|-------|----------|
| `parameter[0]` | ADC 1 | Source — selects a single active bin, linearly, across every bin currently registered with the table. |
| `parameter[1]` | ADC 2 | Density — same as `kClapEngine`. |
| `parameter[2]` | ADC 3 | Spread — same as `kClapEngine`. |
| `parameter[3]` | ADC 4 | Decay — same as `kClapEngine`. |

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
