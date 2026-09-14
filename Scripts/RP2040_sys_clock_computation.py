#!/usr/bin/env python3
"""
RP2040_sys_clock_computation.py

Finds the RP2040 system clock frequency (achievable via the on-chip PLL) that
minimizes the sample-rate error of `hw_interfaces/pwm_audio_codec` for a given
target audio sample rate.

Background
----------
PicoAudioCodec derives its PWM clock divider from the *measured* system clock:

    clock_div = f_clk_sys / (wrap + 1) / target_sample_rate_hz / repetition_rate

(see PicoAudioCodec::ComputeClockDiv() in
hw_interfaces/pwm_audio_codec/src/pwm_audio_codec.cpp)

The RP2040 PWM hardware divider is an 8.4 fixed-point value (1/16th-step
granularity), so the computed divider gets rounded to the nearest 1/16 before
being applied, which shifts the actual achieved sample rate away from the
target. Since the system clock itself can only take specific discrete values
(determined by the PLL's integer feedback divider and two integer post
dividers), picking a different --but still legally achievable-- system clock
can make the rounded divider land almost exactly where it should, essentially
eliminating this error.

This script enumerates every RP2040 system clock frequency achievable from a
12 MHz crystal (the standard Pico/Pico W crystal) via:

    vco      = fbdiv * xosc_hz          (fbdiv in [16, 320], vco in [750MHz, 1600MHz])
    sys_clk  = vco / (postdiv1 * postdiv2)   (postdiv1, postdiv2 in [1, 7])

and reports the candidate(s) that minimize the resulting audio sample-rate
error, alongside the resulting PWM carrier frequency
(sample_rate * repetition_rate).

Only frequencies that are an exact integer number of kHz are considered, since
those are the only ones actually requestable via the pico-sdk's
set_sys_clock_khz(khz, ...) -- the API this project uses (see
palmer_drum.cpp's set_sys_clock_khz(176000, true) call). set_sys_clock_khz()
requires an exact match (integer fbdiv, no rounding/tolerance) and simply fails
if the requested kHz value isn't exactly achievable, so non-integer-kHz PLL
solutions (e.g. 169,333,333.33 Hz) are excluded up front rather than being
reported as usable.

Usage
-----
    python RP2040_sys_clock_computation.py 44100
    python RP2040_sys_clock_computation.py 48000 --top 5
    python RP2040_sys_clock_computation.py 44100 --xosc-hz 12000000 --wrap 1023 --repetition-rate 3
"""

import argparse
from dataclasses import dataclass

# Defaults matching hw_interfaces/pwm_audio_codec/include/pwm_audio_codec.h
DEFAULT_XOSC_HZ = 12_000_000
DEFAULT_WRAP = 1023
DEFAULT_REPETITION_RATE = 3

# RP2040 PLL constraints (see RP2040 datasheet, section 2.18 "Clocks").
MIN_FBDIV, MAX_FBDIV = 16, 320
MIN_POSTDIV, MAX_POSTDIV = 1, 7
MIN_VCO_HZ, MAX_VCO_HZ = 750_000_000, 1_600_000_000

# RP2040 PWM clock divider is an 8.4 fixed-point value: integer part in
# [1, 255], fractional part in 1/16th steps -> valid range is [1, 256).
MIN_CLKDIV, MAX_CLKDIV = 1.0, 256.0
CLKDIV_STEP_DENOM = 16


@dataclass
class Candidate:
    sys_clk_hz: float
    fbdiv: int
    postdiv1: int
    postdiv2: int
    raw_clock_div: float
    quantized_clock_div: float
    actual_sample_rate_hz: float
    carrier_hz: float
    error_hz: float


def enumerate_sys_clocks(xosc_hz: int, min_sys_clk_hz: float, max_sys_clk_hz: float):
    """Yields (sys_clk_hz, fbdiv, postdiv1, postdiv2) for every RP2040 system clock
    frequency in [min_sys_clk_hz, max_sys_clk_hz] that is *exactly* requestable via the
    pico-sdk's set_sys_clock_khz(khz, ...) API.

    set_sys_clock_khz() only accepts an integer kHz value and requires the resulting
    frequency to be hit exactly (integer fbdiv, no rounding/tolerance) -- it does NOT
    round to the closest achievable frequency, and fails outright otherwise. So only
    PLL combinations whose sys_clk_hz is an exact integer multiple of 1000 Hz are
    actually usable through that call; anything else (e.g. 169,333,333.33 Hz) can only
    be reached via the lower-level set_sys_clock_pll(vco_freq, pd1, pd2) API, not
    set_sys_clock_khz(), and is therefore excluded here.
    """
    seen = {}
    for fbdiv in range(MIN_FBDIV, MAX_FBDIV + 1):
        vco = fbdiv * xosc_hz
        if not (MIN_VCO_HZ <= vco <= MAX_VCO_HZ):
            continue
        for postdiv1 in range(MIN_POSTDIV, MAX_POSTDIV + 1):
            for postdiv2 in range(MIN_POSTDIV, postdiv1 + 1):
                sys_clk = vco / (postdiv1 * postdiv2)
                if not (min_sys_clk_hz <= sys_clk <= max_sys_clk_hz):
                    continue
                # Only keep frequencies that are an exact integer number of kHz, i.e.
                # exactly requestable via set_sys_clock_khz(). Guard against float
                # rounding noise (e.g. 152400000.00000003) with a tight tolerance.
                khz = sys_clk / 1000.0
                if abs(khz - round(khz)) > 1e-6:
                    continue
                # Keep the combination with fewest dividers for a given frequency
                # (arbitrary tie-break; doesn't affect the resulting sys_clk_hz).
                key = round(sys_clk)
                if key not in seen or (postdiv1 * postdiv2) < (seen[key][1] * seen[key][2]):
                    seen[key] = (fbdiv, postdiv1, postdiv2)

    for key, (fbdiv, postdiv1, postdiv2) in seen.items():
        yield fbdiv * xosc_hz / (postdiv1 * postdiv2), fbdiv, postdiv1, postdiv2


def best_sys_clocks(
    target_sample_rate_hz: float,
    xosc_hz: int = DEFAULT_XOSC_HZ,
    wrap: int = DEFAULT_WRAP,
    repetition_rate: int = DEFAULT_REPETITION_RATE,
    min_sys_clk_hz: float = 100_000_000,
    max_sys_clk_hz: float = 200_000_000,
    top: int = 10,
):
    """Returns the `top` Candidate entries (sorted by ascending sample-rate
    error) for hitting target_sample_rate_hz."""
    wrap_plus_1 = wrap + 1
    results = []

    for sys_clk_hz, fbdiv, postdiv1, postdiv2 in enumerate_sys_clocks(
        xosc_hz, min_sys_clk_hz, max_sys_clk_hz
    ):
        raw_clock_div = sys_clk_hz / wrap_plus_1 / target_sample_rate_hz / repetition_rate
        if raw_clock_div < MIN_CLKDIV or raw_clock_div >= MAX_CLKDIV:
            # Not representable by the RP2040's 8.4 fixed-point PWM divider.
            continue

        quantized_clock_div = round(raw_clock_div * CLKDIV_STEP_DENOM) / CLKDIV_STEP_DENOM
        quantized_clock_div = max(MIN_CLKDIV, min(quantized_clock_div, MAX_CLKDIV - 1.0 / CLKDIV_STEP_DENOM))

        actual_sample_rate_hz = sys_clk_hz / wrap_plus_1 / quantized_clock_div / repetition_rate
        carrier_hz = actual_sample_rate_hz * repetition_rate
        error_hz = abs(actual_sample_rate_hz - target_sample_rate_hz)

        results.append(
            Candidate(
                sys_clk_hz=sys_clk_hz,
                fbdiv=fbdiv,
                postdiv1=postdiv1,
                postdiv2=postdiv2,
                raw_clock_div=raw_clock_div,
                quantized_clock_div=quantized_clock_div,
                actual_sample_rate_hz=actual_sample_rate_hz,
                carrier_hz=carrier_hz,
                error_hz=error_hz,
            )
        )

    results.sort(key=lambda c: c.error_hz)
    return results[:top]


def main():
    parser = argparse.ArgumentParser(
        description="Find the RP2040 system clock frequency that minimizes PWM audio "
        "sample-rate error for a target sample rate (PicoAudioCodec)."
    )
    parser.add_argument(
        "target_sample_rate_hz",
        type=float,
        help="Desired audio sample rate in Hz (e.g. 44100).",
    )
    parser.add_argument(
        "--xosc-hz",
        type=int,
        default=DEFAULT_XOSC_HZ,
        help=f"Crystal oscillator frequency in Hz (default: {DEFAULT_XOSC_HZ}).",
    )
    parser.add_argument(
        "--wrap",
        type=int,
        default=DEFAULT_WRAP,
        help=f"PWM 'wrap' value, i.e. kDefaultWrap (default: {DEFAULT_WRAP}).",
    )
    parser.add_argument(
        "--repetition-rate",
        type=int,
        default=DEFAULT_REPETITION_RATE,
        help=f"PWM repetition rate, i.e. kRepetitionRate (default: {DEFAULT_REPETITION_RATE}).",
    )
    parser.add_argument(
        "--min-sys-clk-hz",
        type=float,
        default=100_000_000,
        help="Lower bound of the system clock search range in Hz (default: 100e6).",
    )
    parser.add_argument(
        "--max-sys-clk-hz",
        type=float,
        default=200_000_000,
        help="Upper bound of the system clock search range in Hz (default: 200e6).",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=5,
        help="Number of best candidates to print (default: 5).",
    )
    args = parser.parse_args()

    candidates = best_sys_clocks(
        target_sample_rate_hz=args.target_sample_rate_hz,
        xosc_hz=args.xosc_hz,
        wrap=args.wrap,
        repetition_rate=args.repetition_rate,
        min_sys_clk_hz=args.min_sys_clk_hz,
        max_sys_clk_hz=args.max_sys_clk_hz,
        top=args.top,
    )

    if not candidates:
        print("No achievable system clock found in the given search range.")
        return

    best = candidates[0]
    print(f"Target sample rate: {args.target_sample_rate_hz:.2f} Hz\n")
    print("Best choice:")
    print(f"  sys_clk_hz          = {best.sys_clk_hz:.4f} Hz  (set_sys_clock_khz({round(best.sys_clk_hz / 1000)}, true))")
    print(f"  fbdiv/postdiv1/postdiv2 = {best.fbdiv} / {best.postdiv1} / {best.postdiv2}")
    print(f"  raw_clock_div       = {best.raw_clock_div:.4f}")
    print(f"  quantized_clock_div = {best.quantized_clock_div:.4f}")
    print(f"  actual_sample_rate  = {best.actual_sample_rate_hz:.2f} Hz  (error: {best.error_hz:.3f} Hz)")
    print(f"  pwm_carrier_freq    = {best.carrier_hz:.2f} Hz")

    if len(candidates) > 1:
        print(f"\nTop {len(candidates)} candidates:")
        print(
            f"{'sys_clk (Hz)':>15} {'fbdiv':>6} {'pd1':>4} {'pd2':>4} "
            f"{'raw_div':>10} {'quant_div':>10} {'actual_rate (Hz)':>18} "
            f"{'carrier (Hz)':>14} {'error (Hz)':>12} {'set_sys_clock_khz()':>20}"
        )
        for c in candidates:
            print(
                f"{c.sys_clk_hz:15.4f} {c.fbdiv:6d} {c.postdiv1:4d} {c.postdiv2:4d} "
                f"{c.raw_clock_div:10.4f} {c.quantized_clock_div:10.4f} "
                f"{c.actual_sample_rate_hz:18.2f} {c.carrier_hz:14.2f} {c.error_hz:12.3f} "
                f"{round(c.sys_clk_hz / 1000):20d}"
            )


if __name__ == "__main__":
    main()
