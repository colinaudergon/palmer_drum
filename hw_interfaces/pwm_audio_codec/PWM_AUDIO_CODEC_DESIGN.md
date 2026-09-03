# PWM Audio Codec — Design Analysis

This document records the measured/derived audio characteristics of `PicoAudioCodec`
(`hw_interfaces/pwm_audio_codec/`), with the exact source values and computations they
come from. This is a code-derived analysis, not a datasheet copy — every number below
traces back to a constant or formula in `pwm_audio_codec.h` / `pwm_audio_codec.cpp`.

## 1. Bit depth — 10 bits

The PWM compare value range is `[0, kDefaultWrap]`, i.e. `kDefaultWrap + 1` discrete
levels. `kDefaultWrap = 1023` → 1024 levels = **10 bits**.

```cpp
// hw_interfaces/pwm_audio_codec/include/pwm_audio_codec.h
static constexpr uint kDefaultWrap = 1023;
```

Samples are supplied as 16-bit signed integers but are quantized straight down to this
10-bit PWM compare value, with **no dithering or noise shaping**:

```cpp
// hw_interfaces/pwm_audio_codec/src/pwm_audio_codec.cpp
uint16_t PicoAudioCodec::Int16ToPwmSample(int16_t sample) const
{
    // Map [INT16_MIN, INT16_MAX] to [0, kDefaultWrap], centered at the midpoint so the
    // signal can be AC-coupled through a capacitor on the output.
    float scaled = (static_cast<float>(sample) / 65536.0f + 0.5f) * static_cast<float>(kDefaultWrap);
    return static_cast<uint16_t>(scaled + 0.5f);
}
```

## 2. Theoretical SNR — 61.96 dB

Standard ideal-quantizer formula for N-bit uniform quantization of a full-scale sine
wave:

```
SNR(dB) = 6.02 * N + 1.76
```

With N = 10 bits (from §1):

```
SNR(dB) = 6.02 * 10 + 1.76
        = 60.2 + 1.76
        = 61.96 dB
```

Caveats (all applicable here, since the code performs no dithering/noise shaping):
- Assumes a full-scale sine input; real program material peaking below full scale will
  measure worse.
- Assumes uncorrelated quantization noise (valid for dithered/noise-shaped systems).
  Since `Int16ToPwmSample()` truncates directly with no dither, quantization error can
  be signal-correlated (audible as distortion) rather than flat noise, so **61.96 dB is
  a theoretical ceiling**, not a guaranteed measured floor.
- No noise shaping means the quantization noise stays flat across the audio band
  instead of being pushed toward the ~131 kHz carrier (§4).

## 3. Sample rate — 44.1 kHz nominal, ~43.65 kHz actual

Target sample rate constant:

```cpp
// hw_interfaces/pwm_audio_codec/include/pwm_audio_codec.h
static constexpr float kDefaultSampleRateHz = 44100.0f;
static constexpr uint kRepetitionRate = 3;
```

The PWM clock divider needed to hit a target sample rate is derived from the *measured*
system clock:

```cpp
// hw_interfaces/pwm_audio_codec/src/pwm_audio_codec.cpp
float PicoAudioCodec::ComputeClockDiv(float target_sample_rate_hz) const
{
    return f_clk_sys_hz_ / static_cast<float>(kDefaultWrap + 1) / target_sample_rate_hz /
           static_cast<float>(kRepetitionRate);
}
```

```cpp
// hw_interfaces/pwm_audio_codec/src/pwm_audio_codec.cpp — Init()
uint f_clk_sys_khz = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
f_clk_sys_hz_ = static_cast<float>(f_clk_sys_khz) * 1000.0f;
float clock_div = ComputeClockDiv(kDefaultSampleRateHz);
```

The system clock is fixed at 176 MHz elsewhere in the project:

```cpp
// palmer_drum.cpp
set_sys_clock_khz(176000, true);
```

Computation of the raw (unquantized) divider for the 44.1 kHz target:

```
clock_div = f_clk_sys_hz / (wrap + 1) / target_sample_rate_hz / repetition_rate
          = 176,000,000 / 1024 / 44100 / 3
          ≈ 1.2991
```

The RP2040 PWM clock divider is an 8.4 fixed-point value (1/16th-step granularity), so
`pwm_config_set_clkdiv()` quantizes this to the nearest representable step:

```
quantized_clock_div = round(1.2991 * 16) / 16
                     = round(20.79) / 16
                     = 21 / 16
                     = 1.3125
```

Actual sample rate achieved with the quantized divider:

```
actual_sample_rate_hz = f_clk_sys_hz / (wrap + 1) / quantized_clock_div / repetition_rate
                       = 176,000,000 / 1024 / 1.3125 / 3
                       ≈ 43,650 Hz  (~43.65 kHz)
```

`SetOutputFrequency()` performs the same quantization explicitly at runtime and reports
back the actually-configured frequency when it can't hit the target exactly:

```cpp
// hw_interfaces/pwm_audio_codec/src/pwm_audio_codec.cpp
float raw_clock_div = ComputeClockDiv(new_frequency_hz);
// ...
float quantized_clock_div = std::round(raw_clock_div * 16.0f) / 16.0f;
// ...
float actual_frequency_hz = f_clk_sys_hz_ / static_cast<float>(kDefaultWrap + 1) /
                             quantized_clock_div / static_cast<float>(kRepetitionRate);
```

## 4. PWM carrier frequency — 132.3 kHz nominal, ~130.9 kHz actual

Each audio sample's PWM compare value is written to the CC register and held for
`kRepetitionRate = 3` PWM wrap cycles before advancing to the next sample (see the
`pwm_dma_chan` configuration, which transfers `kRepetitionRate` times per sample):

```cpp
// hw_interfaces/pwm_audio_codec/src/pwm_audio_codec.cpp — InitChannel()
dma_channel_configure(
    channel.pwm_dma_chan,
    &pwm_dma_chan_config,
    &pwm_hw->slice[channel.pin_slice].cc,   // Write to this slice's CC register.
    &channel.single_sample,                  // Read from single_sample.
    kRepetitionRate,                         // Transfer once per desired sample repetition.
    false);
```

This means the PWM wrap (carrier) frequency is `repetition_rate` times the audio sample
rate:

```
carrier_hz = sample_rate_hz * repetition_rate
```

Nominal (design target):
```
carrier_hz = 44,100 * 3 = 132,300 Hz  (132.3 kHz)
```

Actual (using the quantized-divider sample rate from §3):
```
carrier_hz = 43,650 * 3 ≈ 130,950 Hz  (~130.9 kHz)
```

Equivalently, the carrier is directly the PWM slice's wrap frequency:
```
carrier_hz = f_clk_sys_hz / (wrap + 1) / quantized_clock_div
           = 176,000,000 / 1024 / 1.3125
           ≈ 130,950 Hz
```

## 5. Summary table

| Quantity                         | Nominal / theoretical | Actual (176 MHz sysclk, 8.4-bit clkdiv quantization) |
|-----------------------------------|------------------------|-------------------------------------------------------|
| Bit depth                        | 10 bits (`kDefaultWrap+1` = 1024 levels) | 10 bits (unchanged — no dithering/noise shaping) |
| Theoretical SNR                  | 61.96 dB (`6.02*10 + 1.76`) | ≤ 61.96 dB (ceiling; real SNR likely lower, no dither) |
| Sample rate                      | 44,100 Hz (`kDefaultSampleRateHz`) | ≈ 43,650 Hz |
| PWM carrier frequency            | 132,300 Hz (`sample_rate * kRepetitionRate`) | ≈ 130,950 Hz |

## 6. Possible future improvement: dual-PWM

Increasing effective resolution beyond 10 bits without lowering the carrier frequency
would require a **dual-PWM** (MSB/LSB split, resistor-summed) output stage. This was
discussed but not implemented:
- Requires a second PWM slice + DMA chain per audio channel (doubling DMA/GPIO usage).
- Requires a precision resistor summing network (ideally ≤0.1% tolerance, or an
  integrated R-2R style DAC) — ordinary 1% resistors would largely negate the
  resolution gain through added non-linearity (INL/DNL).
- Alternative, hardware-free approach: add dithering/noise shaping in
  `Int16ToPwmSample()` to improve *perceived* SNR within the existing 10-bit single-PWM
  path, without raising the hard quantization floor computed in §2.
