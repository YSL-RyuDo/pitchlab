"""Offline regression: python verify_styles.py PitchLab.exe mono.wav output_dir.

Requires numpy/soundfile. Does not open an audio device or play the results.
"""
from pathlib import Path
import argparse
import subprocess
import numpy as np
import soundfile as sf

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('exe', type=Path)
parser.add_argument('source', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
root = args.output.resolve()
root.mkdir(parents=True, exist_ok=True)
exe = args.exe.resolve()


def render(mode, source, folder, *values):
    folder.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(exe), mode, str(source.resolve()), str(folder),
                    *map(str, values)], check=True,
                   creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    suffix = '*_louder.wav' if mode == '--boost-wav' else '*_tuned.wav'
    wet = max(folder.glob(suffix), key=lambda p: p.stat().st_mtime_ns)
    dry = max(folder.glob('*_original.wav'), key=lambda p: p.stat().st_mtime_ns)
    return sf.read(wet)[0], sf.read(dry)[0]


report, outputs = [], []
x, rate = sf.read(args.source)
assert x.ndim == 1 and 16000 <= rate <= 96000, 'Use a mono vocal fixture at 16–96 kHz'
for preset in range(3, 11):
    y, d = render('--render-live', args.source, root / f'style-{preset}', preset, 18, 140)
    assert len(y) == len(x) == len(d)
    assert np.max(np.abs(d-x)) < 4/8388608
    assert np.isfinite(y).all() and np.max(np.abs(y)) < .891252
    assert np.sqrt(np.mean(y*y)) > .001
    outputs.append(y)
    report.append(f'PASS live style {preset-2}: frames={len(y)} peak={np.max(np.abs(y)):.6f} '
                  f'RMS={np.sqrt(np.mean(y*y)):.6f}; dry unchanged')
for i in range(8):
    for j in range(i):
        assert np.sqrt(np.mean((outputs[i]-outputs[j])**2)) > .0001

quiet = root / 'quiet.wav'
sf.write(quiet, .002*np.sin(2*np.pi*220*np.arange(48017)/48000), 48000, subtype='PCM_24')
q, _ = sf.read(quiet)
y, d = render('--boost-wav', quiet, root / 'boost-quiet', 18)
assert len(y) == len(q) and np.max(np.abs(d-q)) < 2/8388608
assert np.max(np.abs(y-q*10**.9)) < 2/8388608
report.append(f'PASS level-only +18 dB: measured {20*np.log10(np.linalg.norm(y)/np.linalg.norm(q)):.6f} dB; '
              f'{len(y)} frames, no added pitch processing')
silence = root / 'silence.wav'
sf.write(silence, np.zeros(513), 48000, subtype='PCM_24')
y, d = render('--boost-wav', silence, root / 'boost-silence', 30)
assert len(y) == 513 and not np.any(y)
report.append('PASS short silence remains exactly silent with +30 dB')
loud = root / 'loud.wav'
sf.write(loud, .8*np.sin(2*np.pi*213*np.arange(48017)/48000), 48000, subtype='PCM_24')
y, d = render('--boost-wav', loud, root / 'boost-loud', 30)
assert np.isfinite(y).all() and np.max(np.abs(y)) < .891252
report.append('PASS overloaded gain-only WAV stays below -1 dBFS sample-peak ceiling')
report.append('No playback or audio device use. These are signal-integrity tests, not artist-similarity or listening tests.')
(root / 'STYLE-TEST-RESULTS.txt').write_text('\n'.join(report)+'\n', encoding='utf-8')
print('\n'.join(report))
