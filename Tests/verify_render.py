"""Verify --render WAV pairs against their inputs, without playback.

Usage: python verify_render.py INPUT_WAV_DIRECTORY RENDER_DIRECTORY
Render files into RENDER_DIRECTORY/preset-0/<input-stem>/ and preset-2/...
Uses only the Python standard library. Inputs and results must be 24-bit PCM.
"""
from pathlib import Path
import math
import sys
import wave


def read(path):
    with wave.open(str(path), "rb") as wav:
        assert wav.getsampwidth() == 3, f"Not 24-bit PCM: {path}"
        rate, channels, frames = wav.getframerate(), wav.getnchannels(), wav.getnframes()
        raw = wav.readframes(frames)
    values = [int.from_bytes(raw[i:i+3], "little", signed=True) / 8388608 for i in range(0, len(raw), 3)]
    return rate, channels, frames, values


def verify(inputs, results):
    compared = {}
    for preset in (0, 2):
        for source in sorted(inputs.glob("*.wav")):
            rate, channels, frames, original = read(source)
            folder = results / f"preset-{preset}" / source.stem
            wet = sorted(folder.glob("*_tuned.wav"))[-1]
            dry = sorted(folder.glob("*_original.wav"))[-1]
            wr, wc, wf, tuned = read(wet)
            dr, dc, df, aligned = read(dry)
            assert (wr, wc, wf) == (rate, 1, frames), "Wet format/length mismatch"
            assert (dr, dc, df) == (rate, 1, frames), "Dry format/length mismatch"
            mono = [sum(original[i:i+channels]) / channels for i in range(0, len(original), channels)]
            dry_error = max(abs(a-b) for a, b in zip(mono, aligned))
            assert dry_error < 4 / 8388608, f"Latency trim or dry alignment error: {dry_error}"
            assert all(math.isfinite(x) and abs(x) <= .500001 for x in tuned), "Output ceiling"
            peak = max(abs(x) for x in tuned)
            step = max((abs(a-b) for a, b in zip(tuned, tuned[1:])), default=0)
            if max(abs(x) for x in mono) == 0:
                assert peak == 0, "Silence must remain silent"
            elif frames > rate:
                assert peak > .001, "Nonempty voice rendered as silence"
            compared[preset, source.stem] = tuned
            print(f"PASS {source.stem} preset={preset} frames={frames} peak={peak:.6f} max_step={step:.6f} dry_error={dry_error:.9f}")
    for source in inputs.glob("*.wav"):
        a, b = compared[0, source.stem], compared[2, source.stem]
        if len(a) > 48000:
            delta = math.sqrt(sum((x-y)**2 for x, y in zip(a, b)) / len(a))
            assert delta > .00001, "Presets produce identical voice output"
            print(f"PASS {source.stem} Natural/Hard RMS difference={delta:.6f}")
    print("No audio played. Signal integrity checks do not establish listening quality.")


if __name__ == "__main__":
    verify(Path(sys.argv[1]), Path(sys.argv[2]))
