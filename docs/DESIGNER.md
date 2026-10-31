# The triangle designer (`shipgen-designer`)

Status: **minimum viable product**, built this session. It works end to end:
the example plan becomes a blueprint the engine's report accepts with no
errors and no warnings, with a closed hull and every room inside it. It has
not been tried in the game yet.

The third program, next to `shipgen` (command line) and `shipgen-gui` (the
generator's wizard). Instead of starting from a 3D model, the ship is drawn.

```bash
./build/bin/shipgen-designer                       # starts with an example ship
./build/bin/shipgen-designer --open plan.json      # a saved plan
./build/bin/shipgen plan-example plan.json         # the example, as a file
./build/bin/shipgen build-plan plan.json --out ship.SoH   # the same build, without the window
```

---

## The idea

The drawing is a grid of square cells seen from above, one grid per deck.
Each cell is cut along both diagonals into four triangles that meet in its
middle, so **four triangles make a square** and **two make a 45 degree
slope**. Everything is drawn on these triangles, in passes:

1. **The hull.** Which triangles of each deck are inside the ship.
2. **The shape.** The triangles become a closed hull, with smoothed edges
   (this happens on its own while drawing).
3. **The rooms.** Which hull triangles belong to which room; the doors, on
   the edge between two cells; ladders between decks.
4. **The paint.** The colour of the hull over each triangle.
5. **The build.** The blueprint, with the rooms' voids, the doors' doorways,
   walk paths, and the stations, berths and system volumes for the kind of
   ship chosen.

```
        tl ------- tr          x runs fore (+x) and aft (-x),
         | \  N  / |           y runs port (+y) and starboard (-y),
         |  \   /  |           z runs up, one deck at a time.
         | W  c  E |
         |  /   \  |           Each triangle is named after the
         | /  S  \ |           cell edge it stands on.
        bl ------- br
```

## Using it

The window has the side panel on the left, the plan of the current deck in
the middle and the 3D view on the right.

* **Drawing** (always shown): the deck being drawn on, the brush (one
  triangle, one cell, or 3 x 3 cells), and the mirror, which draws everything
  on the other side of the centre line too.
* **Mouse**: the left button draws, the right button rubs out, the wheel
  zooms, the middle button pans. In the 3D view the left button turns the
  model and the wheel zooms.
* **Undo/redo**: Ctrl+Z / Ctrl+Shift+Z, 100 steps.

The steps are the tabs of the side panel:

| Step | What it does |
|---|---|
| 1 Ship | Name, kind of ship (one of the generator's templates: it decides the stations, berths and system volumes), FTL drive (none, warp 1-9, or a P, PN or PNN wormhole drive; default wormhole P), cell size (default 5 ft), deck height (10 ft), floor thickness (1 ft), the grid size, and adding or removing decks. |
| 2 Hull | Draw the hull. The deck below shows faintly. Skin (how far the hull surface stands out from the drawn triangles, 2 ft), smoothing (0 to 15) and the face budget (5,000). The 3D view follows the drawing after a short pause. |
| 3 Rooms | Add rooms (the type is one of the engine's own room types; the name has to be unique) and draw them on hull triangles. Switch the tool to doors (click the triangle standing on the cell edge where the door goes; between two rooms it is an interior door, between a room and anything that is not a room it is a hull door) ladders (click a cell that is wholly in a room here and in a room on the deck above), windows (click near an edge between a room and the outside: a cell edge, or a diagonal where the room's wall runs across a cell; a click on a room cell that has only one outer wall takes that one; drawn as a light blue dashed line), vehicle parking spots (ground or space vehicle; click near the grid corner the spot is centred on; drawn as a yellow dashed box), or stairs (click the triangle of the first cell on the side the stairs climb towards; the run has to be whole cells of one room, and on the deck above the cells over it and the cell after it whole cells of one room; drawn as steps with an arrow, and as a dashed box on the deck above). When a click does nothing, the status bar says why until the mouse leaves that triangle. Triangles outside the shaped hull are hatched red. Fore is to the right; the blueprint is turned so that fore is +Y, the way the game wants it, so the 3D view of the built ship is turned a quarter turn from the plan. |
| 4 Paint | A palette: the base coat, any colours, and the owner's empire colours. Paint the hull triangles; the paint covers the hull over them on that deck, sides included. Double-click a colour to change it. |
| 5 Build | Build the blueprint and see the engine's report; save it as `.SoH`. Stations, berths and systems, and walk paths, can be switched off. |

**File** saves and opens plans (`.json`) and exports the blueprint
(Ctrl+E).

For scripts and tests the program also takes `--step <1-5>`,
`--deck <n>`, `--build`, `--export <file.SoH>` (build, save and quit; the exit
code is 0 when the engine accepts it, 3 when it has errors) and
`--screenshot <file.png>`.

## How a plan becomes a blueprint

`src/planbuild.cpp`, in this order.

**The hull.**

1. Every hull triangle of a deck is a prism one deck high. The prisms are
   joined into one surface; a face between two filled prisms is left out.
   Walls get two rows of vertices per deck so that they can bend.
2. With smoothing on, every triangle is split into four, so that the
   rounding has vertices close enough together to follow a curve smaller
   than a cell.
3. The surface is pushed out by the skin, so that the drawn triangles are the
   *inside* of the hull. A corner vertex goes further, so that every face
   moves out by the full skin.
4. Smoothing: Taubin's lambda/mu steps, with each neighbour weighted by how
   close it is and each vertex moved only along its normal. That rounds the
   edges and the steps between decks without dragging the vertices sideways
   (a plain average on this mesh leaves a saw-tooth along every edge), and it
   leaves flat areas flat. A vertex may not move further in than the skin
   less a quarter of a foot, plus up to three quarters of a cell where there
   is no room nearby, so the rounding never cuts into a room.
5. Flat areas of one colour are merged into a few large triangles by moving
   the vertices inside them onto a neighbour. Edges, bends and paint edges
   stay exactly where they are, and no merge may leave a sliver (the engine
   would drop it and open the hull).
6. Over the face budget, nearly flat areas are merged too, with a tolerance
   that grows in steps (2 degrees and 0.05 ft up to 30 degrees and 0.8 ft)
   until it fits. This keeps the surface closed and the paint exact, which
   the quadric method (tried first) did not: it left 700 non-manifold edges.
7. Every face gets its paint: a plain colour, or an empire colour theme.

The result is checked with the same exact solid test `audit` uses. A place
where two prisms only touch along an edge (an even number of faces, half each
way round) counts as closed; see `MeshSolidity::pinchedEdges` in
`src/audit.h`.

**The rooms.** A room on each deck it is drawn on becomes a `Room` mesh and a
constant `RoomVoid` of the same shape, from half the floor thickness above
the deck to half below the next one (9 ft high with the defaults). A room
drawn on two decks becomes two rooms, the second named `<name> deck <n>`. A
room triangle that is not inside the shaped hull is reported.

**Doors, windows, ladders and paths** are made as `docs/SOH_FORMAT.md`
sections 5 and 5a describe (`src/interior.h`). A door is as wide as a cell
less half a foot (at most 10 ft) and up to 8 ft high. Doors on neighbouring
edges of one wall, between the same two rooms (or a room and the outside),
make one **bay door**: as wide as the edges less a foot, and as tall as the
room less half a foot, for vehicles. Doors are cut through
the room walls (and the hull, for a hull door) with the engine's own door
jig, so the openings are real; the door is the piece that was cut out. A hull
door's switches (where it is opened and closed) are set 1 ft in front of it
inside the room and 1 ft outside the hull, 4 ft up. A hull
door also gets a constant hull void in front of it, from below the hull up,
and a way down to the ground: a **ramp** where the door's floor is no more
than 15 ft above the ground (half a foot below the bottom of the hull), as
at the door of the user's aircraft factory: an invisible Barrier that is
walked on (a level landing over the threshold, then a slope of about 34
degrees, 1.5 ft out for each foot down), a mesh that shows it, a hull void
over it and a walk path down it. Higher doors, or every door with
`--no-ramps`, get a ladder path down to 2.5 ft below the hull instead,
standing just outside the hull inside the doorway's voids. Put a hull door on a room cell at the edge of
the hull: the build warns when the way out is a tunnel through more than a
cell and a half of hull. A window is cut the same way with a window jig, a
cell wide less a foot, from the plan's window sill to its window top above
the floor (3 and 7.5 ft unless changed on the Rooms page; the top stays
half a foot under the ceiling). With *Join windows side by side*, windows
next to each other on one straight wall of a room (cell edges or one
diagonal line) are cut as one long window, as deep as the furthest hull
along it, and gets the
room's name as its group so that the room's light falls on it. A window on
a diagonal is a diagonal's length less 0.7 ft wide; for the cut the hull
and the room are turned until the wall is square to an axis, so it gets the
same clean frame, and turned back. Walk paths follow the triangles of a room, by the shortest way, from
the triangle nearest the room's middle to each door and ladder; straight runs
are one segment, and a segment that is already there is not laid again.
Paths lie on the floor. A ladder between decks also gets an opening through
the lower room's ceiling and the upper room's floor (the engine's cutting
jig) and a visible ladder of two rails and rungs.

**Stairs** climb one deck over `StairCells()` cells (deck height times 1.4,
rounded up to whole cells: 3 cells of 5 ft for 10 ft decks, about 34
degrees). They are made the way the user's aircraft factory makes them,
plus the walk path it lacks: the opening through the lower room's ceiling
and the upper room's floor over the whole run (cutting jig) with a void
through it, an invisible sloped Barrier to walk on, steps (a mesh) to see,
a walk path from the foot up the slope to the landing, and a rail round the
opening on the deck above (a Barrier 3.5 ft high and a rail on posts), open
only at the landing. Walk paths across the rooms keep off the opening above
and the run below, except the foot of the stairs.

**Between decks** the hull steps from one deck's outline to the next,
unless *Slope the hull between decks* (Hull page, `slopedDecks`) is on:
then where a deck is narrower than the one below, the ledge rises from the
lower deck's edge to the top of the narrower deck, and under a wider deck
the underside falls to the bottom of the deck below (`SlopeBetweenDecks`).
Points only move outward, so the drawn triangles stay the least the hull
may be (the user: the drawn bounds are the minimum, so that rooms never
clip). The slopes are flat, so the face merging takes them down well: the
example Wedge needs 10,252 faces before thinning instead of 11,794.

**How the inside looks.** Room floors are metal tiles, ceilings light
plates, and walls metal panels in a light shade of the room's colour in the
plan. Hull doors are painted yellow and interior doors blue, so that they
stand out from the walls. Shields, the weapon bay and the transporter can be
left out on the Ship page (`leaveOut` in the plan file: `shield`,
`firecontrol`, `transporter`); the example Wedge leaves all three out.

**Vehicle parking spots** are the game's own (`GroundVehicleParkingSpot`,
6 by 12 m; `SpaceVehicleParkingSpot`, 12 by 17 m), flat on the floor 0.3 ft
up as in the Atlantis' hold, long side fore and aft unless `across`. A spot
has to lie on whole cells of one room.

**The rest.** `FurnishRooms` in `src/generate.h` puts in the stations,
berths, service panels, weapons and the hull volume of the systems, exactly
as the generator does after it has placed its own rooms. It needs a box in
each room for the stations to stand in, which is the largest rectangle of
whole cells of that room. A room with no whole cell gets nothing.

## The plan file

JSON, written by `TriPlan::Save` (`src/triplan.cpp`):

```json
{
  "format": "shipgen-triplan", "version": 1,
  "name": "Wedge", "template": "military_frigate",
  "cellFeet": 5, "deckFeet": 10, "floorFeet": 1, "skinFeet": 2,
  "smoothing": 6, "maxFaces": 5000,
  "windowSill": 3, "windowTop": 7.5, "joinWindows": false, "slopedDecks": false,
  "width": 36, "depth": 16, "decks": 2,
  "rooms":   [ { "type": "Bridge", "label": "Bridge 1", "colour": "#5a8cdc" } ],
  "ftl": "wormhole-p",
  "doors":   [ { "deck": 0, "x": 7, "y": 7, "side": 1 } ],
  "windows": [ { "deck": 1, "x": 12, "y": 13, "side": 2 } ],
  "ladders": [ { "deck": 0, "x": 13, "y": 7 } ],
  "parking": [ { "deck": 0, "x": 4, "y": 8, "kind": "ground", "across": true } ],
  "stairs":  [ { "deck": 0, "x": 14, "y": 8, "side": 1 } ],
  "palette": [ { "colour": "#969ba5", "theme": -1, "name": "Hull" } ],
  "layers":  [ { "hull": "1234*0,8*1,...", "room": "...", "paint": "..." } ]
}
```

Each layer is one deck, from the bottom. Each of its three strings is the
value of every triangle in order (cell row by row, four triangles per cell in
the order S, E, N, W), run-length coded as `count*value`. `room` is -1 for no
room, `paint` an index into the palette. A door's `side` is the cell edge
0 S, 1 E, 2 N, 3 W. A ladder goes up from its deck. Stairs start at cell
(`x`, `y`) of their deck and climb towards `side`, onto the cell after the
run on the deck above. A parking spot is
centred on the grid corner (`x`, `y`); `kind` is `ground` or `space`. `theme` is 0, 1 or 2 for
the owner's three exterior colours, -1 for the plain colour.

## Units

*View > Metric units* shows the lengths in meters; the plan keeps them in
feet, the game's unit, so switching back and forth changes nothing. The
setting is remembered.

## Starting from a 3D model

A big model is thinned first: a quick clustering pass to five times the
face budget, then the careful (quadric) pass. Half a million faces take
about 9 seconds (the careful pass alone took a minute and a half). The
designer reads the model in the background with a progress window that
names the step it is on and can stop it between steps;
`shipgen plan-from-model` prints the steps with their times.

*File > New plan from a 3D model* (or `shipgen plan-from-model`) fills in
the hull triangles from an stl, 3ds or obj model (`PlanFromModel`,
`src/planbuild.h`). A model of more than 20,000 faces is first thinned to
that, the way `shipgen reduce` does it (`--faces` to change it). The model is
repaired when it needs it (and only kept repaired when that helps), split
into its separate pieces (each closed piece is tested on its own, so pieces
that overlap, such as a crystal and the rings round it, still fill), turned so that its longest horizontal side is
fore, scaled to the length asked for and stood on deck 0. A triangle is hull
when enough of the twelve points sampled in it (four across it, at three
heights in the deck) are inside the model; the threshold sets how many, lower
for a fuller hull. The rooms, doors, windows and paint are then drawn as
usual, and the grid can be touched up by hand.

What is lost: parts thinner than about half a cell (antennas, thin fins),
and slopes, which come out in steps of a deck that the shaping then
smooths. A model with holes, or one made of several overlapping shells, has
no clear inside where the holes are; a piece that is not closed is left
out. On an
ellipsoid 120 by 40 by 30 ft it fills 1,268 triangles where the volume
gives about 1,206.

Not done yet: showing the model faintly over the grid.

## The example

`TriPlan::Example()`: a 180 x 80 ft wedge over two decks, 45 degree bow and
cut stern corners, the upper deck narrower. Deck 1 has an engine room aft, a
hall down the middle and crew quarters; deck 2 has the bridge, reached by a
ladder from the hall. Two interior doors, a side passage from the hall to
the port hull with a hull door and a window, eight bridge windows, a red
stripe and an empire colour in the palette.

Built: 4,954 faces after shaping (11,794 before the nearly flat areas were
merged), about 4,700 faces after the doors and windows are cut, 8,258 cubic
meters by the engine's measure, 4 rooms, 3 doors, 9 windows, 1 ladder, 9
stations, 60 berths; the engine's report has 0 errors and 0 warnings. Every
room is checked against the shaped hull before the cuts; after them the hull
has holes, as in any blueprint with doors and windows from the game's
designer, so `inspect` calls the room fit unverified.

## What it does not do yet

* **Open an existing blueprint.** The designer only draws new ships. Taking
  a `.SoH` apart into triangles is possible for ships built on a grid, but
  not for a ship made from a model.
* **Turrets, landing gear, lifts, decals.** Only the hull, rooms, doors,
  windows, ladders, stairs, ramps, paths and the generator's furnishing.
* **Choose where the stations go.** They go where the generator puts them,
  by room type.
* **Rooms of more than one deck's height,** and deployable ramps (the
  ramps at hull doors are fixed).
* **Textures.** Paint is plain colours and empire colours.
* **Buildings.** The designer makes spacecraft only.

None of it has been tried in the game; see the questions in `HANDOFF.md`.
