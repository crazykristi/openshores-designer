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

#include "auscface.h"
#include "triplan.h"

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

PlanHull	BuildPlanHull(const TriPlan& plan);

// A design holding only the painted hull, for showing it quickly.
AuScDesign*	BuildPlanHullDesign(const TriPlan& plan, const PlanHull& hull);

// Which triangles of each deck could hold a room inside the shaped hull:
// the room prism over the triangle is inside the hull's closed surface.
QVector<QVector<bool> >	RoomableTriangles(const TriPlan& plan, const PlanHull& hull);

struct PlanBuildOptions
{
	bool	furnish = true;			// Stations, berths and system volumes from the template.
	bool	walkPaths = true;
	bool	ramps = true;			// A ramp down from each hull door that is low enough, else a ladder.
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
	int				ladders = 0;
	int				stairs = 0;
	int				parking = 0;			// Vehicle parking spots.
	int				ramps = 0;				// Hull doors with a ramp down to the ground.
	int				stations = 0;
	int				berths = 0;
	QStringList		notes;
	QStringList		warnings;
	QStringList		errors;			// The engine's own report.
	QStringList		engineWarnings;
};

// Builds the blueprint. The caller owns the design. Never returns null; a
// design that could not be made complete says why in the report.
AuScDesign*	BuildPlanDesign(const TriPlan& plan, const PlanBuildOptions& options, PlanBuildReport& report);

// A plan whose hull triangles are filled in from a 3D model (stl, 3ds or
// obj), as a start to draw rooms, doors and paint on. The model is repaired,
// turned so that its longest horizontal side is the plan's x (fore, to the
// right), scaled to 'lengthFeet' long, and stood on deck 0. A triangle of a
// deck is hull when at least 'threshold' (0 to 1) of the points sampled in
// it, at three heights in the deck, are inside the model: lower for a fuller
// hull, higher for a slimmer one. Parts thinner than about half a cell are
// lost, and slopes come out stepped by the deck height (the shaping smooths
// them).
struct PlanFromModelOptions
{
	double	lengthFeet = 150.0;
	double	threshold = .5;
	double	cellFeet = 5.0;
	double	deckFeet = 10.0;
	bool	flip = false;		// Turn it round: the model's other end is fore.
	bool	yUp = false;		// The model was made with y up (many obj files are), not z.
	int		maxFaces = 20000;	// A bigger model is thinned to this first, as shipgen reduce does (0: never).

};
bool	PlanFromModel(const QString& modelFile, const PlanFromModelOptions& options, TriPlan& plan,
		QStringList& notes, QString& error);

#endif
