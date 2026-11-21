//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - turning a triangle plan into a blueprint.
//
// The hull. Every hull triangle of a deck is a prism one deck high; the
// prisms are joined into one closed surface (faces between two filled
// prisms are left out). The surface is then pushed out by the skin, so that
// the drawn triangles are the inside of the hull, and smoothed, which rounds
// the edges and the steps between decks. The smoothing is held back near the
// rooms, so it never cuts into one. Flat areas of one colour are then merged
// into a few large triangles, and if the hull is still over the face budget
// nearly flat areas are merged too, a little more at a time. Both keep the
// surface closed and the paint edges exact.
//
// The rooms. Each room becomes, on each deck it is drawn on, a room mesh and
// a constant room void of the same shape, a little lower than the deck so
// that there is a floor and a ceiling. The doors, the hull doors and the
// ladders are made as interior.h describes, and walk paths follow the
// triangles from each room's middle to its doors.
//
// The rest. The engine wants stations for the systems, berths for the crew
// and the hull volume shared out between the systems. The generator already
// knows how to do that (FurnishRooms in generate.h), following the template
// the plan names.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_PLANBUILD_H
#define SHIPGEN_PLANBUILD_H

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

#include "auscface.h"
#include "sohfile.h"
#include "triplan.h"

class AuDesign;
class AuScDesign;

// The shaped hull: its triangles with the colour of each, ready to become
// a hull part.
struct PlanHull
{
	AuScTriList		tris;
	QVector<int>	paint;			// Palette index per triangle.
	int				rawTriangles = 0;	// Before the flat areas were merged.
	bool			thinned = false;	// Nearly flat areas had to be merged to meet the face budget.
	bool			closed = false;
	QStringList		notes;
};

// Told the step being done and how far along it is (total 0: no count);
// returns false to stop (BuildPlanHull goes on regardless).
typedef std::function<bool(const QString& step, int done, int total)> PlanProgress;

PlanHull	BuildPlanHull(const TriPlan& plan, const PlanProgress& progress = PlanProgress());

// The paint brushed in the 3D view (TriPlan::strokes), laid on the hull's
// triangles: each stamp paints the first triangle met by rays from its eye
// through its footprint. Returns how many triangles it painted.
int		ApplyPaintStrokes(const TriPlan& plan, PlanHull& hull);

// The first triangle of 'tris' met by the ray from 'from' along 'dir' (either
// side), or -1; 'distance' is how far along it is (dir need not be a unit).
int		FirstTriangleHit(const AuScTriList& tris, const QVector3D& from, const QVector3D& dir, float* distance = 0);

// A design holding only the painted hull, for showing it quickly.
AuScDesign*	BuildPlanHullDesign(const TriPlan& plan, const PlanHull& hull);

// Where a contrail emitter ends up, in plan space (feet): on the hull's
// aftmost surface straight aft of its triangle. False when no hull is aft of it.
bool	ContrailEmitterAt(const TriPlan& plan, const AuScTriList& hullTris, const PlanEmitter& e, AuPoint& at);

// Which triangles of each deck could hold a room inside the shaped hull:
// the room prism over the triangle is inside the hull's closed surface.
QVector<QVector<bool> >	RoomableTriangles(const TriPlan& plan, const PlanHull& hull);

struct PlanBuildOptions
{
	bool	furnish = true;			// Stations, berths and system volumes from the template.
	bool	walkPaths = true;
	bool	ramps = true;			// A ramp down from each hull door that is low enough, else a ladder.
	bool	hullClearance = true;	// Hull voids round the outside where the game would stop walkers short of the hull.
};

struct PlanBuildReport
{
	bool			ok = false;
	int				hullFaces = 0;			// After the doors and windows are cut.
	int				shapedFaces = 0;		// As shaped, before the cuts; this is what the budget is for.
	double			hullVolumeM3 = 0;
	int				rooms = 0;
	int				doors = 0;
	int				windows = 0;
	int				walkAreas = 0;			// Parts of the ship the crew can walk between.
	int				skylights = 0;			// Windows in a ceiling.
	int				hullSkylights = 0;		// Of them, following the hull.
	int				canopies = 0;			// Of them, canopies: glass from the sill up, walls opened under it.
	int				ladders = 0;
	int				hatches = 0;			// Doors in floors and ceilings.
	int				zeroGJoins = 0;		// Zero-G shafts opened into the one above.
	int				zeroGNoLadder = 0;
	int				windowLining = 0;	// Faces covering the hull's inside behind windows.
	QStringList		canopyRooms;		// Rooms under a canopy: seen through, never lined.	// Of those, joins with no ladder placed by hand.
	int				openJoins = 0;		// "Open to Deck Below" rooms opened into the room below.
	int				stairs = 0;
	int				parking = 0;			// Vehicle parking spots.
	// Generic vehicle spots: ground parking spots in the design that are to
	// be saved as the game's vehicle spots (SaveDesign's `vehicles`).
	QList<VehicleSpotSave>	vehicleSpots;
	int				contrails = 0;			// Contrail emitters.
	int				markings = 0;			// Empire flags and names on the hull.
	int				lifts = 0;				// Turbolifts.
	int				liftStops = 0;			// Their stops, all together.
	int				doorButtons = 0;		// Buttons on walls that open doors.
	int				innerWindows = 0;		// Windows between two rooms.
	int				balconies = 0;			// Openings between two rooms above a parapet.
	int				hullWindows = 0;		// Windows whose glass is the hull's own faces.
	int				weaponEmitters = 0;		// Fire control stations given a weapon emitter.
	int				ramps = 0;				// Hull doors with a ramp down to the ground.
	int				stations = 0;
	int				berths = 0;
	// Of them, placed by hand on the Crew page; and paths drawn by hand.
	int				handStations = 0;
	int				handBerths = 0;
	int				handPads = 0;
	int				handPaths = 0;
	int				decals = 0;				// Decal parts laid over the hull.
	int				decalFaces = 0;			// Their faces.
	QStringList		notes;
	QStringList		warnings;
	QStringList		errors;			// The engine's own report.
	QStringList		engineWarnings;
};

// Builds the blueprint. The caller owns the design. Never returns null; a
// design that could not be made complete says why in the report.
// A ship blueprint (AuScDesign), or in building mode a building blueprint
// (AuBdDesign).
AuDesign*	BuildPlanDesign(const TriPlan& plan, const PlanBuildOptions& options, PlanBuildReport& report);

// A plan whose hull triangles are filled in from a 3D model (stl, 3ds or
// obj), as a start to draw rooms, doors and paint on. The model is turned so
// that its longest horizontal side is the plan's x (fore, to the right),
// scaled to 'lengthFeet' long (or to 'decks' decks high), and stood on deck
// 0. Its inside is found slice by slice (ModelInside::Slices), or from its
// closed pieces after a repair (ModelInside::Solid). A triangle of a deck is
// hull when at least 'threshold' (0 to 1) of the points sampled in it, at
// 'slicesPerDeck' heights in the deck, are inside the model: lower for a
// fuller hull, higher for a slimmer one. Parts thinner than about half a cell
// are lost, and slopes come out stepped by the deck height (the shaping
// smooths them).

// How the inside of a model is found.
enum class ModelInside
{
	Slices,		// Slice by slice, flooding the outside in (works on open models; SliceSolid).
	Solid		// Only the closed pieces count, tested by rays (repaired and thinned first).
};

// The heights each deck is judged at when slicesPerDeck is 0.
int		RecommendedSlicesPerDeck(double deckFeet);

struct PlanFromModelOptions
{
	ModelInside	method = ModelInside::Slices;
	// The number of decks (slices of the model) wanted: the length is then
	// whatever makes the model this many decks high. 0: use lengthFeet.
	int		decks = 0;
	// The heights each deck is judged at (0: RecommendedSlicesPerDeck).
	int		slicesPerDeck = 0;
	// Slicing: gaps in the model's surface narrower than this are closed.
	double	sealFeet = 1.0;
	// Pieces of hull smaller than this many triangles that touch nothing
	// else are taken away (0: kept); single missing triangles with hull on
	// all three sides are filled in.
	int		minPiece = 12;
	bool	fillPinholes = true;
	// The hull is the model itself, smooth (with a wall round the rooms), not
	// the drawn triangles (TriPlan::model). Slicing only.
	bool	modelAsHull = true;
	double	lengthFeet = 150.0;
	double	threshold = .5;
	double	cellFeet = 5.0;
	double	deckFeet = 10.0;
	bool	flip = false;		// Turn it round: the model's other end is fore.
	bool	yUp = false;		// The model was made with y up (many obj files are), not z.
	// Keep the model's axes and size (lengthFeet is then ignored): a
	// blueprint's hull, which is in the game's feet with fore at +Y.
	bool	asIs = false;
	// asIs only: a height in the model that should be a deck's floor (the
	// lowest room floor of a blueprint); NaN for none.
	double	floorAt = NAN;
	int		maxFaces = 20000;	// A bigger model is thinned to this first, as shipgen reduce does (0: never).
	// Told what is being done and how far it has got ('total' 0 when that
	// cannot be said); return false to stop. A big model takes a while
	// (half a million faces: about a minute and a half, mostly thinning).
	std::function<bool(const QString& step, int done, int total)>	progress;

};
// What came of it, beside the notes.
struct PlanFromModelReport
{
	double	keptShare = 0;			// The share of the model (points through its inside) on hull triangles.
	double	lengthFeet = 0;			// The length it was made at (from the decks, when they were asked for).
	int		decks = 0;
	int		suggestedDecks = 0;		// The decks at suggestedLength.
	// When much is lost (under 85 percent), the shortest length tried that
	// keeps 85 percent (or the best one tried), and what it keeps; 0 when
	// no longer length is needed or none keeps at least ten points more. The cells and decks stay
	// the size they are (doors and headroom need them): a longer ship makes
	// the thin parts of the model thick enough for them.
	double	suggestedLength = 0;
	double	suggestedShare = 0;
};
bool	PlanFromModel(const QString& modelFile, const PlanFromModelOptions& options, TriPlan& plan,
		QStringList& notes, QString& error, PlanFromModelReport* report = 0);

// A plan from a blueprint (.SoH): its hull becomes the plan's hull as it is
// (PlanFromModel with asIs), its rooms fill the cells whose triangles they
// hold, and its doors become doors on the nearest room edges. Stations,
// berths and the rest are furnished again by the template when it is built.
// A building blueprint gives a plan in building mode.
bool	PlanFromBlueprint(const QString& file, const PlanFromModelOptions& options, TriPlan& plan,
		QStringList& notes, QString& error);

#endif
