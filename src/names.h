//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the names the game itself uses.
//
// The room and station pickers only offer what the game offers. The
// spacecraft room names come straight from the engine's own room type list,
// so they always match what the game shows.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_NAMES_H
#define SHIPGEN_NAMES_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>

// The room types a spacecraft may have. This is the engine's own list.
// A room without gravity: people float in it and can move up and down. The
// engine calls it a "Gravity Well" (ROOM_GravityWell, room gravity G_Zero);
// the designer calls it a Zero-G Shaft, and still reads the old name.
inline QString ZeroGRoomType() { return QStringLiteral("Zero-G Shaft"); }
inline bool IsZeroGRoomType(const QString& type)
{
	return type.compare(ZeroGRoomType(), Qt::CaseInsensitive) == 0 ||
		type.compare(QStringLiteral("Gravity Well"), Qt::CaseInsensitive) == 0;
}

// A room with no floor: the room on the deck below reaches up through it.
inline bool IsOpenToDeckBelowType(const QString& type)
{
	return type.compare(QStringLiteral("Open to Deck Below"), Qt::CaseInsensitive) == 0;
}

// How a plan's stations look: its key in the plan, and its name. More to come.
inline QList<QPair<QString, QString> > BridgeStyles()
{
	return QList<QPair<QString, QString> >()
			<< qMakePair(QStringLiteral("hazeron"), QStringLiteral("Hazeron default (swivel chairs)"))
			<< qMakePair(QStringLiteral("desks"), QStringLiteral("Integrated desks (experimental)"));
}

// What a fire control station can run, as the game's own station dialog offers
// it: the commodity id of the module, and its name. The Energy Weapon Bay is the default.
inline QList<QPair<int, QString> > FireControlModules()
{
	return QList<QPair<int, QString> >()
			<< qMakePair(252, QStringLiteral("Energy Weapon Bay"))
			<< qMakePair(261, QStringLiteral("Ballistic Weapon Bay"))
			<< qMakePair(254, QStringLiteral("Weapon Launcher Bay"))
			<< qMakePair(319, QStringLiteral("Weapon Deployment Bay"))
			<< qMakePair(258, QStringLiteral("Tractor Beam Bay"))
			<< qMakePair(257, QStringLiteral("Prospector Bay"))
			<< qMakePair(291, QStringLiteral("Wildcatter Bay"))
			<< qMakePair(289, QStringLiteral("Siphon Bay"))
			<< qMakePair(283, QStringLiteral("Particle Collector Bay"))
			<< qMakePair(256, QStringLiteral("Atmosphere Condenser Bay"));
}

QStringList	SpacecraftRoomTypes();

// The textures of the game's library that suit an interior: id and name
// (docs/TEXTURE_LIBRARY.md).
QList<QPair<int, QString> >	InteriorTextures();

// The room types a building may have. The game does not fix a list for
// buildings, so this is the set that its own blueprints use.
QStringList	BuildingRoomTypes();

// The stations a blueprint may have. These are the console types of the
// engine.
QStringList	StationTypes();

// The industry a building serves. The game keeps its industry table in its
// database, so this is the list of the ones a blueprint can be given.
int	IndustryIdFor(const QString& name);
QStringList	KnownIndustryNames();

// The room types that suit each kind of ship. Used to fill the pickers in
// when a template is chosen.
QStringList	ShipRoomTypesFor(const QString& templateKey);
QStringList	BuildingRoomTypesFor(const QString& templateKey);

#endif // SHIPGEN_NAMES_H
