//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - what a design is expected to do, and what it is made of, as the
// game's own design report works it out (AuScReport / AuBdReport).
//
// The game shows this in its design window. Here it is worked out offline
// for a chosen quality (Q) of the modules and materials, so a player can see
// roughly what a ship or building will do before taking it into the game. It
// is only a guide: the game's numbers can change from one version to the
// next, so what the game shows is what counts.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_STATS_H
#define SHIPGEN_STATS_H

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

class AuDesign;

struct DesignStats
{
	int		quality = 1;
	// The game's design report, as html, with the materials list left out
	// (it is in 'materials').
	QString	html;
	// What the shipyard or builder uses up: name and number of units, sorted
	// by name. Modules are counted in the game's units of 256 cubic feet.
	QList<QPair<QString, int> >	materials;
	// Tools the builders need but do not use up.
	QStringList	tools;

	// Crew and berths.
	int		crewStations = 0;
	int		crewBerths = 0;
	int		troopStations = 0;			// Turrets and guard posts.
	int		troopBerths = 0;
	int		captBerths = 0;
	int		officerBerths = 0;
	int		passengerBerths = 0;
	bool	needsCaptainAndOfficer = true;	// Not on a small spaceship (the game's SC_Spaceship).
};

// The report of 'design' with every system at quality 'q' (1 and up).
DesignStats WorkOutStats(const AuDesign& design, int q);

// Plain warnings about the berths: too few for the stations, or none.
// Empty when all is well.
QStringList BerthWarnings(const DesignStats& stats);

// The materials and tools as plain text lines.
QStringList MaterialLines(const DesignStats& stats);

// The game's name for a commodity, worked out from its id when the game's
// commodity list has not been loaded (it never is offline).
QString CommodityLabel(int cid);

// All of it as plain text: the game's report, the berths, the materials and
// the caveat.
QString StatsText(const DesignStats& stats);

// The words to show with any of it.
extern const char* kStatsCaveat;

#endif
