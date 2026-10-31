//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - shared declarations.
//
// Internal layout generator for Shores of Hazeron spacecraft designs.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_H
#define SHIPGEN_H

#include <QList>
#include <QString>
#include <QStringList>

#include "aupoint.h"

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Makes a name safe to use as a file name.
QString	SanitiseName(const QString& text);

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The requirements brief. This is the "purpose and features" input: what the
// ship is for and what it has to contain.

// A room that the layout has to provide. Sizes are in feet (design units).
struct RoomSpec
{
	QString							type;		// Machine key, e.g. "bridge".
	QString							label;		// Human readable name.
	int								count = 1;
	double							sizeX = 0;	// Along the fore-aft axis.
	double							sizeY = 0;	// Across the ship.
	double							sizeZ = 0;	// Deck height.
	int								priority = 5;	// 1 is most important. Rooms that do not fit are dropped in this order.

	RoomSpec() {}
	RoomSpec(const QString& type_, const QString& label_, int count_, double x, double y, double z) :
		type(type_), label(label_), count(count_), sizeX(x), sizeY(y), sizeZ(z) {}
};

// A station that must be staffed. Counted separately from rooms.
struct StationSpec
{
	QString							type;		// "helm", "engineer", ...
	int								count = 1;
	QString							roomType;	// Preferred room type. Empty means any.

	StationSpec() {}
	StationSpec(const QString& type_, int count_, const QString& roomType_ = QString()) :
		type(type_), count(count_), roomType(roomType_) {}
};

// Crew accommodation.
struct BerthSpec
{
	int		capt = 0;
	int		officer = 0;
	int		crew = 0;
	int		passenger = 0;
	int		troop = 0;

	int		Total() const { return capt + officer + crew + passenger + troop; }
};

// Volume allocations. All values are in cubic meters.
struct ModuleSpec
{
	quint32	powerPlant = 0;
	quint32	maneuverDrive = 0;
	quint32	capacitor = 0;
	quint32	fuel = 0;
	quint32	lifeSupport = 0;
	quint32	armor = 0;
	quint32	skeleton = 0;
	quint32	hold = 0;
	quint32	sensors = 0;
	quint32	shields = 0;
	quint32	cryoBerths = 0;

	bool	IsEmpty() const
	{
		return !powerPlant && !maneuverDrive && !capacitor && !fuel && !lifeSupport &&
			!armor && !skeleton && !hold && !sensors && !shields && !cryoBerths;
	}
};

// The complete brief.
struct Brief
{
	QString							name;
	QString							description;
	QString							templateName;
	QString							layout;			// "auto" or a strategy name.
	QList<RoomSpec>					rooms;
	QList<StationSpec>				stations;
	BerthSpec						berths;
	ModuleSpec						modules;
	int								crew = 0;		// Nominal crew size.
	bool							modulesFromTemplate = true;
	// The FTL drive the engineer runs: "none", "warp1" to "warp9",
	// "wormhole-p", "wormhole-pn" or "wormhole-pnn" (see FtlDriveChoices).
	QString							ftlDrive = "wormhole-p";

	// Expand all room and station counts into individual entries.
	QList<RoomSpec>	ExpandedRooms() const;
	QList<StationSpec> ExpandedStations() const;
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Templates. A template is a starting point that fills in a brief; the brief
// can then add, remove or override anything.

struct ShipTemplate
{
	QString							key;
	QString							label;
	QString							description;
	QList<RoomSpec>					rooms;
	QList<StationSpec>				stations;
	BerthSpec						berths;
	ModuleSpec						modules;	// Per 1000 cubic meters of hull.
	int								crew = 0;
};

QList<ShipTemplate>	AllTemplates();
const ShipTemplate*	FindTemplate(const QString& key);

// Apply a template to a brief. Existing entries of the same type are replaced.
void					ApplyTemplate(Brief& brief, const QString& templateKey);

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Brief file I/O. The brief is a small JSON document.

bool					LoadBrief(const QString& fileName, Brief& brief, QString& error);
bool					SaveBrief(const QString& fileName, const Brief& brief, QString& error);
QString				BriefToJson(const Brief& brief);

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#endif // SHIPGEN_H
