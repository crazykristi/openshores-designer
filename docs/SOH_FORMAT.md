# The .SoH blueprint format

What a Shores of Hazeron blueprint file contains, taken from the engine
source in `third_party/soh` (the published v7b2 source) and checked against
real files in `~/Documents/designs`. Line numbers refer to that copy of the
source.

Everything here is read and written by the engine's own classes, which the
tools link against. There is no other implementation of the format in this
project, except `src/sohfile.cpp`, which walks the part list itself so that
it can step over part types the engine source does not know (see
[Part types the engine source does not know](#part-types-the-engine-source-does-not-know)).

---

## 1. The container

`AuDesign::SaveToFile` and `AuDesign::LoadFromDataStream`,
`audata/audesign.cpp:429-520`.

```
file      = quint8 fileType            0x24 (SOH_FILE_TYPE, auutil/aufilever.h:66)
            quint8 fileVersion         0x02 (compressed) or 0x01 (old, uncompressed)
            QByteArray crypt           quint32 big-endian length, then the bytes

crypt     = XOR of: QByteArray inner   (QDataStream Qt_5_0 framing: length + bytes)

inner     = qCompress(Pack())          4-byte big-endian uncompressed size, then zlib
```

The cipher (`AuCrypt::Crypt`, `auutil/aucrypt.cpp:20`) is a running XOR. The
key is `(fileType << 8) | fileVersion`; the seed starts at `(key >> 8) & 0x0f`
(4 for a blueprint) and grows by `key & 0x0f` (2) after every byte, wrapping
at 256. Encrypting and decrypting are the same operation.

`shipgen` reads and writes this with the engine's own code. `ReadDesignBytes`
in `src/sohfile.h` returns the decrypted, uncompressed bytes, which is handy
for looking at a file with `xxd`.

The other `fileType` values in `aufilever.h` are not blueprints.
`SHIP_FILE_TYPE` (the old `.ship` files) is defined but read nowhere, so the
`.ship` files in the designs folder cannot be opened by this engine.

## 2. The packed design

`AuDesign::Pack`, `audata/audesign.cpp:401`. The stream is `QDataStream`
version `Qt_4_1`, big-endian.

```
quint8      designType        1 spacecraft (AuScDesign), 2 building (AuBdDesign), 3 item
AuDesign    the common header
...         the spacecraft or building fields, then the part list
```

### The common header (`AuDesign::operator<<`, `audesign.cpp:235`)

| Field | Type | Notes |
|---|---|---|
| id | quint32 | The engine asserts it is not 0; the tools use 1. |
| name | QString | |
| architect id | AuId (4 bytes) | |
| architect name | QString | |
| version and state | quint8 | Version (currently 4) in the top nibble, `STATE` in the bottom. |
| textures | see `ReadTextures` | The design's texture palette, 16 slots. |
| thumbnail | QImage | |
| description | QString | version 3 and later |
| grid and view positions | quint8 count + (quint8, AuMatrix4x4)...; quint8 count + (quint8, AuDesignPosition)... | Only when the state is not Final or Deleted. |

### A spacecraft (`AuScDesign::operator<<`, `audata/auscdesign.cpp:765`)

`quint8` version in the top nibble (currently 6). From version 1 on:
hull volume (quint32), landing orientation (quint8, version 6+), the modules
(`AuScModules`), **the part list**, station orientation (version 3+),
streamline, structure, minimum quality, armour (version 2+), then the bays
(quint16 count of (part id quint16, volume quint32)), capacitor, cryo berths,
the FTL drives (same shape as the bays), fuel, hold, life support, manoeuvre
drive, power plant, sensors, shields, and skeleton (version 5+).

### A building (`AuBdDesign::operator<<`, `audata/aubddesign.cpp`)

`quint8` version (currently 4), hull volume (quint32), the modules
(`AuBdModules`), **the part list**, structure, industry id (version 3+), then
the system volumes: arena, armour, auditorium, the bays, capacitor, hold
(version 2 folded fuel into it), large, medium and small homes, life
support, lounge, offices, power plant, sensors, shields, shops, skeleton
(version 4+), store.

The industry ids and the volumes a building needs for an industry are in the
game database, not in the source; `src/building.cpp` installs a stand-in
table (`InstallIndustryTable`). The thresholds the engine checks are in
`audata/aubddesign.cpp:2661-2726`.

## 3. The part list

`AuScParts::operator<<`, `audata/auscparts.cpp:702`.

```
quint16 count
count times:
    quint16 partId
    quint8  partType            PART_TYPE, audata/auscparts.h:1228
    ...     the part's own data, written by its class
```

There is **no length** in front of a part's data. A reader that meets a type
it does not know cannot tell where the next part starts, and the engine's
reader simply stops: it prints `invalid part type`, keeps the parts it has,
and returns the design as if nothing had happened. See the next section.

Every part's data starts with the same fields (`AuScPart::operator<<`,
`auscparts.cpp:458`):

| Field | Type | Notes |
|---|---|---|
| bits | quint8 | `BIT_Hide` 0x01, `BIT_Lock` 0x02, `BIT_Omit` 0x04, `BIT_Seal` 0x08 in the bottom nibble. The top nibble used to be the version; 0xF means a version byte follows. |
| version | quint8 | Present when the top nibble is 0xF. Currently 0x11 (`PART_SERIALIZE_VER`). |
| architect id | AuId | version 3+ |
| architect name | QString | version 3+ |
| group | QString | version 0x10+. For rooms and voids this is the **room's name**; see section 5. |

### The part types

| | | | | | |
|---|---|---|---|---|---|
| 0 Invalid | 1 Mesh | 2 DoorJig | 3 WindowJig | 4 **Hull** | 5 **Room** |
| 6 **Door** | 7 Window | 8 LandingGear | 9 Lift | 10 StatusAlert | 11 StatusLifeSupport |
| 12 StatusMission | 13 PanelFTLDrive | 14 PanelLifeSupport | 15 PanelManeuverDrive | 16 PanelPowerPlant | 17 PanelSensor |
| 18 PanelShield | 19 PanelWeapon | 20 StationCommand | 21 StationEngineer | 22 StationFireControl | 23 StationHelm |
| 24 StationMedic | 25 StationNavigator | 26 StationPowerRelay | 27 StationSensor | 28 StationShield | 29 StationTransporter |
| 30 ConsoleCommand | 31 ConsoleEngineer | 32 ConsoleFireControl | 33 ConsoleHelm | 34 ConsoleMedic | 35 ConsoleNavigator |
| 36 ConsolePowerRelay | 37 ConsoleSensor | 38 ConsoleShield | 39 ConsoleTransporter | 40 EnvironmentVoid | 41 Camera |
| 42 TroopPost | 43 GroundVehicleParkingSpot | 44 SpaceVehicleParkingSpot | 45 SpaceVehicleLauncher | 46 BerthCapt | 47 BerthCrew |
| 48 BerthOfficer | 49 BerthPassenger | 50 BerthTroop | 51 GameTable | 52 ContrailEmitter | 53 HeadlightLens (deprecated) |
| 54 NavLightLens | 55 RotatingBeaconLens | 56 StrobeLens | 57 Turret | 58 PodBay | 59 Sketch |
| 60 LaunchTubeJig | 61 CuttingJig | 62 SpaceVehicleLandingSystem | 63 LightBulb | 64 StatusWorldMap | 65 Decal |
| 66 EmpireFlag | 67 EmpireName | 68 ShipName | 69 CaptName | 70 DecalJig | 71 **HullVoid** |
| 72 **RoomVoid** | 73 **PathWalk** | 74 **PathLadder** | 75 PanelCapacitor | 76 WaterVehicleParkingSpot | 77 Barrier |
| 78 RocketParkingSpot | 79 Site | 80 Tree | 81 Shrub | 82 Fence | 83 CitizenPost |
| 84 LivestockPost | 85 SpacecraftParkingSpot | 86 StationDesigner | 87 LightLens | 88 BerthName | 89 BuildZone |

The stations that control a system are the `Console*` types; which system a
console controls is its module (`AuScPart::ModuleSystem`). The game's own room
type names are `ROOMTYPE` in `auutil/audef.h:605`, with
`RoomTypeNameInSpacecraft()` giving the spacecraft names (`src/names.cpp`
lists them).

**A `Console*` part is a swivel chair**, not a wall screen: "the point
location of an old style swiveling chair/screen combo" (`auscparts.h:7209`).
Its icon is a flat quad 1 m wide and 1.7 m deep (0.6 m behind the point,
1.1 m ahead, `AuScConsole::IconBack/Fore`), which lies **on the floor** and
faces its local +Y. The working designs have them so (the scout's helm and
command consoles: z 4.6..4.6, the room floor). The `Station*` types (20 to
29) are the wall screens. Before 2026-10-31 the generator stood the consoles
up on a wall, so in the game the chairs looked stuck on the walls; they now
lie on the floor facing fore, in rows from the fore wall of the room.

### Room lighting

A `Room` mesh has an initial interior light (`INTERIOR_LIGHT`: dark,
bright, dim) and a *sunlit* flag (`AuScRoom::SetInteriorLighting`); the
engine's default is bright and sunlit, and so are the bridges of all the
working designs (`shipgen parts` shows both). Our rooms were made bright but
not sunlit, and the user found the Wedge very dark inside. They are now
sunlit.

### Stairs and ramps

From the user's aircraft factory (`~/Documents/designs/examplebuildings/
aircraftfactory.SoH`, "a semi-working stairwell") and the engine source:

* Each of its two flights of stairs is 26 thin **mesh** slabs (steps about
  0.6 ft high and 0.9 ft deep, 33 ft wide, from z 0.7 to 16.4), with mesh
  railings at the sides, under a **hull void** that covers the whole flight
  (so the hull does not stop anyone there), and a **Barrier** (part type
  `Barrier`, 32 triangles) over the same span, from z -8.8 up to 16.4: the
  slope that is walked on. There is no walk path up it, which is probably
  why it only half works (the crew cannot find their way up).
* The door into the building has two small sloped barriers in front of it
  (z -6.3 and -2.9 up to 0.4): a ramp from the ground to the door.
* `AuScBarrier` (`auscparts.h:12090`) is an invisible solid that blocks
  walking (`IsPathBlocked`), and it can be **associated with a part state**,
  "e.g. turbo lift, door": a barrier that is only there while a door is
  open.
* A door keeps a **mesh for each state** (`AuScStatePart::m_StateMeshes`:
  Closed, Open10 to Open90, Open), and a doorway void can be tied to each
  state (the factory's sliding door 311 has one for every 10%).

So a ramp is a sloped barrier (and something to see), and a deployable ramp
is a door whose open state is the lowered ramp, with the barrier and a walk
path down it tied to the open state.

Measured with `tests/vertprobe.cpp`: the factory's barrier 374 is a level
slab in front of the door (top 0.06 ft above the floor, 10 ft out, 20 ft
wide), and 329 a slab 3 ft thick tilted at 45 degrees from the end of it
down to the ground. Its walk path stays level over both.

What shipgen makes (`src/interior.h`, not yet tried in the game):

* **A ramp at a hull door** (`AddDoorRamp`): one Barrier whose side view is
  a level landing over the threshold (from 0.2 ft inside the hull surface
  to 1.35 ft outside) and then a slope of 1.5 ft out for each foot down
  (34 degrees) to half a foot below the bottom of the hull, 1 ft thick; the
  same shape as a mesh, 0.1 ft narrower each side, to see; a constant hull
  void over it; a walk path from the room over the landing (tied to the
  door's open state) and down the slope. Only for doors up to 15 ft above
  the ground; higher ones keep the ladder.
* **Stairs** (`AddStairFlight`, `AddRail`): a sloped Barrier from the foot
  on the lower floor to the upper floor and half a foot on, steps as a mesh
  (blocks about 0.65 ft high, the slope through the middle of each), the
  opening and the void as for a ladder but over the whole run, a walk path
  up the slope, and on the upper floor a rail round the opening: a Barrier
  0.3 ft thick and 3.5 ft high, and a rail on posts to see.

### Room light and door switches

From the OpenShores source (`third_party/SoH-source/`):

* A ship's room light state starts from the room mesh's initial light
  (`AuScParts::RoomLight`, by the room's name), and the people in it can
  turn it brighter or dimmer. The light only counts while the capacitor has
  charge (`DaSc::InteriorAmbientLight`); with none the room gets the
  engine's darkness level. A sunlit room adds the outside light.
* The working designs' room meshes have every vertex normal square to its
  face, all pointing one way (the scout's into the room). shipgen's pointed
  every way until 2026-10-31.
* A door's two switch locations (`AuScDoor::DoorOpLocs`) are where it is
  opened and closed. Unset (both zero), the game uses the middle of the
  closed door mesh. The Atlantis' hull door has them set, one each side,
  about 4.5 to 5 ft above the floor. `shipgen parts` shows them.

### Barriers outside the hull

`AuScParts::IsPathBlocked` first tests whether a move is inside the extents
of the hull parts (`HullExtents`, only `AuScHull` parts count) plus 2 ft;
outside that box nothing blocks, barriers included. A ramp's barrier that
sticks out past the hull side is therefore walked through unless a hull
part reaches out there too. shipgen's visible ramp is a hull part for that
reason (the user's test of Wedge v6, 2026-10-31).

### Transporter pads

From the user's working airport (`exported/buildngs/airport (teleporter).SoH`)
and the engine source. A transporter console (`ConsoleTransporter`) or wall
station (`StationTransporter`) carries:

* a list of **pad locations** (`PadLocs` / `SetPadLocs`, `AuScParts::
  SetTransporterPadLocs`, `auscparts.cpp:8106`), one point per pad, and
* a **pad effect number** (`WhichEffect` / `SetWhichEffect`, 0 to 9).

The pads themselves are ordinary **mesh** parts: a 3 x 3 ft slab 0.1 ft
thick on the floor, centred on the pad location, in the room's group, with
every face drawn with the surface effect *Transporter Pad N*, where N is the
console's effect number plus one. In the airport, console 264 (pad effect 1,
pads at (34.8, -90.6, 0.1) and (34.8, -93.8, 0.1)) owns meshes 262 and 263.
Each pad costs 3 electronic parts, 1 I/O tube and 1 plastic
(`ListComponents`). `shipgen parts` prints the pads of each transporter.

The generator's transporter stations have no pads yet.

## 4. Part types the engine source does not know

`whitestar-notele.SoH` in `~/Documents/designs` holds **88 parts**. The
engine source reads 33 of them and then stops at part 201, whose type is
**90**: one past `Part_BuildZone`, the last type in the published source (the
`main` and `maintenance/v7b2` branches of the upstream repository have the
same list). The 55 parts after it, among them all six hull parts, three doors
and 38 rooms, were silently lost, and the design read as having one empty
hull. That is why the file used to look "defective". It is not: it was saved
by a game build newer than the published source.

`src/sohfile.cpp` reads it properly:

* It walks the part list itself. At a part of an unknown type it searches
  forward for the next byte position where a record of a known type parses
  and is followed by another (or ends the list). In the White Star that is
  exactly one record of 43,097 bytes.
* The unknown record is kept byte for byte, and a hidden, omitted placeholder
  part (a `Sketch` with the group `shipgen:opaque-part`) holds its id so that
  nothing new gets that id.
* `SaveDesign` puts the original bytes back in place of the placeholder.
  `shipgen resave` uses this; the round trip is checked by `test.sh`.

All the tools load blueprints this way: `inspect`, `audit`, `validate`,
`resave`, `generate --hull`, and the graphical tool's open and save.

### What type 90 looks like

Not decoded completely. What can be seen in the White Star's record:

1. The usual part header: bits 0xF0, version 0x11, an empty architect, and a
   null group (`ffffffff`).
2. A block of 4x4 matrices of doubles and a few floats, in the shape of the
   design's own grid and view positions (a count, then entries).
3. The design's identity: an id, the name "White Star Mk.III" and the
   architect's name.
4. The names of the rooms ("Hold 1", "Engine Room 1", ... "Hall 1"), 46 of
   them.
5. For each deck, the outlines of the rooms as polygons on a **2-foot grid**:
   records that start with the design id and then hold, as single bytes,
   roughly a room index, the deck, a kind, a mirror flag and a point count,
   followed by the points as pairs of signed bytes (for example
   `00 06 3c 20 | 00 2c fd 05 01 00 04 | f8 1a 08 1a 08 18 f8 18`, a
   rectangle). The exact meaning of each byte is a guess. Port and starboard rooms come in
   mirrored pairs; the decks run from -3 to 2, which matches the six 8-foot
   hull slabs of the ship.

It looks like a deck plan of the rooms, such as a ship's interior map would
need. The White Star has **no room voids, hull voids or walk paths** in the
known part types at all, yet it walks well in the game, so the newest game
build probably works out the walkable space from this part, or from the rooms
themselves. Its missing hull void is real: the user confirmed that this is
why players get stuck in its door. The file was saved by the newest game
build. Its format is closely based on the published source, and partly on the
old pre-2016 ship designer; that build's source is not available. Until that is known, the generator keeps making
voids the way the published source wants them (section 5), and the engine's
report accepts them.

The White Star's own hull is six open shells, one per deck, not a closed
solid, so the generator will not put rooms into it (`generate --hull` refuses
it without `--allow-open-hull`).

## 5. Rooms, voids, doors and paths

This is the part of the format that decides whether a player can walk around
a ship, so it is worth being exact.

* A **room** is a `Room` mesh (type 5), which is what is drawn, and a
  `RoomVoid` (72), which is the air the crew can be in. They belong together
  by their **group name**, which is the room's name. A room may have several
  voids with the same group.
* A **void with no associated part is constant**: always there. A void
  associated with a part state (a door's `Open` state, a landing gear state,
  a lift) **comes and goes**: it only exists while that part is in that
  state. The engine says so in so many words:
  `AuScParts::ListRoomNames` (`auscparts.cpp:5996`) skips associated voids
  ("Skip voids that come and go"), and so do `RandomRoomLocation` and
  `VoidContainingLocConstant` (`auscparts.cpp:6877`); only
  `VoidContainingLocVariable` looks at them, against the current door states.
* A **door** (type 6) is a part with states (`Closed`, `Open`, and the steps
  between). The report (`audata/auscreport.cpp:1500-1560`) wants, for every
  door with an `Open` state, a room void associated with that state, and for
  a hull door, a `HullVoid` (71) associated with it as well. A void has one
  association slot.
* A **hull void** cuts the opening through the hull.
* **Walk paths** (73) and **ladder paths** (74) are segments the crew follow.
  A path can be associated with a door state too, and is then only usable
  while the door is in that state (`auscparts.cpp:6947`). Every path type
  inherits `MakePath(start, end)`; a ladder also has a facing (the way one
  faces while on it, `auscparts.h:11937`).
* In the working designs the paths lie **on the floor** (the Arnesh: paths
  and room voids both at z 4.6), and the way out of a hull door is a short
  walk path out of the doorway and then a **ladder path straight down** to
  2 to 3 feet below the bottom of the hull (the hulk: part 38, z -23.0 to
  -6.6 under a hull at -20.7; the scout: part 111), inside the constant hull
  void. Our paths used to float a foot above the floor (so they did not
  meet the ends of the deck ladders) and slope down to a foot above the hull
  bottom, and the doors started half a foot above the floor; in the game the
  user could only get in by flying, and pathing inside was odd. All three
  are fixed (`SHIPGEN_LADDER_BELOW_HULL` in `src/interior.h`).

So a room's own void must **not** be associated with a door, or the room is
only there while the door is open. The right shape, which `src/interior.h`
builds for both the generator and the designer, is:

| What | Parts |
|---|---|
| a room | a `Room` mesh and a constant `RoomVoid`, same group |
| an interior door | the `Door`, cut out of the two room walls with a door jig (section 5a), and a small doorway `RoomVoid` across the wall, reaching 2 feet into each room, associated with the door's `Open` state |
| a hull door | the `Door` (hull door bit), cut out of the room wall and the hull with a door jig; a doorway `RoomVoid` from 2 feet inside the room to 1 foot outside the hull and a `HullVoid` through the hull 1.5 feet bigger than the door on every side but the floor, both associated with the door's `Open` state; and a **constant** `HullVoid` that clears the outside in front of the door, from below the foot of the ladder up to above the door; the door is flush with the floor |
| a ladder | a constant `RoomVoid` shaft through the floor between the two rooms, grouped `Ladder <lower> to <upper>`, and a `PathLadder` |
| a hull door's floor | a thin `Hull` plate (group `Frame`) through the doorway, its top flush with the room floor, from the room wall to 0.35 ft past the hull surface; the constant hull void starts just beyond it, as the scout's does (0.9 ft out), so that it does not take the doorway's floor away |
| walk paths | on the floor: room middle to doorway, through the doorway (associated with the door), doorway to the next room's middle; for a hull door, out to 3 feet beyond the hull and a `PathLadder` from there down to 2.5 feet below the hull, facing the ship |

The generator before this session associated each **room's own void** with
one of its doors, so in the game every room only existed while that door was
open; it also laid its walk paths in straight lines from room to room,
through walls. Both are fixed. `shipgen inspect` now reports any room whose
only void is tied to a door ("Rooms that are only there while a door is
open"), and `test.sh` fails on it.

The constant hull void in front of a hull door is what the working designs
in `exported/ships/` have (hulk part 36, scout part 110, two on the Arnesh,
one by the Atlantis' door 155) and the White Star lacks; players get stuck in
the White Star's door. Side doors are preferred: the user finds belly doors
buggy in the current game build.

### Winding, and which way is fore

Measured as a right-handed signed volume (`shipgen parts` prints it), the
working designs all have **hull meshes negative, room meshes positive and
voids negative**: a hull is visible from outside, a room from inside. A hull
wound the other way is see-through from outside in the game; the generator
and the designer wound it that way until 30 October 2026, and
`src/interior.h` (`OrientAsHull`, `OrientAsRoom`, `OrientAsVoid`) now sets
it.

The game takes **+Y as fore** (`auscparts.h`, "Fore = +Y") and reports the
length along Y (`auscreport.cpp:1841`). The designer draws fore to the right
and turns the blueprint a quarter turn when it builds it; the generator turns
a model that is longer along X (`--keep-axes` leaves it). The engine's hull
volume, which the FTL drive and capacitor are sized from, changes a little
with the orientation, so the turn has to happen before the systems are
sized.

## 5a. Windows, and cutting openings with jigs

Windows are not drawn as glass on top of the hull; the engine's designer
cuts them. `AuScParts::JigCutParts` (`audata/auscparts.cpp:1800`) takes a
**jig** (a closed mesh that is always omitted from the finished design) and
one or two meshes to cut, a hull and a room:

1. The jig's shape is cut out of the hull and out of the room walls
   (`AuScFaceList::JigCut`, in AuMath).
2. The sides of the hole between the two are lined. The lining goes to the
   hull, "because it could be rendered without the interior. We don't want
   to see gaps".
3. A **WindowJig** (type 3) turns the hull faces it cut away into a
   **Window** part (type 7). Cutting a hull and a room gives one two-sided
   pane at the hull side, with opacity 0.25. Cutting a single mesh gives an
   opaque pane. Windows block walking (`AuScWindow::IsPathBlocked`).
4. A **DoorJig** (type 2) does the same for doors: the cut-away faces plus
   the lining become the closed state of an `AuScDoor`, with an Open state
   added.

In the working designs in `exported/ships/`, a window is a flat sheet
(signed volume 0) whose group is the room it looks out of: Arnesh part 14
`[bridge]` sits at the end of the bridge, 0.7 ft beyond the room wall; the
Atlantis has 45 of them. Their hulls are not closed: the window and door
holes are real holes in the hull mesh.

The designer cuts the windows the user places; the generator
(`PlaceWindows`, `src/generate.cpp`) cuts them where a room's outer wall is
close to the hull skin: a band round the bridge, one every 12 ft in
quarters, the galley, halls and the sick bay. When the room is some way
inside the hull, the frame `JigCut` puts round the opening reaches back to
the room wall and lines the tunnel.

## 6. Faces and paint

A face (`AuScFace`, `aumath/auscface.h:1308`) carries its own appearance:

| Field | Meaning |
|---|---|
| `m_TexIndex` | The texture slot, 0 to 15. |
| `m_Paint` | A colour with alpha; alpha 0 is no paint. |
| `m_ColorTheme` | One of `THEME_ExteriorPrimary/Secondary/Tertiary`, `THEME_InteriorPrimary/Secondary/Tertiary`, or `THEME_Count` for the plain `m_Paint` colour. The themes are the owner's empire colours. |
| `m_Opacity`, `m_Reflectivity`, `m_Shine`, `m_SelfIllumination`, `m_Effect` | |

So paint is per face (`AuScFace::SetPaint(QColor)` or
`SetPaint(theme, opacity)`), and so is the texture slot. The design's
palette has 16 slots, of which 14 are for the user (`SCTEXI_User0`,
`SCTEXI_User2`..`SCTEXI_User13`). `src/paint.cpp` gives each hull part a slot
and projects texture coordinates; the designer paints faces directly.

Faces wind the way `AuScFaceList::MakeExtrusion` makes them: for rooms, voids
and doors the normals point **into** the solid (checked with a probe). A hull
from a model winds outwards.

The engine drops a triangle whose corners are on one line when it is added to
a face list, so a mesh must not contain slivers, or the hull opens where the
sliver was.

## 7. Limits

* A ship may be 28 MB and a building 14 MB (packed).
* There is no face limit in the engine, but the game copes with about 15,000
  faces, is comfortable with about 5,000 and happy with about 2,000. The
  tools default to 5,000.
* The ground build size limit gives the warning "The design must be built in
  space" for big ships; it is a warning, not an error.

## 8. Looking at a file

```bash
./build/bin/shipgen inspect design.SoH   # parts, hull solidity, rooms, voids, doors, stations
./build/bin/shipgen audit   design.SoH   # every room box against the hull triangles, exactly
./build/bin/shipgen validate design.SoH  # the engine's own report: errors and warnings
./build/bin/shipgen resave in.SoH out.SoH  # read and write back, keeping unknown parts
```
