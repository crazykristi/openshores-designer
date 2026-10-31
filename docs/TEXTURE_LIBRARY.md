# The game's texture library

A texture palette slot of a blueprint (16 slots, `AuDesignTex`) holds either a
custom image or the id of a texture from the game's own library. The ids and
names below come from the engine source (`augl/autexturelib.h`,
`augl/augleffects.cpp`, `s_TextureLibrary`); the images themselves are Qt
resources (`:data/<name>.png`) inside the game's `libAuTexture.dll`. The live
game may have added textures after these.

`shipgen faces <file.SoH>` lists the palette of a blueprint and how its faces
use it.

| Id | Name | Id | Name | Id | Name |
|---|---|---|---|---|---|
| 0 | Construction Zone | 1 | Concrete Heavy | 2 | Tarmac |
| 3 | Dirt Smooth | 4 | Cobblestone Light | 5 | Dirt Cracked Smooth |
| 6 | Rock Tan | 7 | Snow | 8 | Ice Moderate |
| 9 | Concrete Dark Moderate | 10 | Dirt Coarse | 11 | Dirt Gray |
| 12 | Dirt Heavy | 13 | Dirt Moderate | 14 | Pebbles |
| 15 | Rock Brown | 16 | Rock Brown Moderate | 17 | Rock Gray Coarse |
| 18 | Rock Gray Heavy | 19 | Rock Gray Moderate | 20 | Rock Gray Smooth |
| 21 | Rock Red Heavy | 22 | Rock Red Moderate | 23 | Rock Red Smooth |
| 24 | Sand | 25 | Lava | 26 | Sea Water |
| 27 | Supercooled Liquid | 28 | Tile White Diagonal | 29 | Tile White Square |
| 30 | Black Round Dots | 31 | Black Round Dots Big | 32 | Black Techno Gothic Tile |
| 33 | Wall Blue White | 34 | Brick Light | 35 | Brick Wall |
| 36 | Gold | 37 | Ceiling Metal | 38 | Concrete |
| 39 | Wall Concrete | 40 | Copper Tubes | 41 | Dirt Cracked Moderate |
| 42 | Dirt Rocky | 43 | Wall Gypsum | 44 | Tile Metal |
| 45 | Grass | 46 | Ice Smooth | 47 | Logs |
| 48 | Logs Rough | 49 | Marble Green Gold | 50 | Marble |
| 51 | Tile White Black | 52 | Magmium | 53 | Ceiling Gold |
| 54 | Ceiling Silver | 55 | Vulcium | 56 | Grill Metal Holes |
| 57 | Plate Metal Box | 58 | Plate Metal Squares | 59 | Plate Metal X |
| 60 | Metal Scratch Bumps | 61 | Metal Scratch Rust | 62 | Metal Tread |
| 63 | Marble Obsidian | 64 | OSB | 65 | Popcorn Ceiling Heavy |
| 66 | Popcorn Ceiling Light | 67 | Wall Red White | 68 | Sea Dark |
| 69 | Sea Light | 70 | Sea Medium | 71 | Shingles Shake |
| 72 | Shingles Asphalt | 73 | Stained Glass Abstract | 74 | Stained Glass Angels |
| 75 | Stained Glass Animals | 76 | Stained Glass Deity | 77 | Stained Glass Laborers |
| 78 | Stained Glass Sun Moon | 79 | Tile Stone | 80 | Tile Green Tan Triangles |
| 81 | Stucco Light | 82 | Stucco Heavy | 83 | Marble Tan |
| 84 | Shingles Terra Cotta | 85 | Artwork Thomas Cole 1.Savage | 86 | Artwork Thomas Cole 2.Pastoral |
| 87 | Artwork Thomas Cole 3.Consummation | 88 | Artwork Thomas Cole 4.Destruction | 89 | Artwork Thomas Cole 5.Desolation |
| 90 | Tile Jade | 91 | Grill Black | 92 | Grill Copper |
| 93 | Grill White | 94 | Wall Cargo | 95 | Knockdown |
| 96 | Wall Metal | 97 | Wall Pipes | 98 | Wood Bare |
| 99 | Tile Wood | 100 | Wood Lattice | 101 | Wood Planks Beams |
| 102 | Wood Siding Painted | 103 | Wood Siding | 104 | Tile Wood Rustic |
| 105 | Wall Wood Studs | 106 | Metal Brushed | 107 | Grill Black Oblong Holes |
| 108 | Plate Magmium Box | 109 | Plate Magmium Squares | 110 | Plate Magmium X |
| 111 | Plate Vulcium Box | 112 | Plate Vulcium Squares | 113 | Plate Vulcium X |
| 114 | Decals and Signs | 115 | Fabric | 116 | Stone |
| 117 | Plate Adamantine Box | 118 | Plate Adamantine Squares | 119 | Plate Adamantine X |
| 120 | Chain Link Fence | 121 | Sand Bags | 122 | TEXLIB_Count |

## How the example blueprints use faces

Read with `shipgen faces` from the blueprints in `~/Documents/designs/`.

| Blueprint | Look | How |
|---|---|---|
| Aetherite Capitol | animated stone and gold | whole hull parts with library textures 38 (Concrete) and 53 (Ceiling Gold) and the surface effects *Capacitor* and *Power Plant* |
| Aetherite nuclear plant | the same | plus *Transporter Pad 2* |
| Mithra station | blue energy | paint `#5c6c7d` at alpha 170, effect *Shield*, vertex glow `#00aaff` |
| Ystola church | red glass with blue light | paint `#aa0000` at alpha 202, self-lit 0.26, shine 50, reflect 0.6, effect *Life Support*, glow `#00aaff` |
| Spacecraft factory | magenta energy | paint `#ff007f`, self-lit 1, shine 100, reflect 1, glow `#ff007f`; panes at opacity 0.83 |
| Laser base | dark glass | paint `#0d0d0d`, opacity 0.51, effect *Weapon 1* |
| Airport | lights | self-lit 1, opacity 0.29 to 0.89, glow `#0055ff`, `#ff5500`, `#ffffff` |

An energy field is therefore a face (or a thin two-sided sheet) with paint at
partial alpha, opacity below 1, self-illumination near 1, a vertex glow colour,
and one of the animated surface effects (`SURFACE_EFFECT` in
`aumath/auscface.h`: FTL drive 1-4, life support, maneuver drive, power plant,
sensor, shield, transporter pads 1-10, weapons 1-10, capacitor, hull FTL hot).
