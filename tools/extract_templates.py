#!/usr/bin/env python3
"""Offline helper: extract the top-left HUD text from a screenshot as a binary
fixture for tests/fixtures/ang_roi.rs.

Usage:
  ffmpeg -i shot.png -vf "crop=240:55:0:30" -f rawvideo -pix_fmt rgb24 -vframes 1 roi.rgb
  python3 tools/extract_templates.py roi.rgb > tests/fixtures/ang_roi.rs
"""
import sys

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    data = open(sys.argv[1], 'rb').read()
    w, h = 240, 55
    vals = []
    for y in range(h):
        for x in range(w):
            i = (y * w + x) * 3
            r, g, b = data[i], data[i + 1], data[i + 2]
            hi, lo = max(r, g, b), min(r, g, b)
            fg = 1 if (hi >= 150 and ((hi - lo) <= 64 or (hi >= 248 and lo >= 190))) else 0
            vals.append(fg)
    out = ['//! Offline binary fixture: ANG row region of the user screenshot.',
           'pub const WIDTH: usize = 240;',
           'pub const HEIGHT: usize = 55;',
           f'pub const FOREGROUND: usize = {sum(vals)};',
           'pub const PIXELS: [u8; 13200] = [']
    for y in range(h):
        out.append('    ' + ','.join(str(v) for v in vals[y * w:(y + 1) * w]) + ',')
    out.append('];')
    print('\n'.join(out))
    return 0

if __name__ == '__main__':
    sys.exit(main())
