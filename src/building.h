//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - building blueprint generator.
//
// A building blueprint uses the same part model as a spacecraft: a hull mesh
// for the shell, room meshes and room voids for the interior, doors, windows
// and walk paths. On top of that a building needs a site (the flat piece of
// land it stands on) and an industry (what it is for).
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_BUILDING_H
#define SHIPGEN_BUILDING_H

#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QRectF>
#include <QString>
#include <QStringList>

#include "aubddesign.h"
#include "auscparts.h"
#include "shipgen.h"

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// What a building provides. The volume numbers are in cubic meters, the
// counts are numbers of things.

struct BuildingVolumes
{
	quint32	offices = 0;
	quint32	shops = 0;
	quint32	store = 0;
	quint32	lounge = 0;
	quint32	homesLarge = 0;
	quint32	homesMedium = 0;
	quint32	homesSmall = 0;
	quint32	powerPlant = 0;
	quint32	lifeSupport = 0;
	quint32	capacitor = 0;
	quint32	sensors = 0;
	quint32	shields = 0;
	quint32	hold = 0;
	quint32	skeleton = 0;
	quint32	armor = 0;

	int		generators = 0;
	int		guardPosts = 0;
	int		citizenPosts = 0;
	int		livestockPosts = 0;
	int		turrets = 0;
	int		weaponBays = 0;		// Fire control stations, each running a weapon bay of its own.
	quint32	weaponBayVolume = 0;	// Cubic meters for each weapon bay; 0: 2 % of the hull each.
	int		surgery = 0;
	int		transporters = 0;
	int		parkGround = 0;
	int		parkSpace = 0;
	int		parkWater = 0;
	quint32	arena = 0;			// Cubic meters of arena. The capacity follows from it.
	quint32	auditorium = 0;		// Cubic meters of auditorium.
	int		field = 0;
};

// A station placed by hand (the designer's Crew page), where it stands on
// the floor and which way it faces (radians from +Y, as a console's icon).
struct BuildingStation
{
	QString	type;			// "sensor", "shield", "firecontrol", "medic", "transporter".
	double	x = 0, y = 0, z = 0;
	double	facing = 0;
	int		module = 0;		// Fire control: the weapon bay it runs; 0 for the building's (weaponModule).
};

// The brief for a building.
struct BuildingBrief
{
	QString					name;
	QString					description;
	QString					structure = "building";	// building, biodome, groundplatform, ...
	QString					industry = "auto";		// auto, military, residential, ...
	QString					layout = "auto";		// auto, grid, corridor, atrium
	double					footprintX = 140;		// Feet.
	double					footprintY = 100;
	int						floors = 1;
	double					floorHeight = 14;		// Feet.
	double					wallThickness = 4;		// Feet.
	QList<RoomSpec>			rooms;
	BuildingVolumes			volumes;
	int						citizenPosts = 0;
	int						guardPosts = 0;
	int						livestockPosts = 0;
	int						groundParking = 0;	// Parking for ground vehicles.
	int						spaceParking = 0;	// Landing pads for space vehicles.
	int						spacecraftParking = 0;	// Berths for spacecraft (238 ft square each), beside the building.
	int						rocketParking = 0;	// Space rocket parking spots (about 33 ft square), in front.
	bool					buildZone = true;	// Platforms: a build zone over the deck, for buildings on top.
	double					lightRange = 0;		// Feet a lamp's light reaches (0: 45 ft for a street light,
												// the game's default otherwise). The default is 200 m.
	bool					stackFields = false;	// Lay the fields on top of each other: the game adds up
												// the area of every field site, so a stack of N one-field
												// sites counts as N fields on the ground of one.
	int						weaponModule = 0;	// What the weapon bays are (COMMODITYID); 0: energy weapon bays.
	// Placed by hand; the generator puts in only the rest. Not saved with
	// the brief (the plan keeps them as its stations).
	QList<BuildingStation>	stations;
};

// The outcome of one candidate building.
struct BuildingResult
{
	QString					strategy;
	bool					ok = false;
	int						roomsPlaced = 0;
	int						roomsMissing = 0;
	QStringList				missingRooms;
	int						doorsPlaced = 0;
	int						postsPlaced = 0;
	quint32					faces = 0;			// How many faces the blueprint has.
	quint32					hullFaces = 0;		// How many of them are the shell.
	quint32					triangles = 0;
	QString					industryName;
	QStringList				warnings;
	QStringList				errors;		// From the design report.
	QList<quint16>			weaponBayParts;		// The fire control stations (each a weapon bay).
	QSet<quint16>			handModuleParts;	// Those whose bay was chosen by hand.
};

// The building templates.
struct BuildingTemplate
{
	QString					key;
	QString					label;
	QString					description;
	double					footprintX = 140;
	double					footprintY = 100;
	int						floors = 1;
	double					floorHeight = 14;
	QList<RoomSpec>			rooms;
	BuildingVolumes			volumes;
	QString					industry = "auto";
	int						citizenPosts = 0;
	int						guardPosts = 0;
	int						groundParking = 0;	// Parking for ground vehicles.
	int						spaceParking = 0;	// Landing pads for space vehicles.
	int						spacecraftParking = 0;
	int						rocketParking = 0;
	bool					stackFields = false;
};

QList<BuildingTemplate>		AllBuildingTemplates();
const BuildingTemplate*	FindBuildingTemplate(const QString& key);
void						ApplyBuildingTemplate(BuildingBrief& brief, const QString& templateKey);

// What the game is known to want of a building of the brief's industry,
// learnt from designs the game took or turned down: warnings for the brief
// as it stands (an empty list when it looks right).
QStringList BuildingRuleWarnings(const BuildingBrief& brief);

// Building brief file I/O. The brief is a small JSON document.
bool	LoadBuildingBrief(const QString& fileName, BuildingBrief& brief, QString& error);
bool	SaveBuildingBrief(const QString& fileName, const BuildingBrief& brief, QString& error);
// The same as a JSON object, to keep inside another document (a designer
// plan in building mode).
QJsonObject	BuildingBriefToJson(const BuildingBrief& brief);
void		BuildingBriefFromJson(const QJsonObject& o, BuildingBrief& brief);

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// A set of buildings.
//
// The same model can be made into a set of buildings that all have the same
// look but are for different things: a capitol, an arena, an office block.
// Each building in the set has its own type and its own shape.

struct BuildingSetEntry
{
	QString			templateKey;		// The building template to use, or empty for the brief below.
	QString			shapeKey;			// The named look for the model.
	QString			name;				// The name of the building.
	QStringList		modelFiles;		// The model for this building, or empty for the one in the set.
	double			fit = 0.0;		// Feet, or zero for the one in the set.
	int				variations = 1;	// How many versions of this building to make.
	BuildingBrief	brief;			// Used when there is no template.
};

struct BuildingSet
{
	QString					name;
	QStringList				modelFiles;		// The model the buildings are made from.
	double					fit = 0.0;
	quint32					maxFaces = 5000;
	QList<BuildingSetEntry>	buildings;
};

// The outcome of making a set.
struct BuildingSetResult
{
	QStringList	fileNames;		// The blueprints that were written.
	QStringList	errors;
	QStringList	warnings;
	int			built = 0;
};

// Building set file I/O. The set is a small JSON document.
bool	LoadBuildingSet(const QString& fileName, BuildingSet& set, QString& error);
bool	SaveBuildingSet(const QString& fileName, const BuildingSet& set, QString& error);

// Makes every building in the set and writes them into the folder.
BuildingSetResult	GenerateBuildingSet(const BuildingSet& set, const QString& outDir);

// Generate a building blueprint.
// 'drawn': the rooms are already in the design (a building drawn in the
// designer, with its hull, rooms and doors); the brief's rooms are not
// laid out and no entrance is cut, and the stations and posts go into the
// drawn rooms.
struct PlacedRoom;
BuildingResult	GenerateBuilding(AuBdDesign& design, const BuildingBrief& brief, const QString& strategy,
		const QList<PlacedRoom>* drawn = 0, const QList<QRectF>* keepClear = 0);

// Validate a building blueprint with the engine's own report.
bool	ValidateBuilding(AuBdDesign& design, QStringList& errors, QStringList& warnings);

// The game keeps its industry table in its database, which a standalone tool
// cannot reach. This installs a small stand-in table so that the building
// report can still work out whether a building is useful for anything.
void	InstallIndustryTable(class DgData* data);

// The names of the industries the stand-in table knows about.
QStringList	KnownIndustries();

#endif // SHIPGEN_BUILDING_H
