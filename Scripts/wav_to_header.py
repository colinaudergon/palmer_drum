#!/usr/bin/env python3
"""Convert .wav files into C header files with embedded int16_t sample arrays.

For every .wav file found in the given input folder, this script generates a
corresponding .h file containing:

    const int16_t sample_<name>[] = { ... };
    const size_t sample_<name>_length = N;
    const size_t sample_<name>_num_bins = M;
    const AudioBin sample_<name>_bins[] = { ... };

Each sample is converted to match the format used by the project's audio
codec (see hw_interfaces/pwm_audio_codec):
  - mono (stereo files are downmixed by averaging L+R)
  - 44100 Hz sample rate (resampled if the source differs)
  - signed 16-bit PCM (int16_t), peak-normalized before quantization

Trailing near-silence (e.g. room tone after a hit decays out) is trimmed off
before the sample is split into bins, so a hit's tail doesn't waste whole
bins on near-zero data.

Each sample's audio is additionally split into `--bins` equal-length chunks.
For each chunk, a mean-square "power" value is computed and emitted as an
`AudioBin` entry (index-based: it references an offset/size window into the
*same* `sample_<name>[]` array above, rather than owning separate storage),
so a single sample's data is stored exactly once regardless of how many
consumers use it as whole-sample audio vs. bin-selectable grain material.

Requires: numpy, scipy (pip install numpy scipy)

Examples:
    python wav_to_header.py samples/ --bins 16
    python wav_to_header.py samples/ --bins 16 -o generated_headers/
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
# Trailing samples quieter than this fraction of full scale (post-
# normalization) are considered silence and trimmed off before binning, so
# a hit's trailing silence doesn't waste whole bins on near-zero data.
DEFAULT_TRAILING_SILENCE_THRESHOLD = 0.01


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


def trim_trailing_silence(samples: "np.ndarray", threshold_fraction: float) -> "np.ndarray":
    """Cuts off trailing near-silence (e.g. room tone after a hit decays
    out) so bins aren't wasted on near-zero data.

    Finds the last sample whose absolute value exceeds
    `threshold_fraction` of full scale (32767) and trims everything after
    it. Leading silence and internal quiet passages are left untouched --
    only a trailing run of near-zero samples is removed. If the whole
    signal is at/below the threshold (e.g. a silent file), it's returned
    unchanged rather than trimmed to nothing.
    """
    if len(samples) == 0 or threshold_fraction <= 0:
        return samples
    threshold = threshold_fraction * 32767.0
    above_threshold = np.flatnonzero(np.abs(samples.astype(np.int64)) > threshold)
    if len(above_threshold) == 0:
        return samples
    last_active = int(above_threshold[-1])
    return samples[: last_active + 1]


def split_into_bins(samples: "np.ndarray", num_bins: int) -> "list[tuple[int, int]]":
    """Splits samples into num_bins equal-length chunks, in order, with any
    remainder samples appended to the last bin (so every bin but the last is
    exactly len(samples) // num_bins samples long, and the last is >= that).

    Returns a list of (start_index, size) pairs -- offsets into `samples`
    -- rather than copies of the data, since bins are stored as
    index/size windows into the single embedded sample array.
    """
    total = len(samples)
    bin_size = total // num_bins
    bins = []
    start = 0
    for i in range(num_bins):
        # The last bin absorbs whatever's left (bin_size plus any
        # remainder from the integer division above).
        end = total if i == num_bins - 1 else start + bin_size
        bins.append((start, end - start))
        start = end
    return bins


def compute_bin_power(bin_samples: "np.ndarray") -> "tuple[int, int]":
    """Returns (normalized_power, raw_power) for a bin.

    raw_power is the mean-square power (sum of squares / bin size), as a
    uint32_t-safe integer. Using mean square rather than a raw sum of
    squares avoids overflowing uint32_t for anything but the tiniest bins,
    and keeps power comparable across bins of different sizes (e.g. the
    last, possibly-larger bin from split_into_bins()).

    normalized_power is raw_power rescaled to a 0..65535 (uint16_t) range,
    expressed as a fraction of the maximum possible mean-square energy for
    a full-scale int16 signal (32767^2) -- i.e. 65535 means every sample in
    the bin is at +/-32767, 0 means silence.
    """
    if len(bin_samples) == 0:
        return 0, 0
    squares = bin_samples.astype(np.int64) ** 2
    raw_power = int(squares.sum() // len(bin_samples))
    # int16 samples squared max out at 32767^2 = 1,073,676,289, so
    # raw_power can never exceed that -- comfortably within uint32_t's
    # ~4.29e9 range regardless of bin size.
    max_raw_power = 32767 * 32767
    normalized_power = min(65535, (raw_power * 65535) // max_raw_power)
    return normalized_power, raw_power


def write_header_file(
    output_path: str,
    identifier: str,
    samples: "np.ndarray",
    bins: "list[tuple[int, int]]",
    bin_powers: "list[tuple[int, int]]",
    source_filename: str,
    orig_rate: int,
    orig_channels: int,
    orig_dtype_name: str,
) -> None:
    lines = []
    lines.append(f"// Auto-generated from '{source_filename}' by Scripts/wav_to_header.py")
    lines.append(
        f"// Source: {orig_rate}Hz, {orig_channels}ch, {orig_dtype_name} "
        f"-> {TARGET_SAMPLE_RATE_HZ}Hz mono int16, split into {len(bins)} bin(s)"
    )
    lines.append("// Do not edit by hand; regenerate from the source .wav instead.")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <cstdint>")
    lines.append("#include <cstddef>")
    lines.append("")
    lines.append("#ifndef AUDIOBIN_H_")
    lines.append("#define AUDIOBIN_H_")
    lines.append("typedef struct {")
    lines.append("    size_t index;        // offset of the bin's first sample in the")
    lines.append("                         // sample array it belongs to")
    lines.append("    size_t size;         // number of samples in the bin")
    lines.append("    uint16_t power;      // normalized: 0..65535 fraction of max")
    lines.append("                         // possible mean-square energy (32767^2)")
    lines.append("    uint32_t raw_power;  // raw (unnormalized) mean-square value")
    lines.append("} AudioBin;")
    lines.append("#endif // AUDIOBIN_H_")
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

    lines.append(f"const AudioBin {identifier}_bins[] = {{")
    for (bin_start, bin_size), (norm_power, raw_power) in zip(bins, bin_powers):
        lines.append(
            f"    {{ {bin_start}u, {bin_size}u, {norm_power}u, {raw_power}u }},"
        )
    lines.append("};")
    lines.append("")
    lines.append(f"const size_t {identifier}_num_bins = {len(bins)};")
    lines.append("")

    with open(output_path, "w", newline="\n") as f:
        f.write("\n".join(lines))


def process_folder(
    input_dir: str,
    output_dir: str,
    num_bins: int,
    silence_threshold: float,
) -> int:
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

            trimmed_samples = trim_trailing_silence(samples, silence_threshold)
            trimmed_count = len(samples) - len(trimmed_samples)
            samples = trimmed_samples

            if len(samples) < num_bins:
                raise ValueError(
                    f"file has only {len(samples)} sample(s), can't split into "
                    f"{num_bins} bin(s)"
                )

            bins = split_into_bins(samples, num_bins)
            bin_powers = [
                compute_bin_power(samples[start:start + size]) for start, size in bins
            ]

            write_header_file(
                header_path,
                identifier,
                samples,
                bins,
                bin_powers,
                wav_filename,
                orig_rate,
                orig_channels,
                orig_dtype_name,
            )
            print(
                f"OK   {wav_filename} -> {header_path} "
                f"({len(samples)} samples, {num_bins} bins"
                + (f", trimmed {trimmed_count} trailing silent sample(s))" if trimmed_count else ")")
            )
        except Exception as exc:  # noqa: BLE001 - report and continue with other files
            error_count += 1
            sys.stderr.write(f"FAIL {wav_filename}: {exc}\n")

    print(f"\nProcessed {len(wav_files)} file(s), {error_count} error(s).")
    return 1 if error_count else 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Convert .wav files into C header files (const int16_t arrays) "
            "matching the project's audio codec format (44100Hz, mono, int16), "
            "annotated with index-based AudioBin energy-bin metadata."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "examples:\n"
            "  python wav_to_header.py samples/ --bins 16\n"
            "  python wav_to_header.py samples/ --bins 16 -o generated_headers/\n"
        ),
    )
    parser.add_argument(
        "input_dir",
        help="Folder containing .wav files to convert.",
    )
    parser.add_argument(
        "-b", "--bins",
        type=int,
        required=True,
        help="Number of bins to split each file's audio into.",
    )
    parser.add_argument(
        "-o", "--output-dir",
        default=None,
        help=(
            "Folder to write generated .h files to. Defaults to the same "
            "folder as input_dir. Created if it doesn't exist."
        ),
    )
    parser.add_argument(
        "--silence-threshold",
        type=float,
        default=DEFAULT_TRAILING_SILENCE_THRESHOLD,
        help=(
            "Trailing samples quieter than this fraction of full scale "
            "(0-1) are trimmed off before binning, so a hit's trailing "
            f"silence doesn't waste bins (default: {DEFAULT_TRAILING_SILENCE_THRESHOLD}). "
            "Set to 0 to disable trimming."
        ),
    )
    args = parser.parse_args()

    if args.bins <= 0:
        sys.stderr.write("error: --bins must be a positive integer\n")
        return 1
    if args.silence_threshold < 0:
        sys.stderr.write("error: --silence-threshold must be >= 0\n")
        return 1

    output_dir = args.output_dir if args.output_dir is not None else args.input_dir
    return process_folder(args.input_dir, output_dir, args.bins, args.silence_threshold)


if __name__ == "__main__":
    sys.exit(main())
