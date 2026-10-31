//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the Aether kit.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "aether.h"

#include <QDir>
#include <QMap>
#include <QSet>
#include <cmath>
#include <functional>

#include "aubddesign.h"
#include "auscdesign.h"
#include "auscparts.h"
#include "auglsl.h"
#include "building.h"
#include "generate.h"
#include "interior.h"
#include "shipgen.h"
#include "sohfile.h"

namespace {

const double kPi = 3.14159265358979323846;

double Rad(double degrees)
{
	return degrees * kPi / 180.0;
}

// Adds a triangle with its face normal.
void Tri(AuScFaceList& faces, const AuPoint& a, const AuPoint& b, const AuPoint& c)
{
	AuPoint n = (b - a).Cross(c - a);
	const double len = n.Length();
	if(len < 1e-9)
		return;
	n = n / len;
	faces.AddFace(AuScFaceTri(AuScPoint(a, 0.f, 0.f, n), AuScPoint(b, 0.f, 0.f, n), AuScPoint(c, 0.f, 0.f, n)));
}

void Quad(AuScFaceList& faces, const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& d)
{
	Tri(faces, a, b, c);
	Tri(faces, a, c, d);
}

// A closed solid from rings of points: each ring is a loop, consecutive
// rings are joined, and the two end rings are capped from their middles
// (or meet in a point when a ring has one point).
AuScFaceList Loft(const QList<QList<AuPoint> >& rings)
{
	AuScFaceList faces;
	for(int r = 0; r + 1 < rings.size(); ++r)
	{
		const QList<AuPoint>& a = rings.at(r);
		const QList<AuPoint>& b = rings.at(r + 1);
		if(a.size() == 1 && b.size() == 1)
			continue;
		if(a.size() == 1)
		{
			for(int i = 0; i < b.size(); ++i)
				Tri(faces, a[0], b[(i + 1) % b.size()], b[i]);
			continue;
		}
		if(b.size() == 1)
		{
			for(int i = 0; i < a.size(); ++i)
				Tri(faces, a[i], a[(i + 1) % a.size()], b[0]);
			continue;
		}
		for(int i = 0; i < a.size(); ++i)
		{
			const int j = (i + 1) % a.size();
			Quad(faces, a[i], a[j], b[j], b[i]);
		}
	}
	auto cap = [&faces](const QList<AuPoint>& ring, bool first) {
		if(ring.size() < 3)
			return;
		AuPoint c(0, 0, 0);
		for(const AuPoint& p : ring)
			c += p;
		c = c / double(ring.size());
		for(int i = 0; i < ring.size(); ++i)
		{
			const int j = (i + 1) % ring.size();
			if(first)
				Tri(faces, c, ring[j], ring[i]);
			else
				Tri(faces, c, ring[i], ring[j]);
		}
	};
	cap(rings.first(), true);
	cap(rings.last(), false);
	OrientAsHull(faces);
	return faces;
}

QList<AuPoint> RingZ(const AuPoint& centre, double radius, int sides, double phase, double squashY = 1.0)
{
	QList<AuPoint> ring;
	if(radius <= 1e-6)
		return ring << centre;
	for(int i = 0; i < sides; ++i)
	{
		const double a = Rad(phase) + 2 * kPi * i / sides;
		ring << AuPoint(centre.x + radius * cos(a), centre.y + radius * sin(a) * squashY, centre.z);
	}
	return ring;
}

QList<AuPoint> RingY(const AuPoint& centre, double radius, int sides, double phase, double squashZ)
{
	QList<AuPoint> ring;
	if(radius <= 1e-6)
		return ring << centre;
	for(int i = 0; i < sides; ++i)
	{
		const double a = Rad(phase) + 2 * kPi * i / sides;
		ring << AuPoint(centre.x + radius * cos(a), centre.y, centre.z + radius * sin(a) * squashZ);
	}
	return ring;
}

// Both sides of a set of faces, for sheets that are seen from both sides.
AuScFaceList TwoSided(const AuScFaceList& front)
{
	AuScFaceList faces = front;
	AuScFaceList back = front;
	back.ReverseTriangleFacing();
	back.ReverseVertexNormals();
	faces.AddFaces(back);
	return faces;
}

}	// namespace

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Looks.

// Library texture ids (docs/TEXTURE_LIBRARY.md) by palette slot. Slot 1 is
// the construction material and is left alone.
static const quint16 kPalette[SCTEXI_Count] = {
	36,		// 0 Gold
	0,		// 1 (construction material)
	40,		// 2 Copper Tubes
	49,		// 3 Marble Green Gold
	63,		// 4 Marble Obsidian
	108,	// 5 Plate Magmium Box
	32,		// 6 Black Techno Gothic Tile
	60,		// 7 Metal Scratch Bumps
	106,	// 8 Metal Brushed
	116,	// 9 Stone
	90,		// 10 Tile Jade
	92,		// 11 Grill Copper
	78,		// 12 Stained Glass Sun Moon
	53,		// 13 Ceiling Gold
	61,		// 14 Metal Scratch Rust
};

void SetKitPalette(AuDesign& design)
{
	AuDesignTex palette[SCTEXI_Count];
	design.TexturePalette(palette);
	for(int i = 0; i < SCTEXI_Count; ++i)
		if(kPalette[i])
			palette[i].SetTexLibId(kPalette[i]);
	design.SetTexturePalette(palette);
}

QStringList KitLookNames()
{
	return QStringList() << "brass" << "gold-trim" << "copper" << "grill" << "stone" << "marble" << "obsidian"
			<< "magmium" << "gothic" << "steel" << "rust" << "jade" << "cog" << "stained-sun" << "glass" << "glass-pale"
			<< "aether" << "aether-soft" << "arcane" << "ember" << "crystal" << "drive" << "capacitor-gold" << "power-gold";
}

KitLook KitLookNamed(const QString& name)
{
	KitLook l;
	if(name == "brass")				{ l.slot = 0; l.shine = 40; l.reflect = .5f; l.texFeet = 12; }
	else if(name == "gold-trim")	{ l.slot = 13; l.shine = 60; l.reflect = .7f; l.texFeet = 10; }
	else if(name == "copper")		{ l.slot = 2; l.shine = 30; l.reflect = .4f; l.texFeet = 10; }
	else if(name == "grill")		{ l.slot = 11; l.texFeet = 8; }
	else if(name == "stone")		{ l.slot = 9; l.texFeet = 30; }
	else if(name == "marble")		{ l.slot = 3; l.shine = 30; l.reflect = .3f; l.texFeet = 24; }
	else if(name == "obsidian")		{ l.slot = 4; l.shine = 80; l.reflect = .6f; l.texFeet = 24; }
	else if(name == "magmium")		{ l.slot = 5; l.shine = 40; l.reflect = .4f; l.texFeet = 16; }
	else if(name == "gothic")		{ l.slot = 6; l.texFeet = 20; }
	else if(name == "steel")		{ l.slot = 8; l.shine = 50; l.reflect = .5f; l.texFeet = 16; }
	else if(name == "rust")			{ l.slot = 14; l.texFeet = 16; }
	else if(name == "jade")			{ l.slot = 10; l.shine = 40; l.reflect = .4f; l.texFeet = 12; }
	// Worn, dark bronze for gears: scratched metal under a brown tint. The
	// user found brass gears too smooth and too bright.
	else if(name == "cog")			{ l.slot = 7; l.paint = QColor(120, 84, 40, 140); l.shine = 12; l.reflect = .15f; l.texFeet = 6; }
	// Plain tinted glass for domes and roofs. Stained glass textures look
	// too chaotic on a curved dome, the user found.
	else if(name == "glass")
	{
		l.paint = QColor(255, 196, 110, 235);
		l.opacity = .55f;
		l.selfLit = .25f;
		l.shine = 80;
		l.reflect = .6f;
	}
	else if(name == "glass-pale")
	{
		l.paint = QColor(190, 225, 235, 235);
		l.opacity = .5f;
		l.selfLit = .2f;
		l.shine = 90;
		l.reflect = .7f;
	}
	else if(name == "stained-sun")	{ l.slot = 12; l.opacity = .85f; l.selfLit = .5f; l.glow = QColor(255, 216, 144); l.texFeet = 16; }
	else if(name == "aether")
	{
		l.paint = QColor(0, 170, 255, 140);
		l.opacity = .45f;
		l.selfLit = 1.f;
		l.glow = QColor(0, 170, 255);
		l.effect = EFF_Shield;
	}
	// A fainter field, for big sheets of light: at full strength they are
	// too bright in the game.
	else if(name == "aether-soft")
	{
		l.paint = QColor(0, 170, 255, 70);
		l.opacity = .25f;
		l.selfLit = .45f;
		l.glow = QColor(0, 90, 140);
		l.effect = EFF_Shield;
	}
	else if(name == "arcane")
	{
		l.paint = QColor(160, 64, 255, 150);
		l.opacity = .5f;
		l.selfLit = 1.f;
		l.glow = QColor(176, 96, 255);
		l.effect = EFF_LifeSupport;
	}
	else if(name == "ember")
	{
		l.paint = QColor(255, 122, 0, 220);
		l.selfLit = 1.f;
		l.glow = QColor(255, 85, 0);
		l.effect = EFF_PowerPlant;
	}
	else if(name == "crystal")
	{
		l.paint = QColor(159, 232, 255, 170);
		l.opacity = .7f;
		l.selfLit = .6f;
		l.shine = 100;
		l.reflect = 1.f;
		l.glow = QColor(96, 208, 255);
		l.effect = EFF_Sensor;
	}
	else if(name == "drive")
	{
		l.paint = QColor(64, 192, 255, 255);
		l.selfLit = 1.f;
		l.glow = QColor(64, 192, 255);
		l.effect = EFF_ManeuverDrive;
	}
	else if(name == "capacitor-gold")	{ l.slot = 13; l.shine = 40; l.reflect = .5f; l.effect = EFF_Capacitor; l.texFeet = 10; }
	else if(name == "power-gold")		{ l.slot = 13; l.shine = 40; l.reflect = .5f; l.effect = EFF_PowerPlant; l.texFeet = 10; }
	return l;
}

void ApplyKitLook(AuScFaceList& faces, const KitLook& look)
{
	if(faces.isEmpty())
		return;
	AuScSelectInfo all;
	for(int i = 0; i < faces.size(); ++i)
		all.m_Faces.insert(i);
	faces.TextureUnwrap(all, quint8(look.slot), look.texFeet, AuScFaceList::UNWRAP_ByFace, 45.f, true, 0,
			AuMatrix4x4::IdentityMatrix());
	for(AuScFace* f : faces)
	{
		f->SetTextureIndex(quint8(look.slot));
		f->SetPaint(look.paint.isValid() ? look.paint : QColor(0, 0, 0, 0));
		f->SetOpacity(look.opacity);
		f->SetSelfIllumination(look.selfLit);
		f->SetShine(qMax(1.f, look.shine));
		f->SetReflectivity(look.reflect);
		f->SetEffect(SURFACE_EFFECT(look.effect));
		if(look.glow.isValid())
			for(int v = 0; v < f->VertexCount(); ++v)
				f->SetVertexGlow(v, look.glow);
	}
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Shapes.

AuScFaceList KitBox(const AuPoint& mn, const AuPoint& mx)
{
	AuScFaceList faces = BoxFaces(mn, mx);
	OrientAsHull(faces);
	return faces;
}

AuScFaceList KitFrustum(const AuPoint& base, double r0, double r1, double height, int sides, double phase)
{
	QList<QList<AuPoint> > rings;
	rings << RingZ(base, r0, sides, phase) << RingZ(base + AuPoint(0, 0, height), r1, sides, phase);
	return Loft(rings);
}

AuScFaceList KitFrustumY(const AuPoint& base, double r0, double r1, double length, int sides, double phase, double squashZ)
{
	QList<QList<AuPoint> > rings;
	rings << RingY(base, r0, sides, phase, squashZ) << RingY(base + AuPoint(0, length, 0), r1, sides, phase, squashZ);
	return Loft(rings);
}

AuScFaceList KitTorus(const AuPoint& centre, double radius, double tube, int segments, int sides)
{
	AuScFaceList faces;
	auto at = [&](int i, int j) {
		const double u = 2 * kPi * i / segments, v = 2 * kPi * j / sides;
		const double r = radius + tube * cos(v);
		return AuPoint(centre.x + r * cos(u), centre.y + r * sin(u), centre.z + tube * sin(v));
	};
	for(int i = 0; i < segments; ++i)
		for(int j = 0; j < sides; ++j)
			Quad(faces, at(i, j), at(i + 1, j), at(i + 1, j + 1), at(i, j + 1));
	OrientAsHull(faces);
	return faces;
}

AuScFaceList KitGear(const AuPoint& centre, double radius, int teeth, double toothDepth, double thickness)
{
	// The outline: each tooth is a root, two tips and a root again.
	QList<AuPoint> outline;
	const double inner = radius - toothDepth;
	for(int t = 0; t < teeth; ++t)
	{
		const double a0 = 2 * kPi * t / teeth;
		const double step = 2 * kPi / teeth;
		const double rs[4] = { inner, radius, radius, inner };
		const double as[4] = { a0, a0 + step * .2, a0 + step * .5, a0 + step * .7 };
		for(int k = 0; k < 4; ++k)
			outline << AuPoint(centre.x + rs[k] * cos(as[k]), centre.y + rs[k] * sin(as[k]), centre.z - thickness / 2);
	}
	QList<AuPoint> top;
	for(const AuPoint& p : outline)
		top << p + AuPoint(0, 0, thickness);
	QList<QList<AuPoint> > rings;
	rings << outline << top;
	return Loft(rings);
}

AuScFaceList KitDome(const AuPoint& base, double radius, int rings, int segments, double squash)
{
	QList<QList<AuPoint> > loops;
	for(int r = 0; r <= rings; ++r)
	{
		const double a = (kPi / 2) * r / rings;
		const double rr = radius * cos(a);
		loops << RingZ(base + AuPoint(0, 0, radius * squash * sin(a)), r == rings ? 0.0 : rr, segments, 0);
	}
	return Loft(loops);
}

AuScFaceList KitCrystal(const AuPoint& base, double radius, double body, double tip, int sides)
{
	QList<QList<AuPoint> > loops;
	loops << (QList<AuPoint>() << base)
		  << RingZ(base + AuPoint(0, 0, tip), radius, sides, 0)
		  << RingZ(base + AuPoint(0, 0, tip + body), radius, sides, 0)
		  << (QList<AuPoint>() << base + AuPoint(0, 0, 2 * tip + body));
	return Loft(loops);
}

void KitRotate(AuScFaceList& faces, const AuPoint& pivot, const AuPoint& axis, double degrees)
{
	AuMatrix4x4 t;
	t.LoadIdentity();
	t.Translate(pivot);
	t.RotateAxis(Rad(degrees), axis.x, axis.y, axis.z);
	t.Translate(AuPoint(0, 0, 0) - pivot);
	faces.Transform(t);
}

// Fields of light are thin closed slabs, so that a ship can carry them as
// hull (the game wants every mesh part of a ship inside its hull).
static const double kSheetThickness = .2;

AuScFaceList KitHalo(const AuPoint& centre, double inner, double outer, int segments)
{
	const double h = kSheetThickness / 2;
	if(inner <= 1e-6)
		return KitFrustum(centre - AuPoint(0, 0, h), outer, outer, 2 * h, segments);
	AuScFaceList faces;
	auto at = [&](double r, int i, double z) {
		const double a = 2 * kPi * i / segments;
		return AuPoint(centre.x + r * cos(a), centre.y + r * sin(a), centre.z + z);
	};
	for(int i = 0; i < segments; ++i)
	{
		Quad(faces, at(inner, i, h), at(outer, i, h), at(outer, i + 1, h), at(inner, i + 1, h));
		Quad(faces, at(inner, i, -h), at(inner, i + 1, -h), at(outer, i + 1, -h), at(outer, i, -h));
		Quad(faces, at(outer, i, -h), at(outer, i + 1, -h), at(outer, i + 1, h), at(outer, i, h));
		Quad(faces, at(inner, i, -h), at(inner, i, h), at(inner, i + 1, h), at(inner, i + 1, -h));
	}
	OrientAsHull(faces);
	return faces;
}

AuScFaceList KitSheet(const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& d)
{
	AuPoint n = (b - a).Cross(d - a);
	const double len = n.Length();
	n = len > 1e-9 ? n / len * (kSheetThickness / 2) : AuPoint(0, 0, kSheetThickness / 2);
	QList<QList<AuPoint> > rings;
	rings << (QList<AuPoint>() << a - n << b - n << c - n << d - n)
		  << (QList<AuPoint>() << a + n << b + n << c + n << d + n);
	return Loft(rings);
}

quint16 AddKitHull(AuDesign& design, AuScFaceList faces, const KitLook& look, const QString& group)
{
	ApplyKitLook(faces, look);
	AuScHull part;
	part.AddFaces(faces);
	if(!group.isEmpty())
		part.SetGroupName(group);
	return part.IsValid() ? design.Parts().AddPart(part) : 0;
}

quint16 AddKitMesh(AuDesign& design, AuScFaceList faces, const KitLook& look, const QString& group)
{
	ApplyKitLook(faces, look);
	AuScMesh part;
	part.AddFaces(faces);
	if(!group.isEmpty())
		part.SetGroupName(group);
	return part.IsValid() ? design.Parts().AddPart(part) : 0;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The blueprints.

namespace {

KitLook L(const char* name)
{
	return KitLookNamed(name);
}

// A ship body lying along Y: (y, radius, squash in z) stations, 8 sides.
AuScFaceList ShipBody(const QList<QVector<double> >& stations, double zMiddle, int sides = 8)
{
	QList<QList<AuPoint> > rings;
	for(const QVector<double>& s : stations)
		rings << RingY(AuPoint(0, s[0], zMiddle), s[1], sides, 180.0 / sides, s[2]);
	return Loft(rings);
}

// Ships: the body is the hull the generator fills; the ornaments come after.
struct ShipRecipe
{
	QString		templateKey;
	std::function<void(AuScDesign&)>	body;
	std::function<void(AuScDesign&)>	ornaments;
};

struct BuildingRecipe
{
	QString		templateKey;
	int			floors = 1;
	std::function<void(AuBdDesign&)>	body;
	std::function<void(AuBdDesign&)>	ornaments;
};

// A gear standing upright on the side of a ship or building, facing along X.
void SideGear(AuDesign& d, const AuPoint& centre, double radius, int teeth, const char* look)
{
	AuScFaceList g = KitGear(centre, radius, teeth, radius * .18, radius * .2);
	KitRotate(g, centre, AuPoint(0, 1, 0), 90);
	AddKitMesh(d, g, L(look));
	AuScFaceList hub = KitFrustum(centre - AuPoint(0, 0, radius * .15), radius * .25, radius * .25, radius * .3, 8);
	KitRotate(hub, centre, AuPoint(0, 1, 0), 90);
	AddKitMesh(d, hub, L("ember"));
}

QMap<QString, ShipRecipe> ShipRecipes()
{
	QMap<QString, ShipRecipe> out;

	// A small courier: a dark gothic hull, a brass band, sails of light.
	{
		ShipRecipe r;
		r.templateKey = "patrol_cutter";
		r.body = [](AuScDesign& d) {
			QList<QVector<double> > s;
			s << QVector<double>{ -60, 7, .8 } << QVector<double>{ -48, 13, .8 } << QVector<double>{ -10, 16, .85 }
			  << QVector<double>{ 20, 15, .8 } << QVector<double>{ 48, 7, .7 } << QVector<double>{ 62, 0, 1 };
			AddKitHull(d, ShipBody(s, 14), L("gothic"));
		};
		r.ornaments = [](AuScDesign& d) {
			AuScFaceList band = KitTorus(AuPoint(0, 0, 14), 16.5, 1.6, 24, 8);
			KitRotate(band, AuPoint(0, 0, 14), AuPoint(1, 0, 0), 90);
			AddKitMesh(d, band, L("brass"));
			AuScFaceList band2 = KitTorus(AuPoint(0, -36, 14), 14.5, 1.2, 24, 8);
			KitRotate(band2, AuPoint(0, -36, 14), AuPoint(1, 0, 0), 90);
			AddKitMesh(d, band2, L("gold-trim"));
			// Masts and sails of light.
			AddKitMesh(d, KitFrustum(AuPoint(0, 4, 26), .9, .5, 26, 6), L("brass"));
			AddKitMesh(d, KitFrustum(AuPoint(0, -26, 25), .8, .4, 20, 6), L("brass"));
			AddKitMesh(d, KitSheet(AuPoint(0, -2, 28), AuPoint(0, 14, 30), AuPoint(0, 10, 50), AuPoint(0, -1, 50)), L("aether"));
			AddKitMesh(d, KitSheet(AuPoint(0, -32, 27), AuPoint(0, -18, 29), AuPoint(0, -21, 44), AuPoint(0, -31, 44)), L("aether"));
			// The drive: a glowing ring and a disc at the stern.
			AuScFaceList ring = KitTorus(AuPoint(0, -61, 14), 6, 1, 20, 6);
			KitRotate(ring, AuPoint(0, -61, 14), AuPoint(1, 0, 0), 90);
			AddKitMesh(d, ring, L("copper"));
			AuScFaceList disc = KitHalo(AuPoint(0, -62, 14), 0, 5.5, 20);
			KitRotate(disc, AuPoint(0, -62, 14), AuPoint(1, 0, 0), 90);
			AddKitMesh(d, disc, L("drive"));
			SideGear(d, AuPoint(16, -20, 13), 6, 12, "brass");
			SideGear(d, AuPoint(-16, -20, 13), 6, 12, "brass");
			// Crystal fins.
			for(int side = -1; side <= 1; side += 2)
			{
				AuScFaceList fin = KitCrystal(AuPoint(side * 13, -44, 6), 1.6, 10, 4, 6);
				KitRotate(fin, AuPoint(side * 13, -44, 6), AuPoint(0, 1, 0), side * 55);
				AddKitMesh(d, fin, L("arcane"));
			}
		};
		out.insert("aether-skiff", r);
	}

	// A cruiser of brass and copper with an aether core and side wheels.
	{
		ShipRecipe r;
		r.templateKey = "military_frigate";
		r.body = [](AuScDesign& d) {
			QList<QVector<double> > s;
			s << QVector<double>{ -150, 22, .7 } << QVector<double>{ -130, 34, .7 } << QVector<double>{ -40, 40, .72 }
			  << QVector<double>{ 60, 36, .7 } << QVector<double>{ 120, 20, .6 } << QVector<double>{ 150, 4, .5 };
			AddKitHull(d, ShipBody(s, 30), L("magmium"));
		};
		r.ornaments = [](AuScDesign& d) {
			// The bridge tower.
			AddKitMesh(d, KitFrustum(AuPoint(0, 30, 52), 12, 9, 14, 8, 22.5), L("gothic"));
			AddKitMesh(d, KitFrustum(AuPoint(0, 30, 66), 9.5, 9.5, 2, 8, 22.5), L("gold-trim"));
			AddKitMesh(d, KitHalo(AuPoint(0, 30, 70), 11, 15, 32), L("aether"));
			// Copper pipes along the flanks.
			for(int side = -1; side <= 1; side += 2)
				for(int k = 0; k < 2; ++k)
					AddKitMesh(d, KitFrustumY(AuPoint(side * (37 - k * 4), -120, 22 + k * 10), 1.8, 1.8, 170, 8), L("copper"));
			// Paddle wheels of dark bronze, glowing hubs, set into the flat
			// sides of the hull (at 36 ft) so they stand out only a little.
			SideGear(d, AuPoint(37.5, -50, 30), 14, 16, "cog");
			SideGear(d, AuPoint(-37.5, -50, 30), 14, 16, "cog");
			// The aether core on the back.
			AddKitMesh(d, KitDome(AuPoint(0, -60, 50), 10, 4, 16), L("crystal"));
			AuScFaceList orbit = KitTorus(AuPoint(0, -60, 54), 15, 1, 32, 6);
			KitRotate(orbit, AuPoint(0, -60, 54), AuPoint(1, 0, 0), 25);
			AddKitMesh(d, orbit, L("gold-trim"));
			AddKitMesh(d, KitHalo(AuPoint(0, -60, 54), 16, 22, 32), L("arcane"));
			// Three drives.
			const AuPoint drives[3] = { AuPoint(0, -151, 30), AuPoint(-16, -146, 22), AuPoint(16, -146, 22) };
			for(const AuPoint& p : drives)
			{
				AuScFaceList cone = KitFrustumY(p - AuPoint(0, 8, 0), 9, 6, 8, 12);
				AddKitMesh(d, cone, L("brass"));
				AuScFaceList disc = KitHalo(p - AuPoint(0, 8.2, 0), 0, 8.5, 20);
				KitRotate(disc, p - AuPoint(0, 8.2, 0), AuPoint(1, 0, 0), 90);
				AddKitMesh(d, disc, L("ember"));
			}
			// A ram of gold at the bow.
			AddKitMesh(d, KitFrustumY(AuPoint(0, 146, 26), 4, 0, 18, 6), L("gold-trim"));
		};
		out.insert("brass-cruiser", r);
	}

	// A survey lancer: a slim obsidian needle wreathed in halos and crystals.
	{
		ShipRecipe r;
		r.templateKey = "science_lab";
		r.body = [](AuScDesign& d) {
			QList<QVector<double> > s;
			s << QVector<double>{ -110, 10, .9 } << QVector<double>{ -95, 22, .9 } << QVector<double>{ -20, 24, .9 }
			  << QVector<double>{ 40, 20, .9 } << QVector<double>{ 90, 8, .9 } << QVector<double>{ 125, 0, 1 };
			AddKitHull(d, ShipBody(s, 24, 10), L("obsidian"));
		};
		r.ornaments = [](AuScDesign& d) {
			for(int k = 0; k < 3; ++k)
			{
				const double y = 60 - k * 22;
				AuScFaceList halo = KitHalo(AuPoint(0, y, 24), 22 - k * 1.5, 26 - k * 1.5, 40);
				KitRotate(halo, AuPoint(0, y, 24), AuPoint(1, 0, 0), 90);
				AddKitMesh(d, halo, L(k == 1 ? "arcane" : "aether"));
				AuScFaceList ring = KitTorus(AuPoint(0, y, 24), 21 - k * 1.5, .8, 32, 6);
				KitRotate(ring, AuPoint(0, y, 24), AuPoint(1, 0, 0), 90);
				AddKitMesh(d, ring, L("gold-trim"));
			}
			// Crystal vanes around the stern.
			for(int k = 0; k < 6; ++k)
			{
				const double a = 60.0 * k + 30;
				const AuPoint base(cos(Rad(a)) * 18, -80, 24 + sin(Rad(a)) * 18);
				AuScFaceList vane = KitCrystal(base, 2.2, 16, 6, 6);
				KitRotate(vane, base, AuPoint(0, 1, 0), 90 - a);
				AddKitMesh(d, vane, L("crystal"));
			}
			AuScFaceList disc = KitHalo(AuPoint(0, -111, 24), 0, 9, 24);
			KitRotate(disc, AuPoint(0, -111, 24), AuPoint(1, 0, 0), 90);
			AddKitMesh(d, disc, L("drive"));
			AuScFaceList rim = KitTorus(AuPoint(0, -110, 24), 10, 1.2, 24, 6);
			KitRotate(rim, AuPoint(0, -110, 24), AuPoint(1, 0, 0), 90);
			AddKitMesh(d, rim, L("jade"));
		};
		out.insert("crystal-lancer", r);
	}
	return out;
}

QMap<QString, BuildingRecipe> BuildingRecipes()
{
	QMap<QString, BuildingRecipe> out;

	// A seat of government: an octagonal stone hall under a gothic spire,
	// crowned with a golden cog and a floating crystal in rings of light.
	{
		BuildingRecipe r;
		r.templateKey = "capitol";
		r.floors = 2;
		r.body = [](AuBdDesign& d) {
			AddKitHull(d, KitFrustum(AuPoint(0, 0, 0), 92, 86, 32, 8, 22.5), L("stone"));
		};
		r.ornaments = [](AuBdDesign& d) {
			AddKitMesh(d, KitFrustum(AuPoint(0, 0, 32), 88, 86, 3, 8, 22.5), L("gold-trim"));
			AddKitMesh(d, KitFrustum(AuPoint(0, 0, 35), 34, 22, 70, 8, 22.5), L("gothic"));
			AddKitMesh(d, KitFrustum(AuPoint(0, 0, 105), 22, 4, 26, 8, 22.5), L("capacitor-gold"));
			AddKitMesh(d, KitGear(AuPoint(0, 0, 108), 30, 20, 5, 4), L("power-gold"));
			AddKitMesh(d, KitCrystal(AuPoint(0, 0, 140), 7, 14, 9, 6), L("crystal"));
			for(int k = 0; k < 3; ++k)
			{
				AuScFaceList halo = KitHalo(AuPoint(0, 0, 155), 13 + k * 5, 15 + k * 5, 40);
				KitRotate(halo, AuPoint(0, 0, 155), AuPoint(cos(Rad(60.0 * k)), sin(Rad(60.0 * k)), 0), 35);
				AddKitMesh(d, halo, L(k == 1 ? "arcane" : "aether"));
			}
			// Buttresses with lamps of ember.
			for(int k = 0; k < 8; ++k)
			{
				const double a = Rad(45.0 * k);
				const AuPoint p(cos(a) * 94, sin(a) * 94, 0);
				AddKitMesh(d, KitFrustum(p, 6, 3, 44, 4, 45.0 * k + 45), L("stone"));
				AddKitMesh(d, KitCrystal(p + AuPoint(0, 0, 44), 2.4, 4, 3, 6), L("ember"));
			}
			// A curtain of light over the entrance side.
			AddKitMesh(d, KitSheet(AuPoint(-30, -95, 4), AuPoint(30, -95, 4), AuPoint(30, -95, 30), AuPoint(-30, -95, 30)), L("aether"));
		};
		out.insert("aether-spire", r);
	}

	// A works hall: brick-red machinery with copper stacks and great wheels.
	{
		BuildingRecipe r;
		r.templateKey = "factory";
		r.floors = 1;
		r.body = [](AuBdDesign& d) {
			AddKitHull(d, KitBox(AuPoint(-90, -60, 0), AuPoint(90, 60, 30)), L("rust"));
		};
		r.ornaments = [](AuBdDesign& d) {
			AddKitMesh(d, KitBox(AuPoint(-92, -62, 30), AuPoint(92, 62, 33)), L("copper"));
			// A glass-and-light roof lantern.
			AddKitMesh(d, KitDome(AuPoint(0, 0, 33), 30, 5, 16, .6), L("glass"));
			AddKitMesh(d, KitHalo(AuPoint(0, 0, 52), 0, 12, 24), L("aether-soft"));
			// Stacks with glowing tops.
			for(int k = 0; k < 4; ++k)
			{
				const AuPoint p(-70 + k * 46.0, 40, 33);
				AddKitMesh(d, KitFrustum(p, 6, 4.5, 40, 12), L("copper"));
				AddKitMesh(d, KitTorus(p + AuPoint(0, 0, 40), 5, 1.2, 16, 6), L("brass"));
				AddKitMesh(d, KitHalo(p + AuPoint(0, 0, 39.5), 0, 4.4, 16), L("ember"));
			}
			// Great wheels on the ends.
			for(int side = -1; side <= 1; side += 2)
			{
				SideGear(d, AuPoint(side * 93, -20, 26), 22, 18, "cog");
				SideGear(d, AuPoint(side * 93, 22, 16), 13, 12, "copper");
			}
			// Pipes along the front.
			for(int k = 0; k < 3; ++k)
			{
				AuScFaceList pipe = KitFrustumY(AuPoint(-88 + k * 3.0, -62, 6 + k * 7), 1.5, 1.5, 124, 8);
				KitRotate(pipe, AuPoint(-88 + k * 3.0, -62, 6 + k * 7), AuPoint(0, 0, 1), -90);
				AddKitMesh(d, pipe, L("copper"));
			}
		};
		out.insert("gear-works", r);
	}

	// A round marble conservatory under a dome of stained glass, ringed with
	// crystals of light.
	{
		BuildingRecipe r;
		r.templateKey = "residential";
		r.floors = 1;
		r.body = [](AuBdDesign& d) {
			AddKitHull(d, KitFrustum(AuPoint(0, 0, 0), 70, 70, 22, 16, 11.25), L("marble"));
		};
		r.ornaments = [](AuBdDesign& d) {
			AddKitMesh(d, KitFrustum(AuPoint(0, 0, 22), 72, 72, 3, 16, 11.25), L("gold-trim"));
			AddKitMesh(d, KitDome(AuPoint(0, 0, 25), 66, 6, 24, .8), L("glass-pale"));
			AddKitMesh(d, KitCrystal(AuPoint(0, 0, 78), 4, 10, 6, 6), L("ember"));
			for(int k = 0; k < 12; ++k)
			{
				const double a = Rad(30.0 * k);
				const AuPoint p(cos(a) * 82, sin(a) * 82, 0);
				AuScFaceList c = KitCrystal(p, 3 + (k % 3), 12 + 6 * (k % 2), 5, 6);
				KitRotate(c, p, AuPoint(-sin(a), cos(a), 0), 12);
				AddKitMesh(d, c, L(k % 2 ? "arcane" : "crystal"));
			}
			AddKitMesh(d, KitHalo(AuPoint(0, 0, .5), 74, 92, 48), L("aether-soft"));
		};
		out.insert("crystal-conservatory", r);
	}

	// An observatory: an obsidian drum with a brass column carrying the
	// rings and spheres of an orrery.
	{
		BuildingRecipe r;
		r.templateKey = "office_block";
		r.floors = 1;
		r.body = [](AuBdDesign& d) {
			AddKitHull(d, KitFrustum(AuPoint(0, 0, 0), 60, 56, 24, 8, 22.5), L("obsidian"));
		};
		r.ornaments = [](AuBdDesign& d) {
			AddKitMesh(d, KitFrustum(AuPoint(0, 0, 24), 57, 57, 2, 8, 22.5), L("gold-trim"));
			AddKitMesh(d, KitFrustum(AuPoint(0, 0, 26), 6, 4, 70, 12), L("brass"));
			AddKitMesh(d, KitDome(AuPoint(0, 0, 92), 9, 4, 16), L("ember"));
			AddKitMesh(d, KitDome(AuPoint(0, 0, 92), -9, 4, 16), L("ember"));
			const double tilts[3] = { 15, 35, 60 };
			const char* planets[3] = { "jade", "crystal", "marble" };
			for(int k = 0; k < 3; ++k)
			{
				const double radius = 22 + k * 12;
				AuScFaceList ring = KitTorus(AuPoint(0, 0, 92), radius, .9, 48, 6);
				KitRotate(ring, AuPoint(0, 0, 92), AuPoint(cos(Rad(70.0 * k)), sin(Rad(70.0 * k)), 0), tilts[k]);
				AddKitMesh(d, ring, L("gold-trim"));
				AuScFaceList halo = KitHalo(AuPoint(0, 0, 92), radius - 2, radius - .8, 48);
				KitRotate(halo, AuPoint(0, 0, 92), AuPoint(cos(Rad(70.0 * k)), sin(Rad(70.0 * k)), 0), tilts[k]);
				AddKitMesh(d, halo, L("aether"));
				AuScFaceList planet = KitDome(AuPoint(radius, 0, 92), 4 + k, 3, 12);
				planet.AddFaces(KitDome(AuPoint(radius, 0, 92), -(4 + k), 3, 12));
				KitRotate(planet, AuPoint(0, 0, 92), AuPoint(cos(Rad(70.0 * k)), sin(Rad(70.0 * k)), 0), tilts[k]);
				AddKitMesh(d, planet, L(planets[k]));
			}
		};
		out.insert("orrery-observatory", r);
	}
	return out;
}

}	// namespace

QList<KitItem> KitItems()
{
	QList<KitItem> items;
	auto add = [&items](const char* key, const char* label, const char* kind, const char* description) {
		KitItem i;
		i.key = key;
		i.label = label;
		i.kind = kind;
		i.description = description;
		items << i;
	};
	add("aether-skiff", "Aether Skiff", "ship", "A small courier: a dark gothic hull, brass bands, sails of blue light, crystal fins.");
	add("brass-cruiser", "Brass Cruiser", "ship", "A frigate of magmium and brass with paddle wheels, copper pipes, an aether core and three ember drives.");
	add("crystal-lancer", "Crystal Lancer", "ship", "A survey needle of obsidian wreathed in halos of light, with crystal vanes.");
	add("aether-spire", "Aether Spire", "building", "A capitol: an octagonal stone hall under a gothic spire, a golden cog and a floating crystal in rings of light.");
	add("gear-works", "Gear Works", "building", "A factory hall with copper stacks, great brass wheels and a lantern roof.");
	add("crystal-conservatory", "Crystal Conservatory", "building", "Homes under a stained glass dome, ringed with crystals of light.");
	add("orrery-observatory", "Orrery Observatory", "building", "Offices in an obsidian drum under the rings and spheres of an orrery.");
	return items;
}

bool BuildKitItem(const QString& key, const QString& outDir, QStringList& log, QString* savedAs)
{
	KitItem item;
	for(const KitItem& i : KitItems())
		if(i.key == key)
			item = i;
	if(item.key.isEmpty())
	{
		log << QString("There is no kit blueprint called '%1'.").arg(key);
		return false;
	}
	const QString fileName = QDir(outDir).filePath(QString(item.label).replace(' ', '_') + ".SoH");
	QStringList errors, warnings;

	if(item.kind == "ship")
	{
		const ShipRecipe recipe = ShipRecipes().value(key);
		AuScDesign design;
		design.SetId(1);
		design.SetArchitectId(AuId(0));
		design.SetArchitectName("shipgen");
		design.SetName(item.label);
		design.SetStructure(SC_Spaceship);
		design.SetStreamline(SLINE_None);
		SetKitPalette(design);
		recipe.body(design);

		// The game wants the bottom of the hull at z 0 (the report calls a
		// hull above it "CG outside design extents below").
		double bottom = 1e30;
		for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
		{
			AuPoint mn, mx;
			if(it.value() && it.value()->IsHullMesh() && it.value()->ModelExtents(mn, mx))
				bottom = qMin(bottom, mn.z);
		}
		AuMatrix4x4 lift;
		lift.LoadIdentity();
		lift.Translate(AuPoint(0, 0, bottom < 1e29 ? -bottom : 0));
		design.Parts().Transform(lift);

		Brief brief;
		brief.name = item.label;
		ApplyTemplate(brief, recipe.templateKey);
		LayoutOptions opts;
		opts.centerHull = false;			// The body is drawn where it is, fore along +Y.
		opts.strategy = "decks";
		HullAnalysis hull;
		QString error;
		if(!AnalyseHull(design, opts, hull, error))
		{
			log << QString("The body could not be measured: %1").arg(error);
			return false;
		}
		const LayoutResult layout = GenerateLayout(design, brief, hull, opts);
		log << QString("%1 rooms, %2 doors, %3 windows, %4 stations, %5 berths.").arg(layout.roomsPlaced)
				.arg(layout.doorsPlaced).arg(layout.windowsPlaced).arg(layout.stationsPlaced).arg(layout.berthsPlaced);
		for(const QString& w : layout.warnings)
			log << "Note: " + w;
		QSet<quint16> before;
		for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
			before.insert(it.key());
		recipe.ornaments(design);
		// The ornaments are hull on a ship: the game wants every mesh part of
		// a ship inside its hull. They add to the hull volume, so the systems
		// are sized again.
		QList<quint16> ornaments;
		for(AuScParts::iterator it = design.Parts().begin(); it != design.Parts().end(); ++it)
			if(!before.contains(it.key()) && it.value())
				ornaments << it.key();
		for(quint16 id : ornaments)
		{
			AuScPart* part = design.Parts().value(id);
			part->Transform(lift);
			if(part->Type() != AuScPart::Part_Mesh)
				continue;
			AuScFaceList faces;
			part->Faces(faces);
			AuScHull hullPart;
			hullPart.AddFaces(faces);
			hullPart.SetGroupName(part->GroupName());
			AuScParts deleted;
			design.Parts().DeletePart(id, deleted);
			if(hullPart.IsValid())
				design.Parts().AddPart(hullPart);
		}
		design.RefreshObstructionsCache();
		QStringList notes;
		AllocateSystemVolumes(design, brief, notes);
		for(const QString& n : notes)
			log << "Note (with the ornaments): " + n;
		ValidateDesign(design, errors, warnings);
		if(!SaveDesign(design, fileName))
		{
			log << "Could not write " + fileName;
			return false;
		}
	}
	else
	{
		const BuildingRecipe recipe = BuildingRecipes().value(key);
		BuildingBrief brief;
		brief.name = item.label;
		ApplyBuildingTemplate(brief, recipe.templateKey);
		brief.floors = recipe.floors;
		AuBdDesign design;
		design.SetId(1);
		design.SetArchitectId(AuId(0));
		design.SetArchitectName("shipgen");
		design.SetName(item.label);
		SetKitPalette(design);
		recipe.body(design);
		const BuildingResult result = GenerateBuilding(design, brief, "auto");
		log << QString("%1 rooms, %2 doors.").arg(result.roomsPlaced).arg(result.doorsPlaced);
		recipe.ornaments(design);
		ValidateBuilding(design, errors, warnings);
		if(!SaveDesign(design, fileName))
		{
			log << "Could not write " + fileName;
			return false;
		}
	}

	log << QString("The engine's report: %1 errors, %2 warnings.").arg(errors.size()).arg(warnings.size());
	for(const QString& e : errors)
		log << "  " + e;
	for(const QString& w : warnings)
		log << "  " + w;
	log << "Saved as " + fileName;
	if(savedAs)
		*savedAs = fileName;
	return errors.isEmpty();
}
