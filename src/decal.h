//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - decals: one image put onto the hull at a chosen place.
//
// The game has a Decal part. It is drawn after the hull, so a decal can lie
// over the hull without cutting it. The tool takes the hull faces under the
// image, trims them exactly to the image's edges (the game repeats a texture
// past its edge), gives them texture coordinates that run from 0 to 1 over
// the image, lifts them 0.02 ft off the hull so that they never flicker, and
// makes them into one Decal part. The image goes into a free slot of the
// design's texture palette, at most 256 by 256 pixels.
//
// Two ways to project:
//   Flat - like a slide projector: straight along one direction. Good for
//          flat sides and buildings; it stretches on steep slopes.
//   Wrap - round the hull's long axis, so that the image keeps its width
//          round a curved fuselage.
// Faces turned away from the projection, and faces hidden behind other parts
// of the hull, are left out.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_DECAL_H
#define SHIPGEN_DECAL_H

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "audesign.h"

enum class DecalProjection
{
	Flat,
	Wrap
};

enum class DecalSides
{
	One,			// Only where it was aimed.
	Both,			// And a copy on the other side (port and starboard), reading the right way round.
	BothMirrored	// And a mirror image on the other side.
};

struct DecalOptions
{
	QString				image;				// An image file.

	DecalProjection		projection = DecalProjection::Flat;

	// Where the projector stands, seen from the design: +x is starboard, +y
	// the bow and +z up. Need not be a unit vector.
	double				fromX = 1, fromY = 0, fromZ = 0;

	// The point aimed at. The decal's centre is where a ray along the
	// projection through this point first meets the hull. Without one, the
	// middle of the hull is aimed at.
	bool				haveAim = false;
	double				aimX = 0, aimY = 0, aimZ = 0;

	double				width = 20.0;		// Feet across the image.
	double				rotate = 0.0;		// Degrees, anticlockwise as seen by the projector.
	bool				mirror = false;		// Flip the image left to right.
	DecalSides			sides = DecalSides::One;

	double				selfLit = 0.0;		// 0 to 1: how much the decal lights itself.
	double				opacity = 1.0;		// Below 1 makes it see-through.

	bool				skip = false;		// Left out of the build (the designer's Build page).

	bool IsSet() const
	{
		return !image.isEmpty();
	}

	QJsonObject			ToJson() const;
	static DecalOptions	FromJson(const QJsonObject& obj);
};

struct DecalResult
{
	int					faces = 0;			// Faces the decal(s) added.
	int					parts = 0;			// Decal parts added.
	int					slot = -1;			// Palette slot the image went into.
	QStringList			notes;
};

// Names for the command line, the plan file and the combo boxes.
QString				DecalProjectionName(DecalProjection projection);
bool				DecalProjectionFromName(const QString& name, DecalProjection& projection);
QString				DecalSidesName(DecalSides sides);
bool				DecalSidesFromName(const QString& name, DecalSides& sides);

// Reads +x, -y, top, bottom, port, starboard, front, back, or "x,y,z".
bool				DecalFromDirection(const QString& text, double& x, double& y, double& z);

// Adds the decal(s) to the design. Leaves the design alone on failure.
bool				AddDecal(AuDesign& design, const DecalOptions& opts, DecalResult& result, QString& error);

// Adds every decal of a list that is not skipped. Each one that cannot be
// placed gives a warning; the rest still go on.
struct DecalsApplied
{
	int					parts = 0;
	int					faces = 0;
	QStringList			notes;
	QStringList			warnings;
};
DecalsApplied		ApplyDecals(AuDesign& design, const QList<DecalOptions>& decals);

// Takes away the decal parts this tool made (named "Decal ..."), so that a
// changed list can be put on again. Returns how many went.
int					RemoveDecals(AuDesign& design);

// Every face of every part, as the game counts them.
int					DesignFaceCount(const AuDesign& design);

// How full the design is, as a percentage of the game's blueprint size
// limit (14,000,000 bytes uncompressed).
double				DesignSizePercent(const AuDesign& design);

#endif // SHIPGEN_DECAL_H
