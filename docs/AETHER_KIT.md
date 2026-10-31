# The Aether kit

A first set of blueprints in the style of the user's Aetherite Capitol: high
technology and magic together. Brass and copper machinery, gothic stone and
obsidian, crystal, and fields of light. `src/aether.h` / `src/aether.cpp`.

```bash
./build/bin/shipgen kit list                 # what there is
./build/bin/shipgen kit all --out kit/       # build every one
./build/bin/shipgen kit aether-spire --out kit/
```

| Key | Blueprint | Kind | What it is |
|---|---|---|---|
| `aether-skiff` | Aether Skiff | ship (patrol cutter) | A small courier: a dark gothic hull, brass bands, two sails of blue light, side wheels with ember hubs, crystal fins, a blue drive. |
| `brass-cruiser` | Brass Cruiser | ship (military frigate) | Magmium plate with a gothic bridge tower under a halo, copper pipes along the flanks, dark bronze paddle wheels set into the sides, a crystal aether core in a gold orbit and a violet halo, three ember drives, a golden ram. |
| `crystal-lancer` | Crystal Lancer | ship (science vessel) | A slim obsidian needle wreathed in three halos of light with gold rims, six crystal vanes round the stern, a jade-rimmed drive. |
| `aether-spire` | Aether Spire | building (capitol) | An octagonal stone hall under a gothic spire, a gold crown and cog with the Aetherite's *Capacitor* and *Power Plant* effects, a floating crystal in three tilted rings of light, buttresses with ember lamps, a curtain of light over the entrance. |
| `gear-works` | Gear Works | building (factory) | A rust-iron works hall with a copper cornice, an amber glass lantern roof under a faint disc of light, four copper stacks with glowing tops, great dark bronze and copper wheels on the ends, pipes along the front. |
| `crystal-conservatory` | Crystal Conservatory | building (residential) | A marble drum under a pale glass dome with an ember crystal on top, ringed with twelve crystals of light and a faint halo on the ground. |
| `orrery-observatory` | Orrery Observatory | building (office block) | An obsidian drum with a brass column carrying an orrery: three tilted gold rings with halos of light, and a jade, a crystal and a marble planet round an ember sun. |

All seven are accepted by the engine with 0 errors (warnings: the two big
ships must be built in space; the works has no citizen post). The 3D views
of our tools do not have the game's library textures, so the materials can
only be judged in the game.

## What the user saw in the game (2026-10-31)

* Aether Skiff: "looks awesome", and it can be built planetside.
* Crystal Lancer: "looks very interesting". Orrery Observatory: the
  animation "fits perfectly".
* Brass Cruiser: the wheels stuck out too far. They are now smaller (radius
  14 ft, was 18) and set into the flat sides of the hull, in the new *cog*
  look.
* Crystal Conservatory and Gear Works: the stained glass domes looked too
  chaotic, and the big fields of light too bright. The domes are now plain
  tinted glass (*glass-pale*, *glass*), and the ground ring and the roof
  disc use *aether-soft*. The Gear Works wheels were too smooth and bright:
  the big ones are now *cog*, the small ones copper.
* All ships wanted 3 to 4 times more maneuver drive; the generator now
  gives 3.5 times its share (`SHIPGEN_MANEUVER_BOOST` in `src/generate.cpp`).

These changes have not been looked at in the game yet.

## How the look is made

From the example blueprints (`docs/TEXTURE_LIBRARY.md`): every face has a
texture slot, a paint colour with an alpha, opacity, self-illumination,
shine, reflectivity, a vertex glow colour and one of the game's animated
surface effects. The kit's looks (`KitLookNamed`):

| Look | Slot (library texture) | Light and effect |
|---|---|---|
| brass | 0 (36 Gold) | shine 40, reflect 0.5 |
| gold-trim | 13 (53 Ceiling Gold) | shine 60, reflect 0.7 |
| copper | 2 (40 Copper Tubes) | shine 30 |
| grill | 11 (92 Grill Copper) | |
| stone | 9 (116 Stone) | |
| marble | 3 (49 Marble Green Gold) | shine 30 |
| obsidian | 4 (63 Marble Obsidian) | shine 80, reflect 0.6 |
| magmium | 5 (108 Plate Magmium Box) | shine 40 |
| gothic | 6 (32 Black Techno Gothic Tile) | |
| steel | 8 (106 Metal Brushed) | shine 50 |
| rust | 14 (61 Metal Scratch Rust) | |
| jade | 10 (90 Tile Jade) | shine 40 |
| cog | 7 (60 Metal Scratch Bumps) | brown paint at alpha 140, shine 12, reflect 0.15 (worn bronze for gears) |
| stained-sun | 12 (78 Stained Glass Sun Moon) | opacity 0.85, self-lit 0.5, warm glow (no longer used on domes) |
| glass, glass-pale | none | amber or pale blue paint at alpha 235, opacity 0.55 / 0.5, self-lit 0.25 / 0.2, shine 80 / 90 |
| aether | none | blue paint at alpha 140, opacity 0.45, self-lit 1, glow `#00aaff`, effect *Shield* |
| aether-soft | none | the same, fainter: alpha 70, opacity 0.25, self-lit 0.45, glow `#005a8c` (for big sheets) |
| arcane | none | violet paint at alpha 150, opacity 0.5, self-lit 1, glow `#b060ff`, effect *Life Support* |
| ember | none | orange paint, self-lit 1, glow `#ff5500`, effect *Power Plant* |
| crystal | none | pale blue paint at alpha 170, opacity 0.7, self-lit 0.6, shine 100, glow `#60d0ff`, effect *Sensor* |
| drive | none | blue paint, self-lit 1, effect *Maneuver Drive* |
| capacitor-gold, power-gold | 13 (Ceiling Gold) | effect *Capacitor* / *Power Plant*, as on the Aetherite |

Slot 1 is left alone: it is the construction material's texture.

## How a blueprint is put together

* **Shapes** (`KitBox`, `KitFrustum`, `KitFrustumY`, `KitTorus`, `KitGear`,
  `KitDome`, `KitCrystal`, and the thin slabs `KitHalo` and `KitSheet` for
  fields of light) are closed meshes, wound as hulls.
* **The body** is the hull the generators fill. A ship's body is lofted
  along Y (fore is +Y) and gets its rooms, doors, stations and systems from
  the generator (`GenerateLayout`, the template in the table); a building's
  body gets its site and parking from the building generator.
* **The ornaments** come after, so they do not change the layout. On a
  building they are mesh parts. On a ship they are hull parts, because the
  game wants every mesh part of a ship inside its hull ("Mesh is outside the
  hull"), and since they add to the hull volume the systems are sized again
  (`AllocateSystemVolumes`). The ship is moved so that the bottom of its hull
  is at z 0, which the game's report wants.

## Adding to it

A blueprint is a recipe in `ShipRecipes()` or `BuildingRecipes()` (a
template key, a body function and an ornaments function) plus an entry in
`KitItems()`. A new look is a branch in `KitLookNamed`, and a texture slot
not yet in `kPalette` can be given any library texture id.

## Open questions for the user

* Do the looks come out as meant in the game: the library textures, the
  surface effects, the glow and the see-through fields?
* Should ship ornaments be hull (solid, counted in the volume, as now) or is
  there a part type the game draws outside a ship without counting it?
* More of the kit: which kinds of buildings and ships come next, and should
  the triangle designer get these looks in its paint step?
