//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - painting a model.
//
// The engine wants texture coordinates on the faces of the hull before it
// will show an image on it. Doing that face by face in the designer is slow
// and easy to get wrong, so the tool does it by itself: pick one or more
// images (or plain colours), pick how they should be wrapped around the
// model, and every hull part gets coordinates.
//
// A face carries a texture index, not a texture of its own. There are 14
// user slots in the engine palette, so a model can have up to 14 different
// textures. Assigning one slot per hull part - rather than per face - is
// what makes this practical: a model that is already split into parts (a
// hull, a ring, a stand) gets one texture each, and a single model gets one
// texture over all of it.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_PAINT_H
#define SHIPGEN_PAINT_H

#include <QColor>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

#include "audesign.h"

// How the texture is put onto the faces.
enum class PaintProjection
{
	ByFace,			// Every face is laid flat on its own, aligned to the grid. No stretching.
	Planar,			// One flat projection, usually from above. Good for decks and sites.
	Cylindrical,	// Wrapped around the long axis. Good for fuselages.
	Spherical,		// Wrapped around a ball. Good for round hulls.
	Keep			// Leave the coordinates alone; only set the texture index.
};

struct PaintOptions
{
	// Image files, or colours written as #rrggbb. The engine has 14 user
	// texture slots, so at most 14 of these are used.
	QStringList			textures;

	PaintProjection		projection = PaintProjection::ByFace;

	// Feet per texture repeat. A smaller number makes the texture smaller.
	double				size = 8.0;

	// Give each hull part the next texture in the list. With one texture
	// this makes no difference.
	bool				perPart = true;

	// True when there is anything to do at all.
	bool IsSet() const
	{
		return !textures.isEmpty();
	}
};

// The names that the command line and the combo boxes use.
QStringList			PaintProjectionNames();
QString				PaintProjectionName(PaintProjection projection);
bool				PaintProjectionFromName(const QString& name, PaintProjection& projection);

// Reads an image or a colour. Colours are made into a small panel texture so
// that they are not just a flat wash.
bool				LoadPaintImage(const QString& spec, QImage& image, QString& error);

// The colour a texture averages out to, for swatches and the preview.
QColor				PaintAverageColour(const QImage& image);

// Paints every hull part of a design. 'notes' receives a line per texture,
// which the tools show to the user. Leaves the design alone when there is
// nothing to paint.
bool				PaintDesign(AuDesign& design, const PaintOptions& opts, QStringList* notes, QString& error);

#endif // SHIPGEN_PAINT_H
