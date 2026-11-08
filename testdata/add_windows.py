#!/usr/bin/env python3
# Puts a lounge on the ellipsoid's fourth deck with ten windows on each long
# wall, near the hull's flank, for the hull-window test (test.sh). With
# 'skylights', ceiling windows instead: following the hull, a block of four
# cells and two triangles beside it (one outline); flat, a whole cell, a
# half cell (two triangles) and a single triangle.
#   add_windows.py <plan.json> <out.json> [windowStyle] [skylights]
import json, sys
def unrle(s):
    out = []
    for part in s.split(','):
        n, v = part.split('*'); out += [int(v)] * int(n)
    return out
def rle(a):
    parts = []; i = 0
    while i < len(a):
        j = i
        while j < len(a) and a[j] == a[i]: j += 1
        parts.append(f"{j-i}*{a[i]}"); i = j
    return ",".join(parts)
plan = json.load(open(sys.argv[1]))
W, deck = plan['width'], 3
plan['rooms'] = [{"type": "Lounge", "label": "Lounge 1", "colour": "#60a0c0"}]
room = unrle(plan['layers'][deck]['room'])
for cx in range(25, 37):
    for cy in range(3, 19):
        for k in range(4):
            room[(cy * W + cx) * 4 + k] = 0
plan['layers'][deck]['room'] = rle(room)
plan['windows'] = [{"deck": deck, "x": x, "y": 3, "side": 0} for x in range(26, 36)] + \
                  [{"deck": deck, "x": x, "y": 18, "side": 2} for x in range(26, 36)]
plan['doors'] = []
if len(sys.argv) > 3 and sys.argv[3]:
    plan['windowStyle'] = sys.argv[3]
if len(sys.argv) > 4 and sys.argv[4] == 'skylights':
    plan['windows'] = []
    plan['joinWindows'] = True
    sky = [{"deck": deck, "x": x, "y": y, "shape": "hull"} for x in (28, 29) for y in (8, 9)]
    sky += [{"deck": deck, "x": 30, "y": 8, "k": k, "shape": "hull"} for k in (2, 3)]
    sky += [{"deck": deck, "x": 33, "y": 12, "shape": "flat"}]
    sky += [{"deck": deck, "x": 33, "y": 14, "k": k, "shape": "flat"} for k in (0, 1)]
    sky += [{"deck": deck, "x": 33, "y": 16, "k": 2, "shape": "flat"}]
    plan['skylights'] = sky
json.dump(plan, open(sys.argv[2], 'w'), indent=1)
