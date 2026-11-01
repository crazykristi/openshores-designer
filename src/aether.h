//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the Aether kit: blueprints in a style that mixes high technology
// and magic (brass and copper machinery, gothic stone, crystal, and fields of
// light), after the user's Aetherite Capitol.
//
// How the look is made, learned from the example blueprints
// (docs/TEXTURE_LIBRARY.md): every face carries a texture slot (the slots
// hold textures from the game's own library), a paint colour with an alpha
// for how much it covers the texture, an opacity, self-illumination, shine,
// reflectivity, a vertex glow colour, and one of the game's animated surface
// effects. An energy field is a thin sheet with paint at partial alpha,
// opacity below 1, self-illumination 1, a glow, and an effect such as Shield.
//
// The solid shapes become hull parts (closed, wound as the game wants hulls);
// the sheets of light become two-sided mesh parts, which are decoration and
// do not count as hull. Ships get their rooms, doors and stations from the
// generator, buildings from the building generator, inside the main body;
// the ornaments are added afterwards so they do not change the layout.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_AETHER_H
#define SHIPGEN_AETHER_H

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

#include "auscface.h"
#include "aupoint.h"

class AuDesign;

// How a face looks.
struct KitLook
{
	int		slot = 0;			// Texture palette slot (0, or 2 to 14; 1 is the construction material).
	QColor	paint;				// Invalid for none; the alpha is how much of the texture it covers.
	float	opacity = 1.f;
	float	selfLit = 0.f;		// 0 lit by the world, 1 fully self-lit.
	float	shine = 0.f;		// Specular exponent.
	float	reflect = 0.f;		// Specular intensity, 0 to 1.
	int		effect = 0;			// SURFACE_EFFECT (aumath/auscface.h).
	QColor	glow;				// Invalid for none.
	double	texFeet = 20.0;		// How many feet one repeat of the texture covers.
};

// The looks of the kit, by name: "brass", "copper", "stone", "obsidian",
// "jade", "gothic", "magmium", "glass", "aether" (blue field), "arcane"
// (violet field), "ember" (glowing orange core), "gold-glow", ...
KitLook			KitLookNamed(const QString& name);
QStringList		KitLookNames();

// The texture palette the kit's looks use (library texture ids per slot).
void			SetKitPalette(AuDesign& design);

// Gives every face the look, with texture coordinates for its slot.
void			ApplyKitLook(AuScFaceList& faces, const KitLook& look);

// Closed shapes, wound as hulls. Angles in degrees; z is up.
AuScFaceList	KitBox(const AuPoint& mn, const AuPoint& mx);
// A prism or frustum standing on 'base': 'sides' sides, radius r0 at the
// bottom and r1 at the top (0 for a point), turned 'phase' degrees.
AuScFaceList	KitFrustum(const AuPoint& base, double r0, double r1, double height, int sides, double phase = 0);
// The same lying along Y, from 'base' forwards (for ship bodies).
AuScFaceList	KitFrustumY(const AuPoint& base, double r0, double r1, double length, int sides, double phase = 0,
		double squashZ = 1.0);
AuScFaceList	KitTorus(const AuPoint& centre, double radius, double tube, int segments, int sides);
// A cog wheel lying flat (axis Z) at 'centre'.
AuScFaceList	KitGear(const AuPoint& centre, double radius, int teeth, double toothDepth, double thickness);
AuScFaceList	KitDome(const AuPoint& base, double radius, int rings, int segments, double squash = 1.0);
// A crystal: a prism with a pyramid on each end.
AuScFaceList	KitCrystal(const AuPoint& base, double radius, double body, double tip, int sides);

// Turns faces about an axis through a point (degrees).
void			KitRotate(AuScFaceList& faces, const AuPoint& pivot, const AuPoint& axis, double degrees);

// A flat ring or disc of light (two-sided), lying flat at 'centre'.
AuScFaceList	KitHalo(const AuPoint& centre, double inner, double outer, int segments);
// A flat quad of light (two-sided) from four corners in order.
AuScFaceList	KitSheet(const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& d);

// Adds faces as a part: a hull part (closed solids) or a mesh part
// (decoration, sheets of light).
quint16			AddKitHull(AuDesign& design, AuScFaceList faces, const KitLook& look, const QString& group = QString());
quint16			AddKitMesh(AuDesign& design, AuScFaceList faces, const KitLook& look, const QString& group = QString());

// The blueprints of the kit.
struct KitItem
{
	QString		key;
	QString		label;
	QString		kind;			// "ship" or "building"
	QString		description;
};
QList<KitItem>	KitItems();

// Builds one blueprint of the kit and saves it in 'outDir' as <label>.SoH.
// 'log' gets what happened, including the engine's report.
bool			BuildKitItem(const QString& key, const QString& outDir, QStringList& log, QString* savedAs = 0);

class AuBdDesign;
struct BuildingBrief;

// How a kit building is made: the building template it starts from, the
// floors, what the brief changes (industry and volumes), the body (the hull
// the building generator fills) and the ornaments (mesh parts, added after).
struct KitBuildingRecipe
{
	QString		templateKey;
	int			floors = 1;
	std::function<void(BuildingBrief&)>	brief;
	std::function<void(AuBdDesign&)>	body;
	std::function<void(AuBdDesign&)>	ornaments;
};

#endif
