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
};

// A ladder up from a cell of one deck into the same cell of the deck above.
// The cell has to be in a room on both decks.
struct PlanLadder
{
	int		deck = 0;		// The lower deck.
	int		cx = 0;
	int		cy = 0;
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
	double	sizeFeet = 6.5616798;	// 2 m.
};

// A colour in the paint palette: a plain colour, or one of the owner's
// empire colours (AuScFace::THEME_ExteriorPrimary and the two after it).
struct PlanPaint
{
	QColor	colour = QColor(150, 155, 165);
	int		theme = -1;		// 0, 1 or 2 for an empire colour; -1 for the plain colour.
	QString	name;
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

	bool	IsSet() const { return !file.isEmpty(); }
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
	bool	joinWindows = false;		// Windows side by side on one straight wall make one long window.
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
	QList<PlanStair>	stairs;
	QList<PlanParking>	parking;
	QList<PlanEmitter>	emitters;
	QList<PlanPaint>	palette;		// Entry 0 is the base coat.

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

	bool	Valid(const TriRef& t) const;
	bool	Hull(const TriRef& t) const;
	int		Room(const TriRef& t) const;		// -1 when the triangle is not in a room.
	int		Paint(const TriRef& t) const;		// A palette index.
	void	SetHull(const TriRef& t, bool on);
	void	SetRoom(const TriRef& t, int room);
	void	SetPaint(const TriRef& t, int paint);

	// Where things are, in feet. The grid is centred on the origin.
	QPointF	Corner(int ix, int iy) const;			// Grid corner (0..Width, 0..Depth).
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

	int		DoorAt(int deck, int cx, int cy, int side) const;	// Index in doors, -1 if none.
	int		WindowAt(int deck, int cx, int cy, int side, int edge = 0) const;	// Index in windows, -1 if none.
	// The two ends of edge 'edge' of a triangle (0 the cell edge, 1 and 2 the
	// diagonals, as Across numbers them).
	void	EdgeEnds(const TriRef& t, int edge, QPointF& a, QPointF& b) const;
	int		LadderAt(int deck, int cx, int cy) const;			// Index in ladders, -1 if none.
	int		ParkingAt(int deck, int cx, int cy) const;			// Index in parking, -1 if none.
	int		EmitterAt(const TriRef& t, bool weapon) const;		// Index in emitters, -1 if none.
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
