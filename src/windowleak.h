//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - holes round the windows.
//
// The game does not draw the hull from inside. Between a room's wall and the
// glass of a window lies the hull's inside wherever the window's sides are
// open, or where a face of the hull in front of the window was not taken
// into the glass, and there the sky shows through (the user's screenshots,
// 2026-11-13). The check looks from each window's room at a grid over the
// window and round it: what a ray meets first is right when it is glass, a
// wall, or the hull from outside, and a leak when it is the hull from inside
// or nothing at all. Sealing lays an inward facing copy of every hull face
// seen from inside, a little in from it, as a mesh part ("Window Lining").
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_WINDOWLEAK_H
#define SHIPGEN_WINDOWLEAK_H

#include <QList>
#include <QPair>
#include <QStringList>

#include "audesign.h"

struct WindowLeaks
{
	int					windows = 0;		// Windows with a room to look from.
	int					leaky = 0;
	int					rays = 0;
	int					leaks = 0;
	int					escapes = 0;		// Of the leaks, rays that met nothing.
	QStringList			lines;				// One per leaky window.
	QList<QPair<quint16, int> >	insideFaces;	// (hull part, face) seen from inside.
};

// 'grid' rays across each window (and its margin) from each place looked
// from; sealing looks with more, and again with other grids.
WindowLeaks		FindWindowLeaks(const AuDesign& design, int grid = 10, const QStringList& skipRooms = QStringList());

// Covers the hull faces seen from inside through the windows. Returns how
// many faces the lining has (0: nothing to seal). The windows of the rooms
// in 'skipRooms' are left as they are: under a canopy the pilot looks out
// through the hull's undrawn inside, and a lining would stand in the way.
int				SealWindowLeaks(AuDesign& design, QStringList& notes, const QStringList& skipRooms = QStringList());

#endif // SHIPGEN_WINDOWLEAK_H
