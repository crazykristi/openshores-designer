/*
 * styleguide.cpp
 *
 * See styleguide.h. The shapes and styles began as suggestions written for
 * an AI (building_types.json and styles.json); here each has the numbers and
 * tool calls a model needs to draw it, so different models make the same
 * kind of building from the same words.
 */
#include "styleguide.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace {

struct Shape
{
	const char* key;
	const char* look;			// What it looks like.
	int minDecks, maxDecks;		// Storeys of 12 ft.
	const char* footprint;		// In 5 ft cells.
	const char* templ;			// The game's building template closest to it.
	const char* recipe;			// How to draw it with the tools.
};

// Decks of a building are 12 ft and cells 5 ft (new_plan kind building).
const Shape kShapes[] = {
	{ "Apartment_Residential_block",
	  "A tall, narrow-to-wide tower or slab: stacked floors, repetitive vertical bands of windows, clean vertical lines, "
	  "few setbacks, little ornament on top.",
	  8, 20, "6 x 6 (tower) to 8 x 16 (slab)", "residential",
	  "draw_hull rect with deck 1 and toDeck the top deck, centred. Optionally one setback: the top 2 decks a cell "
	  "smaller each side. On every deck, rooms along the outer walls with add_window on each (length 2, one per room) "
	  "for the vertical bands; a Hall in the middle with stairs or ladders (connect_decks) up through every deck." },
	{ "Office_tower",
	  "Very tall and strongly vertical: a glassy, windowed skin with corporate proportions, a plain or tapered shaft, a "
	  "grid of uniform windows from base to crown.",
	  15, 30, "6 x 6 to 10 x 10", "office_block",
	  "draw_hull rect for the shaft on every deck. For a taper, every 5 decks draw the next band one cell smaller on "
	  "each side (erase the outer ring with draw_hull erase, or draw the smaller rect on decks that have none). Long "
	  "windows (add_window length 3-4) on every outer room of every deck make the grid; keep them the same on each "
	  "deck. A glass-like paint (see a style) on the whole tower." },
	{ "Factory_Industrial",
	  "Low, wide and sprawling: a heavy plinth, exposed bulk, few small windows, big utilitarian openings, a strong "
	  "horizontal emphasis.",
	  1, 3, "16 x 12 to 30 x 20", "factory",
	  "draw_hull rect on decks 1-2 for the hall; an office block of 6 x 6 cells on deck 3 at one end. A wide hull door "
	  "(add_door to outside, width 3, decksUp 1) on the long side. Few windows: one short add_window per office room. "
	  "Paint deck 1 as a darker plinth band (paint_hull deck 1)." },
	{ "Mall_Commercial_block",
	  "Wide and low-to-medium on a spread footprint, a flat or slightly stepped roofline, many entrances, repetitive "
	  "storefronts.",
	  2, 4, "16 x 16 to 30 x 20", "office_block",
	  "draw_hull rect on every deck; the top deck 1-2 cells smaller each side for the step. On deck 1, a row of small "
	  "rooms (4 x 4 cells) along each outer wall, each with its own door to outside and a window (the storefronts); a "
	  "Hall behind them joining all. Upper decks: larger rooms with long windows." },
	{ "Spire",
	  "A slender vertical needle that tapers hard towards a point or cap, with very little width.",
	  12, 30, "4 x 4 at the base, 2 x 2 or 1 x 1 at the top", "capitol",
	  "draw_hull ellipse (or rect) of 4 x 4 cells, centred, on the lower third of the decks; 3 x 3 on the middle "
	  "third; 2 x 2 above, and 1 x 1 on the top deck for the point (the hull is smoothed round it). Rooms only on the "
	  "lower decks, where they fit; a ladder up the middle (connect_decks)." },
	{ "Watchtower_Beacon",
	  "Slim and tall, sometimes tapered, with a distinct cap or light on top: a single strong vertical with a cap.",
	  6, 15, "3 x 3 to 5 x 5, the cap 1-2 cells wider", "command_post",
	  "draw_hull rect 4 x 4 centred on every deck but the top; the top deck 6 x 6 for the cap. A self-lit paint "
	  "(selfLit 1 with a glow colour) on the top deck only (paint_hull deck and toDeck the top deck) for the light. A "
	  "room in the cap with windows on every side." },
	{ "Cargo_Warehouse",
	  "Low, boxy and utilitarian on a wide deck footprint, with a sparse, functional surface.",
	  1, 2, "12 x 10 to 24 x 16", "warehouse",
	  "draw_hull rect on each deck. One big Hold room filling deck 1, a small office room with a window, wide hull "
	  "doors (width 3) on one long side. Plain paint, one darker band along deck 1." },
	{ "Research_Lab",
	  "Tall and segmented: modular bays stacked in clean geometric repetition.",
	  6, 14, "8 x 8 to 12 x 10", "office_block",
	  "Make it of modules of 3 decks each: draw_hull rect for decks 1-3, the next 3 decks shifted 1 cell along x (or 1 "
	  "cell narrower), and so on, so each module shows. Paint a thin contrasting band on the first deck of each module "
	  "(paint_hull deck N toDeck N). Same windows on every deck of a module." },
	{ "Religious_Temple",
	  "Tiered and stepped, tapering in stages, with decorative banding between the tiers.",
	  4, 10, "14 x 14 at the base, 2 cells smaller per tier", "capitol",
	  "Tiers of 2 decks: draw_hull rect 14 x 14 centred for decks 1-2, 10 x 10 for decks 3-4, 6 x 6 for 5-6, an "
	  "ellipse 3 x 3 on top. A band of a second paint on the top deck of each tier (paint_hull deck N toDeck N) for "
	  "the banding. A tall hall on deck 1 with a wide door (width 2, decksUp 1) at the front." },
	{ "Government_Civic_Citadel",
	  "Monumental, wide, blocky and symmetrical: an imposing base with a rising central mass.",
	  4, 12, "20 x 16 base, central mass 8 x 8", "capitol",
	  "draw_hull rect 20 x 16 centred on decks 1-3; the central mass 8 x 8 centred on every deck above. Keep every "
	  "room and window mirrored about the middle (same rooms port and starboard). A wide front door (width 3) in the "
	  "middle of the fore side." },
	{ "Police_Security_Station",
	  "Low-to-medium, fortified and angular, with heavy trim or crenellations.",
	  2, 5, "10 x 10 to 14 x 12", "command_post",
	  "draw_hull rect on every deck; hullStyle angular (set_settings) for flat, hard faces. Heavy trim: a darker band on "
	  "the top deck (paint_hull). Few, short windows. Crenellations are too small for whole cells: use the band." },
	{ "Hospital_Clinic",
	  "Wide and low-to-medium with many windows in a calm grid.",
	  3, 6, "16 x 10 to 24 x 14", "office_block",
	  "draw_hull rect on every deck. Rooms of the same size along both long walls on every deck, each with one "
	  "add_window length 2, a Hall down the middle; stairs at both ends (connect_decks kind stairs). Light, plain paint "
	  "(see Minimalist_Modernist)." },
	{ "Hotel_Resort",
	  "Tall with a wide base and many windows, sometimes a stepped or setback crown.",
	  8, 20, "base 16 x 12 (decks 1-2), tower 8 x 8 to 10 x 8", "residential",
	  "draw_hull rect 16 x 12 centred on decks 1-2 for the lobby base; the tower above it centred; the top 2-3 decks a "
	  "cell smaller each side for the setback crown. Rooms along the outer walls with a window each on every tower "
	  "deck." },
	{ "Energy_Power_plant",
	  "Low and heavy with cooling towers, domes or tall stacks: a massive base with tall appendages.",
	  2, 10, "base 16 x 12 (2 decks), stacks 2 x 2 to 4 x 4 up to the top deck", "power_plant",
	  "draw_hull rect 16 x 12 on decks 1-2. Two or three ellipses of 4 x 4 (cooling towers) or 2 x 2 (stacks) on every "
	  "deck above, set apart on the base. Rooms only in the base." },
	{ "Dock_Harbor_Wharf",
	  "Wide, low and horizontal, with jetties and quays jutting out.",
	  1, 2, "a 20 x 8 shed with 2-3 jetties 3 x 10 cells off one side", "warehouse",
	  "draw_hull rect for the shed on decks 1-2; the jetties as narrow rects on deck 1 only, reaching out from one long "
	  "side (leave cells between them). Holds in the shed, wide doors towards the jetties." },
	{ "Market_Bazaar",
	  "An irregular footprint of many small bays with pitched or peaked roofs and a busy silhouette.",
	  1, 3, "many bays of 3 x 3 to 5 x 5 over 16 x 16 to 24 x 20", "office_block",
	  "On deck 1, draw_hull many small rects of different sizes touching each other (not one big rect). On deck 2, a "
	  "smaller rect or a wedge over some bays only, for the peaks. A small room with its own door to outside in many "
	  "bays." },
	{ "Fortress_Bastion",
	  "Low, wide and heavily fortified, crenellated and blunt, with few soft edges.",
	  2, 4, "20 x 20 with 5 x 5 corner towers one deck higher", "barracks",
	  "draw_hull rect 20 x 20 centred on decks 1-2; four 5 x 5 rects at the corners on deck 3 (the towers); hullStyle "
	  "angular. Few windows; one strong door. A stone texture (see Fantasy_Medieval)." },
	{ "Mixed_use",
	  "A tall base with a rising residential spire: mixed massing, base and spire.",
	  8, 25, "base 16 x 16 (3-4 decks), tower 8 x 8", "residential",
	  "draw_hull rect 16 x 16 centred on decks 1-3 (shops: Mall_Commercial_block deck 1); the tower 8 x 8 centred above; "
	  "the top 2 decks 6 x 6. A different paint on the base and the tower." },
	{ "Dome_Rotunda",
	  "A wide base capped by a large dome or drum.",
	  3, 6, "16 x 16 to 24 x 24", "biodome",
	  "draw_hull ellipse 16 x 16 centred on decks 1-2 (the drum); then each deck above an ellipse 2-4 cells smaller "
	  "(12 x 12, 8 x 8, 4 x 4) for the dome. hullStyle smooth. Windows round the drum." },
};

struct Paint
{
	const char* role;			// What it goes on.
	const char* name;
	const char* colour;			// #rrggbb or 0.
	int texture;				// A library texture, -1 for none.
	double tint;				// Of the colour over the texture.
	double shine, reflect, selfLit;
	const char* glow;			// #rrggbb or 0.
	// A procedural texture (proctex.h) instead of 'texture'; 0 for none.
	const char* proc;			// Left out of the brace lists: 0, as are the rest.
	const char* colour2;
	const char* colour3;
	double wear;
	double lit;
	const char* wrap;			// 0: face (or facade for the storey kinds).
	double feet;				// textureFeet; 0 for the default.
};

struct Style
{
	const char* key;
	const char* look;
	const char* silhouette;
	const char* lighting;
	Paint paints[5];
	int count;
	const char* layOn;			// How the paints go on with paint_hull.
	const char* notYet;			// What the tools cannot make (yet).
};

// Glow and self-lit paints are steady: the tools have no flashing effects.
const Style kStyles[] = {
	{ "Cozy_neon",
	  "Warm, lived-in streets at night: brick and dark wood, warm amber windows, soft neon in pink, teal and amber "
	  "along the edges, small signs; inviting rather than threatening (the Nivalis look).",
	  "compact, layered, small setbacks", "warm amber light with soft pink and teal neon",
	  { { "base", "Warm brick", "#8a5a44", -1, 0, 10, .05, 0, 0, "brick", "#cbbfae", 0, .3 },
		{ "bands", "Dark wood trim", "#4a3226", -1, 0, 20, .05, 0, 0, "planks", "#20160f", 0, .2 },
		{ "neon", "Soft pink neon", "#ff6fb5", -1, 0, 200, .2, 1, "#ff6fb5" },
		{ "neon", "Teal neon", "#3fe0d0", -1, 0, 200, .2, 1, "#3fe0d0" },
		{ "windows", "Warm windows", "#6b4a3a", -1, 0, 60, .3, 0, 0, "windows", "#2a1d16", "#34485a", .25, .55, "facade", 10 } }, 5,
	  "Warm windows on the decks between deck 1 and the top (paint_hull deck 2 toDeck top-1). Base coat on everything (index 0). Dark wood on deck 1 (a shop-front band) and the top deck. Pink neon as a thin "
	  "band on the top deck's edge, teal neon on one vertical corner (paint_hull with x, y, sizeX 1, sizeY 1 over all "
	  "decks). Keep neon to a few lines: it should glow, not cover.",
	  "Signs and holo-ads need decals, which the tools do not place yet." },
	{ "Cyberpunk_Neon-noir",
	  "Sleek, rain-slicked chrome and black plating; electric neon trim (cyan, magenta, amber) glowing along edges and "
	  "signage; huge billboards; angular, high-contrast silhouettes; crowded, layered vertical density.",
	  "angular, high-contrast", "glowing neon accents and billboards",
	  { { "base", "Black plate", "#1a1b22", -1, 0, 300, .5, 0, 0, "panels", "#07070a", 0, .45 },
		{ "bands", "Chrome", "#c8ccd4", -1, 0, 800, .9, 0, 0 },
		{ "neon", "Cyan neon", "#00e5ff", -1, 0, 200, .2, 1, "#00e5ff" },
		{ "neon", "Magenta neon", "#ff2bd6", -1, 0, 200, .2, 1, "#ff2bd6" },
		{ "windows", "Night windows", "#1a1b22", -1, 0, 500, .6, 0, 0, "windows", "#07070a", "#1f3a4f", .4, .35, "facade", 8 } }, 5,
	  "Night windows on most decks (paint_hull deck 2 toDeck top-1). hullStyle angular. Black plate as the base coat; chrome bands every 4-5 decks; cyan neon on the vertical corners "
	  "(1 x 1 cell boxes over all decks), magenta on the top deck's edge.",
	  "Billboards and holo-ads need decals, which the tools do not place yet." },
	{ "Brutalist_Neo-brutalist",
	  "Raw, board-formed concrete; heavy, blunt masses; exposed structure; massive horizontal and vertical bands; a "
	  "sculptural, monumental weight.",
	  "blunt, monumental", "plain, shadow-heavy",
	  { { "base", "Raw concrete", "#9a9893", -1, 0, 2, 0, 0, 0, "concrete", "#5a5956", 0, .35 },
		{ "bands", "Dark concrete", "#66645f", -1, 0, 2, 0, 0, 0, "concrete", "#3a3936", 0, .5 } }, 2,
	  "hullStyle angular or classic. Raw concrete everywhere; dark concrete bands on every 3rd deck and on deck 1. "
	  "Deep setbacks and overhangs (a deck wider than the one below) give the weight.",
	  "" },
	{ "Chrome_High-tech",
	  "Polished metal and glass; exposed structure (piped columns, gantries); clean, precise geometry; a corporate, "
	  "engineered sheen with service ducts as ornament.",
	  "clean vertical, precise", "clean and restrained",
	  { { "base", "Polished steel", "#b9c0c8", -1, 0, 600, .8, 0, 0, "panels", "#7d848c", 0, .05 },
		{ "bands", "Glass facade", "#9aa3ab", -1, 0, 900, .9, 0, 0, "windows", "#5b636b", "#4a6a86", .05, .1, "facade", 6 },
		{ "accent", "Copper ducts", "#b87333", 40, .2, 300, .5, 0, 0 } }, 3,
	  "Steel base coat; blue glass on the decks between the first and the top (a band over most of the height); copper "
	  "ducts on one or two 1 x 1 cell vertical columns.",
	  "Gantries and piped columns are too fine for whole cells." },
	{ "Art_Deco_Streamline",
	  "Symmetrical, tapering profiles; stepped ziggurat setbacks; glossy geometric ornament; elegant verticality with "
	  "streamlined detailing.",
	  "tapering, symmetrical", "subtle, geometric",
	  { { "base", "Cream stone", "#e8dcc2", -1, 0, 80, .2, 0, 0, "stone", "#bfb39a", 0, .1 },
		{ "bands", "Gold trim", "#c9a24a", 36, .2, 500, .6, 0, 0 },
		{ "accent", "Black lacquer", "#1c1c1c", -1, 0, 600, .6, 0, 0 } }, 3,
	  "Symmetrical setbacks (every 3-4 decks a cell smaller on each side). Cream stone base; gold on the top deck of each "
	  "setback; black lacquer on deck 1.",
	  "" },
	{ "Gothic_Vertical",
	  "Reaching upward: tall spires and pinnacles, pointed vertical emphasis, intricate ornate detailing.",
	  "rising, pinnacle-heavy", "stained, dramatic",
	  { { "base", "Grey stone", "#7d7a74", -1, 0, 5, 0, 0, 0, "stone", "#4e4b46", 0, .35 },
		{ "bands", "Dark stone", "#55524d", -1, 0, 5, 0, 0, 0, "stone", "#2f2d2a", 0, .45 },
		{ "accent", "Stained glass", "#7a2a8c", -1, 0, 300, .3, .6, "#b04ad0" } }, 3,
	  "Spires and pinnacles as small rects or ellipses (1 x 1 to 2 x 2) on the upper decks at the corners. Grey stone "
	  "base; dark stone on deck 1 and the tiers' tops; stained glass for a band of the decks with tall windows.",
	  "Carved ornament is too fine for whole cells." },
	{ "Steampunk_Victorian-industrial",
	  "Ornate ironwork and riveted metal; brass and copper accents; exposed gears and pipes; warm amber light; "
	  "cluttered, mechanical richness.",
	  "cluttered, mechanical", "warm amber",
	  { { "base", "Riveted iron", "#4b4a48", -1, 0, 100, .3, 0, 0, "panels", "#2a2928", 0, .4 },
		{ "bands", "Brass", "#b5893b", 36, .3, 400, .6, 0, 0 },
		{ "accent", "Copper pipes", "#b87333", 40, .2, 300, .5, 0, 0 },
		{ "neon", "Amber lamps", "#ffb347", -1, 0, 100, .1, 1, "#ffb347" } }, 4,
	  "Iron base; brass bands on deck 1 and the top deck; copper pipes on vertical 1 x 1 columns; amber lamps on a band "
	  "where the windows are. The Aether kit (shipgen kit) has examples of this look.",
	  "Gears are too fine for whole cells." },
	{ "Solarpunk_Eco-futurist",
	  "Bright, hopeful, organic forms; greenery and living walls; glass and light materials; humane, sunlit proportions.",
	  "flowing, organic", "bright, sunlit",
	  { { "base", "White render", "#f2f2ee", -1, 0, 60, .2, 0, 0, "plaster", "#d6d6d0", 0, .05 },
		{ "bands", "Living wall", "#4f8a3c", 45, .3, 5, 0, 0, 0 },
		{ "accent", "Ribbon windows", "#f2f2ee", -1, 0, 800, .7, 0, 0, "strip_windows", "#9aa39a", "#a8d8e8", .05, .2, "facade", 12 } }, 3,
	  "hullStyle smooth; terraces (each deck a cell smaller on one side). White base; grass on the terraces' tops "
	  "(paint_hull with awayFromEdge 1 on each deck) and on one band; light glass on the window decks.",
	  "Trees and planters are not something the tools place." },
	{ "Minimalist_Modernist",
	  "Clean lines, flat planes, little ornament; honest materials; a calm, restrained palette; functional surfaces.",
	  "flat planes, clean", "plain",
	  { { "base", "Off-white", "#e9e7e2", -1, 0, 20, .05, 0, 0 },
		{ "bands", "Grey", "#8d8f92", -1, 0, 20, .05, 0, 0 },
		{ "windows", "Ribbon windows", "#e9e7e2", -1, 0, 300, .4, 0, 0, "strip_windows", "#55585c", "#5d7a8f", .05, .15, "facade", 12 } }, 3,
	  "hullStyle classic or angular. Off-white everywhere; grey only on deck 1 or one band; ribbon windows on the upper decks.", "" },
	{ "Industrial_decay_Rust",
	  "Weathered, corroded metal; peeling paint; grime and wear; patched surfaces; gritty and lived-in.",
	  "weathered, patched", "dim, grimy",
	  { { "base", "Rusted sheet", "#7a4a2e", -1, 0, 10, .05, 0, 0, "corrugated", 0, 0, .8 },
		{ "bands", "Grimy steel", "#5a5853", -1, 0, 40, .1, 0, 0, "panels", "#34332f", 0, .75 },
		{ "accent", "Faded paint", "#6f7f6a", -1, 0, 5, 0, 0, 0 } }, 3,
	  "Patches: paint_hull boxes of 2-5 cells in the accent and band paints at uneven places over the rust base, not "
	  "neat bands.", "" },
	{ "Gritty_street-level",
	  "Dense, cluttered, chaotic signage; layered textures; mismatched materials; a worn, urban atmosphere.",
	  "cluttered, layered", "mixed neon, worn",
	  { { "base", "Worn concrete", "#77746e", -1, 0, 5, 0, 0, 0, "concrete", "#4a4844", 0, .7 },
		{ "bands", "Old brick", "#7b4b3a", -1, 0, 5, 0, 0, 0, "brick", "#9d958a", 0, .6 },
		{ "accent", "Tile", "#c9c9c4", -1, 0, 40, .1, 0, 0, "tiles", "#7d7a74", 0, .5 },
		{ "neon", "Worn neon", "#ff9a3c", -1, 0, 100, .1, .8, "#ff9a3c" } }, 4,
	  "Mismatched: a different paint on each 1-2 deck band and on the lower deck's boxes; one or two small neon boxes.",
	  "Signage needs decals, which the tools do not place yet." },
	{ "Corporate_Brutal_corporate",
	  "Imposing and impersonal: massive glass-and-concrete blocks, understated luxury, cold and powerful scale.",
	  "massive, imposing", "cool, understated",
	  { { "base", "Smooth concrete", "#a4a5a6", -1, 0, 20, .05, 0, 0, "concrete", "#6c6d6e", 0, .1 },
		{ "bands", "Dark glass facade", "#3a3d40", -1, 0, 900, .9, 0, 0, "windows", "#15181b", "#202a33", .05, .15, "facade", 6 } }, 2,
	  "Concrete base; dark glass on most decks between deck 2 and the top, so the building reads as glass framed by "
	  "concrete.", "" },
	{ "Fantasy_Medieval",
	  "Stone, battlements, turrets and pitched roofs; a hand-built, ancient, organic feel.",
	  "crenellated, turret-heavy", "warm, torch-lit",
	  { { "base", "Old stone", "#8b857a", -1, 0, 5, 0, 0, 0, "stone", "#5d5850", 0, .4 },
		{ "bands", "Timber", "#5b4030", -1, 0, 5, 0, 0, 0, "planks", "#2e2017", 0, .4 },
		{ "roof", "Roof slate", "#4a4f57", -1, 0, 10, 0, 0, 0, "tiles", "#2c3036", 0, .3 } }, 3,
	  "Turrets as 3 x 3 ellipses at the corners, a deck higher; stone base, timber on an upper band, slate on the top "
	  "deck (paint_hull with awayFromEdge 1 for the roof only).",
	  "Battlements are too small for whole cells: use the band." },
	{ "Sci-fi_utilitarian_Military",
	  "Functional, modular and repetitive; utilitarian greys and greens; heavy armour plating; mass-produced.",
	  "modular, repetitive", "plain, functional",
	  { { "base", "Armour grey", "#6b6f6a", -1, 0, 80, .2, 0, 0, "panels", "#3c3f3b", 0, .3 },
		{ "bands", "Olive", "#556040", -1, 0, 30, .1, 0, 0 },
		{ "accent", "Hazard", "#d9b62b", -1, 0, 30, .1, 0, 0, "hazard", "#1d1d1d", 0, .3 } }, 3,
	  "hullStyle angular. Grey base; olive bands per module of decks; hazard stripes only round doors (small boxes on "
	  "deck 1).", "" },
	{ "Organic_Biomimetic",
	  "Curved, flowing, natural forms; shell- or bone-like structures; smooth surfaces that seem grown, not built.",
	  "curved, flowing", "soft, diffused",
	  { { "base", "Shell", "#e6ddcf", -1, 0, 150, .3, 0, 0, "plaster", "#cbbfa8", 0, .05 },
		{ "bands", "Bone", "#cbbfa8", -1, 0, 60, .1, 0, 0 },
		{ "neon", "Soft glow", "#bfe8ff", -1, 0, 100, .1, .4, "#bfe8ff" } }, 3,
	  "hullStyle smooth with high smoothing (set_settings smoothing 8-10); ellipses on every deck, their sizes changing "
	  "a little from deck to deck. Shell base, bone bands, a soft glow band.", "" },
};

QString PaintArgs(const Paint& p)
{
	QJsonObject o;
	o["name"] = QString::fromLatin1(p.name);
	if(p.colour)
		o["colour"] = QString::fromLatin1(p.colour);
	if(p.proc)
	{
		o["procedural"] = QString::fromLatin1(p.proc);
		if(p.colour2)
			o["colour2"] = QString::fromLatin1(p.colour2);
		if(p.colour3)
			o["colour3"] = QString::fromLatin1(p.colour3);
		o["wear"] = p.wear;
		if(p.lit > 0)
			o["lit"] = p.lit;
		if(p.wrap)
			o["wrap"] = QString::fromLatin1(p.wrap);
		if(p.feet > 0)
			o["textureFeet"] = p.feet;
	}
	else if(p.texture >= 0)
	{
		o["texture"] = QString::number(p.texture);
		o["tint"] = p.tint;
	}
	o["shine"] = p.shine;
	o["reflect"] = p.reflect;
	if(p.selfLit > 0)
		o["selfLit"] = p.selfLit;
	if(p.glow)
		o["glow"] = QString::fromLatin1(p.glow);
	return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace

QStringList StyleGuideShapes()
{
	QStringList keys;
	for(const Shape& s : kShapes)
		keys << s.key;
	return keys;
}

QStringList StyleGuideStyles()
{
	QStringList keys;
	for(const Style& s : kStyles)
		keys << s.key;
	return keys;
}

QString StyleGuideText(const QString& shape, const QString& style, QString& error)
{
	error.clear();
	QStringList out;
	if(shape.isEmpty() && style.isEmpty())
	{
		out << "Building shapes (style_guide shape=<key> for the numbers and how to draw it):";
		for(const Shape& s : kShapes)
			out << QString("  %1: %2 decks, template %3").arg(s.key).arg(s.minDecks == s.maxDecks
					? QString::number(s.minDecks) : QString("%1-%2").arg(s.minDecks).arg(s.maxDecks)).arg(s.templ);
		out << "" << "Styles (style_guide style=<key> for the palette and how to lay it on):";
		for(const Style& s : kStyles)
			out << QString("  %1: %2").arg(s.key, s.silhouette);
		out << "" << "Ask for a shape and a style together to get both.";
		return out.join("\n");
	}
	if(!shape.isEmpty())
	{
		const Shape* found = 0;
		for(const Shape& s : kShapes)
			if(shape.compare(s.key, Qt::CaseInsensitive) == 0)
				found = &s;
		if(!found)
		{
			error = "There is no shape \"" + shape + "\". The shapes: " + StyleGuideShapes().join(", ") + ".";
			return QString();
		}
		out << QString("%1").arg(found->key)
			<< QString("Looks: %1").arg(found->look)
			<< QString("Size: %1-%2 decks of 12 ft (%3-%4 ft high); footprint %5 cells of 5 ft.")
					.arg(found->minDecks).arg(found->maxDecks).arg(found->minDecks * 12).arg(found->maxDecks * 12)
					.arg(found->footprint)
			<< QString("Game template: %1 (new_plan kind building, template %1). The template decides what the "
					   "building does in the game; the shape only how it looks.").arg(found->templ)
			<< QString("How to draw it: %1").arg(found->recipe)
			<< "Leave a free ring of cells round the hull for the site (the build plot), and keep the hull within the "
			   "face budget: build and read the report after the hull is drawn, before the rooms.";
	}
	if(!style.isEmpty())
	{
		const Style* found = 0;
		for(const Style& s : kStyles)
			if(style.compare(s.key, Qt::CaseInsensitive) == 0)
				found = &s;
		if(!found)
		{
			error = "There is no style \"" + style + "\". The styles: " + StyleGuideStyles().join(", ") + ".";
			return QString();
		}
		if(!out.isEmpty())
			out << "";
		out << QString("%1").arg(found->key)
			<< QString("Looks: %1").arg(found->look)
			<< QString("Silhouette: %1. Lighting: %2.").arg(found->silhouette, found->lighting)
			<< "Palette (one set_paint call each; the first with index 0, the base coat):";
		for(int i = 0; i < found->count; ++i)
		{
			const Paint& p = found->paints[i];
			QString args = PaintArgs(p);
			if(i == 0)
				args.insert(1, "\"index\":0,");
			out << QString("  %1: set_paint %2").arg(p.role, args);
		}
		out << QString("How to lay it on: %1").arg(found->layOn);
		out << "Procedural textures are made for the design and line up: with wrap facade one repeat is one deck, so a "
			   "windows paint gives one row of windows on each deck it is painted on.";
		out << "Self-lit and glowing paints shine steadily; there are no flashing effects.";
		if(*found->notYet)
			out << QString("Not possible with the tools yet: %1").arg(found->notYet);
	}
	return out.join("\n");
}
