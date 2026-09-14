#!/usr/bin/env python3
"""Convert .wav files into C header files with embedded int16_t sample arrays.

For every .wav file found in the given input folder, this script generates a
corresponding .h file containing:

    const int16_t sample_<name>[] = { ... };
    const size_t sample_<name>_length = N;

Each sample is converted to match the format used by the project's audio
codec (see hw_interfaces/pwm_audio_codec):
  - mono (stereo files are downmixed by averaging L+R)
  - 44100 Hz sample rate (resampled if the source differs)
  - signed 16-bit PCM (int16_t), peak-normalized before quantization

Requires: numpy, scipy (pip install numpy scipy)

Examples:
    python wav_to_header.py samples/
    python wav_to_header.py samples/ -o generated_headers/
"""

import argparse
import os
import re
import sys

try:
    import numpy as np
    from scipy.io import wavfile
    from scipy.signal import resample
except ImportError as exc:  # pragma: no cover - environment guard
    sys.stderr.write(
        "error: this script requires 'numpy' and 'scipy'.\n"
        "       install them with: pip install numpy scipy\n"
        f"       (import failed: {exc})\n"
    )
    sys.exit(1)


TARGET_SAMPLE_RATE_HZ = 44100
VALUES_PER_LINE = 20
# Leave a little headroom below full scale when peak-normalizing, to avoid
# any rounding pushing a sample just past int16 range.
PEAK_NORMALIZE_TARGET = 0.999


def sanitize_identifier(name: str) -> str:
    """Turns an arbitrary filename stem into a valid, prefixed C identifier."""
    lowered = name.lower()
    replaced = re.sub(r"[^a-z0-9_]+", "_", lowered)
    collapsed = re.sub(r"_+", "_", replaced).strip("_")
    if not collapsed:
        collapsed = "unnamed"
    return f"sample_{collapsed}"


def to_float_mono(data: "np.ndarray", dtype: "np.dtype") -> "np.ndarray":
    """Downmixes to mono (if needed) and converts to float64 in [-1, 1]."""
    if data.ndim > 1:
        # Average all channels down to mono. Convert to float first so the
        # sum can't overflow the original integer dtype.
        data = data.astype(np.float64).mean(axis=1)
    else:
        data = data.astype(np.float64)

    if np.issubdtype(dtype, np.integer):
        info = np.iinfo(dtype)
        # Center unsigned formats (e.g. uint8) around 0 first.
        if info.min == 0:
            midpoint = (info.max + 1) / 2.0
            data = data - midpoint
            scale = midpoint
        else:
            scale = max(abs(info.min), abs(info.max))
        data = data / scale
    elif np.issubdtype(dtype, np.floating):
        # Assume already roughly in [-1, 1]; peak normalization below still
        # applies afterward regardless.
        pass
    else:
        raise ValueError(f"unsupported source sample dtype: {dtype}")

    return data


def resample_to_target(data: "np.ndarray", orig_rate: int) -> "np.ndarray":
    if orig_rate == TARGET_SAMPLE_RATE_HZ or len(data) == 0:
        return data
    new_length = max(1, round(len(data) * TARGET_SAMPLE_RATE_HZ / orig_rate))
    return resample(data, new_length)


def normalize_and_quantize(data: "np.ndarray") -> "np.ndarray":
    peak = np.max(np.abs(data)) if len(data) else 0.0
    if peak > 0:
        data = data * (PEAK_NORMALIZE_TARGET / peak)
    quantized = np.round(data * 32767.0)
    quantized = np.clip(quantized, -32768, 32767)
    return quantized.astype(np.int16)


def convert_wav_file(wav_path: str) -> "tuple[np.ndarray, int, int, str]":
    """Returns (int16 mono 44100Hz samples, orig_rate, orig_channels, orig_dtype_name)."""
    orig_rate, raw_data = wavfile.read(wav_path)
    orig_dtype = raw_data.dtype
    orig_channels = 1 if raw_data.ndim == 1 else raw_data.shape[1]

    mono_float = to_float_mono(raw_data, orig_dtype)
    resampled = resample_to_target(mono_float, orig_rate)
    quantized = normalize_and_quantize(resampled)

    return quantized, orig_rate, orig_channels, str(orig_dtype)


def write_header_file(
    output_path: str,
    identifier: str,
    samples: "np.ndarray",
    source_filename: str,
    orig_rate: int,
    orig_channels: int,
    orig_dtype_name: str,
) -> None:
    lines = []
    lines.append(f"// Auto-generated from '{source_filename}' by Scripts/wav_to_header.py")
    lines.append(
        f"// Source: {orig_rate}Hz, {orig_channels}ch, {orig_dtype_name} "
        f"-> {TARGET_SAMPLE_RATE_HZ}Hz mono int16"
    )
    lines.append("// Do not edit by hand; regenerate from the source .wav instead.")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <cstdint>")
    lines.append("#include <cstddef>")
    lines.append("")
    lines.append(f"const int16_t {identifier}[] = {{")

    values = samples.tolist()
    for i in range(0, len(values), VALUES_PER_LINE):
        chunk = values[i:i + VALUES_PER_LINE]
        lines.append("    " + ", ".join(str(v) for v in chunk) + ",")

    lines.append("};")
    lines.append("")
    lines.append(f"const size_t {identifier}_length = {len(values)};")
    lines.append("")

    with open(output_path, "w", newline="\n") as f:
        f.write("\n".join(lines))


def process_folder(input_dir: str, output_dir: str) -> int:
    if not os.path.isdir(input_dir):
        sys.stderr.write(f"error: input folder does not exist: {input_dir}\n")
        return 1

    os.makedirs(output_dir, exist_ok=True)

    wav_files = sorted(
        f for f in os.listdir(input_dir) if f.lower().endswith(".wav")
    )

    if not wav_files:
        print(f"No .wav files found in {input_dir}")
        return 0

    error_count = 0
    for wav_filename in wav_files:
        wav_path = os.path.join(input_dir, wav_filename)
        stem = os.path.splitext(wav_filename)[0]
        identifier = sanitize_identifier(stem)
        header_filename = f"{identifier}.h"
        header_path = os.path.join(output_dir, header_filename)

        try:
            samples, orig_rate, orig_channels, orig_dtype_name = convert_wav_file(wav_path)
            write_header_file(
                header_path,
                identifier,
                samples,
                wav_filename,
                orig_rate,
                orig_channels,
                orig_dtype_name,
            )
            print(f"OK   {wav_filename} -> {header_path} ({len(samples)} samples)")
        except Exception as exc:  # noqa: BLE001 - report and continue with other files
            error_count += 1
            sys.stderr.write(f"FAIL {wav_filename}: {exc}\n")

    print(f"\nProcessed {len(wav_files)} file(s), {error_count} error(s).")
    return 1 if error_count else 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Convert .wav files into C header files (const int16_t arrays) "
            "matching the project's audio codec format (44100Hz, mono, int16)."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "examples:\n"
            "  python wav_to_header.py samples/\n"
            "  python wav_to_header.py samples/ -o generated_headers/\n"
        ),
    )
    parser.add_argument(
        "input_dir",
        help="Folder containing .wav files to convert.",
    )
    parser.add_argument(
        "-o", "--output-dir",
        default=None,
        help=(
            "Folder to write generated .h files to. Defaults to the same "
            "folder as input_dir. Created if it doesn't exist."
        ),
    )
    args = parser.parse_args()

    output_dir = args.output_dir if args.output_dir is not None else args.input_dir
    return process_folder(args.input_dir, output_dir)


if __name__ == "__main__":
    sys.exit(main())
