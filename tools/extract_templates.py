#!/usr/bin/env python3
"""Extract real glyph templates from the user's 8 screenshots.

Reads build/pic-raw/N.rgb (320x120 top-left, extracted from pic/N.png with
ffmpeg), binarizes with the runtime rule, finds the ANG-row digit components
and aligns them against the vision-verified ground truth, then builds one
12x18 majority-vote template per glyph class and prints Rust source.

Usage: python3 tools/extract_templates.py
"""

W, H = 320, 120
TW, TH = 12, 18

# (pitch yaw roll) ground truth per screenshot, verified by vision review.
TRUTH = {
    "1": "-43.73 50.19 0.00",
    "2": "-10.07 -36.18 0.00",
    "3": "-45.63 154.24 0.00",
    "4": "-89.00 122.35 0.00",
    "5": "-89.00 122.35 0.00",
    "6": "-89.00 122.35 0.00",
    "7": "-34.12 68.75 0.00",
    "8": "-41.08 157.71 0.00",
}

# Allowed glyph center rows (2*y+h); digits y57 h7 -> 121.
ANG_CY0, ANG_CY1 = 118, 128
# Expected starting x bands for the three ANG columns.
BANDS = [(40, 95), (95, 150), (150, 320)]


def load_fg(name):
    data = open(f"build/pic-raw/{name}.rgb", "rb").read()
    fg = [0] * (W * H)
    for i in range(W * H):
        r, g, b = data[i * 3], data[i * 3 + 1], data[i * 3 + 2]
        hi, lo = max(r, g, b), min(r, g, b)
        if hi >= 150 and ((hi - lo) <= 64 or (hi >= 248 and lo >= 190)
                        or (hi >= 155 and (hi - lo) <= 100)):
            fg[i] = 1
    return fg


def components(fg):
    seen = [[False] * W for _ in range(H)]
    out = []
    for yy in range(H):
        for xx in range(W):
            if not fg[yy * W + xx] or seen[yy][xx]:
                continue
            stack = [(xx, yy)]
            seen[yy][xx] = True
            cells = []
            minx = miny = 10**9
            maxx = maxy = -1
            while stack:
                cx, cy = stack.pop()
                cells.append((cx, cy))
                minx, maxx = min(minx, cx), max(maxx, cx)
                miny, maxy = min(miny, cy), max(maxy, cy)
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0:
                            continue
                        nx, ny = cx + dx, cy + dy
                        if 0 <= nx < W and 0 <= ny < H and fg[ny * W + nx] and not seen[ny][nx]:
                            seen[ny][nx] = True
                            stack.append((nx, ny))
            grid = [[0] * (maxx - minx + 1) for _ in range(maxy - miny + 1)]
            for cx, cy in cells:
                grid[cy - miny][cx - minx] = 1
            out.append({"x": minx, "y": miny, "w": maxx - minx + 1,
                        "h": maxy - miny + 1, "grid": grid})
    return out


def merge_components(comps):
    """Merge components that touch/overlap vertically (broken strokes)."""
    out = list(comps)
    changed = True
    while changed:
        changed = False
        for i in range(len(out)):
            for j in range(i + 1, len(out)):
                a, b = out[i], out[j]
                overlap_x = max(0, min(a["x"] + a["w"], b["x"] + b["w"]) - max(a["x"], b["x"]))
                gap_y = max(a["y"], b["y"]) - min(a["y"] + a["h"], b["y"] + b["h"])
                if overlap_x >= 1 and gap_y <= 1:
                    minx = min(a["x"], b["x"])
                    miny = min(a["y"], b["y"])
                    maxx = max(a["x"] + a["w"], b["x"] + b["w"])
                    maxy = max(a["y"] + a["h"], b["y"] + b["h"])
                    grid = [[0] * (maxx - minx) for _ in range(maxy - miny)]
                    for c in (a, b):
                        for cy in range(c["h"]):
                            for cx in range(c["w"]):
                                if c["grid"][cy][cx]:
                                    grid[c["y"] + cy - miny][c["x"] + cx - minx] = 1
                    merged = {"x": minx, "y": miny, "w": maxx - minx,
                              "h": maxy - miny, "grid": grid}
                    out[i] = merged
                    out.pop(j)
                    changed = True
                    break
            if changed:
                break
    out.sort(key=lambda c: (c["x"], c["y"]))
    return out


def normalize(c):
    gw, gh = c["w"], c["h"]
    scale = min(TW / gw, TH / gh, 1.0)
    sw = max(1, round(gw * scale))
    sh = max(1, round(gh * scale))
    ox = (TW - sw) // 2
    oy = (TH - sh) // 2
    out = [[0] * TW for _ in range(TH)]
    for y in range(sh):
        sy = min(int(y / scale), gh - 1)
        for x in range(sw):
            sx = min(int(x / scale), gw - 1)
            if c["grid"][sy][sx]:
                out[oy + y][ox + x] = 1
    return out


def split_tokens(comps, gap=8):
    tokens = []
    for c in comps:
        if tokens and c["x"] - (tokens[-1][-1]["x"] + tokens[-1][-1]["w"]) <= gap:
            tokens[-1].append(c)
        else:
            tokens.append([c])
    return tokens


def main():
    samples = {}
    for name, text in sorted(TRUTH.items()):
        fg = load_fg(name)
        comps = []
        for c in components(fg):
            cy = 2 * c["y"] + c["h"]
            if not (ANG_CY0 <= cy <= ANG_CY1):
                continue
            if not (1 <= c["w"] <= 12 and 1 <= c["h"] <= 9):
                continue
            comps.append(c)
        comps = merge_components(comps)
        comps.sort(key=lambda c: c["x"])
        truth_tokens = text.split()
        for tok_text, (lo, hi) in zip(truth_tokens, BANDS):
            band = [c for c in comps if lo <= c["x"] < hi]
            tokens = split_tokens(band)
            n = len(tok_text)
            match = None
            for t in tokens:
                if len(t) == n:
                    match = t
                    break
            if match is None:
                for t in tokens:
                    if len(t) == n + 1:
                        match = t[:-1] if True else t[1:]
                        break
            if match is None:
                print(f"WARN {name}: token '{tok_text}' unmatched in band {lo}..{hi}")
                continue
            for c, ch in zip(match, tok_text):
                samples.setdefault(ch, []).append(normalize(c))
    print("sample counts:")
    for ch in sorted(samples):
        print(f"  {ch}: {len(samples[ch])}")
    print()
    print("--- Rust source ---")
    for ch in sorted(samples):
        grids = samples[ch]
        n = len(grids)
        vote = [[0] * TW for _ in range(TH)]
        for g in grids:
            for y in range(TH):
                for x in range(TW):
                    vote[y][x] += g[y][x]
        rows = []
        for y in range(TH):
            rows.append("".join("#" if vote[y][x] * 2 >= n else "." for x in range(TW)))
        rows_str = ", ".join('"%s"' % r for r in rows)
        print("    Template { id: '%s', rows: &[%s] }," % (ch, rows_str))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
