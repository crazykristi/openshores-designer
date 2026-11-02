//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - hull analysis and internal layout generation.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_GENERATE_H
#define SHIPGEN_GENERATE_H

#include <QString>
#include <QStringList>
#include <QStringList>

#include "auscparts.h"
#include "audit.h"
#include "base.h"
#include "paint.h"
#include "shape.h"
#include "audesign.h"
#include "auscdesign.h"
#include "shipgen.h"

// Knobs for the layout generator.
struct LayoutOptions
{
	QString	strategy = "decks";		// "decks", "spine" or "blocks".
	double	wallThickness = 8.0;		// Feet of hull left between the interior and the hull surface.
	double	deckThickness = 4.0;		// Feet of structure between decks.
	double	corridorWidth = 14.0;		// Feet. Spine layout only.
	bool	centerHull = true;			// Move the hull so it is centred on the origin.
	// The game takes +Y as fore and measures the length along Y. With this
	// set, a hull that is longer along X is turned a quarter turn (+X
	// becomes +Y) when it is centred.
	bool	foreAlongLongAxis = true;
	bool	placeServicePanels = true;
	bool	placeBerths = true;
	bool	placeWalkPaths = true;		// Connect the rooms with walk paths.
	bool	placeWindows = true;		// Windows where a room's outer wall is against the hull.
	// How stations look: "hazeron" for the game's swivel chairs, "desks" for
	// integrated stations on desks in curved rows (BridgeStyles in names.h).
	QColor	deskPaint = QColor(150, 155, 160);
	QColor	deskSeatPaint = QColor(90, 95, 100);
	QString	bridgeStyle = "hazeron";
	int		fireControlModule = 0;		// What a fire control station runs. 0: the Energy Weapon Bay.
	bool	deskStanding = false;		// The operator of a desk stands, with no stool.
	double	deskTopFeet = 3.0;			// The top edge of a desk above the floor.
	// Boxes kept clear of desks: doors, ladders and stairs, in the blueprint's coordinates.
	QList<QPair<AuPoint, AuPoint> > stationKeepOut;
};

// What the generator found out about the hull it has to work with.
struct HullAnalysis
{
	bool		valid = false;
	AuPoint	min;					// Hull extents in feet (design units).
	AuPoint	max;
	AuScSilhouette3d	silhouette;
	QList<AuScSilhouette3d>	partSilhouettes;	// One per hull part, as the engine builds them.
	// The same hull parts as closed triangle meshes, for the exact room
	// test. A part that is not closed cannot answer "inside" and is skipped
	// by the exact test; the silhouette is only a fallback then.
	QList<SolidMesh>	solids;
	quint32			faces = 0;			// How many faces the hull has.
	quint32			triangles = 0;
	double	volumeM3 = 0;			// Enclosed volume in cubic meters.
	AuPoint	usableMin;			// Interior box after eroding the hull by the wall thickness.
	AuPoint	usableMax;
	char	longAxis = 'x';		// The horizontal axis the hull is longest along.
	bool	turned = false;		// The hull was turned to put its length along Y.
};

// The FTL drives a brief can ask for: key and label, in order. "none" first.
QList<QPair<QString, QString> > FtlDriveChoices();
// The engine module for a key; CID_Invalid for "none" or an unknown key.
COMMODITYID FtlDriveModule(const QString& key);

// Shares the hull volume out between the systems again, for a design whose
// hull changed after it was furnished (the stations stay as they are).
void AllocateSystemVolumes(AuScDesign& design, const Brief& brief, QStringList& notes);

// The quarter turn about Z that takes +X to +Y, the game's fore.
AuMatrix4x4 QuarterTurnToFore();

// A room that was placed into the design.
struct PlacedRoom
{
	QString	type;
	QString	label;
	AuPoint	min;					// Feet.
	AuPoint	max;
	quint16	voidId = 0;
	quint16	meshId = 0;
	double	volumeM3 = 0;
};

// The outcome of one candidate layout.
struct LayoutResult
{
	QString			strategy;
	QString			description;
	bool			ok = false;
	int				roomsPlaced = 0;
	int				roomsMissing = 0;
	QStringList		missingRooms;
	int				stationsPlaced = 0;
	int				berthsPlaced = 0;
	int				doorsPlaced = 0;
	int				windowsPlaced = 0;
	int				rampsPlaced = 0;		// Hull doors with a ramp down to the ground.
	QStringList		warnings;
	QStringList		errors;		// From the design report.
	QList<PlacedRoom>	rooms;
};

// Analyse the hull of a design.
bool	AnalyseHull(AuScDesign& design, const LayoutOptions& opts, HullAnalysis& out, QString& error);

// Builds a silhouette from the hull parts only. The engine's obstruction
// model also swallows the faces of any room or door that touches it, so it
// grows outwards when a part sticks out of the hull. This one does not, which
// makes it the honest test for "is this part really inside the hull?".
// 'perPart' receives one silhouette per hull part when it is not null.
bool	HullOnlySilhouette(const AuScParts& parts, AuScSilhouette3d& silhouette,
		QList<AuScSilhouette3d>* perPart = 0);

// Returns true when every sampled point of the box is inside the silhouette.
// The sample is the 8 corners, the 12 edge midpoints, the 6 face centres and
// the centre, plus a 3x3x3 grid. Checking corners alone misses a concavity
// that cuts through the middle of a room.
bool	BoxInsideSilhouette(const AuScSilhouette3d& silhouette, const AuPoint& mn, const AuPoint& mx);

// Minimum room dimension in feet that is still usable for a player
// character. A Hazeron character is roughly two metres tall, so a room needs
// headroom a little above that. Rooms are never shrunk below these sizes; a
// room that cannot fit is reported and left out instead of poking through the
// hull or becoming a crawlspace.
#define SHIPGEN_MIN_ROOM_FEET 6.0
#define SHIPGEN_MIN_ROOM_HEIGHT 7.0

// Place the interior of a ship according to the brief.
LayoutResult	GenerateLayout(AuScDesign& design, const Brief& brief, const HullAnalysis& hull,
		const LayoutOptions& opts);

// Furnish rooms that are already in the design, for instance drawn by hand:
// stations, berths, service panels, weapons and the hull volume of the
// systems, the same way GenerateLayout does after it has placed its rooms.
// Doors and walk paths are left to the caller. Each room's box should be a
// part of the room that stations can stand in.
LayoutResult	FurnishRooms(AuScDesign& design, const Brief& brief, const HullAnalysis& hull,
		const LayoutOptions& opts, const QList<PlacedRoom>& rooms);

// Validate a design with the engine's own report and collect the messages.
bool	ValidateDesign(AuScDesign& design, QStringList& errors, QStringList& warnings);

// How a model is turned into a hull.
struct ModelImportOptions
{
	double	scale = 1.0;		// Feet per model unit.
	double	fit = 0.0;		// If set, the model is scaled so its largest side is this many feet.
	double	decimate = 0.0;	// Grid size in feet for thinning the model out.
	bool	centre = true;		// Put the model in the middle of the world with its base on the ground.
	quint32	maxFaces = 5000;	// How many faces the model may have at most.
	ModelShape	shape;			// What to do to the shape of the model.
	BaseShape	base;			// A primitive the models are put on, if any.
	PaintOptions	paint;		// Textures for the hull, if any.
	bool		repair = true;	// Close the holes and put the windings right.
	// What the importer had to put right, for the tool to say.
	QStringList*	notes = 0;
};

// Import one or more 3ds or stl models as the hull of a design. Several files
// are put together, which is what a model that comes in parts needs.
bool	ImportHullModel(AuDesign& design, const QStringList& fileNames, const ModelImportOptions& opts,
		AuPoint& modelMin, AuPoint& modelMax, QString& error);

// Copy the hull parts of one design into another.
bool	CopyHull(AuScDesign& from, AuScDesign& to, QString& error, bool allowOpenHull = false);

// Names of the available layout strategies.
QStringList	LayoutStrategies();

// Puts a triangle mesh back together as a closed solid, as well as it can:
// duplicates and slivers go, the windings are made to agree, holes are closed
// and near corners snapped together.
AuScTriList	SolidifyMesh(AuScTriList tris);

#endif // SHIPGEN_GENERATE_H
