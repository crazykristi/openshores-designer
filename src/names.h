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

// The room types a spacecraft may have. This is the engine's own list.
QStringList	SpacecraftRoomTypes();

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
