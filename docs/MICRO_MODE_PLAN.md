# Micro mode: small craft from a 3D model (research, 7 November 2026)

**Status (8 November 2026):** a first, experimental *fighter mode* is in
0.3.7-beta-rc1, built on the deck plan rather than the separate 3D editor
proposed below: the model is the hull, the plan has 2.5 ft cells and 4.5 ft
decks, the canopy is windows that follow the hull, entry is a side door, and
seats are placed by hand (Crew step). See `DESIGNER.md`, *Fighter mode*. Not
done: the belly hatch, an opening canopy, picking canopy faces in 3D.

The user asked whether importing a 3D model could switch the designer between
**macro** (as now: decks, 5 ft cells) and **micro**: custom fighters under
about 30 x 30 m, with complex shapes, cockpit canopies, and detail down to
10 cm. Entry must suit a fighter: a hatch in the belly with a ladder through
it when there is landing gear, or a door in the side of the cockpit that looks
like the hull, with only a button showing.

## What the game allows

* **A fighter has to be a spacecraft design.** The game's vehicles (ground
  and space vehicle parking spots) are fixed types, not player designs. A
  spacecraft design has no lower size limit we know of; anything up to 238 ft
  can spawn on the ground and land on a spacecraft landing spot (track 1c), so a
  carrier can take it.
* **No FTL drive.** The FTL drive's minimum volume for a 150 m3 hull is more
  than the whole hull (`generate.cpp`, the FTL formula), so a fighter is built
  with FTL "none". The other systems have small minimums and fit.
* **Small rooms work.** The game tests movement at points
  (`AuScParts::IsPathBlocked`), and a point inside a void is never blocked, so
  a cockpit void of any shape is walkable. The 2 ft margin round a hull only
  applies outside it. The game has a crouched pose (`DaCreature::IsCrouched`);
  a cockpit lower than a standing person's eye (about 5.5 ft) needs testing in
  the game: whether a player crouching or sitting at the helm fits.
* **Hatches and hull-coloured doors.** A door keeps a mesh for each state
  (`AuScStatePart::m_StateMeshes`, Closed, Open10..Open90, Open), and a door
  cut with a door jig is made of the hull pieces cut away (`JigCut`), so it
  looks like the hull. Nothing ties a door to a wall: a door lying flat in the
  belly (a hatch) with an inside ladder through it is the same set of parts
  as now, turned. The button is the door button we have (track 11), on the
  outside.
* **Canopies.** A window that follows the hull (track 4) already takes the
  hull's own faces as its glass, so a canopy is a set of hull faces turned into
  a window part.

## What the models give

The example models in `3dmodelsraw/rl military` are all STL: geometry only,
with no materials, so nothing marks the canopy; it has to be picked in the
designer. Read with `shipgen mesh-info`:

| Model | Triangles | Closed | Units |
|---|---|---|---|
| F-22 (`Final_Model.STL`) | 51,696 | yes, some faces wound the wrong way | 452 long: scale model, needs `--length` |
| F-35 (`WEA.stl`) | 6,642 | yes | 100 long |
| JAS-39 Gripen (`JAS-39_Gripen.stl`) | 158,300 | yes | 213 long (could not be read before 7 November, see `CHANGES`) |
| Mirage 2000C | several parts (nose, fuselage, wings, ...) | the fuselage has holes | 150 long per part |

The real sizes are known (Gripen 14.1 m, Mirage 2000 14.4 m, F-22 18.9 m,
F-35A 15.7 m), so `--length` sets the scale, as `plan-from-model` does now.
Multi-part kits (the Mirage) need their parts joined into one solid first.

## Why micro mode can't be the deck plan with smaller cells

Macro mode builds the hull from the triangle plan: the model is sampled into
5 ft cells on 10 ft decks, and the hull is rebuilt from that. At fighter
scale this throws the shape away: a 15 m fighter is 10 cells long and one or
two decks high. With 10 cm cells, rooms (which fill a deck's height) and decks
lose their meaning: a pilot needs about 4 ft of height, which is 12 decks of
10 cm.

## Proposed design

Micro mode keeps the **model's own surface** as the hull and puts the few
interior pieces in 3D, on the model:

1. **Import:** the model, its real length, and which way is fore and up (as
   now). The surface is repaired (windings, duplicates, small holes) and
   thinned to a face budget with the hull decimation we have, keeping detail
   where it matters (canopy edges, livery edges).
2. **Canopy:** click the canopy in the 3D view; neighbouring faces join until
   a crease (an angle that can be set), with a brush to add or remove faces.
   The faces become the window glass; a 10 cm grid in the canopy's own plane
   for cutting a straight edge where the model has none.
3. **Cockpit:** a room as a box or the hull's inside offset inward (the
   solid-model code can make that shell), snapped to a 10 cm grid, clipped to
   the hull; the helm station and seat in it, facing fore.
4. **Entry:** either a belly hatch (with landing gear: a flat door cut from the
   hull with a door jig, an inside ladder up through it, the button outside
   by the hatch) or a side door on the cockpit (cut from the hull, so it looks
   like the hull, with the button outside). An outside ladder is not offered.
5. **Systems:** FTL none, a small-craft template (one berth, helm, maybe
   sensors and a weapon bay at the nose or under the wings).
6. **Paint:** paint by picking faces or the liveries we have; the 3D view is
   the editor, not the deck plan.

The plan file gets `"mode": "micro"` and keeps these pieces in model
coordinates; the build goes straight from them to the blueprint, past the deck
plan.

## Questions for the user

* Must a fighter fly in space only, or also land on worlds (then landing gear
  matters: do the models have it, and should we add simple gear)?
* A cockpit lower than a standing person: tested in the game on 8 November
  2026 with `cockpit_test_4ft` (about 4 ft of headroom): it works.
* The user (8 November): fighters land on the ground too and can be built
  planetside; two- and three-seaters should be possible, with each console and
  seat placed by the user; an opening canopy is wanted if the game allows it
  (an open canopy also makes getting in easier). FBX models can now be read
  (Assimp).
* One seat, or two-seaters too (the Gripen D, F-15E)?
* Should the canopy open (a door whose closed state is the glass), or stay
  shut, with entry through the hatch or side door?

## Effort

Large: a new 3D editing mode (picking faces, placing boxes on the model) and
a build path past the deck plan. In steps: (1) import, repair and scale with
a hull-only build (`--micro`), to try a fighter in the game early; (2) canopy
picking; (3) cockpit room and helm; (4) entry; (5) paint.
