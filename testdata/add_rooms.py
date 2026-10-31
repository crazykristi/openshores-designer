#!/usr/bin/env python3
# Fills the two middle decks of a plan with one room wherever a cell is hull
# on that deck and the decks above and below: rooms right up against the
# hull, the hardest case for the hull simplifier (test.sh).
#   add_rooms.py <plan.json> <out.json> [maxFaces]
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
hull = [unrle(l['hull']) for l in plan['layers']]
def full(d, cx, cy):
    return 0 <= d < D and 0 <= cx < W and 0 <= cy < H and all(hull[d][(cy*W+cx)*4+k] for k in range(4))
mids = [D//2 - 1, D//2]
count = 0
for d in mids:
    room = [-1] * (W*H*4)
    for cy in range(H):
        for cx in range(W):
            if all(full(dd, cx+i, cy+j) for dd in (d-1, d, d+1) for i in (0,) for j in (0,)):
                for k in range(4):
                    room[(cy*W+cx)*4+k] = 0; count += 1
    plan['layers'][d]['room'] = rle(room)
plan['rooms'] = [{'colour': '#5a8cdc', 'label': 'Hold 1', 'type': 'Cargo Hold'}]
if len(sys.argv) > 3:
    plan['maxFaces'] = int(sys.argv[3])
json.dump(plan, open(sys.argv[2], 'w'), indent=1)
print(f"{count} room triangles on decks {mids}")
