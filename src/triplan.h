//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - a ship drawn with triangles.
//
// The plan is a grid of square cells seen from above, one grid per deck. Each
// cell is split along its diagonals into four triangles that meet in its
// middle, so four triangles make a square and two make a 45 degree slope:
//
//        tl ------- tr          x runs fore (+x) and aft (-x),
//         | \  N  / |           y runs port (+y) and starboard (-y),
//         |  \   /  |           z runs up, one deck at a time.
//         | W  c  E |
//         |  /   \  |
//         | /  S  \ |
//        bl ------- br
//
// The drawing goes in three passes, all on the same triangles:
//
//   1. the hull: which triangles of each deck are inside the ship;
//   2. the rooms: which hull triangles belong to which room, and the doors,
//      which sit on the edge between two cells;
//   3. the paint: the colour of the hull over each triangle.
//
// planbuild.h turns a plan into a blueprint: the triangles become a closed,
// smoothed hull, the rooms become room meshes with their voids, the doors get
// their doorway voids, and the rest is furnished by the generator.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_TRIPLAN_H
#define SHIPGEN_TRIPLAN_H

#include <QColor>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

// The four triangles of a cell, named after the cell edge they stand on.
enum TriSide
{
	TRI_S = 0,		// On the -y edge.
	TRI_E = 1,		// On the +x edge.
	TRI_N = 2,		// On the +y edge.
	TRI_W = 3		// On the -x edge.
};

// One triangle of the plan.
struct TriRef
{
	int		deck = 0;
	int		cx = 0;
	int		cy = 0;
	int		k = 0;		// TriSide.

	TriRef() {}
	TriRef(int deck_, int cx_, int cy_, int k_) : deck(deck_), cx(cx_), cy(cy_), k(k_) {}
	bool operator==(const TriRef& o) const { return deck == o.deck && cx == o.cx && cy == o.cy && k == o.k; }
};

struct PlanRoom
{
	QString	type;			// An engine room type, such as "Bridge" (names.h, SpacecraftRoomTypes()).
	QString	label;			// The room's name in the design, such as "Bridge 1".
	QColor	colour;			// How it is drawn in the editor.
	// Textures from the game's library (InteriorTextures in names.h), or -1
	// for the usual look.
	int		wallTex = -1;
	int		floorTex = -1;
	int		ceilingTex = -1;
	// A turbolift's car (IsLiftRoomType): how high it is inside, in feet; 0
	// for the deck's room height. Taller for vehicles; the cells above its
	// highest stop must then be drawn as the lift too.
	double	carFeet = 0;
	// Who may be in it, as the game's PERSONNEL_ACCESS bits: 1 officers
	// (captain and officers), 2 crew, 4 passengers, 8 troops; 15 everyone.
	int		access = 15;
};

// A door on the edge of a cell. The edge is the one triangle 'side' of the
// cell stands on, so a door joins that triangle and the one across the edge.
struct PlanDoor
{
	int		deck = 0;
	int		cx = 0;
	int		cy = 0;
	int		side = TRI_S;
	// Windows only: 0 for the cell edge 'side', 1 or 2 for one of the
	// diagonals of triangle 'side' (TriPlan::Across), where a room's wall runs
	// across a cell.
	int		edge = 0;
	// Doors: neighbours on one wall with the same row are one wide door; a
	// dragged row shares a number, a single click gets its own. 0 is for plans
	// from before rows existed: every neighbour with 0 joins. Windows: a
	// dragged run shares a number and is joined into one window (straight
	// stretches, and round corners one flat pane), as joinWindows joins all
	// windows; 0 joins only with joinWindows.
	int		row = 0;
	int		tex = -1;		// A texture from the game's library, or -1 for the usual look.
	// Hull doors only; a row takes these from the door in its middle, and the
	// designer sets them for a whole row. The way down: empty for the ship's
	// hullDoorExit, or "auto", "ladder", "ramp" (up to SHIPGEN_RAMP_LONGEST_RISE,
	// where "auto" stops at SHIPGEN_RAMP_MAX_RISE) or "none" (a docking door).
	QString	exit;
	// (Plans from 2 to 6 November 2026 may have "exitOpenOnly", a way down tied
	// to the door's state. It did not work in the game and is ignored.)
	// Feet of ramp for each foot it falls; 0 for the usual SHIPGEN_RAMP_RUN_PER_RISE.
	double	rampRun = 0;
	// Doors only: what is in the opening (tracks 8 and 9). Empty for a door;
	// "window" for a window between two rooms (a pane from the plan's sill to
	// its top), "balcony" for an opening above a parapet a third of the
	// room's height. Both only between two rooms; nobody walks through them.
	QString	kind;
	// Doors only: buttons that open and close it (the user, 2026-11-06), at
	// most two for a row (one each side), in eighths of a cell from the grid's
	// corner 0,0 (fractions allowed since 8 November 2026; before, each was an
	// eighth of a cell in from the middle of a wall edge). Inside, a point a
	// little in from a wall of one of the door's rooms: the triangle it lies
	// in says on which side of the wall the button is, and the button goes
	// where the point is along the wall (TriPlan::ButtonWallSpot). For a hull
	// door also outside, a point beyond the door's wall line: the button goes
	// on the hull surface, there along the wall. 4 ft above the floor.
	QList<QPointF> buttons;
	// Wall windows only: how this one is made. "" as the plan's windowStyle,
	// "frame" cut with a window jig (a frame round it), "hull" the hull's own
	// triangles become the glass (follows the hull).
	QString	style;
	bool	skip = false;		// Kept in the plan but left out of the build (Build page).
};

// A ladder up from a cell of one deck into the same cell of the deck above.
// The cell has to be in a room on both decks.
struct PlanLadder
{
	int		deck = 0;		// The lower deck.
	int		cx = 0;
	int		cy = 0;
	bool	skip = false;		// Kept in the plan but left out of the build (Build page).
};

// A window in the ceiling of a cell of a room, straight out through the hull
// above it (experimental). No room may be above it, on any deck. 'floor' is the
// same in the floor: it is what a ceiling window becomes when a plan is turned
// upside down. 'k' is one triangle of the cell (a TriSide), or -1 for the
// whole cell. 'shape' is "hull" (the glass is the hull's own surface above
// it), "flat" (a level pane in a cutout) or "" (the plan's windowStyle).
struct PlanSkylight
{
	int		deck = 0;
	int		cx = 0;
	int		cy = 0;
	int		k = -1;
	bool	floor = false;
	QString	shape;			// "", "hull", "flat" or "canopy" (glass from the sill up, the walls under it opened).
	bool	skip = false;		// Kept in the plan but left out of the build (Build page).
};

// A flight of stairs up from one deck to the deck above. It starts at cell
// (cx, cy) of the lower deck and climbs towards the cell edge 'side' (a
// TriSide: TRI_E climbs along +x), over TriPlan::StairCells() cells, onto
// the landing: the next cell, on the deck above. The run has to be in one
// room on the lower deck, and the run and the landing in one room on the
// deck above (the opening is cut through that room's floor, with a rail
// round it).
struct PlanStair
{
	int		deck = 0;		// The lower deck.
	int		cx = 0;
	int		cy = 0;
	int		side = TRI_E;
	bool	skip = false;		// Kept in the plan but left out of the build (Build page).
};

// A vehicle parking spot on the floor of a room, centred on a cell corner
// (the corner at the cell's low x and y). The game's spots have fixed sizes:
// a ground vehicle 6 by 12 m, a space vehicle 12 by 17 m. The long side is
// along the plan's x, which is fore and aft in the game, unless 'across'. The room needs a bay door (doors on
// neighbouring edges of one wall make one wide door) to get vehicles out.
struct PlanParking
{
	int		deck = 0;
	int		cx = 0;
	int		cy = 0;
	bool	space = false;		// A space vehicle; a ground vehicle otherwise.
	bool	across = false;
	// A landing spot for a whole spacecraft (AuScSpacecraftParkingSpot): a
	// square 238 ft each way (twice GroundSpawnSizeLimit), on a deck or on
	// top of the hull. It does not have to lie in one room; it only must not
	// overlap another landing spot on the same deck.
	bool	spacecraft = false;
	bool	skip = false;		// Kept in the plan but left out of the build (Build page).
};

// An emitter on the hull, put in from a hull triangle of a deck (seen from
// above, it stands at mid-deck height over the triangle's middle):
//
//  * a contrail emitter, the trail the ship leaves behind it in the game: on
//    the hull straight aft of the triangle, facing aft; 'sizeFeet' is the
//    trail's width (the game's own is 2 m);
//  * a weapon emitter, where the ship's guns fire from: on the hull nearest
//    the triangle. The game keeps one on each fire control station, so the
//    first goes to the first station, and so on (planbuild.cpp).
struct PlanEmitter
{
	int		deck = 0;
	int		cx = 0;
	int		cy = 0;
	int		k = 0;				// TriSide.
	bool	weapon = false;		// A weapon emitter; a contrail emitter otherwise.
	// A weapon emitter's bay: the module its own fire control station runs
	// (FireControlModules in names.h); 0 for the plan's fireControlModule. Each
	// weapon emitter is a weapon bay of its own (the user, 2026-11-06).
	int		module = 0;
	// A marking instead of an emitter: 1 the owner's empire flag
	// (AuScEmpireFlag), 2 the empire's name (AuScEmpireName), flat on the
	// nearest point of the hull, sizeFeet wide (a flag 2/3 as high, a name
	// 1/4). The game draws the owner's flag or name on it.
	int		marking = 0;
	double	sizeFeet = 6.5616798;	// 2 m.
	bool	skip = false;		// Kept in the plan but left out of the build (Build page).
};

// A colour in the paint palette: a plain colour, or one of the owner's
// empire colours (AuScFace::THEME_ExteriorPrimary and the two after it).
struct PlanPaint
{
	QColor	colour = QColor(150, 155, 165);
	int		theme = -1;		// 0, 1 or 2 for an empire colour; -1 for the plain colour.
	QString	name;
	// A livery colour (a painted pattern to keep sharp): on a hull shaped from
	// a model or angular, the hull is cut along its drawn outline, so the
	// colour edge is where it was drawn, not stepped along the hull's own
	// triangles. Costs faces.
	bool	livery = false;
	// The finish, as the game's own designer sets it on faces (Transparency,
	// Self Illumination, Polish, Glow and the surface effects).
	float	opacity = 1.f;		// 1 opaque, 0 invisible.
	float	selfLit = 0.f;		// 0 lit by the world, 1 fully self-lit.
	float	shine = 0.f;		// Specular exponent, 0 and up (the game suggests up to 1000).
	float	reflect = 0.f;		// Specular intensity, 0 to 1.
	QColor	glow;				// Added light at every corner; invalid or black for none.
	int		effect = 0;			// SURFACE_EFFECT (aumath/auscface.h); 0 for none.

	bool	HasFinish() const
	{
		return opacity < 1.f || selfLit > 0.f || shine > 0.f || reflect > 0.f || HasGlow() || effect != 0;
	}
	bool	HasGlow() const { return glow.isValid() && (glow.red() || glow.green() || glow.blue()); }
};

// The 3D model a plan was made from, and how it was placed: turned (y up
// to z up, a quarter turn about z, a half turn), moved so that 'centre' is
// at the origin, and scaled. With 'asHull' the hull is the model itself
// (with the drawn hull triangles added), not the triangles alone
// (planbuild.cpp, ModelHullMesh).
struct PlanModel
{
	QString	file;				// Absolute path.
	bool	yUp = false;
	bool	turn = false;		// A quarter turn about z (the model was longer in y).
	bool	flip = false;		// A half turn about z.
	double	centre[3] = { 0, 0, 0 };
	double	scale = 1;
	double	sealFeet = 1;
	bool	asHull = false;
	// How the model is made the hull (asHull): sharp keeps its edges sharp
	// and its flat faces flat; else it is smoothed (rounded, as before).
	bool	sharp = false;
	// Flipped with the plan after it was placed (TriPlan::Flip): x, y or z
	// mirrored about the grid's centre (z about half of 'height').
	bool	mirrorX = false;
	bool	mirrorY = false;
	bool	mirrorZ = false;
	double	height = 0;			// The decks' height when it was turned upside down.
	// Quarter turns clockwise (seen from above) about the grid's middle, after
	// the mirrors (TriPlan::Rotate): 0 to 3.
	int		quarterTurns = 0;

	bool	IsSet() const { return !file.isEmpty(); }
};

// Placed by hand on the Crew page (since 8 November 2026); the build puts in
// what is still missing by itself (TriPlan::autoStations and the rest).
// Points are in cells from the grid's corner 0,0 (as a door's buttons are in
// eighths), so they move with the grid; 'yaw' is the way the operator or the
// berth faces, in degrees from fore (+x) towards port (+y).
struct PlanStation
{
	int		deck = 0;
	double	x = 0;
	double	y = 0;
	double	yaw = 0;
	QString	type;			// A station of the template: "helm", "sensor", "engineer", "firecontrol", ... (StationChoices).
	QString	seat;			// "chair" (the game's swivel chair), "desk" (an integrated desk), "" the ship's bridge style.
	int		module = 0;		// Fire control only: the weapon bay it runs; 0 for the ship's.
	bool	skip = false;
};

// A berth (bed) placed by hand: "crew", "officer", "capt", "passenger" or "troop".
struct PlanBerth
{
	int		deck = 0;
	double	x = 0;
	double	y = 0;
	double	yaw = 0;
	QString	kind = "crew";
	bool	skip = false;
};

// A transporter pad placed by hand (3 ft square), for the transporter station
// in the same room. A room with pads placed by hand gets no others.
struct PlanPad
{
	int		deck = 0;
	double	x = 0;
	double	y = 0;
	bool	skip = false;
};

// A walk path drawn by hand: straight from the middle of one triangle to the
// middle of another, on the floor of one room (the automatic paths run
// through triangle middles too, so they meet).
struct PlanPath
{
	int		deck = 0;
	int		ax = 0, ay = 0, ak = 0;
	int		bx = 0, by = 0, bk = 0;
	bool	skip = false;
};

class TriPlan
{
public:
	TriPlan();

	// The whole plan.
	QString	name = "Drawn ship";
	QString	templateKey = "military_frigate";	// What the furnishing follows (shipgen.h, AllTemplates()).
	QString	ftlDrive = "wormhole-p";			// The FTL drive (Brief::ftlDrive).
	// Stations of the template left out, and what goes with them:
	// "shield" (and the shields), "firecontrol" (and the weapon bay),
	// "transporter".
	QStringList	leaveOut;
	int		fireControlModule = 252;	// What the fire control station runs (FireControlModules in names.h).
	double	cellFeet = 5.0;			// The side of a cell.
	double	deckFeet = 10.0;			// From one deck floor to the next.
	double	floorFeet = 1.0;			// The structure between a room and the deck above or below it.
	double	skinFeet = 2.0;			// How far the hull surface stands out from the drawn triangles.
	int		smoothing = 6;			// How much the hull edges are rounded, 0 for none.
	int		panels = 0;				// A hull of about this many flat panels (planbuild.cpp, PanelPartition); 0: smooth.
	PlanModel	model;				// The 3D model it was made from, if any.
	int		maxFaces = 5000;		// The face budget of the hull.
	// The hull between decks: false, it steps from one deck's outline to the
	// next; true, it slopes (the ledge rises to the top of the narrower deck
	// above, an overhang falls to the bottom of the deck below). Either way
	// the drawn triangles are the least the hull may be.
	bool	slopedDecks = false;
	double	windowSillFeet = 3.0;		// Windows: from this high above the floor
	double	windowTopFeet = 7.5;		// up to this high (less, in a low room).
	QString	bridgeStyle = "hazeron";	// How stations look (BridgeStyles in names.h).
	bool	deskStanding = false;		// Integrated desks: the operator stands, with no stool.
	double	deskHeightFeet = 0;			// Their top edge above the floor. 0: the usual height for sitting or standing.
	// The height of the desks' top edge, within what suits sitting or standing.
	double DeskTopFeet() const
	{
		if(deskStanding)
			return deskHeightFeet > 0 ? qBound(2.8, deskHeightFeet, 5.5) : 4.2;
		return deskHeightFeet > 0 ? qBound(2.2, deskHeightFeet, 4.5) : 3.0;
	}
	// Textures from the game's library for the whole ship, for every room and
	// door that does not ask for one of its own (-1: none, the usual look).
	int		wallTex = -1;
	int		floorTex = -1;
	int		ceilingTex = -1;
	int		doorTex = -1;
	bool	joinWindows = false;		// Windows side by side on one straight wall make one long window.
	bool	flatWindows = true;			// Joined windows along a zigzag wall are one flat pane, not one per tooth.
	// How wall windows are made: "" cut with a window jig (a box through the
	// hull, a frame round it), or "hull": the hull's own triangles inside the
	// window's outline become the glass, so the window follows the hull's
	// shape and edges (the user, 2026-11-06: for model hulls). Where too few
	// whole triangles fit, the window is cut as usual.
	QString	windowStyle;
	// The way out of the hull doors: "auto" a ramp where the door is low
	// enough, else a ladder down to the ground; "ladder" always a ladder;
	// "none" no way down (a docking door, for a ship that stays in space).
	QString	hullDoorExit = "auto";
	// How the hull is shaped from the drawn triangles: "smooth" pushed out
	// by the skin and rounded; "angular" sloped between the decks and
	// pushed out with mitred corners, so that flat areas stay flat panels;
	// "classic" as the old Shores of Hazeron designer made it (its hull wall
	// skinner): straight up from the drawn outline, flat on top and below
	// each deck, with mitred corners.
	QString	hullStyle = "smooth";

	QList<PlanRoom>		rooms;
	QList<PlanDoor>		doors;
	QList<PlanDoor>		windows;		// On edges between a room and the outside.
	QList<PlanLadder>	ladders;
	QList<PlanSkylight>	skylights;
	QList<PlanStair>	stairs;
	QList<PlanParking>	parking;
	QList<PlanEmitter>	emitters;
	QList<PlanPaint>	palette;		// Entry 0 is the base coat.
	// Placed by hand on the Crew page; the build fills in the rest unless
	// these are off (then only what was placed by hand).
	QList<PlanStation>	stations;
	QList<PlanBerth>	berths;
	QList<PlanPad>		pads;
	QList<PlanPath>		paths;
	bool	autoStations = true;		// The template's other stations, wherever the build finds room.
	bool	autoBerths = true;
	bool	autoPads = true;			// Pads for transporters whose room has none placed by hand.
	bool	autoPaths = true;			// Paths through every door, ladder and stairs.
	// What the build leaves out, by kind (Build page, BuildKinds()): "doors",
	// "stations", "paths", ...
	QStringList	buildLeaveOut;
	// "" a ship as usual, or "fighter": fighter mode (experimental, the user
	// 2026-11-08): a small craft, often from a 3D model, with small cells and
	// low decks; the build has no FTL drive (it would not fit) and checks
	// that there is a cockpit and a way in.
	QString	mode;
	bool	IsFighter() const { return mode == "fighter"; }
	// Makes this plan a fighter: the mode, the fighter template, no FTL,
	// windows that follow the hull (a canopy), no automatic berths, a
	// Cockpit room when there is no Bridge; with 'sizes' also the small
	// cells and low decks (2.5 ft, 4.5 ft, as the cockpit tried in the game
	// on 8 November 2026), for a plan with nothing drawn yet.
	void	MakeFighter(bool sizes);
	static const double kFighterCellFeet, kFighterDeckFeet;
	// Of the kinds the Build page lists: key and label.
	static QList<QPair<QString, QString> > BuildKinds();
	// A copy with what the build leaves out taken away: the kinds in
	// buildLeaveOut and the items marked skip.
	TriPlan	ForBuild() const;
	// The room at a point (in cells from the corner) of a deck; -1 if none.
	int		RoomAtPoint(int deck, double x, double y) const;
	QPointF	CellPointToWorld(double x, double y) const { return Corner(0, 0) + QPointF(x, y) * cellFeet; }

	int		Width() const { return m_width; }		// Cells along x.
	int		Depth() const { return m_depth; }		// Cells along y.
	int		Decks() const { return m_decks; }

	// Changes the size of the grid, keeping what is drawn centred.
	void	Resize(int width, int depth);
	// Adds a deck on top, or below with 'below'.
	void	AddDeck(bool below = false);
	void	RemoveDeck(int deck);
	// Flips the plan, or only one deck of it (deck >= 0): fore and aft
	// swapped (x mirrored), port and starboard (y mirrored), and, for the
	// whole plan only, upside down (the decks in the opposite order). Both x
	// and y together is turning it round. The rooms, doors, windows, ladders,
	// stairs, parking and paint go with it, and the 3D model a plan was made
	// from too; what no longer fits is taken away (Tidy). Returns how many
	// ladders, stairs and parking spots were taken away.
	int		Flip(bool foreAft, bool portStarboard, bool upsideDown = false, int deck = -1);
	// Turns the plan a quarter turn about the grid's middle, clockwise or
	// anticlockwise seen from above, or only one deck of it (deck >= 0).
	// Everything drawn goes with it as with Flip; the whole plan's grid swaps
	// its width and depth, and for one deck the grid is made bigger first
	// when the turned deck would not fit (then 'grew' is set). Returns how
	// many ladders, stairs and parking spots were taken away.
	int		Rotate(bool clockwise, int deck = -1, bool* grew = 0);

	bool	Valid(const TriRef& t) const;
	bool	Hull(const TriRef& t) const;
	int		Room(const TriRef& t) const;		// -1 when the triangle is not in a room.
	int		Paint(const TriRef& t) const;		// A palette index.
	void	SetHull(const TriRef& t, bool on);
	void	SetRoom(const TriRef& t, int room);
	void	SetPaint(const TriRef& t, int paint);

	// Where things are, in feet. The grid is centred on the origin.
	QPointF	Corner(int ix, int iy) const;			// Grid corner (0..Width, 0..Depth).
	QPointF	EighthPoint(const QPointF& q) const;	// A point in eighths of a cell from corner 0,0 (PlanDoor::buttons).
	QPointF	ToEighths(const QPointF& world) const;	// The other way round.
	// Where on a wall of triangle t a door button near 'p' goes: the wall edge
	// nearest p (not a doorway), at the point of it nearest p, kept 'margin'
	// feet from the ends of the edge. 'inward' is a unit vector into t.
	bool	ButtonWallSpot(const TriRef& t, const QPointF& p, double margin, QPointF& spot, QPointF& inward,
					QPointF* wallA = 0, QPointF* wallB = 0) const;
	bool	TriAt(int deck, const QPointF& world, TriRef& out) const;	// The triangle a point lies in.
	QPointF	CellCentre(int cx, int cy) const;
	void	TriCorners(const TriRef& t, QPointF out[3]) const;	// Counter-clockwise from above.
	QPointF	TriCentre(const TriRef& t) const;
	double	DeckFloor(int deck) const { return deck * deckFeet; }

	// The triangle across each of the three edges of a triangle, in the same
	// order as its corners: edge 0 is the cell edge, 1 and 2 the diagonals.
	// An invalid TriRef (Valid() false) when the edge is on the grid's border.
	TriRef	Across(const TriRef& t, int edge) const;
	// The triangle on the other side of the cell edge 'side' of cell (cx, cy).
	TriRef	AcrossCellEdge(int deck, int cx, int cy, int side) const;
	// The same triangle on the other side of the ship (y mirrored).
	TriRef	Mirror(const TriRef& t) const;

	// Removes rooms, doors and paint that no longer make sense: rooms on
	// triangles that are not hull, doors whose edge has no room beside it.
	void	Tidy();

	int		DoorAt(int deck, int cx, int cy, int side) const;
	// Whether two doors are next to each other on one wall, between the same
	// rooms, in one row, however each happens to be named.
	bool	DoorsJoin(const PlanDoor& a, const PlanDoor& b) const;
	int		NewDoorRow() const;	// Index in doors, -1 if none.
	int		WindowAt(int deck, int cx, int cy, int side, int edge = 0) const;	// Index in windows, -1 if none.
	// The two ends of edge 'edge' of a triangle (0 the cell edge, 1 and 2 the
	// diagonals, as Across numbers them).
	void	EdgeEnds(const TriRef& t, int edge, QPointF& a, QPointF& b) const;
	int		LadderAt(int deck, int cx, int cy) const;			// Index in ladders, -1 if none.
	int		NewWindowRow() const;								// A row number no window has yet.
	int		SkylightAt(int deck, int cx, int cy) const;			// Index in skylights, -1 if none.
	int		SkylightOn(const TriRef& t) const;					// The skylight over a triangle (its own or its cell's), -1 if none.
	bool	SkylightFits(const PlanSkylight& window) const;		// Room (a whole cell, or the triangle) with no room beyond it; a canopy may be over hull too.
	bool	WindowIsHull(const PlanDoor& window) const
	{
		return window.style == "hull" || (window.style.isEmpty() && windowStyle == "hull");
	}
	bool	SkylightIsHull(const PlanSkylight& window) const;		// Its glass follows the hull.
	int		ParkingAt(int deck, int cx, int cy) const;			// Index in parking, -1 if none.
	int		EmitterAt(const TriRef& t, bool weapon, int marking = 0) const;		// Index in emitters, -1 if none.
	// How many cells a flight of stairs runs over, for a slope of about 34
	// degrees (like the stairs in the user's aircraft factory).
	int		StairCells() const;
	// Cell 'i' of a flight: 0 is where it starts, StairCells() the landing.
	void	StairCell(const PlanStair& stair, int i, int& cx, int& cy) const;
	// The flight whose run (on its lower deck) covers a cell, -1 if none.
	int		StairAt(int deck, int cx, int cy) const;
	// Whether a flight can go there; if not, why not.
	bool	StairFits(const PlanStair& stair, QString* why = 0) const;
	// The size of a parking spot in feet (x, y), and whether it lies wholly
	// on the cells of one room.
	QPointF	ParkingSize(const PlanParking& p) const;
	bool	ParkingFits(const PlanParking& p) const;
	// The room the whole cell is in, or -1 when its triangles are not all in one room.
	int		CellRoom(int deck, int cx, int cy) const;

	bool	Save(const QString& fileName, QString& error) const;
	bool	Load(const QString& fileName, QString& error);

	// A small ship to start from, so that the tool has something to show.
	static TriPlan Example();

private:
	int		Index(int cx, int cy, int k) const { return (cy * m_width + cx) * 4 + k; }

	int		m_width = 40;
	int		m_depth = 16;
	int		m_decks = 2;
	QVector<QVector<quint8> >	m_hull;		// Per deck, per triangle.
	QVector<QVector<qint16> >	m_room;
	QVector<QVector<quint8> >	m_paint;
};

#endif
