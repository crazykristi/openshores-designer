#!/usr/bin/env python3
# Two cases the builder has to mend (test.sh), from a plan such as the
# example: an empty deck under the ship, so its hull begins above the
# design's origin (the engine's "CG outside design extents" error; the game
# hung on such a design), and on the top deck a corner where two filled
# cells meet only diagonally (four faces on one hull edge).
#   hull_edge_cases.py <plan.json> <out.json>
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
W, H, D = plan['width'], plan['depth'], plan['decks']
top = plan['layers'][D - 1]
hull, room = unrle(top['hull']), unrle(top['room'])
def cell(cx, cy):
    return [(cy * W + cx) * 4 + k for k in range(4)]
def free(cx, cy):
    return all(hull[i] and room[i] < 0 for i in cell(cx, cy))
# A 2x2 block of hull cells with no room in it, as far from the middle line
# as possible (the plan mirrors port and starboard, the case does not care).
spot = next(((cx, cy) for cy in range(1, H - 2) for cx in range(1, W - 2)
             if all(free(cx + i, cy + j) for i in (-1, 0, 1, 2) for j in (-1, 0, 1, 2))), None)
if spot is None:
    sys.exit("no spot for the diagonal corner")
cx, cy = spot
for i in cell(cx + 1, cy) + cell(cx, cy + 1):
    hull[i] = 0
top['hull'] = rle(hull)
# The empty deck below, and everything one deck up.
empty = {'hull': f"{W*H*4}*0", 'paint': f"{W*H*4}*0", 'room': f"{W*H*4}*-1"}
plan['layers'] = [empty] + plan['layers']
plan['decks'] = D + 1
for key in ('doors', 'windows', 'ladders', 'parking', 'stairs', 'emitters'):
    for item in plan.get(key, []):
        item['deck'] += 1
json.dump(plan, open(sys.argv[2], 'w'), indent=1)
print(f"diagonal corner at cell {cx + 1},{cy + 1} of deck {D}; empty deck 0")
