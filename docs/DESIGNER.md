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
* **Redrawing the 3D view**: after each change the hull is shaped again
  and the 3D view redrawn, in the background, so drawing goes on meanwhile.
  On a big model that takes a while: untick *Redraw 3D while drawing* (bottom
  right, or the View menu; remembered) and press **F5** (View, *Redraw the
  3D view*) when you want to see the changes. The Hull page says when the
  3D view is out of date. Opening a plan, a new plan and the Build step
  always shape it.
* **The task bar** (bottom right) shows anything that takes a while:
  reading a 3D model, shaping the hull, building the blueprint. It names the
  task, the step it is on (numbered), how far along that step is where it
  can be counted (a progress bar with a percentage; a moving bar where it
  cannot), and the seconds so far. Reading a model can be stopped there
  between steps.

The steps are the tabs of the side panel:

| Step | What it does |
|---|---|
| 1 Ship | Name, kind of ship (one of the generator's templates: it decides the stations, berths and system volumes), FTL drive (none, warp 1-9, or a P, PN or PNN wormhole drive; default wormhole P), cell size (default 5 ft), deck height (10 ft), floor thickness (1 ft), the grid size, adding or removing decks, turning one deck around, mirroring it or turning it a quarter turn (90°, clockwise or anticlockwise seen from above, about the grid's middle; the grid grows when the turned deck would not fit), and turning the whole ship around (fore and aft), mirroring it (port and starboard), turning it upside down or a quarter turn either way (the grid's width and depth swap), for a 3D model that came in the wrong way round or sideways (also in the Edit menu; `shipgen plan-flip` with `--clockwise` or `--anticlockwise` on the command line). Everything drawn goes with it, and so does the 3D model when it is the hull; a ladder or stairs that no longer meet after a one-deck flip are taken away, and stairs go when the ship is turned upside down (they would climb down). |
| 2 Hull | Draw the hull. The deck below shows faintly. Hull style (smooth, angular or classic, below), skin (how far the hull surface stands out from the drawn triangles, 2 ft), smoothing (0 to 15, smooth style only) and the face budget (5,000). For a plan from a 3D model: whether the model is the hull, and whether that hull is sharp or smooth. The 3D view follows the drawing after a short pause. |
| 3 Rooms | Add rooms (*Add a room...* asks for the type, one of the engine's own room types, and a name, which has to be unique; the type and name boxes below the list change the room picked in it) and draw them on hull triangles. Switch the tool to doors (click the triangle standing on the cell edge where the door goes; between two rooms it is an interior door, between a room and anything that is not a room it is a hull door; *Hull doors* chooses their way down: a ramp or a ladder, always a ladder, or none for a docking door) ladders (click a cell that is wholly in a room here and in a room on the deck above), windows (click near an edge between a room and the outside: a cell edge, or a diagonal where the room's wall runs across a cell; a click on a room cell that has only one outer wall takes that one; drag along the walls for a row of windows, which build as one window, as *Join windows side by side* joins them, and drag from a window to take a row out; *Glass* below the tool says how the windows put in from now on are made: *Window frame*, a box cut through the hull with a frame round it, *Follow the hull*, the hull's own triangles inside the window become the glass, or as the Hull page's setting; drawn as a light blue line, dashed when framed, dotted when following the hull; in the plan `"style": "frame"` or `"hull"` on the window), vehicle parking spots (ground or space vehicle, or a 238 ft spacecraft landing spot, which need not lie in a room and is taken away by a click anywhere on it; an outline of the spot's size follows the mouse, snapped to the nearest grid corner: green fits, red does not, orange means a click takes that spot away; R turns it a quarter turn; drawn as a yellow dashed box), or stairs (click the triangle of the first cell on the side the stairs climb towards; the run has to be whole cells of one room, and on the deck above the cells over it and the cell after it whole cells of one room; drawn as steps with an arrow, and as a dashed box on the deck above), contrail emitters (click a hull triangle: the emitter goes on the hull's aftmost surface straight aft of it at mid-deck height, facing aft, as wide as *Contrail width*, 2 m by default; drawn as a blue ball where it ends up on the hull, with an arrow the way the trail goes and a dotted line back to the triangle it was put in from; click the ball or the triangle to take it away; the 3D view shows the same ball and arrow) or weapon emitters (click a hull triangle: a weapon bay that fires from the nearest point of the hull, of the type picked in *Weapon bay* below the tool, or the ship's *Fire control* on the Ship page; each emitter gets a fire control station of its own running that bay, and a right click gives an emitter the type picked; drawn as a red ring with a cross, and the bay's first letter when it has its own; in the plan `"module"` on the emitter) or the owner's flag or empire name (*Place empire flags and names*: pick *Flag* or *Name* and a width, then click a hull triangle; it goes upright on the nearest hull wall straight out to the side at mid-deck height, or on the nearest hull surface where there is no wall, 0.1 ft outside it; the game paints the owner's flag or name on it; drawn as a small flag or an "N" plate; in the plan an emitter of kind `flag` or `name`, `size` its width). When a click does nothing, the status bar says why until the mouse leaves that triangle. Triangles outside the shaped hull are hatched red. Fore is to the right; the blueprint is turned so that fore is +Y, the way the game wants it, so the 3D view of the built ship is turned a quarter turn from the plan. |
| 4 Crew | Optional. Stations, berths, transporter pads and walk paths placed by hand; left alone, the build puts all of them in by itself. Pick *Place stations* (the station, its seat: the bridge style's, a swivel chair or an integrated desk, and for fire control the weapon bay), *Place berths* (crew, officer, captain, passenger or troops), *Place transporter pads* or *Draw walk paths*, then click on a room's floor: an outline of the item follows the mouse, green where all of it is on one room's floor, red where not, orange over one already there (a click takes it away). R turns a station or berth 45° (Shift+R the other way); the mirror puts in the other side's too. A walk path is dragged from one triangle of a room to another of the same room, and runs straight between their middles on the floor, where the automatic paths meet it; a click on one takes it away. Ladders and stairs always get their paths. What is placed by hand counts towards what the ship needs: the build puts in only the rest, unless *Put in the rest automatically* is switched off for that kind (then only those placed by hand). In the plan: `stations`, `berths`, `pads`, `paths`, `autoStations`, `autoBerths`, `autoPads`, `autoPaths`. |
| 5 Paint | A palette: the base coat, any colours, and the owner's empire colours. Paint the hull triangles; the paint covers the hull over them on that deck, sides included. Double-click a colour to change it. *Livery* marks the colour picked as a pattern to keep sharp (an L in the list): on a hull shaped from a model, or an angular hull, the hull is cut along the outline drawn in it, so the edge is where it was drawn instead of stepping along the hull's own triangles, and thinning the hull to its face budget does not move that edge. A hull made from the drawn triangles keeps every colour edge already. |
| 6 Build | Build the blueprint and see the engine's report; save it as `.SoH`. *What gets built* lists every kind of thing in the plan (doors, door buttons, windows, ladders, ramps, parking, emitters, stations, berths, pads, system volumes, walk paths, ...): untick a kind to leave all of it out, or open it and untick single items. Nothing is deleted; the plan keeps them (`buildLeaveOut`, and `"skip": true` on an item) and ticking them again builds them. Clicking an item shows its deck. |

Every control has a tooltip saying what it does; the room tools have one
for each choice in their list.

The 3D view draws the hull see-through so that the rooms show, and only
its nearest layer (since 31 October 2026: before, the far side of the hull
was blended over the near side wherever it happened to be drawn later, so
the back faces seemed to be the ones shown).

**File** saves and opens plans (`.json`) and exports the blueprint
(Ctrl+E).

For scripts and tests the program also takes `--step <1-6>`,
`--deck <n>`, `--build`, `--export <file.SoH>` (build, save and quit; the exit
code is 0 when the engine accepts it, 3 when it has errors) and
`--screenshot <file.png>`.

## Fighter mode (experimental)

For small craft such as fighters (`docs/MICRO_MODE_PLAN.md`). *File > New
fighter* starts an empty plan of 24 by 12 cells of 2.5 ft and three decks of
4.5 ft; *File > New fighter from a 3D model* reads a model at its real length
(an F-35 is 15.7 m) with the model itself as the hull. *Fighter mode* on the
Ship page turns an existing plan into one (without changing its cell and deck
sizes). A fighter plan:

* is the *Fighter (experimental)* kind of ship: one pilot at the helm, most
  of the hull for power plant and maneuver drive;
* has no FTL drive (the drive's least volume is more than a fighter's hull)
  and no automatic berths (place one by hand in the Crew step if wanted);
* makes its windows follow the hull, so a canopy is the hull's own shape;
* allows decks down to 4 ft (a pilot sits; the 4 ft cockpit worked in the
  game);
* shows a yellow *experimental* banner on the Ship page and *fighter mode -
  experimental* in the window title, and the build notes it.

Draw the cockpit (a Bridge room) where the body is thick enough, a hull door
on its side (*auto* way down: a ramp when the door is low, else a ladder; the
game puts a button by the door), a canopy of windows or ceiling windows that
follow the hull, and place the pilot's seat (and any more seats) in the Crew
step. The build warns when there is no cockpit or no hull door. Not there
yet: a hatch in the belly, an opening canopy. In the plan, `"mode":
"fighter"`.

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
6. Over the face budget, the hull is **simplified** (`DecimateHull`, since
   31 October 2026): edges are collapsed onto one of their own ends, the
   cheapest first by the quadric error (Garland and Heckbert), until the
   budget is met. Unlike the general quadric method (tried before, which left
   700 non-manifold edges) it keeps what a hull needs: an edge goes only when
   exactly two faces share it and its ends have exactly two neighbours in
   common (the link condition, so the surface stays closed); no face may
   turn over or become a sliver; a vertex on a paint edge may only slide
   along that edge, and paint edges carry extra planes so they stay straight;
   and every vertex taken away is kept as a witness, which the new surface
   may not move in past by more than its distance to the nearest room less a
   wall (half the skin, at least half a foot), nor move past by more than
   40 percent of a cell (2 ft) either way. The Cyclops frigate: 52,948 faces
   to 5,000 in 2 s, its volume 2 percent less; the Nautilus 21,128 to 5,000;
   an ellipsoid 300 ft long 45,528 to 5,000 with rooms filling two decks
   right up to the hull still inside it (`test.sh`). When no collapse is
   left within that limit, the limit grows (half as much again each time,
   up to 1 percent of the ship's longest side) and the pass goes on: a big
   ship has room to move further. The Truman (600 ft): 306,928 to 22,578
   with the fixed 2 ft, 12,388 with the growing limit, 7,480 with the
   import tidied (below). The wall round the rooms holds at every limit.
   Thin triangles are kept out at a ratio of 0.05: at 0.01 the engine's
   cutting of doors and windows (`JigCut`) gave a different hull from one
   run to the next.
7. Still over the budget, nearly flat areas are merged too, with a tolerance
   that grows in steps (2 degrees and 0.05 ft up to 30 degrees and 0.8 ft)
   until it fits.
8. Every face gets its paint: a plain colour, or an empire colour theme.

**Flat panels** (experimental, *Flat panels* in the hull step, `build-plan
--panels <n>`, 0 by default). Before the budget pass the hull is split into
that many nearly flat regions (variational shape approximation, Cohen-Steiner
and others, 2004: each face goes to the region whose plane suits its normal
best, and the planes are refitted, in turns), and the vertices are drawn
onto their regions' planes, three rounds, each followed by a simplification
that keeps faces facing their region. It does cut faces (an ellipsoid with
100 panels: 2,540), but the regions still come out crumpled rather than as
clean plates, so it stays off. The proper way, meshing each region as one
polygon from its corner points, is still to do.

**The model as the hull** (*Use the 3D model as the hull* in the hull step,
on by default for a plan from a model; `build-plan --model-hull` or
`--grid-hull`). *Model hull* says how: **sharp** (`"sharp": true` in the
plan's `"model"`, the default for new plans since 0.3.3-alpha) or
**smooth** (plans saved before). The hull style does not apply to it. The plan remembers its model (`"model"` in the plan file:
the file, how it was turned and scaled, the seal), and the hull is built
from the sliced model itself on a grid of cubes a quarter of a cell across,
with the rooms grown by the skin added (so a room drawn out past the model
still has its wall). Hull triangles drawn or rubbed out do not change it;
untick the box to go back to the drawing.

**Sharp** (since 0.3.3-alpha): the cube grid is not blurred, and each
corner of the cubes' surface is put where the planes round it meet (dual
contouring): the model's own triangles where the solid ends at the model
(found by casting the edges between cube centres through it), a room's wall
where it ends there. Edges stay sharp and flat faces flat, then the flat
areas are merged; details smaller than a cube go. The Lantean Daedalus
comes out at 4,178 faces, recognisably the model.

**Smooth**: the surface is smooth, not stepped (since 31 October 2026, after the
user's Omega destroyer came out jagged): the cube grid is blurred a little
([1 2 1] each way), and the surface of the cubes over the half level has
its corners moved onto that level (surface nets). It is not smoothed or
inflated again afterwards. Edges are rounded by about a cube; parts thinner
than about two cubes (antennas, thin pipes) go. Tried and left out:
marching tetrahedra (so many slivers that the simplifying stalled, the
Cyclops at 16,674 faces) and drawing the points onto the model's own
triangles (it followed every little detail and came out crumpled). Set
`SHIPGEN_DUMP_HULL=<prefix>` to have `build-plan` write the hull at each
stage as obj files.

The Omega destroyer (150 ft): 5,000 faces, closed, 0.7 s. The Cyclops
frigate (300 ft): 184,292 faces before simplifying, a close copy of the
model; 5,000 after, closed, 7.9 s. The Truman (600 ft): 5,000, closed,
120 s and 2.5 GB, most of it reading its 451 MB obj (was 9,248 stepped). A model this detailed loses a lot at
5,000 faces: the engine copes with about 15,000, so a higher face budget
keeps more of it. If the model cannot be read, the drawn hull is used and
the notes say why.

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
less half a foot (at most 10 ft) and up to 8 ft high. Doors in one row
on neighbouring edges of one wall, between the same two rooms (or a room and
the outside), make one **bay door**: as wide as the edges less a foot, and as
tall as the room less half a foot, for vehicles. With the door tool, dragging
along a wall makes such a row, and dragging over separate doors joins them; a
single click makes a door of its own, and a click on a door takes it away. A
right click gives a door and the rest of its row the texture picked under
*Door texture*. Plans from before rows existed have no row numbers, and there
every neighbour joins. Doors are cut through
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
standing just outside the hull inside the doorway's voids. *Hull doors (ship)* on
the Rooms page (`hullDoorExit`) chooses for the whole ship, and *Door's way
down* for one door or row (see below): `auto` (a ramp where it fits, else
a ladder), `ladder` (always a ladder) or `none` (a docking door: no way
down). The hull void outside the door ends at the hull's real surface
(found by sight lines out through it at three heights), and the clear space
below the door reaches only as low as the way down needs. Put a hull door on a room cell at the edge of
the hull: the build warns when the way out is a tunnel through more than a
cell and a half of hull. A window is cut the same way with a window jig, a
cell wide less a foot, from the plan's window sill to its window top above
the floor (3 and 7.5 ft unless changed on the Rooms page; the top stays
half a foot under the ceiling). With *Join windows side by side*, windows
next to each other on one straight wall of a room (cell edges or one
diagonal line) are cut as one long window (without it, only windows dragged
out together, which share a `"row"` in the plan), as deep as the furthest hull
along it, and gets the
room's name as its group so that the room's light falls on it. With
*Windows follow the hull's own triangles* (`"windowStyle": "hull"`), the
glass is instead the hull's own surface in front of the window: the hull's
triangles there (cut along the window's outline when they are bigger than
it) become the window part, the room's wall gets an opening, and a lining
runs from the wall out to the glass; a window where less than half of it
finds hull in front is cut the old way. A window on
a diagonal is a diagonal's length less 0.7 ft wide; for the cut the hull
and the room are turned until the wall is square to an axis, so it gets the
same clean frame, and turned back. Walk paths follow the triangles of a room, by the shortest way, from
the triangle nearest the room's middle to each door and ladder; straight runs
are one segment, and a segment that is already there is not laid again.
Paths lie on the floor. A ladder between decks also gets an opening through
the lower room's ceiling and the upper room's floor (the engine's cutting
jig) and a visible ladder of two rails and rungs.

**Zero-G shafts** (room type *Zero-G Shaft*; the engine calls it a
"Gravity Well", and plans that say so still open) have no gravity: people
float in them and move up and down freely (in the game, while the ship's
gravity is working; without it the room has the gravity around the ship).
Where a zero-G shaft has a zero-G shaft directly above it on the next deck,
the lower one's ceiling and the upper one's floor are cut away over the
triangles they share, and a zero-G void fills the structure between them,
so the two are one space to float through, with no ladder needed. The cut
stays .05 ft inside the walls, which keeps the walls whole, and the crew get a
path up the shaft. Each such join is counted in the build report (`Zero-G joins` in
`build-plan`).

**Open to Deck Below** rooms are the top of a tall room, as in the game's old
designer: the room below reaches up into this deck. Wherever a triangle of
one has a triangle of any room under it, the ceiling and floor between them
are cut away and a void fills the structure, with the gravity of the room
below, so people fall to the lower floor. The shape need not match; only the
overlap opens. With no room under it at all the floor stays and the build
warns. The upper part gets no stations or berths, as a zero-G shaft doesn't.
Each join is counted in the build report (`Open to below` in `build-plan`).

**Windows and balconies between rooms** (since 7 November 2026; the tools
*Place windows between rooms* and *Place balconies* on the Rooms page, since
8 November their own tools rather than a choice under the door tool; `"kind"`
on a door in the plan). Placed on the edge between two rooms, and dragged
into rows as doors are. A click with one of the three tools (doors, windows
between rooms, balconies) on an opening of another kind makes it that kind,
its whole row; a click on one of its own kind takes it away.
A *window* cuts both walls from the ship's window sill to its top and fills
the hole with a pane (a window jig through both room walls, as the game's
designer does). A *balcony* cuts both walls open above a parapet a third of
the room's height, up to just under the ceiling; over a room open to the deck
below it is a gallery. The two rooms' voids stay apart, so nobody walks
through; they are for looking. Drawn light blue and dashed, and orange and
dotted. One on an edge with a room on one side only is dropped. **Known**: not
yet tried in the game: whether the game draws the other room through the pane,
and whether someone can climb over a parapet.

**Doorways stay clear.** A window's frame, and the lining of its tunnel to
the hull surface, is cut away where it runs into an interior doorway (a window
on a wall beside a door, with a long tunnel, did that in the arness).

**Door buttons** (since 7 November 2026). After a door is placed (clicked or
dragged out), the next click on a wall of one of its rooms, within 12 ft of
it, puts a button there; a second click can put one on the other side. Esc,
or a click anywhere else, ends it. Shift and a click on a door places its
buttons again. The wall that would get the button lights up green under the
mouse; placed buttons are small green squares. In the build each button is a
dark plate with a lit green button standing 0.15 ft out of the wall, 4 ft up,
and the door's switch (the game has two per door, where the player clicks to
work it, within 3 m) goes on its face. An interior door with a button is no
longer automatic: the game opens an automatic door for anyone passing near a
switch, so it would open for people walking past the button. With one button,
the door's other switch stays on the door itself. A hull door keeps its
outside switch. In the plan: `"buttons": [[x, y], ...]` on a door, in eighths
of a cell from grid corner 0,0, an eighth of a cell in from the middle of the
wall edge, on the button's side.

**Turbolifts** (room type *Turbo Lift* or *Public Lift*, since 7 November
2026) are drawn as a room on the same cells of every deck they serve. Each deck
where the lift has a door (placed with the door tool, between the lift and a
room, or the outside for a hull lift door; doors side by side make one wide
door for vehicles) is a stop. The build makes one car, the game's `Turbo Lift`
part (`AuScLift`), on the lowest deck the lift is drawn on: its states are the
car's mesh and its void, and each stop is the car moved straight up to that
deck. Each stop's door is cut with the car standing there, as the game's own
designer does, and is named after its stop, `Deck 2 Bridge 1` (the deck and the
room it opens into), which makes it a lift door. Through each door a walk path
runs from inside the car into the room; when the blueprint is finalized the
game links the stops with these, so the crew use the lift. There is no shaft:
as in the Atlantis, the car goes from stop to stop. It carries everyone and
everything standing in it, vehicles too. *Car height* (`"carHeight"` on the
room, in feet; 0 for the deck's height) makes the car taller, for vehicles;
the lift's cells must then be drawn on the decks above its highest stop as far
as the car reaches, or the build warns that the car leaves the hull or runs
into another room. A lift door is as tall as the deck's room (no taller than
the car). The lift gets no stations, windows, ladders or parking. `build-plan`
reports `Lifts: 1 (3 stops)`. **Known**: a stop at ground level below the
hull, for vehicles to drive straight in, is not made yet.

**Ceiling windows** (experimental; `"skylights"` in the plan) go in a
triangle of a room (`"k"`, as rooms have; a click with the triangle brush) or
a whole cell (no `"k"`; the cell brush, or Shift+click), or the cell and the
eight round it (the 3x3 brush), with no room on any deck above it, straight up
through the hull. Taking one triangle out of a whole cell leaves the other
three, and four triangles alike become the whole cell again. Each has a
glass shape (`"shape"`; *Glass* below the tool): `"hull"` takes the hull's
own surface over it as the glass, in the hull's shape, where neighbouring
triangles of one room make one window of whatever outline they draw; the
hull's and the ceiling's triangles are cut along that outline, and a lining
stands on its edges up to the hull. `"flat"` is a level pane in a cutout: the
room and hull are turned so the ceiling is a wall, cut as a wall window is,
and turned back; *Join windows side by side* makes neighbouring whole cells of
one room one rectangular pane, and two triangles side by side in a cell one
half-cell pane. With no shape the plan's *Windows follow the hull's own
triangles* decides. A hull window that finds hull over less than half of it is
cut flat. The plan view draws them dotted when they follow the hull and
dashed when flat. Deep ones warn that they look down a tunnel. A plan turned
upside down makes them windows in the floor.

**Bridge styles** (`"bridgeStyle"` in the plan, on the Build page) choose how
stations look. `hazeron`, the default, is the game's swivel chairs, as before.
`desks` (experimental) uses the engine's integrated stations (`AuScStation`):
a desk with a sloping top, the station's screen on it and a stool. In each
room, curved rows wrap round the captain, against the fore wall, then straight
rows for what the curves could not hold. Stations with no room of their own go
on the bridge. Desks keep clear of doors, ladders and stairs. Medic and
transporter stations stay chairs, because their integrated versions need more
setting up, and a station no desk fits gets a chair and a note in the report.
Desks do not block walking; a walk path may pass through one.

With desks, the Build page also sets, for the whole ship, whether operators sit
on a stool or stand with no stool (`"deskStanding"`), and how high the desks'
top edge is above the floor (`"deskHeight"`, in feet: 2.2 to 4.5 sitting, 2.8 to
5.5 standing). Left unset, it is 3.0 ft sitting and 4.2 ft standing. The screen
follows the desk. Lower suits short characters.

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

**Hull styles** (*Hull style* on the Hull page, `hullStyle`, since
0.3.3-alpha):

* **Smooth** (`smooth`, the default): the hull is pushed out round the
  drawn triangles by the skin and its edges rounded (*Smoothing*); with
  *Slope the hull between decks*, sloped as above first.
* **Angular** (`angular`, experimental): the hull slopes from deck to deck
  in straight, flat panels and keeps sharp edges. Each deck's outline gets
  a distance field (in the octagon norm, so corners are cut off at 45
  degrees, not rounded), and between two decks that both have hull the
  outline is blended from one to the other over each deck's height, with
  the same rule as the slopes above; the surface is taken from a grid of
  cubes with each corner put where the planes round it meet, and flat areas
  are merged. A slope that does not change is one flat face. Decks slope
  into each other only where their edges are within a cell and a half;
  further apart the hull steps. An empty deck between two others stays a
  gap. The smoothing and *Slope the hull between decks* are not used. The
  style follows the drawing exactly: an outline drawn as a staircase comes
  out fluted, so draw slanted walls with the diagonals.
* **Classic** (`classic`): the old Shores of Hazeron designer's look,
  straight walls up from each deck's outline and flat tops and bottoms,
  grown by the skin with mitred (sharp) corners; not sloped or smoothed.

**How the inside looks.** Room floors are metal tiles, ceilings light
plates, and walls metal panels in a light shade of the room's colour in the
plan. Hull doors are painted yellow and interior doors blue, so that they
stand out from the walls. Shields, the weapon bay and the transporter can be
left out on the Ship page (`leaveOut` in the plan file: `shield`,
`firecontrol`, `transporter`); the example Wedge leaves all three out.

**Fire control** on the Ship page chooses what the fire control station runs
(`fireControlModule` in the plan, a commodity id; left out, it is 252): Energy
Weapon Bay (252), Ballistic Weapon Bay (261), Weapon Launcher Bay (254), Weapon
Deployment Bay (319), Tractor Beam Bay (258), Prospector Bay (257), Wildcatter
Bay (291), Siphon Bay (289), Particle Collector Bay (283) or Atmosphere
Condenser Bay (256), the same list as the game's own station dialog.

**Vehicle parking spots** are the game's own (`GroundVehicleParkingSpot`,
6 by 12 m; `SpaceVehicleParkingSpot`, 12 by 17 m), flat on the floor 0.3 ft
up as in the Atlantis' hold, long side fore and aft unless `across`. A spot
has to lie on whole cells of one room. A **spacecraft landing spot**
(`SpacecraftParkingSpot`, kind `spacecraft`) is a square 238 ft each way, the
largest hull the game lets land (twice `GroundSpawnSizeLimit`). It need not
lie in a room, only clear of the other landing spots on its deck; the build
warns when the square reaches past the hull of its deck and the deck below.

**The rest.** `FurnishRooms` in `src/generate.h` puts in the stations,
berths, service panels, weapons and the hull volume of the systems, exactly
as the generator does after it has placed its own rooms. It needs a box in
each room for the stations to stand in, which is the largest rectangle of
whole cells of that room. A room with no whole cell gets nothing.

**What a room type does in the game.** In a ship, the game reads only a room's
*hold access* (`DaSc::InsideRoomType`); the name and the type you see are
labels. The build gives each room the hold access of its type:

| Room type | In the game |
|---|---|
| Galley | food (pantry) |
| Sick Bay | medicine (pharmacy); it also gets the medic (surgery) station |
| Engine Room, Fuel Cell | refuelling (fuel coupling) |
| Brig | feeding prisoners |
| Barracks | the armory; troops' sea chests |
| Lounge | the bar |
| Crew Cabin, Officer Cabin, Stateroom, Crew Quarters | clothes (wardrobe) |
| Vehicle Bay | vehicles park, refuel and rearm |
| Hold, Store | the cargo hold |
| everything else | nothing: a corridor |

Bridge, Transporter Room, Sensor, Shield and Weapon rooms get their stations;
Open to Deck Below and Zero-G Shaft change the room's shape and gravity. A
**Transporter Room** gets its transporter station even if the template leaves
it out, and up to 12 **transporter pads** (3 ft square, their middles at
least 4.65 ft apart, so 0.5 m between pads), grouped near the console and clear
of its furniture and doors. Before 7 November 2026 every room of every ship
was a cargo hold in the game. The type list in the designer is grouped by
this; hover over a type to see what it does.

**Cargo.** Every ship's cargo hold (the hold module) is at least 2% of its
hull volume, whatever its template. The cargo is loaded and unloaded in the
rooms with the cargo hold's access; when no room is a Hold or Store, the rooms
that give nothing else (halls, corridors, the bridge; not lifts or zero-G
shafts) get it, and the build says so in a note.

**Who may enter a room.** *Who may enter* on the Rooms page (in the plan,
`"access"` on the room: a list of `"officers"`, `"crew"`, `"passengers"`,
`"troops"`; left out for everyone) sets the room's personnel access, which the
game keeps on the room's void and shows in its own designer ("Restricted to
Officers, Crew"). **Known**: the game client does not hold anyone to it yet
(`AuScRoomVoid::IsAccessGranted` grants every request); `shipgen parts` shows
it as `only officers, crew`.

**Room and door textures.** A room can have its own wall, floor and ceiling
texture from the game's library (*The room picked* on the Rooms page; in the
plan file `wallTexture`, `floorTexture` and `ceilingTexture`, library ids from
`docs/TEXTURE_LIBRARY.md`), and a door its own (`texture`). A ship has one
palette of 15 textures: one already in it is reused, a new one takes a slot
nothing uses, and when none is free the build warns and the rest keep their
usual look. A chosen texture is shown unpainted.

*Textures (whole ship)* on the Rooms page (in the plan, `wallTexture`,
`floorTexture`, `ceilingTexture` and `doorTexture` at the top level) are for
every room and door with none of its own, so they need not be picked for each
new room. A room's or door's own choice comes first; *Same as the ship* in a
room's lists means it has none.

**Flat windows.** With *Join windows* on, windows that follow a wall zigzagging
over the triangles are one flat pane on a straight line across the teeth, if
the teeth stay within one cell of that line. The pane sits at the furthest hull
at the window's middle and top, so a ledge below it does not push it out. The
hull is cut directly here, because the engine's jig did not finish on an
opening this wide across a zigzag hull. Turn off *One flat pane across a zigzag
wall* (`"flatWindows": false`) to cut tooth by tooth.

**A way down for each hull door.** *Door's way down* and *Ramp length per
height* on the Rooms page apply to hull doors you place from then on; with the
door tool, a right click on a door gives it and the rest of its row these
settings and the door texture. A door with a way of its own shows a letter in the
plan: L ladder, R ramp, D docking, A ramp or ladder.

* A ramp asked for by name reaches the ground from up to 60 ft (the automatic
  choice stops at 15 ft); higher, the door gets a ladder and the build warns.
* *Ramp length per height* is feet of ramp for each foot it falls, 1.5 unless you
  change it. At 4, a door 15 ft up has a ramp of about 60 ft.
* Ramps and ladders are always there, whether the door is open or closed. (An
  earlier *Only while the door is open* did not work in the game and is gone;
  older plans that have it build as if it were off.)
* Whether ground vehicles can drive up a ramp has not been tried yet.
  `shipgen door-check` shows what each way down does, closed and open, and
  whether each ladder can be taken hold of from the ground.

**Crew routes.** The build warns when the rooms' walking paths fall into
separate parts, naming the rooms in each. `shipgen sight` shows what is seen
along a line of sight, leaving out faces seen from behind, as the game does.

## The plan file

JSON, written by `TriPlan::Save` (`src/triplan.cpp`):

```json
{
  "format": "shipgen-triplan", "version": 1,
  "name": "Wedge", "template": "military_frigate",
  "cellFeet": 5, "deckFeet": 10, "floorFeet": 1, "skinFeet": 2,
  "smoothing": 6, "maxFaces": 5000,
  "windowSill": 3, "windowTop": 7.5, "joinWindows": false, "windowStyle": "", "slopedDecks": false,
  "hullStyle": "angular", "hullDoorExit": "ladder",
  "width": 36, "depth": 16, "decks": 2,
  "rooms":   [ { "type": "Bridge", "label": "Bridge 1", "colour": "#5a8cdc" } ],
  "ftl": "wormhole-p",
  "doors":   [ { "deck": 0, "x": 7, "y": 7, "side": 1, "row": 1, "texture": 111,
                 "exit": "ramp", "rampRun": 4 } ],
  "windows": [ { "deck": 1, "x": 12, "y": 13, "side": 2, "style": "hull" } ],
  "ladders": [ { "deck": 0, "x": 13, "y": 7 } ],
  "parking": [ { "deck": 0, "x": 4, "y": 8, "kind": "ground", "across": true } ],
  "skylights": [ { "deck": 1, "x": 12, "y": 8 }, { "deck": 1, "x": 13, "y": 8, "k": 3, "shape": "hull" } ],
  "stairs":  [ { "deck": 0, "x": 14, "y": 8, "side": 1 } ],
  "emitters": [ { "deck": 0, "x": 2, "y": 8, "side": 3, "kind": "contrail", "size": 6.56 },
                { "deck": 0, "x": 30, "y": 8, "side": 1, "kind": "weapon", "module": 261 } ],
  "stations": [ { "deck": 1, "x": 18.5, "y": 8.5, "yaw": 90, "type": "helm", "seat": "chair" } ],
  "berths":  [ { "deck": 0, "x": 14.5, "y": 3.5, "yaw": 0, "kind": "crew" } ],
  "pads":    [ { "deck": 0, "x": 20, "y": 6 } ],
  "paths":   [ { "deck": 1, "from": [19, 4, 0], "to": [25, 8, 1] } ],
  "autoBerths": false, "buildLeaveOut": [ "windows" ],
  "palette": [ { "colour": "#969ba5", "theme": -1, "name": "Hull" }, { "colour": "#c02020", "theme": -1, "name": "Stripe", "livery": true } ],
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
centred on the grid corner (`x`, `y`); `kind` is `ground`, `space` or `spacecraft`. An
emitter stands on triangle `side` of cell (`x`, `y`); `kind` is `contrail`,
`weapon`, `flag` or `name`, `size` a contrail's or a flag's width in feet. A plan made from a 3D model
has a `"model"` object (the file, how it was placed, `"asHull"`,
`"sharp"` when the model hull is sharp, and `"mirror"`, three booleans,
when it was flipped). `hullStyle` is `smooth` (the default, then left out),
`angular` or `classic`; `hullDoorExit` is `auto` (the default, left out),
`ladder` or `none`. `theme` is 0, 1 or 2 for
the owner's three exterior colours, -1 for the plain colour. Stations,
berths and pads placed by hand (Crew step) stand at (`x`, `y`) in cells from
the grid's corner 0,0 (fractions allowed), `yaw` in degrees from fore towards
port; a path runs from triangle `from` to triangle `to` (`[x, y, side]`).
`autoStations`, `autoBerths`, `autoPads` and `autoPaths` are written only
when false. `buildLeaveOut` lists the kinds the Build step leaves out
(`TriPlan::BuildKinds`), and any item can have `"skip": true`.

## Units

*View > Metric units* shows the lengths in meters; the plan keeps them in
feet, the game's unit, so switching back and forth changes nothing. The
setting is remembered.

## Starting from a 3D model

**The model is sliced** (since 31 October 2026, after 0.3.1-alpha; the
user's idea). It is cut into thin horizontal slabs, an eighth of a cell
thick, and in each slab every square of a grid as fine that a triangle of
the model passes through is marked; then the outside is flooded in from the
edge of the grid, round the marks, and what it cannot reach is inside
(`SliceSolid`, `src/planbuild.cpp`). Gaps in the surface narrower than about
a foot are closed first (`--seal`). Each slab is flooded on its own, so only
a gap the outside can flow through sideways matters: a missing top or
bottom does not. Models from the internet are mostly open panels laid
against each other, and the older method, below, counts only the closed
pieces; slicing needs no closed surface, no repair and no thinning, since
every face is visited once.

On the user's models (`3dmodelsraw` in their cloud folder), at the same
length:

| Model | Faces | Closed pieces only | Sliced |
|---|---|---|---|
| Cyclops civilian frigate, 300 ft | 95,017 | 4,535 hull triangles, 40 s | 6,607, 1.8 s |
| Nautilus explorer, 300 ft | 17,794 | 1,855, 6.8 s | 2,331, 0.5 s |
| Truman class (Expanse), 600 ft | 3,989,625 | **27**, 356 s, 5.5 GB | **111,503**, 72 s (66 s reading the obj), 2.5 GB |

The sliced plans hold nearly every triangle of the old ones (16 fewer on
the Cyclops) and add the parts that were open panels.

**The number of slices.** The decks are the slices of the model: their
height stays (headroom), so *Decks* in the dialog (`--decks`) sets how many
the model is cut into, and the length follows from it; the recommended
length below says how many decks it gives. *Slices per deck*
(`--slices`) is how many heights in a deck each triangle is judged at;
the recommendation is about one every 3 ft, and always odd (a deck the
model fills half way has an even count tie with the threshold of one
half): three in a 10 ft deck. *Inside found by* (`--method solid`) still
offers the older method.

**Tidying.** Specks the slicing leaves (pieces of fewer than 12 triangles,
counted across edges and from deck to deck; `--min-piece`) are taken away,
and single triangles missing inside a filled area are filled in
(`--no-pinholes` to keep them). The Truman's hull came down from 12,388
faces to 7,480 from this alone.

The older method, *closed pieces only*:

A big model is thinned first: a quick clustering pass to five times the
face budget, then the careful (quadric) pass. Half a million faces take
about 9 seconds (the careful pass alone took a minute and a half). The
designer reads the model in the background, with the task bar at the
bottom of the window naming the step it is on and how far along it is, and
a Stop button that stops it between steps (in 0.3.0-alpha the
closing of that window counted as Stop and threw every plan away; fixed in
0.3.1-alpha); `shipgen plan-from-model` prints the steps with their times.

Before anything else the model is **repaired the way the mesh tool does
it** (`RepairMesh`: triangles with no area and doubles taken away, windings
turned, holes closed), and again after thinning; a repair is kept only when
it leaves fewer broken edges (on some models closing holes makes things
worse), and the notes say what was done. The user's Babylon 5 Warlock
gained about 40 percent more hull triangles from it.

How much of the model came through is measured by points spread through
its inside: the share that lands on hull triangles. Below 85 percent the
importer tries longer lengths (1.5 to 4 times, up to 2000 ft) and
**recommends** the shortest that holds 85 percent, or the best one if it
gains at least ten points, with the grid it gives. The cells and decks keep
their size, which doors and headroom need; a longer ship makes the thin
parts of the model thick enough for them. In the designer the message has a
button to import again at that length. The Warlock: about 63 percent at
150 ft, 83 percent at the recommended 450 ft (which the real import then
confirms).

What went wrong is said plainly: a plan that could not be made gives the
reason in a message box, and a plan that was made but needs looking over
(the model is not a closed surface, it is in many pieces, or little of it
came through) says so too. Below half it also suggests a lower
*Inside at least*.

*File > New plan from a 3D model* (or `shipgen plan-from-model`) fills in
the hull triangles from an stl, 3ds or obj model, or FBX, glTF and the other
formats Assimp reads when the program was built with it (`PlanFromModel`,
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
