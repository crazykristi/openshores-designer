//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - what the helm and the captain can see from their seats.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
//
// Rays are cast from the eye of whoever sits at a station, in a fan round the
// way the station faces, and the first thing each meets is noted: window
// glass, the hull (whose inside is not drawn, so it is seen through), nothing
// at all, or something that stands in the way (a room's wall or ceiling, a
// frame, furniture, the station's own console).
//
#ifndef SHIPGEN_VIEW_H
#define SHIPGEN_VIEW_H

#include <QList>
#include <QMap>
#include <QString>

#include "aupoint.h"

class AuScParts;

// One part of the view: a band of directions.
struct ViewSector
{
	QString				name;			// "ahead", "sides", "up", "down ahead".
	int					rays = 0;
	int					glass = 0;		// First met window glass.
	int					hull = 0;		// First met the hull itself (its inside is not drawn).
	int					open = 0;		// Met nothing.
	QMap<QString, int>	blockers;		// What stood in the way, and how often.

	int		SeesOut() const { return glass + hull + open; }
	int		Percent(int n) const { return rays ? qRound(100.0 * n / rays) : 0; }
};

struct StationView
{
	quint16				partId = 0;
	QString				type;			// "Helm Station", ...
	QString				group;
	AuPoint				eye;
	AuPoint				facing;			// Level, unit.
	QList<ViewSector>	sectors;

	const ViewSector*	Sector(const QString& name) const
	{
		for(const ViewSector& s : sectors)
			if(s.name == name)
				return &s;
		return 0;
	}
	// Ahead is what a pilot needs: at least half of it out.
	bool				SeesAhead() const
	{
		const ViewSector* s = Sector("ahead");
		return s && s->rays && s->SeesOut() * 2 >= s->rays;
	}
};

// The helm and command stations (consoles and integrated desks too), each
// looked out from at 'eyeAboveSeat' feet over its seat.
QList<StationView>	CheckStationViews(const AuScParts& parts, double eyeAboveSeat = 2.4);

#endif // SHIPGEN_VIEW_H
