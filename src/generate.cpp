//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - hull analysis and internal layout generation.
//
// Design units are feet. Volumes reported to the engine are cubic meters.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "generate.h"

#include <QFileInfo>
#include <QMap>
#include <QRectF>
#include <QSet>
#include <QVector>
#include <cmath>

#include "aufilever.h"
#include "auscface.h"
#include "interior.h"
#include "austation.h"
#include <algorithm>
#include "mesh.h"
#include "auscreport.h"
#include "aubdreport.h"
#include <QList>

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

QStringList LayoutStrategies()
{
	return QStringList() << "decks" << "spine" << "blocks";
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Hull analysis.

// Counts the faces of a set of parts.
static quint32 CountFaces(const AuScParts& parts)
{
	quint32 count = 0;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		AuScFaceList faces;
		(*it)->Faces(faces);
		count += faces.size();
	}
	return count;
}

// Counts the triangles of a set of parts.
static quint32 CountTriangles(const AuScParts& parts)
{
	quint32 count = 0;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		AuScFaceList faces;
		(*it)->Faces(faces);
		AuScTriList tris;
		faces.Triangles(tris);
		count += tris.size();
	}
	return count;
}

bool HullOnlySilhouette(const AuScParts& parts, AuScSilhouette3d& silhouette,
		QList<AuScSilhouette3d>* perPart)
{
	if(perPart)
		perPart->clear();

	AuScFaceList hullFaces;
	bool haveHull = false;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p || !p->IsHullMesh())
			continue;
		haveHull = true;
		AuScFaceList faces;
		p->Faces(faces);
		hullFaces.AddFaces(faces);
		if(perPart)
			perPart->append(AuScSilhouette3d(faces, 0.f));
	}
	if(!haveHull)
		return false;

	silhouette.Build(hullFaces, 0.f);
	return silhouette.IsValid();
}

bool BoxInsideSilhouette(const AuScSilhouette3d& silhouette, const AuPoint& mn, const AuPoint& mx)
{
	if(!silhouette.IsValid())
		return false;

	// A box is convex, so if all of its corners are inside a convex region
	// then the whole box is. The engine's silhouettes are rasterised and can
	// be concave, so also sample the faces, the edges and the middle. That
	// catches a notch that cuts into a room without touching a corner.
	for(int ix = 0; ix <= 2; ++ix)
	{
		for(int iy = 0; iy <= 2; ++iy)
		{
			for(int iz = 0; iz <= 2; ++iz)
			{
				const AuPoint p(mn.x + (mx.x - mn.x) * ix / 2.,
						mn.y + (mx.y - mn.y) * iy / 2.,
						mn.z + (mx.z - mn.z) * iz / 2.);
				if(!silhouette.IsInside(p))
					return false;
			}
		}
	}
	return true;
}

AuMatrix4x4 QuarterTurnToFore()
{
	AuMatrix4x4 t;
	t.LoadIdentity();
	t.RotateZ(M_PI / 2);
	if(t.Project(AuPoint(1, 0, 0)).y < 0.5)		// The engine's sign convention.
	{
		t.LoadIdentity();
		t.RotateZ(-M_PI / 2);
	}
	return t;
}

bool AnalyseHull(AuScDesign& design, const LayoutOptions& opts, HullAnalysis& out, QString& error)
{
	out = HullAnalysis();

	const AuScParts& parts = design.Parts();

	// First find the extents of the hull parts. The extents have to be known
	// before the hull can be centred.
	AuScFaceList extentFaces;
	bool haveHull = false;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p || !p->IsHullMesh())
			continue;
		haveHull = true;
		// Faces() replaces the list it is given, it does not add to it.
		// Collect each part on its own before adding, or only the last part
		// (in hash order, which changes between runs) would be measured.
		AuScFaceList partFaces;
		p->Faces(partFaces);
		extentFaces.AddFaces(partFaces);
	}
	if(!haveHull)
	{
		error = "the design has no hull part";
		return false;
	}

	AuScTriList tris;
	extentFaces.Triangles(tris);
	if(tris.empty())
	{
		error = "the hull has no triangles";
		return false;
	}
	AuPoint min, max;
	tris.Extents(min, max);

	// Put the length of the ship along Y, the game's fore and aft.
	if(opts.centerHull && opts.foreAlongLongAxis && (max.x - min.x) > (max.y - min.y) * 1.001)
	{
		const AuMatrix4x4 t = QuarterTurnToFore();
		design.Parts().Transform(t);
		const AuPoint a = t.Project(min), b = t.Project(max);
		min = a.Min(b);
		max = a.Max(b);
		out.turned = true;
	}

	// Centre the hull on the origin so the designer view is usable.
	if(opts.centerHull)
	{
		AuPoint shift(-(min.x + max.x)/2, -(min.y + max.y)/2, -min.z);
		AuMatrix4x4 t;
		t.Translate(shift);
		design.Parts().Transform(t);
		min += shift;
		max += shift;
	}

	out.min = min;
	out.max = max;
	out.faces = CountFaces(parts);
	out.triangles = CountTriangles(parts);

	// Build the silhouettes from the hull parts in their final positions. If
	// they were built before the centring shift they would sit at the old
	// coordinates and every containment test would be wrong.
	if(!HullOnlySilhouette(design.Parts(), out.silhouette, &out.partSilhouettes))
	{
		error = "the hull has no usable silhouette";
		return false;
	}

	// Let the engine build its own silhouettes. They are used for the final
	// engine report and for paths, not for deciding whether a room fits: the
	// engine's model absorbs the faces of whatever touches it, so a room that
	// sticks out would make its own hole in the hull look filled.
	design.RefreshObstructionsCache();

	// The same hull parts as exact triangle meshes. A room is fitted with
	// these when they are closed, because three silhouettes can call a spot
	// inside the hull when it is really in the open air of a concavity.
	out.solids.clear();
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p || p->IsOmitted() || !p->IsHullMesh())
			continue;
		AuScFaceList faces;
		p->Faces(faces);
		AuScTriList triangles;
		faces.Triangles(triangles);
		out.solids << SolidMesh(triangles);
	}

	// Enclosed volume, in cubic meters.
	double volCubicFeet = out.silhouette.Volume();
	out.volumeM3 = volCubicFeet * 0.0283168466;

	// Which horizontal axis is the ship longest along? The layout is laid out
	// along that axis.
	out.longAxis = (max.x - min.x) >= (max.y - min.y) ? 'x' : 'y';

	// Usable interior: the hull box shrunk by the wall thickness. On a thin
	// hull the wall has to be thinner, otherwise there is no room left.
	const double dims[3] = { max.x - min.x, max.y - min.y, max.z - min.z };
	double smallest = dims[0];
	for(int i = 1; i < 3; ++i)
		smallest = qMin(smallest, dims[i]);
	const double wall = qMin(opts.wallThickness, smallest * .25);

	out.usableMin = min + AuPoint(wall, wall, wall);
	out.usableMax = max - AuPoint(wall, wall, wall);
	if(out.usableMin.x >= out.usableMax.x || out.usableMin.y >= out.usableMax.y || out.usableMin.z >= out.usableMax.z)
	{
		error = QString("the hull is too small to hold any interior (%1 x %2 x %3 feet)")
				.arg(qRound(max.x - min.x)).arg(qRound(max.y - min.y)).arg(qRound(max.z - min.z));
		return false;
	}

	out.valid = true;
	return true;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Small helpers for building parts.

static AuMatrix4x4 IdentityPlane()
{
	AuMatrix4x4 m;
	m.LoadIdentity();
	return m;
}

// Builds the matrix of a wall so that its normal points into the room.
// The wall runs from 'x0' to 'x1' along X and from 'z0' to 'z1' along Z.
static AuMatrix4x4 WallPlane(double x0, double x1, double y, double z0, double z1)
{
	AuMatrix4x4 m;
	m.Translate(AuPoint(x1, y, z0));
	m.RotateToPlane(AuPoint(x0 - x1, 0, 0), AuPoint(0, 0, z1 - z0));
	return m;
}

// Maps a point from layout space back into design space. When the long axis
// is Y, the game's fore and aft, the layout's start (x = 0, where the first
// rooms of the brief, the bridge, go) is put at the bow: y is mirrored about
// the middle of the hull, 'mirrorSum' being twice that middle. A NaN
// 'mirrorSum' leaves the start at the stern.
static AuPoint ToDesign(const AuPoint& p, bool swap, const AuPoint& origin, double mirrorSum)
{
	if(swap && qIsNaN(mirrorSum))
		return AuPoint(p.y + origin.x, p.x + origin.y, p.z + origin.z);
	if(swap)
		return AuPoint(p.y + origin.x, mirrorSum - (p.x + origin.y), p.z + origin.z);
	return AuPoint(p.x + origin.x, p.y + origin.y, p.z + origin.z);
}

// Checks that a box sits fully inside the hull. A room must not poke out of
// the hull even a little: the engine only asks for one vertex to be inside,
// but the game then shows a room jutting through the side of the ship, and
// the room's own faces inflate the hull obstruction model so the engine
// happily accepts it. Use the hull-only silhouette and require every sampled
// point to be inside. When there are several hull parts, the room has to fit
// inside one of them, which is the rule the engine uses when it decides which
// hull a room belongs to.
static bool BoxInsideHull(const AuScDesign& design, const HullAnalysis& hull, const AuPoint& mn, const AuPoint& mx)
{
	// The exact test comes first. A silhouette is three projections of the
	// hull, and a room can be inside all three while sitting in the open air
	// of a concavity or between two parts of a split hull.
	bool haveClosed = false;
	for(const SolidMesh& solid : hull.solids)
	{
		if(!solid.IsClosed())
			continue;
		haveClosed = true;
		if(solid.BoxInside(mn, mx))
			return true;
	}
	if(haveClosed)
		return false;						// The exact answer is "no".

	// No hull part is a closed solid, so the volume is not defined and the
	// silhouette is all that is left. The caller warns about this; see
	// HullAnalysis::solids.
	if(hull.partSilhouettes.isEmpty())
	{
		if(hull.silhouette.IsValid())
			return BoxInsideSilhouette(hull.silhouette, mn, mx);

		// No silhouette at all: fall back to the engine's point test, but
		// still require every sampled point to be inside.
		for(int ix = 0; ix <= 2; ++ix)
			for(int iy = 0; iy <= 2; ++iy)
				for(int iz = 0; iz <= 2; ++iz)
				{
					const AuPoint p(mn.x + (mx.x - mn.x) * ix / 2.,
							mn.y + (mx.y - mn.y) * iy / 2.,
							mn.z + (mx.z - mn.z) * iz / 2.);
					if(!design.Parts().IsInsideObstructionModel(p))
						return false;
				}
		return true;
	}

	for(const AuScSilhouette3d& part : hull.partSilhouettes)
		if(BoxInsideSilhouette(part, mn, mx))
			return true;
	return false;
}

// Creates the room's constant void and the matching room mesh for a box.
static bool MakeRoomParts(AuScDesign& design, const QString& groupName, const AuPoint& mn, const AuPoint& mx,
		quint16& voidId, quint16& meshId)
{
	return AddBoxRoom(design, groupName, mn, mx, voidId, meshId);
}

// Collects the hull faces so that a hull door can find the surface it has to
// cut through.
static void CollectHullFaces(const AuScParts& parts, AuScFaceList& faces)
{
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(p && p->IsHullMesh())
		{
			// Faces() replaces the list, so collect each part separately.
			AuScFaceList partFaces;
			p->Faces(partFaces);
			faces.AddFaces(partFaces);
		}
	}
}

// A door and the two rooms it joins; 'roomB' is -1 for the hull door.
struct DoorLink
{
	int		roomA = -1;
	int		roomB = -1;
	quint16	doorId = 0;
	AuPoint	centre;			// The middle of the doorway, on its floor.
	AuPoint	outside;		// Hull door only: a point just outside the hull.
	AuPoint	ground;			// Hull door only: the foot of the ladder below 'outside'.
	AuPoint	facing;			// Hull door only: which way one faces on the ladder (the hull).
	bool	ramp = false;	// Hull door only: a ramp down to the ground instead of the ladder,
	AuPoint	rampTop;		// from the end of its landing
	AuPoint	rampFoot;		// to its foot.
};

// A candidate interior door on the wall two rooms share.
struct DoorCandidate
{
	int		a = -1;
	int		b = -1;
	int		normalAxis = 0;	// 0: the wall is at a constant x. 1: at a constant y.
	AuPoint	mn;
	AuPoint	mx;
	double	width = 0;
};

static int FindRoot(QVector<int>& parent, int i)
{
	while(parent[i] != i)
	{
		parent[i] = parent[parent[i]];
		i = parent[i];
	}
	return i;
}

// Places doors so that every room can be reached, and one hull door so that
// the ship can be entered from outside. Each door gets its own doorway void,
// associated with its open state; the rooms' own voids stay constant (see
// interior.h for why that matters).
static int PlaceDoors(AuScDesign& design, const HullAnalysis& hull, QList<PlacedRoom>& rooms,
		QList<DoorLink>& links, QStringList& warnings)
{
	if(rooms.isEmpty())
		return 0;

	// Every wall two rooms share, wide and tall enough for a door.
	QList<DoorCandidate> candidates;
	for(int i = 0; i < rooms.size(); ++i)
	{
		for(int j = i + 1; j < rooms.size(); ++j)
		{
			const PlacedRoom& a = rooms[i];
			const PlacedRoom& b = rooms[j];

			const double z0 = qMax(a.min.z, b.min.z);
			const double z1 = qMin(a.max.z, b.max.z);
			if(z1 - z0 < 6.5)
				continue;
			// Flush with the floor: a sill trips people up in the game.
			const double dz0 = z0;
			const double dz1 = dz0 + qMin(8.0, z1 - z0 - .5);

			for(int axis = 0; axis < 2; ++axis)
			{
				const double aLo = axis == 0 ? a.min.x : a.min.y, aHi = axis == 0 ? a.max.x : a.max.y;
				const double bLo = axis == 0 ? b.min.x : b.min.y, bHi = axis == 0 ? b.max.x : b.max.y;
				double wall;
				if(fabs(aHi - bLo) < .05)
					wall = aHi;
				else if(fabs(bHi - aLo) < .05)
					wall = bHi;
				else
					continue;

				// The overlap along the wall.
				const double o0 = axis == 0 ? qMax(a.min.y, b.min.y) : qMax(a.min.x, b.min.x);
				const double o1 = axis == 0 ? qMin(a.max.y, b.max.y) : qMin(a.max.x, b.max.x);
				if(o1 - o0 < 5.0)
					continue;
				const double width = qMin(10.0, o1 - o0 - 1.0);
				const double c = (o0 + o1) / 2;

				DoorCandidate dc;
				dc.a = i;
				dc.b = j;
				dc.normalAxis = axis;
				dc.width = o1 - o0;
				if(axis == 0)
				{
					dc.mn = AuPoint(wall - .5, c - width/2, dz0);
					dc.mx = AuPoint(wall + .5, c + width/2, dz1);
				}
				else
				{
					dc.mn = AuPoint(c - width/2, wall - .5, dz0);
					dc.mx = AuPoint(c + width/2, wall + .5, dz1);
				}
				candidates << dc;
				break;
			}
		}
	}

	// The widest shared walls first, and only as many doors as it takes to
	// join every room to every other: a spanning tree.
	std::sort(candidates.begin(), candidates.end(),
			[](const DoorCandidate& l, const DoorCandidate& r) { return l.width > r.width; });
	QVector<int> parent(rooms.size());
	for(int i = 0; i < parent.size(); ++i)
		parent[i] = i;

	int placed = 0;
	QSet<int> hasDoor;
	for(const DoorCandidate& dc : candidates)
	{
		const int ra = FindRoot(parent, dc.a), rb = FindRoot(parent, dc.b);
		if(ra == rb)
			continue;
		quint16 doorId = 0;
		if(!AddInteriorDoor(design, dc.mn, dc.mx, dc.normalAxis, rooms[dc.a].label, doorId,
				rooms[dc.a].meshId, rooms[dc.b].meshId))
			continue;
		parent[ra] = rb;
		++placed;
		hasDoor << dc.a << dc.b;
		DoorLink link;
		link.roomA = dc.a;
		link.roomB = dc.b;
		link.doorId = doorId;
		link.centre = AuPoint((dc.mn.x + dc.mx.x)/2, (dc.mn.y + dc.mx.y)/2, dc.mn.z);
		links << link;
	}

	// A hull door. Look from a room towards the outside and use the hull face
	// that the ray hits.
	AuScFaceList hullFaces;
	CollectHullFaces(design.Parts(), hullFaces);
	if(!hullFaces.isEmpty())
	{
		const double span = qMax(hull.max.x - hull.min.x, hull.max.y - hull.min.y) * 2 + 1000;
		const AuPoint dirs[4] = { AuPoint(1, 0, 0), AuPoint(-1, 0, 0), AuPoint(0, 1, 0), AuPoint(0, -1, 0) };

		int bestRoom = -1;
		int bestDir = -1;
		double bestDistance = 1e30;
		AuPoint bestHit;
		// Side doors (along X; the game's fore is +Y) are preferred: the user
		// finds belly and end doors buggy in the current game. Fore and aft
		// are only tried when no room can reach the side.
		for(int pass = 0; pass < 2 && bestRoom < 0; ++pass)
		for(int ri = 0; ri < rooms.size(); ++ri)
		{
			const PlacedRoom& r = rooms[ri];
			if(r.max.z - r.min.z < 6.5)
				continue;
			const AuPoint centre((r.min.x + r.max.x)/2, (r.min.y + r.max.y)/2, r.min.z + 1.0);
			for(int di = pass * 2; di < pass * 2 + 2; ++di)
			{
				AuPoint hit = centre + dirs[di] * span;
				if(!hullFaces.Sight(centre, hit))
					continue;
				const double distance = (hit - centre).Length();
				if(distance >= bestDistance)
					continue;
				// The hit has to be outside the room, otherwise it is one of
				// the room faces or a nearby part.
				const double roomHalf = (di < 2) ? (r.max.x - r.min.x) / 2 : (r.max.y - r.min.y) / 2;
				if(distance < roomHalf)
					continue;
				bestDistance = distance;
				bestRoom = ri;
				bestDir = di;
				bestHit = hit;
			}
		}

		if(bestRoom >= 0)
		{
			const PlacedRoom& r = rooms[bestRoom];
			const double z0 = r.min.z;		// Flush with the floor.
			const double z1 = qMin(r.max.z - .5, z0 + 8.0);
			const bool alongX = (bestDir < 2);
			const double spanAcross = alongX ? (r.max.y - r.min.y) : (r.max.x - r.min.x);
			const double doorWidth = qMin(10.0, spanAcross - 1.0);
			const double across = alongX ? (r.min.y + r.max.y)/2 : (r.min.x + r.max.x)/2;
			const double sgn = (bestDir == 0 || bestDir == 2) ? 1.0 : -1.0;
			const double wallCoord = alongX ? (sgn > 0 ? r.max.x : r.min.x) : (sgn > 0 ? r.max.y : r.min.y);

			if(z1 - z0 >= 6.0 && doorWidth >= 4.0)
			{
				// The door slab sits just inside the hull surface.
				const double surface = alongX ? bestHit.x : bestHit.y;
				const double doorPlane = surface - sgn * 1.0;
				AuPoint mn, mx;
				if(alongX)
				{
					mn = AuPoint(doorPlane - .5, across - doorWidth/2, z0);
					mx = AuPoint(doorPlane + .5, across + doorWidth/2, z1);
				}
				else
				{
					mn = AuPoint(across - doorWidth/2, doorPlane - .5, z0);
					mx = AuPoint(across + doorWidth/2, doorPlane + .5, z1);
				}

				quint16 doorId = 0;
				if(AddHullDoor(design, mn, mx, dirs[bestDir], wallCoord, surface, r.label, doorId, r.meshId, hull.min.z))
				{
					++placed;
					hasDoor << bestRoom;
					DoorLink link;
					link.roomA = bestRoom;
					link.doorId = doorId;
					link.centre = alongX ? AuPoint(wallCoord, across, z0) : AuPoint(across, wallCoord, z0);
					link.outside = alongX ? AuPoint(surface + sgn * SHIPGEN_LADDER_OUTSIDE, across, z0)
							: AuPoint(across, surface + sgn * SHIPGEN_LADDER_OUTSIDE, z0);
					// Down to the ground by a ladder, as in the working
					// designs: straight down from outside the door to a
					// little below the bottom of the hull.
					link.ground = link.outside;
					link.ground.z = qMin(z0, hull.min.z - SHIPGEN_LADDER_BELOW_HULL);
					link.facing = -dirs[bestDir];
					// A ramp where the door is low enough, as at the user's
					// aircraft factory; the ladder otherwise.
					link.ramp = AddDoorRamp(design, mn, mx, dirs[bestDir], surface,
							hull.min.z - SHIPGEN_RAMP_BELOW_HULL, link.rampTop, link.rampFoot);
					links << link;
				}
			}
		}
	}

	for(int i = 0; i < rooms.size(); ++i)
		if(!hasDoor.contains(i))
			warnings << QString("room '%1' has no door; it cannot be reached from the rest of the ship.")
					.arg(rooms[i].label);

	return placed;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Windows, where a room's outer wall is against the hull skin (agreed with the
// user, 2026-10-31): the bridge sees out from as many sides as it can, in a
// band of windows; quarters, the galley, halls and the sick bay get one every
// 12 feet; working rooms none. Each is cut through the room wall and the hull
// with the engine's window jig, as the designer does (JigCut).

static bool WantsWindows(const QString& type, bool& band)
{
	band = (type == "Bridge");
	return band || type == "Crew Quarters" || type == "Barracks" || type == "Galley" || type == "Hall"
		|| type == "Sick Bay";
}

static int PlaceWindows(AuScDesign& design, const QList<PlacedRoom>& rooms, QStringList& warnings)
{
	// The hull to look out through: not the frames of doors and windows
	// already cut.
	AuScFaceList hullFaces;
	for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
		if(it.value() && it.value()->IsHullMesh() && it.value()->GroupName() != "Frame")
		{
			AuScFaceList partFaces;
			it.value()->Faces(partFaces);
			hullFaces.AddFaces(partFaces);
		}
	if(hullFaces.isEmpty())
		return 0;

	// The doors, so that no window is cut into one or next to it.
	QList<QPair<AuPoint, AuPoint> > doors;
	for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		AuPoint mn, mx;
		if(p && p->IsDoor() && p->ModelExtents(mn, mx))
			doors << qMakePair(mn - AuPoint(2, 2, 2), mx + AuPoint(2, 2, 2));
	}
	auto nearDoor = [&doors](const AuPoint& mn, const AuPoint& mx) {
		for(const QPair<AuPoint, AuPoint>& d : doors)
			if(mn.x < d.second.x && mx.x > d.first.x && mn.y < d.second.y && mx.y > d.first.y
					&& mn.z < d.second.z && mx.z > d.first.z)
				return true;
		return false;
	};

	const AuPoint dirs[4] = { AuPoint(1, 0, 0), AuPoint(-1, 0, 0), AuPoint(0, 1, 0), AuPoint(0, -1, 0) };
	int placed = 0, failed = 0;
	for(const PlacedRoom& r : rooms)
	{
		bool band = false;
		if(!r.meshId || !WantsWindows(r.type, band))
			continue;
		const double z0 = r.min.z, z1 = r.max.z;
		const double sill = z0 + qMin(3.0, (z1 - z0) / 3);
		const double top = qMin(z1 - .5, z0 + 7.5);
		if(top - sill < 2.0)
			continue;
		const double width = band ? 4.0 : 3.0;
		const double spacing = band ? 5.5 : 12.0;
		// How far the hull skin may be from the wall. The tunnel between them
		// is lined with the frame (JigCut's sleeve); further than this and
		// the window would look down a long tube.
		const double maxGap = band ? 10.0 : 7.0;
		for(int di = 0; di < 4; ++di)
		{
			const bool alongX = di < 2;
			const double sgn = (di == 0 || di == 2) ? 1.0 : -1.0;
			const double wall = alongX ? (sgn > 0 ? r.max.x : r.min.x) : (sgn > 0 ? r.max.y : r.min.y);
			const double lo = alongX ? r.min.y : r.min.x, hi = alongX ? r.max.y : r.max.x;
			const int count = int((hi - lo - 2.0) / spacing);
			if(count < 1)
				continue;
			for(int k = 0; k < count; ++k)
			{
				const double c = (lo + hi) / 2 + (k - (count - 1) / 2.0) * spacing;
				// Look out from just inside the wall at the corners and the
				// middle of the window; all have to meet the hull close by.
				double nearest = 1e30, farthest = -1e30;
				bool ok = true;
				const double acrossPts[3] = { c - width/2, c, c + width/2 };
				const double zPts[3] = { sill + .3, (sill + top) / 2, top - .3 };
				for(int a = 0; a < 3 && ok; ++a)
					for(int b = 0; b < 3 && ok; ++b)
					{
						const AuPoint eye = alongX ? AuPoint(wall - sgn * .5, acrossPts[a], zPts[b])
								: AuPoint(acrossPts[a], wall - sgn * .5, zPts[b]);
						AuPoint hit = eye + dirs[di] * (maxGap + 10.0);
						if(!hullFaces.Sight(eye, hit))
						{
							ok = false;
							break;
						}
						const double gap = sgn * ((alongX ? hit.x : hit.y) - wall);
						if(gap < 0 || gap > maxGap)
							ok = false;
						nearest = qMin(nearest, gap);
						farthest = qMax(farthest, gap);
					}
				if(!ok)
					continue;
				const double surface = wall + sgn * farthest;
				const double inner = wall - sgn * .5, outer = surface + sgn * 3.0;
				AuPoint mn, mx;
				if(alongX)
				{
					mn = AuPoint(qMin(inner, outer), c - width/2, sill);
					mx = AuPoint(qMax(inner, outer), c + width/2, top);
				}
				else
				{
					mn = AuPoint(c - width/2, qMin(inner, outer), sill);
					mx = AuPoint(c + width/2, qMax(inner, outer), top);
				}
				if(nearDoor(mn, mx))
					continue;
				AuPoint probe = (mn + mx) / 2.0;
				if(alongX)
					probe.x = wall + sgn * nearest;
				else
					probe.y = wall + sgn * nearest;
				const quint16 hullId = HullPartAt(design, probe);
				AuScFaceList jig = BoxFaces(mn, mx);
				OrientAsHull(jig);
				QList<quint16> added;
				if(!hullId || !JigCut(design, AuScPart::Part_WindowJig, jig, hullId, r.meshId, &added, alongX ? 0 : 1, true, wall)
						|| added.isEmpty())
				{
					++failed;
					continue;
				}
				for(quint16 id : added)
					if(AuScPart* part = design.Parts().value(id))
						if(part->IsWindow())
							part->SetGroupName(r.label);		// So that the room's light falls on it.
				++placed;
			}
		}
	}
	if(failed)
		warnings << QString("%1 windows could not be cut.").arg(failed);
	return placed;
}

// Walk paths that follow the doors: from each room's middle to the doorway,
// through it while it is open, and on to the next room's middle. A path is a
// straight segment, and a room is a box, so the pieces inside a room stay
// inside it.
static void PlaceWalkPaths(AuScDesign& design, const QList<PlacedRoom>& rooms, const QList<DoorLink>& links)
{
	auto middle = [&rooms](int i, double z) {
		const PlacedRoom& r = rooms[i];
		return AuPoint((r.min.x + r.max.x)/2, (r.min.y + r.max.y)/2, z);
	};
	for(const DoorLink& link : links)
	{
		const double z = link.centre.z;
		if(link.roomB < 0)
		{
			AddWalkPath(design, middle(link.roomA, z), link.centre);
			if(link.ramp)
			{
				AddWalkPath(design, link.centre, link.rampTop, link.doorId);
				AddWalkPath(design, link.rampTop, link.rampFoot);
				continue;
			}
			// The top of the ladder is at the height of someone standing in the
			// doorway, or they could not finish climbing it.
			AuPoint top = link.outside;
			if(top.z - link.ground.z > .5)
				top.z += SHIPGEN_LADDER_HOLD;
			AddWalkPath(design, link.centre, top, link.doorId);
			if(top.z - link.ground.z > .5)
				AddLadderPath(design, link.ground, top, link.facing);
			continue;
		}
		const PlacedRoom& a = rooms[link.roomA];
		const PlacedRoom& b = rooms[link.roomB];
		// Points just either side of the wall, inside each room.
		AuPoint inA = link.centre, inB = link.centre;
		const bool xWall = fabs(a.max.x - b.min.x) < .05 || fabs(b.max.x - a.min.x) < .05;
		const double step = SHIPGEN_DOORWAY_REACH;
		if(xWall)
		{
			const double s = (a.min.x < b.min.x) ? 1. : -1.;
			inA.x -= s * step;
			inB.x += s * step;
		}
		else
		{
			const double s = (a.min.y < b.min.y) ? 1. : -1.;
			inA.y -= s * step;
			inB.y += s * step;
		}
		AddWalkPath(design, middle(link.roomA, z), inA);
		AddWalkPath(design, inA, inB, link.doorId);
		AddWalkPath(design, inB, middle(link.roomB, z));
	}
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Reduces the number of triangles in a face list. The vertices are snapped
// onto a grid of 'tolerance' feet and the triangles that collapse are
// dropped. The engine has a hard limit on how big a design may be, so a
// very detailed model has to be thinned out before it can be used.
// Thins a model out. The vertices are snapped onto a grid of 'tolerance'
// feet and the triangles that collapse are dropped. The grid cells are found
// with a hash. When two cells have the same key the clash is probed, so that
// different cells are never mixed up.
static AuScTriList DecimateTris(const AuScTriList& tris, double tolerance)
{
	AuScTriList out;
	if(tolerance <= 0. || tris.isEmpty())
		return tris;

	QHash<quint64, int> cells;
	QVector<QVector<int> > cellCoords;

	const int mask = 0x1FFFFF;		// 21 bits per axis.
	for(const AuScTri& t : tris)
	{
		int idx[3];
		for(int i = 0; i < 3; ++i)
		{
			const AuScPoint& v = t.Vertex(i);
			const int qx = int(floor(v.x / tolerance));
			const int qy = int(floor(v.y / tolerance));
			const int qz = int(floor(v.z / tolerance));
			quint64 key = (quint64(quint32(qx) & mask) << 42) | (quint64(quint32(qy) & mask) << 21)
					| quint64(quint32(qz) & mask);

			// Find the cell, probing when the key is taken.
			int slot = -1;
			for(int probe = 0; probe < 64; ++probe)
			{
				QHash<quint64, int>::const_iterator it = cells.constFind(key);
				if(it == cells.constEnd())
				{
					slot = cellCoords.size();
					cells.insert(key, slot);
					cellCoords.append(QVector<int>() << qx << qy << qz);
					break;
				}
				const QVector<int>& other = cellCoords[it.value()];
				if(other[0] == qx && other[1] == qy && other[2] == qz)
				{
					slot = it.value();
					break;
				}
				++probe;
				key = key * 6364136223846793005ULL + 1442695040888963407ULL;
			}
			if(slot < 0)
				slot = 0;		// Give up on this corner.
			idx[i] = slot;
		}
		if(idx[0] == idx[1] || idx[1] == idx[2] || idx[0] == idx[2])
			continue;	// The triangle collapsed.

		// The new corners sit in the middles of the grid cells.
		AuScPoint v[3];
		for(int i = 0; i < 3; ++i)
		{
			const QVector<int>& cell = cellCoords[idx[i]];
			AuScPoint p = t.Vertex(i);
			p.x = (cell[0] + .5) * tolerance;
			p.y = (cell[1] + .5) * tolerance;
			p.z = (cell[2] + .5) * tolerance;
			v[i] = p;
		}
		out.append(AuScTri(v[0], v[1], v[2]));
	}
	return out;
}

// Part type lookup tables.

struct StationTypeEntry
{
	const char*	key;
	AuScPart::PART_TYPE	type;
	COMMODITYID	module;
};

static const StationTypeEntry kStations[] = {
	{ "helm",			AuScPart::Part_ConsoleHelm,			CID_CommandAndControlModule },
	{ "navigator",		AuScPart::Part_ConsoleNavigator,		CID_CommandAndControlModule },
	{ "command",		AuScPart::Part_ConsoleCommand,			CID_CommandAndControlModule },
	{ "engineer",		AuScPart::Part_ConsoleEngineer,		CID_Warp1FTLDriveModule },
	{ "firecontrol",	AuScPart::Part_ConsoleFireControl,		CID_EnergyWeaponBayModule },
	{ "medic",			AuScPart::Part_ConsoleMedic,			CID_SurgeryMedicalModule },
	{ "sensor",			AuScPart::Part_ConsoleSensor,			CID_SensorModule },
	{ "shield",			AuScPart::Part_ConsoleShield,			CID_EnergyShieldModule },
	{ "transporter",	AuScPart::Part_ConsoleTransporter,		CID_PersonnelTransporterModule },
	{ "powerrelay",		AuScPart::Part_ConsolePowerRelay,		CID_LifeSupportModule },
};

QList<QPair<QString, QString> > FtlDriveChoices()
{
	QList<QPair<QString, QString> > out;
	out << qMakePair(QString("none"), QString("No FTL drive"));
	for(int w = 1; w <= 9; ++w)
		out << qMakePair(QString("warp%1").arg(w), QString("Warp %1").arg(w));
	out << qMakePair(QString("wormhole-p"), QString("Wormhole P (positive)"))
		<< qMakePair(QString("wormhole-pn"), QString("Wormhole PN (positive, negative)"))
		<< qMakePair(QString("wormhole-pnn"), QString("Wormhole PNN (intergalactic)"));
	return out;
}

COMMODITYID FtlDriveModule(const QString& key)
{
	static const COMMODITYID warp[9] = { CID_Warp1FTLDriveModule, CID_Warp2FTLDriveModule, CID_Warp3FTLDriveModule,
			CID_Warp4FTLDriveModule, CID_Warp5FTLDriveModule, CID_Warp6FTLDriveModule, CID_Warp7FTLDriveModule,
			CID_Warp8FTLDriveModule, CID_Warp9FTLDriveModule };
	if(key.startsWith("warp"))
	{
		const int w = key.mid(4).toInt();
		if(w >= 1 && w <= 9)
			return warp[w - 1];
	}
	if(key == "wormhole-p")		return CID_WormholePFTLDriveModule;
	if(key == "wormhole-pn")	return CID_WormholePNFTLDriveModule;
	if(key == "wormhole-pnn")	return CID_WormholePNNFTLDriveModule;
	return CID_Invalid;
}

static bool FindStationType(const QString& key, AuScPart::PART_TYPE& type, COMMODITYID& module)
{
	for(const StationTypeEntry& e : kStations)
	{
		if(key == e.key)
		{
			type = e.type;
			module = e.module;
			return true;
		}
	}
	return false;
}

static bool FindBerthType(const QString& key, AuScPart::PART_TYPE& type)
{
	if(key == "capt")			{ type = AuScPart::Part_BerthCapt; return true; }
	if(key == "officer")		{ type = AuScPart::Part_BerthOfficer; return true; }
	if(key == "crew")			{ type = AuScPart::Part_BerthCrew; return true; }
	if(key == "passenger")		{ type = AuScPart::Part_BerthPassenger; return true; }
	if(key == "troop")			{ type = AuScPart::Part_BerthTroop; return true; }
	return false;
}

static const char* kPanelTypes[] = {
	"powerPlant", "maneuverDrive", "capacitor", "lifeSupport", "sensors", "shields", "ftl"
};

static AuScPart::PART_TYPE PanelTypeFor(const QString& key)
{
	if(key == "powerPlant")		return AuScPart::Part_PanelPowerPlant;
	if(key == "maneuverDrive")	return AuScPart::Part_PanelManeuverDrive;
	if(key == "capacitor")		return AuScPart::Part_PanelCapacitor;
	if(key == "lifeSupport")	return AuScPart::Part_PanelLifeSupport;
	if(key == "sensors")		return AuScPart::Part_PanelSensor;
	if(key == "shields")		return AuScPart::Part_PanelShield;
	if(key == "ftl")			return AuScPart::Part_PanelFTLDrive;
	return AuScPart::Part_Invalid;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The layout strategies.
//
// Every strategy works in the same way: it produces a list of boxes in feet,
// one per room, that all sit inside the usable interior of the hull.

namespace {

struct Box
{
	QString	type;
	QString	label;
	AuPoint	mn;
	AuPoint	mx;
};

// Shrinks a box about its centre. A dimension that was already usable is
// never taken below SHIPGEN_MIN_ROOM_FEET, so a fitted room stays walkable.
static double FitDimension(double size, double scale, double minimum)
{
	double s = size * scale;
	if(size >= minimum)
		s = qMax(minimum, s);
	return s;
}

static Box ShrinkAboutCentre(const Box& b, double lengthScale, double depthScale, double heightScale)
{
	Box t = b;
	const double cx = (t.mn.x + t.mx.x) / 2;
	const double cy = (t.mn.y + t.mx.y) / 2;
	const double cz = (t.mn.z + t.mx.z) / 2;
	const double sx = FitDimension(t.mx.x - t.mn.x, lengthScale, SHIPGEN_MIN_ROOM_FEET);
	const double sy = FitDimension(t.mx.y - t.mn.y, depthScale, SHIPGEN_MIN_ROOM_FEET);
	const double sz = FitDimension(t.mx.z - t.mn.z, heightScale, SHIPGEN_MIN_ROOM_HEIGHT);
	t.mn = AuPoint(cx - sx/2, cy - sy/2, cz - sz/2);
	t.mx = AuPoint(cx + sx/2, cy + sy/2, cz + sz/2);
	return t;
}


// True when two rooms share interior space. Rooms that only share a wall do
// not count, and neither does a tiny numeric overlap.
static bool BoxesOverlap(const Box& a, const Box& b)
{
	const double e = .05;
	return a.mn.x < b.mx.x - e && a.mx.x > b.mn.x + e
			&& a.mn.y < b.mx.y - e && a.mx.y > b.mn.y + e
			&& a.mn.z < b.mx.z - e && a.mx.z > b.mn.z + e;
}

// How much of a room's wall touches rooms that are already placed. A door can
// only go in a shared wall, so a room that touches nothing is a room the crew
// cannot reach.
static double SharedWallArea(const Box& b, const QList<Box>& placed)
{
	const double e = .05;
	double area = 0.;
	for(const Box& o : placed)
	{
		// Walls that face along X.
		if(qAbs(b.mn.x - o.mx.x) < e || qAbs(b.mx.x - o.mn.x) < e)
		{
			const double y = qMin(b.mx.y, o.mx.y) - qMax(b.mn.y, o.mn.y);
			const double z = qMin(b.mx.z, o.mx.z) - qMax(b.mn.z, o.mn.z);
			if(y > e && z > e)
				area += y * z;
		}
		// Walls that face along Y.
		if(qAbs(b.mn.y - o.mx.y) < e || qAbs(b.mx.y - o.mn.y) < e)
		{
			const double x = qMin(b.mx.x, o.mx.x) - qMax(b.mn.x, o.mn.x);
			const double z = qMin(b.mx.z, o.mx.z) - qMax(b.mn.z, o.mn.z);
			if(x > e && z > e)
				area += x * z;
		}
	}
	return area;
}

// A room is only accepted when it is inside the hull and does not sit on top
// of a room that has already been placed.
static bool BoxFits(const AuScDesign& design, const HullAnalysis& hull, const Box& b, const QList<Box>& placed)
{
	if(!BoxInsideHull(design, hull, b.mn, b.mx))
		return false;
	for(const Box& o : placed)
		if(BoxesOverlap(b, o))
			return false;
	return true;
}

// Greedily splits the rooms into decks. A deck holds rooms whose widths add up
// to the usable length; all rooms in a deck share the deck height.
QList<QList<RoomSpec> > PackIntoDecks(const QList<RoomSpec>& rooms, double usableLength, double usableDepth,
		double usableHeight, int& outDeckCount)
{
	QList<QList<RoomSpec> > decks;
	QList<RoomSpec> current;
	double used = 0;
	double deckHeight = 0;

	for(const RoomSpec& r : rooms)
	{
		double w = r.sizeX > 0 ? r.sizeX : usableLength * .25;
		double h = r.sizeZ > 0 ? r.sizeZ : usableHeight * .5;
		bool tooWide = (used + w) > usableLength && !current.isEmpty();
		bool tooTall = (deckHeight > 0 && h > deckHeight * 1.6) && !current.isEmpty();

		if(tooWide || tooTall)
		{
			decks << current;
			current.clear();
			used = 0;
			deckHeight = 0;
		}
		current << r;
		used += w;
		deckHeight = qMax(deckHeight, h);
	}
	if(!current.isEmpty())
		decks << current;

	outDeckCount = decks.size();
	return decks;
}

// Scales every room so that the whole layout fits into the usable interior
// of the hull. Without this a template sized for a frigate simply will not
// fit into a small hull.
static void ScaleRooms(QList<RoomSpec>& rooms, double scale)
{
	for(RoomSpec& r : rooms)
	{
		r.sizeX = qMax(SHIPGEN_MIN_ROOM_FEET, r.sizeX * scale);
		r.sizeY = qMax(SHIPGEN_MIN_ROOM_FEET, r.sizeY * scale);
		r.sizeZ = qMax(SHIPGEN_MIN_ROOM_HEIGHT, r.sizeZ * scale);
	}
}

// Places the rooms into the layout space. Returns false when the layout does
// not fit at all.
static bool PlaceRooms(const QList<RoomSpec>& rooms, double usableLength, double usableDepth, double usableHeight,
		const LayoutOptions& opts, QList<Box>& boxes)
{
	boxes.clear();

	if(opts.strategy == "spine")
	{
		// A corridor runs along the ship. Rooms alternate between the port and
		// starboard sides of it. On a narrow hull the corridor has to give way,
		// or each room would be narrower than a person can use.
		const double maxCorridor = qMax(0., usableDepth - 2. * SHIPGEN_MIN_ROOM_FEET);
		const double corridor = qMin(opts.corridorWidth, maxCorridor);
		double x = 0;
		double z = 0;
		double deckHeight = 0;
		int side = 0;

		for(const RoomSpec& r : rooms)
		{
			double w = r.sizeX > 0 ? r.sizeX : usableLength * .2;
			double d = r.sizeY > 0 ? r.sizeY : usableDepth * .4;
			double h = r.sizeZ > 0 ? r.sizeZ : usableHeight * .5;

			w = qMin(w, usableLength);
			d = qMin(d, (usableDepth - corridor) * .5);
			// A hull too narrow for two rows keeps one room right across it.
			if(d < SHIPGEN_MIN_ROOM_FEET)
				d = usableDepth;
			h = qMin(h, usableHeight);

			if(x + w > usableLength)
			{
				// Wrap onto a new deck.
				x = 0;
				z += deckHeight + opts.deckThickness;
				deckHeight = 0;
				side = 0;
			}
			if(z + h > usableHeight)
				break;	// Out of vertical space.

			Box b;
			b.type = r.type;
			b.label = r.label;
			b.mn = AuPoint(x, 0, z);
			b.mx = AuPoint(x + w, 0, z + h);
			if(side % 2 == 0 || d >= usableDepth - 1e-6)
			{
				// Port side of the corridor, or the whole width.
				b.mn.y = 0;
				b.mx.y = d;
			}
			else
			{
				b.mn.y = usableDepth - d;
				b.mx.y = usableDepth;
			}
			boxes << b;
			x += w;
			deckHeight = qMax(deckHeight, h);
			++side;
		}
		return true;
	}

	if(opts.strategy == "blocks")
	{
		// Two dimensional packing inside each deck.
		int deckCount = 0;
		QList<QList<RoomSpec> > decks = PackIntoDecks(rooms, usableLength, usableDepth, usableHeight, deckCount);
		double z = 0;
		for(const QList<RoomSpec>& deck : decks)
		{
			double deckHeight = 0;
			for(const RoomSpec& r : deck)
				deckHeight = qMax(deckHeight, r.sizeZ > 0 ? r.sizeZ : usableHeight * .5);
			deckHeight = qMin(deckHeight, usableHeight);
			if(z + deckHeight > usableHeight)
				break;

			// Lay the rooms out in rows along X, alternating between the
			// fore and aft halves of the ship. The rows are kept short so
			// that the ship ends up with a blocky, two abreast interior
			// rather than a single long corridor.
			const double rowLength = usableLength * .5;
			const double rowDepth = usableDepth >= 2. * SHIPGEN_MIN_ROOM_FEET ? usableDepth * .5 : usableDepth;
			double x = 0;
			double y = 0;
			bool aft = true;
			for(const RoomSpec& r : deck)
			{
				double w = r.sizeX > 0 ? r.sizeX : usableLength * .25;
				double d = r.sizeY > 0 ? r.sizeY : usableDepth * .5;
				double h = r.sizeZ > 0 ? r.sizeZ : deckHeight;
				w = qMin(w, usableLength);
				d = qMin(d, rowDepth);
				h = qMin(h, deckHeight);
				h = qMin(h, usableHeight);
				if(x + w > rowLength)
				{
					// Next row.
					x = 0;
					y = aft ? 0 : rowDepth;
					aft = !aft;
					if(y + d > usableDepth)
						continue;	// No room left on this deck.
				}
				Box b;
				b.type = r.type;
				b.label = r.label;
				b.mn = AuPoint(x, y, z);
				b.mx = AuPoint(x + w, y + d, z + h);
				boxes << b;
				x += w;
			}
			z += deckHeight + opts.deckThickness;
		}
		return true;
	}

	// "decks": horizontal decks, rooms side by side along the ship.
	int deckCount = 0;
	QList<QList<RoomSpec> > decks = PackIntoDecks(rooms, usableLength, usableDepth, usableHeight, deckCount);
	double z = 0;
	for(const QList<RoomSpec>& deck : decks)
	{
		double deckHeight = 0;
		for(const RoomSpec& r : deck)
			deckHeight = qMax(deckHeight, r.sizeZ > 0 ? r.sizeZ : usableHeight * .5);
		deckHeight = qMin(deckHeight, usableHeight);
		if(z + deckHeight > usableHeight)
			break;

		double x = 0;
		for(const RoomSpec& r : deck)
		{
			double w = r.sizeX > 0 ? r.sizeX : usableLength * .25;
			double d = r.sizeY > 0 ? r.sizeY : usableDepth;
			double h = r.sizeZ > 0 ? r.sizeZ : deckHeight;
			w = qMin(w, usableLength);
			d = qMin(d, usableDepth);
			h = qMin(h, deckHeight);
			h = qMin(h, usableHeight);
			if(x + w > usableLength)
				break;	// This deck is full.

			Box b;
			b.type = r.type;
			b.label = r.label;
			b.mn = AuPoint(x, 0, z);
			b.mx = AuPoint(x + w, d, z + h);
			boxes << b;
			x += w;
		}
		z += deckHeight + opts.deckThickness;
	}
	return true;
}

} // namespace


//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

// Everything that goes into the rooms once they are there: stations,
// berths, service panels, walk paths and the hull volume of the systems.
// Shares the hull volume out between the systems: the FTL drive and the
// capacitor first, as much as they need to engage, then the rest by the
// brief's proportions.
// How many times the maneuver drive share of the brief the ships get.
#define SHIPGEN_MANEUVER_BOOST 3.5

static void AllocateVolumes(AuScDesign& design, const Brief& brief, const QList<quint16>& ftlParts,
		QList<quint16> weaponParts, const QSet<QString>& placedStationTypes, QStringList& warnings)
{
	// Module volumes.
	//
	// The hull volume is split up between the systems. Some of the systems
	// have a hard minimum: the FTL drive has to be big enough to engage and
	// the capacitor has to be big enough to power it. The rest is shared out
	// in the proportions the brief asks for.
	ModuleSpec modules = brief.modules;
	{
		// Work out the real hull volume now that the rooms are in place.
		design.RefreshObstructionsCache();
		const quint32 hullVolume = qMax(1u, design.HullVolume());

		// The FTL drive.
		const double ftlUnitsCuFt = pow(.75 * hullVolume * CubicFeetPerCubicMeter() * PI, 1./3.) * 256.;
		const quint32 ftlVolume = quint32(ceil(ftlUnitsCuFt / CubicFeetPerCubicMeter())) + 1;
		for(quint16 id : ftlParts)
			design.SetFTLDriveVolume(id, ftlVolume);

		// Weapon bay. It needs sensors to aim it, so it is only fitted when
		// there is a sensor station.
		const quint32 weaponVolume = qMax(1u, quint32(hullVolume * .02));
		if(weaponParts.isEmpty() || !placedStationTypes.contains("sensor"))
		{
			weaponParts.clear();
		}
		else
		{
			for(quint16 id : weaponParts)
				design.SetBayVolume(id, weaponVolume);
		}

		// The capacitor stores its charge in units of 256 cubic feet, and the
		// quality of the module scales how much of it can be used.
		const double qb = QualityBoostF(1);
		const double capUnitsPerM3 = CubicFeetPerCubicMeter() / 256. * qb;
		double capacitorMin = 0;
		if(!ftlParts.isEmpty())
		{
			// The capacitor has to be able to engage the FTL drive.
			const double powerToEngage = -.5 + ftlUnitsCuFt / 256.;
			capacitorMin = (powerToEngage / qb) / capUnitsPerM3;
		}
		if(!weaponParts.isEmpty())
		{
			// One shot has to be possible.
			const double projectors = weaponVolume * CubicFeetPerCubicMeter() / 256. / 25. + 1.;
			capacitorMin = qMax(capacitorMin, projectors / capUnitsPerM3);
		}
		const quint32 capacitorVolume = quint32(ceil(capacitorMin)) + 1;

		// Life support for every soul on board.
		const int berths = design.CaptBerths() + design.CrewBerths() + design.OfficerBerths() +
			design.PassengerBerths() + design.TroopBerths();
		const quint32 lifeSupportMin = qMax(1u, quint32(ceil(berths / QualityBoostF(1))));

		// Everything else shares what is left over.
		const quint32 fixed = ftlVolume * ftlParts.size() + weaponVolume * weaponParts.size() +
			capacitorVolume + lifeSupportMin;
		const quint32 flexible = (hullVolume > fixed) ? (hullVolume - fixed) : 0;

		struct Share { quint32* target; double weight; double fallback; };
		Share shares[] = {
			{ &modules.powerPlant,	brief.modules.powerPlant,	0.10 },
			{ &modules.maneuverDrive,	brief.modules.maneuverDrive,	0.08 },
			{ &modules.fuel,			brief.modules.fuel,		0.20 },
			{ &modules.armor,			brief.modules.armor,		0.08 },
			{ &modules.skeleton,		brief.modules.skeleton,	0.40 },
			{ &modules.sensors,		brief.modules.sensors,	0.02 },
			{ &modules.shields,		brief.modules.shields,	0.02 },
		};
		double weightSum = 0;
		for(const Share& sh : shares)
			weightSum += (*sh.target > 0) ? double(*sh.target) : sh.fallback;
		if(weightSum <= 0)
			weightSum = 1;
		for(const Share& sh : shares)
		{
			const double weight = (*sh.target > 0) ? double(*sh.target) : sh.fallback;
			*sh.target = quint32(flexible * weight / weightSum);
		}

		// The user found the ships in the game wanted 3 to 4 times more
		// maneuver drive than the shares give, so the drive gets that much
		// more, taken from the structure (which keeps at least half of its
		// share).
		{
			const quint32 wanted = quint32(modules.maneuverDrive * (SHIPGEN_MANEUVER_BOOST - 1.0));
			const quint32 spare = modules.skeleton / 2;
			const quint32 extra = qMin(wanted, spare);
			modules.maneuverDrive += extra;
			modules.skeleton -= extra;
		}

		modules.capacitor = capacitorVolume;
		modules.lifeSupport = lifeSupportMin;
		if(!modules.hold)
			modules.hold = 0;

		// Sensors and shields can only be used if there is a station for them.
		if(!placedStationTypes.contains("sensor"))
			modules.sensors = 0;
		if(!placedStationTypes.contains("shield"))
			modules.shields = 0;

		// Every system needs something, even on a small ship. The structure
		// gives up the room.
		struct Minimum { quint32* target; quint32 value; };
		Minimum minimums[] = {
			{ &modules.powerPlant,		qMax(1u, hullVolume / 50) },
			{ &modules.maneuverDrive,	qMax(1u, quint32(hullVolume / 50 * SHIPGEN_MANEUVER_BOOST)) },
			{ &modules.capacitor,		qMax(1u, capacitorVolume) },
			{ &modules.fuel,			qMax(1u, hullVolume / 20) },
			{ &modules.lifeSupport,	qMax(1u, lifeSupportMin) },
			// Some cargo space on every ship, whatever its template (the user,
			// 2026-11-07): 2 percent of the hull.
			{ &modules.hold,			qMax(1u, hullVolume / SHIPGEN_HOLD_MIN_SHARE) },
			{ &modules.skeleton,		1 },
		};
		for(const Minimum& m : minimums)
		{
			if(*m.target >= m.value)
				continue;
			const quint32 missing = m.value - *m.target;
			* m.target = m.value;
			// Take the space from the structure.
			if(modules.skeleton > missing)
				modules.skeleton -= missing;
			else
				modules.skeleton = 1;
		}

		if(!ftlParts.isEmpty())
			warnings << QString("%1 cubic meters of hull volume were allocated to the FTL drive.").arg(ftlVolume);
		if(!weaponParts.isEmpty())
			warnings << QString("%1 cubic meters of hull volume were allocated to the weapon bay.").arg(weaponVolume);
		warnings << QString("hull volume %1 cubic meters: capacitor %2, life support %3, power plant %4, "
				"maneuver drive %5, fuel %6, armor %7, skeleton %8, hold %9, sensors %10, shields %11.")
				.arg(hullVolume).arg(modules.capacitor).arg(modules.lifeSupport).arg(modules.powerPlant)
				.arg(modules.maneuverDrive).arg(modules.fuel).arg(modules.armor).arg(modules.skeleton)
				.arg(modules.hold).arg(modules.sensors).arg(modules.shields);
	}

	design.SetPowerPlantVolume(modules.powerPlant);
	design.SetManeuverDriveVolume(modules.maneuverDrive);
	design.SetCapacitorVolume(modules.capacitor);
	design.SetFuelVolume(modules.fuel);
	design.SetLifeSupportVolume(modules.lifeSupport);
	design.SetArmorVolume(modules.armor);
	design.SetSkeletonVolume(modules.skeleton);
	design.SetHoldVolume(modules.hold);
	design.SetSensorVolume(modules.sensors);
	design.SetShieldVolume(modules.shields);
	design.SetCryoBerthVolume(modules.cryoBerths);

	// The ship has to be able to manoeuvre. If the vertical acceleration is
	// too low, move some volume from the structure to the maneuver drive.
	for(int i = 0; i < 12; ++i)
	{
		AuScReport report;
		design.ReportDesign(report);
		if(report.ManeuverAccelerationVertical() / FeetPerMeter() >= 10.)
			break;
		const quint32 extra = qMax(1u, quint32(modules.maneuverDrive * .5));
		if(modules.skeleton <= extra)
			break;
		modules.skeleton -= extra;
		modules.maneuverDrive += extra;
		design.SetManeuverDriveVolume(modules.maneuverDrive);
		design.SetSkeletonVolume(modules.skeleton);
	}

	// The capacitor has to hold a minute of the idle power (the engine warns
	// "Capacitor is inadequate" otherwise; small craft got the least, 1 m3).
	// Grown a step at a time, from the structure.
	const quint32 capacitorBefore = modules.capacitor;
	for(int i = 0; i < 40; ++i)
	{
		AuScReport report;
		design.ReportDesign(report);
		if(report.CapacitorMax() >= report.IdlePowerWithShields() * 60)
		{
			if(modules.capacitor != capacitorBefore)
				warnings << QString("The capacitor was made %1 cubic meters, to hold a minute of the idle power.")
						.arg(modules.capacitor);
			break;
		}
		const quint32 extra = qMax(1u, quint32(modules.capacitor * .25));
		if(modules.skeleton <= extra + 1)
			break;
		modules.skeleton -= extra;
		modules.capacitor += extra;
		design.SetCapacitorVolume(modules.capacitor);
		design.SetSkeletonVolume(modules.skeleton);
	}

	// Without an engineer station the power plant and maneuver drive have to
	// stay under 100 m3 together (the engine warns otherwise): a fighter or
	// shuttle. Both shrink in step, the rest goes to the structure, as long
	// as the ship can still climb at 10 m/s2.
	if(!placedStationTypes.contains("engineer") && modules.powerPlant + modules.maneuverDrive > 100)
	{
		const quint32 oldPlant = modules.powerPlant, oldDrive = modules.maneuverDrive;
		const double scale = 99.0 / double(oldPlant + oldDrive);
		modules.powerPlant = qMax(1u, quint32(oldPlant * scale));
		modules.maneuverDrive = qMax(1u, quint32(oldDrive * scale));
		modules.skeleton += (oldPlant - modules.powerPlant) + (oldDrive - modules.maneuverDrive);
		design.SetPowerPlantVolume(modules.powerPlant);
		design.SetManeuverDriveVolume(modules.maneuverDrive);
		design.SetSkeletonVolume(modules.skeleton);
		AuScReport report;
		design.ReportDesign(report);
		const double powerOutput = report.FuelBurnRate() * report.PowerPerFuel();
		if(report.ManeuverAccelerationVertical() / FeetPerMeter() < 10. || report.IdlePowerWithShields() > powerOutput)
		{
			modules.skeleton -= (oldPlant - modules.powerPlant) + (oldDrive - modules.maneuverDrive);
			modules.powerPlant = oldPlant;
			modules.maneuverDrive = oldDrive;
			design.SetPowerPlantVolume(modules.powerPlant);
			design.SetManeuverDriveVolume(modules.maneuverDrive);
			design.SetSkeletonVolume(modules.skeleton);
		}
		else
			warnings << QString("With no engineer station, the power plant and maneuver drive were made %1 and %2 cubic "
					"meters (under 100 together).").arg(modules.powerPlant).arg(modules.maneuverDrive);
	}
}

void AllocateSystemVolumes(AuScDesign& design, const Brief& brief, QStringList& notes)
{
	QList<quint16> ftlParts, weaponParts;
	QSet<QString> placedStationTypes;
	for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p)
			continue;
		if(p->ModuleSystem() == SYS_FTLDrive && p->Module() != CID_Invalid)
			ftlParts << it.key();
		else if(p->ModuleSystem() == SYS_Weapon)
			weaponParts << it.key();
		for(const StationTypeEntry& e : kStations)
			if(p->Type() == e.type)
				placedStationTypes.insert(e.key);
	}
	AllocateVolumes(design, brief, ftlParts, weaponParts, placedStationTypes, notes);
}

// The integrated station that does a console's work, or the console's own type
// when there is none (a medic or transporter station needs more setting up).
static AuScPart::PART_TYPE DeskStationType(AuScPart::PART_TYPE console)
{
	switch(console)
	{
	case AuScPart::Part_ConsoleHelm:		return AuScPart::Part_StationHelm;
	case AuScPart::Part_ConsoleNavigator:	return AuScPart::Part_StationNavigator;
	case AuScPart::Part_ConsoleCommand:		return AuScPart::Part_StationCommand;
	case AuScPart::Part_ConsoleEngineer:	return AuScPart::Part_StationEngineer;
	case AuScPart::Part_ConsoleFireControl:	return AuScPart::Part_StationFireControl;
	case AuScPart::Part_ConsoleSensor:		return AuScPart::Part_StationSensor;
	case AuScPart::Part_ConsoleShield:		return AuScPart::Part_StationShield;
	case AuScPart::Part_ConsolePowerRelay:	return AuScPart::Part_StationPowerRelay;
	default:								return console;
	}
}

struct DeskSpot
{
	double	x = 0, y = 0;		// Where the operator sits, from above.
	double	yaw = 0;			// The way they face.
};

// Where the desks of a room go. Curved rows face fore and out towards the walls,
// on circles round the captain, who sits behind them looking fore. The rows are
// as tight as the number of desks allows, against the fore wall, so a big room
// keeps its space aft; each next row goes further in. A room too shallow for
// that gets one nearly straight row, the captain in the middle of it. Spots that
// leave the room or meet a door, ladder or stairs are skipped.
struct DeskLayout
{
	bool			hasCaptain = false;
	DeskSpot		captain;
	QList<DeskSpot>	crew;
	QList<QRectF>	taken;
};

static DeskLayout MakeDeskLayout(const PlacedRoom& room, int crewCount, bool wantCaptain, const LayoutOptions& opts)
{
	DeskLayout layout;
	QList<QRectF> keepOut;
	for(const QPair<AuPoint, AuPoint>& k : opts.stationKeepOut)
		if(k.first.z < room.max.z && room.min.z < k.second.z)
			keepOut << QRectF(QPointF(k.first.x, k.first.y), QPointF(k.second.x, k.second.y)).normalized();
	auto footprint = [](const DeskSpot& d) {
		const double c = cos(d.yaw), sn = sin(d.yaw);
		QRectF r;
		for(double u : { -kDeskBehind, kDeskReach })
			for(double w : { -kDeskWidth / 2, kDeskWidth / 2 })
			{
				const QPointF p(d.x + u * c - w * sn, d.y + u * sn + w * c);
				r = r.isNull() ? QRectF(p, QSizeF(1e-6, 1e-6)) : r.united(QRectF(p, QSizeF(1e-6, 1e-6)));
			}
		return r;
	};
	// The desk and the operator's place as they really stand, so that desks in a
	// curve can sit side by side: boxes round them would keep them far apart.
	QList<DeskSpot> placed;
	auto corners = [](const DeskSpot& d, QPointF out[4]) {
		const double c = cos(d.yaw), sn = sin(d.yaw), w = kDeskWidth / 2 + .4;
		const double us[4] = { -kDeskBehind, kDeskReach, kDeskReach, -kDeskBehind };
		const double ws[4] = { -w, -w, w, w };
		for(int i = 0; i < 4; ++i)
			out[i] = QPointF(d.x + us[i] * c - ws[i] * sn, d.y + us[i] * sn + ws[i] * c);
	};
	auto overlap = [&corners](const DeskSpot& a, const DeskSpot& b) {
		QPointF pa[4], pb[4];
		corners(a, pa);
		corners(b, pb);
		for(const DeskSpot* d : { &a, &b })
			for(double angle : { d->yaw, d->yaw + M_PI / 2 })
			{
				const QPointF axis(cos(angle), sin(angle));
				double a0 = 1e30, a1 = -1e30, b0 = 1e30, b1 = -1e30;
				for(int i = 0; i < 4; ++i)
				{
					const double u = QPointF::dotProduct(pa[i], axis), v = QPointF::dotProduct(pb[i], axis);
					a0 = qMin(a0, u);
					a1 = qMax(a1, u);
					b0 = qMin(b0, v);
					b1 = qMax(b1, v);
				}
				if(a1 <= b0 || b1 <= a0)
					return false;
			}
		return true;
	};
	auto fits = [&](const DeskSpot& d) {
		const QRectF r = footprint(d);
		if(r.left() < room.min.x + .3 || r.right() > room.max.x - .3 || r.top() < room.min.y + .3 || r.bottom() > room.max.y - .3)
			return false;
		for(const QRectF& o : keepOut)
			if(o.intersects(r))
				return false;
		for(const DeskSpot& o : placed)
			if(overlap(d, o))
				return false;
		return true;
	};
	auto take = [&](const DeskSpot& d) {
		placed << d;
		layout.taken << footprint(d);
	};

	const double middle = (room.min.x + room.max.x) / 2;
	const double frontY = room.max.y - .4 - kDeskReach;
	const double wanted = qBound(10.0, crewCount * (kDeskWidth + 1.0) / 2.0, 20.0);
	const double most = frontY - (room.min.y + kDeskBehind + .3);
	const bool roomy = most >= kDeskReach + 5.0;
	const double radius = roomy ? qMin(wanted, most) : 40.0;
	const double centreY = frontY - radius;
	// The captain sits on the centre line, behind every row: as far aft as they
	// fit, further forward when a door or the wall is in the way. The front row
	// only if nothing is left.
	if(wantCaptain)
		for(double y = qMax(centreY, room.min.y + kDeskBehind + .3); y <= frontY - (kDeskReach + kDeskBehind + .5); y += .5)
		{
			DeskSpot captain;
			captain.x = middle;
			captain.y = y;
			captain.yaw = M_PI / 2;
			if(!fits(captain))
				continue;
			layout.captain = captain;
			layout.hasCaptain = true;
			take(captain);
			break;
		}
	int left = crewCount + (wantCaptain && !layout.hasCaptain ? 1 : 0);
	const double limit = 80.0 * M_PI / 180;
	for(double r = radius; r >= 6.0 && left > 0; r -= 6.0)
	{
		const double step = (kDeskWidth + 1.0) / (r - kDeskBehind);		// So that the inner corners just clear.
		int slotCount = 1 + 2 * int(limit / step);		// Odd: one in the middle.
		if(left % 2 == 0)
			slotCount = qMax(2, 2 * int((limit + step / 2) / step));
		QList<double> angles;
		for(int i = 0; i < slotCount; ++i)
			angles << (i - (slotCount - 1) / 2.0) * step;
		std::sort(angles.begin(), angles.end(), [](double a, double b) {
			return fabs(a) != fabs(b) ? fabs(a) < fabs(b) : a < b;
		});
		QList<DeskSpot> row;
		for(double a : angles)
		{
			if(left <= 0)
				break;
			DeskSpot d;
			d.x = middle + r * sin(a);
			d.y = centreY + r * cos(a);
			d.yaw = M_PI / 2 - a;
			if(!fits(d))
				continue;
			take(d);
			row << d;
			--left;
		}
		layout.crew << row;
	}
	// Whatever the curves had no room for: straight rows, facing fore, from the
	// fore wall back.
	for(double y = frontY; y >= room.min.y + kDeskBehind && left > 0; y -= kDeskReach + kDeskBehind + 1.0)
		for(int n = 0; n < 200 && left > 0; ++n)
		{
			DeskSpot d;
			d.x = middle + ((n + 1) / 2) * (n % 2 ? 1 : -1) * (kDeskWidth + 1.0);
			d.y = y;
			d.yaw = M_PI / 2;
			if(fabs(d.x - middle) > (room.max.x - room.min.x) / 2)
				break;
			if(!fits(d))
				continue;
			take(d);
			layout.crew << d;
			--left;
		}
	// Too shallow for a captain's place of their own: the middle of the row.
	if(wantCaptain && !layout.hasCaptain && !layout.crew.isEmpty())
	{
		layout.captain = layout.crew.takeFirst();
		layout.hasCaptain = true;
	}
	return layout;
}

// Pads for each transporter: 3 x 3 ft slabs on the floor of its room, drawn
// with its pad effect, their middles in the console's list of pad locations
// (as in the user's working airport; SOH_FORMAT.md, "Transporter pads"). At
// least 0.5 m between pads (the user, 2026-11-06), so their middles are
// SHIPGEN_PAD_SPACING apart; clear of the room's walls, its stations and
// furniture, and its doors, ladders and stairs. Up to SHIPGEN_PADS_MAX.
void AddTransporterPads(AuDesign& design, const QList<PlacedRoom>& rooms, const QList<QPair<AuPoint, AuPoint> >& keepOut,
		QStringList& warnings, const QHash<QString, QList<AuPoint> >& fixedPads, bool autoPads)
{
	const double half = 1.5, spacing = SHIPGEN_PAD_SPACING;
	int effect = 0;
	QList<quint16> transporters;
	for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
		if(it.value() && (it.value()->Type() == AuScPart::Part_ConsoleTransporter
				|| it.value()->Type() == AuScPart::Part_StationTransporter))
			transporters << it.key();
	for(quint16 id : transporters)
	{
		AuScPart* console = design.Parts().value(id);
		const AuPoint op = console->OperatorLoc();
		const PlacedRoom* room = 0;
		for(const PlacedRoom& r : rooms)
			if(op.x >= r.min.x - .5 && op.x <= r.max.x + .5 && op.y >= r.min.y - .5 && op.y <= r.max.y + .5
					&& op.z >= r.min.z - .5 && op.z <= r.max.z + .5)
				room = &r;
		if(!room)
			continue;
		AuPointArray pads;
		if(fixedPads.contains(room->label))
		{
			// Placed by hand: exactly there.
			for(const AuPoint& c : fixedPads.value(room->label))
				pads.append(AuPoint(c.x, c.y, room->min.z + .1));
		}
		else if(!autoPads)
			continue;
		if(pads.isEmpty())
		{
		// What is in the way: everything standing in the room but the room
		// itself, and the places kept clear.
		QList<QPair<AuPoint, AuPoint> > inTheWay = keepOut;
		const double roomArea = (room->max.x - room->min.x) * (room->max.y - room->min.y);
		for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
		{
			const AuScPart* p = it.value();
			if(!p || p->IsRoomVoid() || p->IsHullVoid() || p->IsRoomMesh() || p->IsHullMesh() || p->IsPath())
				continue;
			AuPoint mn, mx;
			p->ModelExtents(mn, mx);
			if(mx.x < room->min.x || mn.x > room->max.x || mx.y < room->min.y || mn.y > room->max.y
					|| mx.z < room->min.z || mn.z > room->max.z)
				continue;
			if((mx.x - mn.x) * (mx.y - mn.y) > roomArea * .8)
				continue;		// The room's own shell.
			if(dynamic_cast<const AuScConsole*>(p))
				mn.z = qMin(mn.z, room->min.z);		// A console in a building is an icon in the air: its floor too.
			inTheWay << qMakePair(mn, mx);
		}
		inTheWay << qMakePair(op - AuPoint(1.5, 1.5, 0), op + AuPoint(1.5, 1.5, 6));
		auto clear = [&](const AuPoint& c) {
			for(const QPair<AuPoint, AuPoint>& b : inTheWay)
				if(c.x + half + .5 > b.first.x && c.x - half - .5 < b.second.x
						&& c.y + half + .5 > b.first.y && c.y - half - .5 < b.second.y
						&& room->min.z + 1.0 > b.first.z && room->min.z < b.second.z)
					return false;
			return true;
		};
		// On a grid of the spacing, tried at every quarter-foot offset: the
		// tightest group of the most pads, nearest the console.
		const double x0 = room->min.x + half + .5, x1 = room->max.x - half - .5;
		const double y0 = room->min.y + half + .5, y1 = room->max.y - half - .5;
		double bestScore = 1e30;
		for(double ox = 0; ox < spacing - 1e-6; ox += .25)
			for(double oy = 0; oy < spacing - 1e-6; oy += .25)
			{
				QList<AuPoint> spots;
				for(double y = y0 + oy; y <= y1 + 1e-6; y += spacing)
					for(double x = x0 + ox; x <= x1 + 1e-6; x += spacing)
					{
						const AuPoint c(x, y, room->min.z + .1);
						if(clear(c))
							spots << c;
					}
				for(const AuPoint& seed : spots)
				{
					QList<AuPoint> closest = spots;
					std::sort(closest.begin(), closest.end(), [&seed](const AuPoint& u, const AuPoint& v) {
						return hypot(u.x - seed.x, u.y - seed.y) < hypot(v.x - seed.x, v.y - seed.y); });
					const int n = qMin(int(closest.size()), SHIPGEN_PADS_MAX);
					double spread = 0;
					AuPoint mid;
					for(int i = 0; i < n; ++i)
					{
						spread += hypot(closest[i].x - seed.x, closest[i].y - seed.y);
						mid = mid + closest[i] / double(n);
					}
					// Fewer pads cost most, then spread, then the walk from the console.
					const double score = (SHIPGEN_PADS_MAX - n) * 1e6 + spread * 10 + hypot(mid.x - op.x, mid.y - op.y);
					if(n > 0 && score < bestScore)
					{
						bestScore = score;
						pads.clear();
						for(int i = 0; i < n; ++i)
							pads.append(closest[i]);
					}
				}
			}
		if(pads.isEmpty())
		{
			warnings << QString("'%1' has no room for a transporter pad (3 ft square, clear of its stations and doors).")
					.arg(room->label);
			continue;
		}
		}
		for(const AuPoint& c : pads)
		{
			AuScFaceList slab = BoxFaces(AuPoint(c.x - half, c.y - half, room->min.z), AuPoint(c.x + half, c.y + half, c.z));
			OrientAsHull(slab);
			slab.SetEffect(SURFACE_EFFECT(EFF_TransporterPad1 + effect));
			AuScMesh pad;
			pad.AddFaces(slab);
			pad.SetGroupName(room->label);
			if(pad.IsValid())
				design.Parts().AddPart(pad);
		}
		console = design.Parts().value(id);
		console->SetPadLocs(pads);
		console->SetWhichEffect(effect);
		effect = (effect + 1) % 10;
	}
}

static void FurnishLayout(AuScDesign& design, const Brief& brief, const HullAnalysis& hull, const LayoutOptions& opts,
		LayoutResult& result, const QList<DoorLink>& doorLinks)
{
	QMap<QString, QList<PlacedRoom> > byType;
	for(const PlacedRoom& pr : result.rooms)
		byType[pr.type] << pr;

	// Stations. They go on the port wall of a matching room, or on any room
	// when the brief does not say where. Several stations can share a room.
	// How many berths can the rooms hold? There cannot be more crew stations
	// than there are berths for the crew.
	int berthCapacity = 0;
	for(const PlacedRoom& r : result.rooms)
	{
		const int columns = qMax(1, int((r.max.x - r.min.x - 6) / 4));
		const int rows = qMax(1, int((r.max.y - r.min.y - 6) / 4));
		berthCapacity += columns * rows;
	}

	// Can this ship carry an FTL drive? The drive and the capacitor that
	// feeds it need more room than a small hull has, so on a small ship the
	// drive is left out and the engineers run the maneuver drives.
	const double hullM3 = qMax(1.0, hull.volumeM3);
	const double ftlUnitsCuFt = pow(.75 * hullM3 * CubicFeetPerCubicMeter() * PI, 1./3.) * 256.;
	const double ftlVolumeNeeded = ceil(ftlUnitsCuFt / CubicFeetPerCubicMeter()) + 1;
	const double qb = QualityBoostF(1);
	const double capUnitsPerM3 = CubicFeetPerCubicMeter() / 256. * qb;
	const double capacitorNeeded = ((-.5 + ftlUnitsCuFt / 256.) / qb) / capUnitsPerM3;
	const bool canHaveFtl = (ftlVolumeNeeded + capacitorNeeded) < hullM3 * .95;
	if(!canHaveFtl)
		result.warnings << QString("the hull is too small for an FTL drive, so the ship is fitted without one.");

	QMap<QString, int> consoleCount;
	QMap<QString, DeskLayout> deskLayouts;
	QMap<QString, int> desksUsed;
	QMap<QString, QList<QRectF> > chairSpots;		// Where the chairs of each room stand, from above.
	QMap<QString, int> stationCount;		// Per room type.
	QSet<QString> placedStationTypes;
	QList<quint16> ftlParts;
	QList<quint16> weaponParts;
	int crewStations = 0;
	// Placed by hand first, exactly where they were put; each counts against
	// one of the template's stations of its type.
	QMap<QString, int> fixedLeft;
	for(const LayoutOptions::FixedStation& f : opts.fixedStations)
	{
		AuScPart::PART_TYPE partType;
		COMMODITYID module;
		if(!FindStationType(f.type, partType, module))
		{
			result.warnings << QString("unknown station type '%1' (placed by hand) was skipped.").arg(f.type);
			continue;
		}
		if(partType == AuScPart::Part_ConsoleFireControl)
			module = COMMODITYID(f.module > 0 ? f.module : opts.fireControlModule > 0 ? opts.fireControlModule : int(module));
		if(f.type == "engineer")
		{
			if(FtlDriveModule(brief.ftlDrive) == CID_Invalid || !canHaveFtl)
			{
				result.warnings << QString("the engineer station placed in '%1' was left out: the ship has no FTL drive.").arg(f.room);
				continue;
			}
			module = FtlDriveModule(brief.ftlDrive);
		}
		const AuScPart::PART_TYPE deskType = DeskStationType(partType);
		const bool desk = deskType != partType && (f.seat == "desk" || (f.seat.isEmpty() && opts.bridgeStyle == "desks"));
		quint16 partId = 0;
		if(desk)
		{
			// A desk's yaw is from +X; the facing is from +Y.
			const double yaw = f.facing + M_PI / 2;
			AuMatrix4x4 screenPlane;
			AuPoint corner1, corner2;
			AuScPart* part = AuScPart::Create(deskType);
			AuScStation* station = dynamic_cast<AuScStation*>(part);
			if(station && AddStationDesk(design, f.floor, yaw, f.room, screenPlane, corner1, corner2, opts.deskPaint,
					opts.deskSeatPaint, opts.deskTopFeet, !opts.deskStanding))
			{
				const AuPoint seat = opts.deskStanding ? f.floor : f.floor + AuPoint(0, 0, AuStation::SeatHeight());
				station->MakeStation(opts.deskStanding ? AuScStation::Operator_StandStationary : AuScStation::Operator_SitStationary,
						seat, seat + AuPoint(cos(yaw), sin(yaw), 0) * FeetPerMeter(), corner1, corner2, screenPlane);
				station->SetModule(module);
				if(deskType == AuScPart::Part_StationFireControl)
					station->SetEmitLoc(f.floor + AuPoint(0, 0, 3.0));
				if(station->IsValid())
					partId = design.Parts().AddPart(*station);
			}
			delete part;
		}
		else
		{
			AuScPart* part = AuScPart::Create(partType);
			AuScConsole* console = dynamic_cast<AuScConsole*>(part);
			if(console)
			{
				console->MakeIcon(f.floor, float(f.facing), AuMatrix4x4());
				console->SetModule(module);
				if(AuScConsoleFireControl* fc = dynamic_cast<AuScConsoleFireControl*>(console))
					fc->SetEmitLoc(f.floor + AuPoint(0, 0, 3.0));
				if(console->IsValid())
					partId = design.Parts().AddPart(*console);
			}
			delete part;
		}
		if(!partId)
		{
			result.warnings << QString("the '%1' station placed in '%2' was not taken by the engine.").arg(f.type, f.room);
			continue;
		}
		const AuScPart* added = design.Parts().value(partId);
		if(added && added->ModuleSystem() == SYS_FTLDrive)
			ftlParts << partId;
		else if(added && added->ModuleSystem() == SYS_Weapon)
			weaponParts << partId;
		placedStationTypes.insert(f.type);
		if(partType != AuScPart::Part_ConsoleCommand)
			++crewStations;
		++result.stationsPlaced;
		fixedLeft[f.type] += 1;
		// The automatic chairs and desks keep clear of it.
		for(const PlacedRoom& r : result.rooms)
			if(r.label == f.room)
			{
				const QString key = QString("%1,%2,%3").arg(r.min.x).arg(r.min.y).arg(r.min.z);
				chairSpots[key] << QRectF(f.floor.x - 3, f.floor.y - 3, 6, 6);
				stationCount[r.type] += 1;
			}
	}
	for(const StationSpec& s : brief.ExpandedStations())
	{
		// One of this type was placed by hand.
		if(fixedLeft.value(s.type) > 0)
		{
			fixedLeft[s.type] -= 1;
			continue;
		}
		if(!opts.autoStations)
			continue;
		AuScPart::PART_TYPE partType;
		COMMODITYID module;
		if(!FindStationType(s.type, partType, module))
		{
			result.warnings << QString("unknown station type '%1' was skipped.").arg(s.type);
			continue;
		}
		if(partType == AuScPart::Part_ConsoleFireControl && opts.fireControlModule)
			module = COMMODITYID(opts.fireControlModule);

		// Find a room for it. Rooms of the wanted type come first, and among
		// those the one with the fewest stations already in it.
		// One engineer runs the one FTL drive a ship may have. The game fits
		// the maneuver drive by itself, so no engineer is needed for it.
		if(s.type == "engineer")
		{
			if(FtlDriveModule(brief.ftlDrive) == CID_Invalid)
				continue;
			if(!canHaveFtl)
			{
				result.warnings << QString("no engineer station, because the hull has no room for an FTL drive.");
				continue;
			}
			if(!ftlParts.isEmpty())
				continue;
			module = FtlDriveModule(brief.ftlDrive);
		}
		PlacedRoom target;
		bool found = false;
		int bestCount = 0;
		if(!s.roomType.isEmpty())
		{
			for(const PlacedRoom& r : result.rooms)
			{
				if(r.type != s.roomType)
					continue;
				const int count = stationCount[r.type];
				if(!found || count < bestCount)
				{
					target = r;
					bestCount = count;
					found = true;
				}
			}
		}
		// With desks, a station with no room of its own goes on the bridge
		// rather than in whatever room has the fewest.
		if(!found && opts.bridgeStyle == "desks" && DeskStationType(partType) != partType)
		{
			for(const PlacedRoom& r : result.rooms)
				if(r.type == "Bridge" && (!found || stationCount[r.type] < bestCount))
				{
					target = r;
					bestCount = stationCount[r.type];
					found = true;
				}
		}
		if(!found)
		{
			for(const PlacedRoom& r : result.rooms)
			{
				const int count = stationCount[r.type];
				if(!found || count < bestCount)
				{
					target = r;
					bestCount = count;
					found = true;
				}
			}
		}
		if(!found)
		{
			result.warnings << QString("no room left for the '%1' station.").arg(s.type);
			continue;
		}
		if(partType != AuScPart::Part_ConsoleCommand && crewStations >= berthCapacity)
		{
			result.warnings << QString("no spare berth for the '%1' station.").arg(s.type);
			continue;
		}
		stationCount[target.type] = bestCount + 1;

		// As an integrated station on a desk, where the style asks for it and
		// this station has one; otherwise it is a chair below.
		const AuScPart::PART_TYPE deskType = DeskStationType(partType);
		if(opts.bridgeStyle == "desks" && deskType != partType)
		{
			const QString deskKey = QString("%1,%2,%3").arg(target.min.x).arg(target.min.y).arg(target.min.z);
			if(!deskLayouts.contains(deskKey))
			{
				int crew = 0;
				bool captain = false;
				for(const StationSpec& o : brief.ExpandedStations())
				{
					AuScPart::PART_TYPE otherType;
					COMMODITYID otherModule;
					if(!FindStationType(o.type, otherType, otherModule) || DeskStationType(otherType) == otherType)
						continue;
					// The room it will go to: its own type if there is one, or the bridge.
					bool ownRoom = false;
					for(const PlacedRoom& r : result.rooms)
						ownRoom = ownRoom || (!o.roomType.isEmpty() && r.type == o.roomType);
					if((ownRoom ? o.roomType : QString("Bridge")) != target.type)
						continue;
					if(o.type == "engineer" && FtlDriveModule(brief.ftlDrive) == CID_Invalid)
						continue;
					if(otherType == AuScPart::Part_ConsoleCommand)
						captain = true;
					else
						++crew;
				}
				LayoutOptions clear = opts;
				for(const LayoutOptions::FixedStation& f : opts.fixedStations)
					clear.stationKeepOut << qMakePair(f.floor - AuPoint(3, 3, 0), f.floor + AuPoint(3, 3, 8));
				deskLayouts.insert(deskKey, MakeDeskLayout(target, crew, captain, clear));
				chairSpots[QString("%1,%2,%3").arg(target.min.x).arg(target.min.y).arg(target.min.z)] = deskLayouts.value(deskKey).taken;
			}
			const DeskLayout& desks = deskLayouts[deskKey];
			const bool isCaptain = partType == AuScPart::Part_ConsoleCommand;
			const int used = desksUsed.value(deskKey);
			if(isCaptain ? desks.hasCaptain : used < desks.crew.size())
			{
				const DeskSpot spot = isCaptain ? desks.captain : desks.crew.at(used);
				if(!isCaptain)
					desksUsed[deskKey] = used + 1;
				const AuPoint floorLoc(spot.x, spot.y, target.min.z);
				AuMatrix4x4 screenPlane;
				AuPoint corner1, corner2;
				AuScPart* part = AuScPart::Create(deskType);
				AuScStation* station = dynamic_cast<AuScStation*>(part);
				if(station && AddStationDesk(design, floorLoc, spot.yaw, target.label, screenPlane, corner1, corner2, opts.deskPaint, opts.deskSeatPaint,
						opts.deskTopFeet, !opts.deskStanding))
				{
					// Someone standing is placed by their feet, as in the Atlantis and the
					// Arnesh; someone sitting by the seat.
					const AuPoint seat = opts.deskStanding ? floorLoc : floorLoc + AuPoint(0, 0, AuStation::SeatHeight());
					station->MakeStation(opts.deskStanding ? AuScStation::Operator_StandStationary : AuScStation::Operator_SitStationary,
							seat, seat + AuPoint(cos(spot.yaw), sin(spot.yaw), 0) * FeetPerMeter(), corner1, corner2, screenPlane);
					station->SetModule(module);
					if(deskType == AuScPart::Part_StationFireControl)
						station->SetEmitLoc(floorLoc + AuPoint(0, 0, 3.0));
					if(station->IsValid())
					{
						const quint16 partId = design.Parts().AddPart(*station);
						if(station->ModuleSystem() == SYS_FTLDrive)
							ftlParts << partId;
						else if(station->ModuleSystem() == SYS_Weapon)
							weaponParts << partId;
						placedStationTypes.insert(s.type);
						if(partType != AuScPart::Part_ConsoleCommand)
							++crewStations;
						++result.stationsPlaced;
						delete part;
						continue;
					}
				}
				delete part;
			}
			result.warnings << QString("no room for a desk for the '%1' station in '%2'; it gets a swivel chair.").arg(s.type, target.label);
		}

		// A console is the game's swivel chair: a flat icon on the floor,
		// 1 m wide and 1.7 m deep, facing its local +Y. In a room with
		// windows the chairs sit in front of them, looking out (the user
		// wanted the bridge chairs at the windows); otherwise they face fore
		// (+Y) in rows from the fore wall backwards. They are counted per
		// room, not per station type.
		const QString roomKey = QString("%1,%2,%3").arg(target.min.x).arg(target.min.y).arg(target.min.z);
		const int index = consoleCount[roomKey]++;
		const double feet = FeetPerMeter();
		const double chairWidth = feet * 1.0 + 1.5, chairDepth = feet * 1.7 + 2.5;
		const double usableX = (target.max.x - target.min.x) - 2.0;
		const int perRow = qMax(1, int(usableX / chairWidth));
		const int rows = qMax(1, int(((target.max.y - target.min.y) - 1.5) / chairDepth));
		const double rowWidth = perRow * chairWidth;
		// A spot in the rows facing fore, the n-th from the fore wall back;
		// past the last row, half a chair over.
		auto rowSpot = [&](int n, double& x, double& y) {
			const int slot = n % perRow;
			const int row = (n / perRow) % rows;
			const double shift = ((n / (perRow * rows)) % 2) * chairWidth * .5;
			x = (target.min.x + target.max.x)/2 - rowWidth/2 + chairWidth * (slot + .5) + shift;
			y = target.max.y - 1.0 - feet * 1.1 - row * chairDepth;
		};
		double cx = 0, cy = 0;
		rowSpot(index, cx, cy);
		const double cz = target.min.z;
		float facing = 0.f;
		{
			// The room's windows, and the wall each is beyond.
			QList<QPair<AuPoint, int> > windows;		// Middle, and the wall: 0 +x, 1 -x, 2 +y, 3 -y.
			for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
			{
				const AuScPart* w = it.value();
				AuPoint wmn, wmx;
				if(!w || !w->IsWindow() || w->GroupName() != target.label || !w->ModelExtents(wmn, wmx))
					continue;
				// The wall it is beyond: windows are out at the hull skin,
				// which may be some way from the room.
				const AuPoint m = (wmn + wmx) / 2.0;
				const double d[4] = { m.x - target.max.x, target.min.x - m.x, m.y - target.max.y, target.min.y - m.y };
				int wall = 0;
				for(int k = 1; k < 4; ++k)
					if(d[k] > d[wall])
						wall = k;
				windows << qMakePair(m, wall);
			}
			// Only the fore windows draw the chairs to them: chairs face the
			// front (the user, 2026-10-31: at side windows they faced the
			// sides). The helm gets the middle of the fore windows.
			QList<QPair<AuPoint, int> > byWall[4];
			for(const QPair<AuPoint, int>& w : windows)
				byWall[w.second] << w;
			for(QList<QPair<AuPoint, int> >& list : byWall)
				std::sort(list.begin(), list.end(), [](const QPair<AuPoint, int>& a, const QPair<AuPoint, int>& b) {
					return a.first.y != b.first.y ? a.first.y > b.first.y : a.first.x < b.first.x;
				});
			windows.clear();
			{
				// From the middle outwards, so the first chairs are central.
				QList<QPair<AuPoint, int> > fore = byWall[2];
				const double middleX = (target.min.x + target.max.x) / 2;
				std::sort(fore.begin(), fore.end(), [middleX](const QPair<AuPoint, int>& a, const QPair<AuPoint, int>& b) {
					return fabs(a.first.x - middleX) < fabs(b.first.x - middleX);
				});
				for(const QPair<AuPoint, int>& w : fore)
					windows << w;
			}
			// MakeIcon turns the chair's +Y by 'facing' about z.
			static const float turns[4] = { float(-PI / 2), float(PI / 2), 0.f, float(PI) };
			// The chair from above, with a little room round it.
			auto footprint = [feet](double x, double y, float turn) {
				const double c = cos(turn), sn = sin(turn);
				QRectF r;
				for(double lx : { -.5 * feet, .5 * feet })
					for(double ly : { -.6 * feet, 1.1 * feet })
					{
						const QPointF p(x + lx * c - ly * sn, y + lx * sn + ly * c);
						r = r.isNull() ? QRectF(p, QSizeF(1e-6, 1e-6)) : r.united(QRectF(p, QSizeF(1e-6, 1e-6)));
					}
				return r.adjusted(-.3, -.3, .3, .3);
			};
			QList<QRectF>& taken = chairSpots[roomKey];
			// The room's box is the largest box of whole cells in it, so a
			// chair inside the box is inside the room.
			auto inside = [&](const QRectF& r) {
				return !(r.left() < target.min.x || r.right() > target.max.x || r.top() < target.min.y || r.bottom() > target.max.y);
			};
			auto free = [&](const QRectF& r) {
				if(!inside(r))
					return false;
				for(const QRectF& o : taken)
					if(o.intersects(r))
						return false;
				return true;
			};
			bool placed = false;
			// In front of the windows first, a chair further in each round.
			for(int k = 0; k < windows.size() * 3 && !placed; ++k)
			{
				const QPair<AuPoint, int>& w = windows.at(k % windows.size());
				const double inset = 1.0 + feet * 1.1 + (k / windows.size()) * chairDepth;
				double x = w.first.x, y = w.first.y;
				if(w.second == 0) x = target.max.x - inset;
				if(w.second == 1) x = target.min.x + inset;
				if(w.second == 2) y = target.max.y - inset;
				if(w.second == 3) y = target.min.y + inset;
				if(free(footprint(x, y, turns[w.second])))
				{
					cx = x;
					cy = y;
					facing = turns[w.second];
					placed = true;
				}
			}
			// Then the rows.
			for(int n = 0; n < perRow * rows * 2 && !placed; ++n)
			{
				double x, y;
				rowSpot(n, x, y);
				if(free(footprint(x, y, 0.f)))
				{
					cx = x;
					cy = y;
					facing = 0.f;
					placed = true;
				}
			}
			// Then anywhere in the box, any way round, nearest the fore wall
			// first; then overlapping another chair rather than sticking out
			// of the room (the user, 2026-11-08: chairs clipped through walls).
			for(int pass = 0; pass < 2 && !placed; ++pass)
				for(double y = target.max.y; y >= target.min.y && !placed; y -= .5)
				{
					const double middleX = (target.min.x + target.max.x) / 2;
					for(double off = 0; off <= (target.max.x - target.min.x) / 2 + .5 && !placed; off += .5)
						for(double x : { middleX - off, middleX + off })
							for(int w : { 2, 0, 1, 3 })
								if(!placed && (pass == 0 ? free(footprint(x, y, turns[w])) : inside(footprint(x, y, turns[w]))))
								{
									cx = x;
									cy = y;
									facing = turns[w];
									placed = true;
								}
				}
			if(!placed)
			{
				// Smaller than a chair: in the middle, along its longer side.
				cx = (target.min.x + target.max.x) / 2;
				cy = (target.min.y + target.max.y) / 2;
				facing = (target.max.x - target.min.x) > (target.max.y - target.min.y) ? turns[0] : 0.f;
				const QRectF r = footprint(0, 0, facing);
				cx -= r.center().x();
				cy -= r.center().y();
				result.warnings << QString("'%1' is too small for the '%2' station's chair (about 1 by 1.7 m with room "
						"round it); it reaches out of the room. Give it a bigger block of whole cells.").arg(target.label, s.type);
			}
			taken << footprint(cx, cy, facing);
		}

		AuScPart* part = AuScPart::Create(partType);
		if(!part)
			continue;
		AuScConsole* console = dynamic_cast<AuScConsole*>(part);
		if(!console)
		{
			delete part;
			continue;
		}
		console->MakeIcon(AuPoint(cx, cy, cz), facing, AuMatrix4x4());
		console->SetModule(module);
		if(AuScConsoleFireControl* fc = dynamic_cast<AuScConsoleFireControl*>(console))
			fc->SetEmitLoc(AuPoint(cx, cy, cz + 3.0));
		if(!console->IsValid())
		{
			delete console;
			continue;
		}
		const quint16 partId = design.Parts().AddPart(*console);
		if(console->ModuleSystem() == SYS_FTLDrive)
			ftlParts << partId;
		else if(console->ModuleSystem() == SYS_Weapon)
			weaponParts << partId;
		placedStationTypes.insert(s.type);
		if(partType != AuScPart::Part_ConsoleCommand)
			++crewStations;
		delete console;
		++result.stationsPlaced;
	}

	// A weapon bay needs sensors to aim it. If there is no sensor station,
	// the fire control stations have to go.
	if(!placedStationTypes.contains("sensor") && !weaponParts.isEmpty())
	{
		AuScParts deleted;
		for(quint16 id : weaponParts)
			design.Parts().DeletePart(id, deleted);
		weaponParts.clear();
		result.warnings << QString("the fire control stations were removed, because there are no sensors to aim them.");
		result.stationsPlaced -= 1;
	}

	// Berths.
	// Berths placed by hand, exactly there; each counts against one of its kind.
	QMap<QString, int> fixedBerths;
	for(const LayoutOptions::FixedBerth& f : opts.fixedBerths)
	{
		AuScPart::PART_TYPE partType;
		if(!FindBerthType(f.kind, partType))
		{
			result.warnings << QString("unknown berth kind '%1' (placed by hand) was skipped.").arg(f.kind);
			continue;
		}
		AuScPart* part = AuScPart::Create(partType);
		AuScBerth* berth = dynamic_cast<AuScBerth*>(part);
		if(berth)
		{
			berth->MakeIcon(f.floor + AuPoint(0, 0, 2), float(f.facing), IdentityPlane());
			if(berth->IsValid())
			{
				design.Parts().AddPart(*berth);
				++result.berthsPlaced;
				fixedBerths[f.kind] += 1;
			}
		}
		delete part;
	}
	if(opts.placeBerths)
	{
		struct BerthGroup { const char* key; int count; };
		// The crew berths come first so that there is always a berth for
		// every crew station. Those placed by hand count.
		BerthGroup groups[] = {
			{ "crew", qMax(brief.berths.crew, crewStations) - fixedBerths.value("crew") },
			{ "capt", brief.berths.capt - fixedBerths.value("capt") },
			{ "officer", brief.berths.officer - fixedBerths.value("officer") },
			{ "passenger", brief.berths.passenger - fixedBerths.value("passenger") },
			{ "troop", brief.berths.troop - fixedBerths.value("troop") },
		};
		// Spread the berths over the rooms, preferring crew quarters.
		QMap<int, int> berthCount;
		QStringList preferTypes;
		preferTypes << "crew" << "passenger" << "bridge" << "lab" << "cargo" << "holds";
		for(const BerthGroup& g : groups)
		{
			if(g.count <= 0)
				continue;
			AuScPart::PART_TYPE partType;
			if(!FindBerthType(g.key, partType))
				continue;
			for(int i = 0; i < g.count; ++i)
			{
				// Find the room with the most free berth slots. Crew quarters
				// are preferred, but any room will do.
				int targetIndex = -1;
				int bestFree = 0;
				for(int pass = 0; pass < 2 && bestFree <= 0; ++pass)
				{
					for(int ri = 0; ri < result.rooms.size(); ++ri)
					{
						const PlacedRoom& r = result.rooms[ri];
						const int columns = qMax(1, int((r.max.x - r.min.x - 6) / 4));
						const int rows = qMax(1, int((r.max.y - r.min.y - 6) / 4));
						const int free = columns * rows - berthCount.value(ri);
						if(free <= 0)
							continue;
						if(pass == 0 && !preferTypes.contains(r.type))
							continue;
						if(free > bestFree)
						{
							bestFree = free;
							targetIndex = ri;
						}
					}
				}
				if(targetIndex < 0)
					break;
				const PlacedRoom target = result.rooms[targetIndex];

				AuScPart* part = AuScPart::Create(partType);
				if(!part)
					break;
				AuScBerth* berth = dynamic_cast<AuScBerth*>(part);
				if(!berth)
				{
					delete part;
					break;
				}
				// Lay the berths out in a grid inside the room so that they do
				// not end up on top of each other.
				const double roomWidth = target.max.x - target.min.x;
				const double roomDepth = target.max.y - target.min.y;
				const int columns = qMax(1, int((roomWidth - 6) / 4));
				const int index = berthCount.value(targetIndex);
				double bx = target.min.x + 3 + (index % columns) * 4;
				double by = target.max.y - 3 - (index / columns) * 4;
				// Not on a berth placed by hand.
				bool onFixed = false;
				for(const LayoutOptions::FixedBerth& f : opts.fixedBerths)
					onFixed = onFixed || (fabs(f.floor.x - bx) < 3 && fabs(f.floor.y - by) < 3 && fabs(f.floor.z - target.min.z) < 3);
				if(onFixed)
				{
					berthCount[targetIndex] = index + 1;
					--i;
					delete berth;
					continue;
				}
				berth->MakeIcon(AuPoint(bx, by, target.min.z + 2), 0.f, IdentityPlane());
				if(berth->IsValid())
				{
					design.Parts().AddPart(*berth);
					++result.berthsPlaced;
					berthCount[targetIndex] = index + 1;
				}
				delete berth;
			}
		}
	}

	// Walk paths so that the crew can find their way around. They go
	// through the doors, never through a wall.
	if(opts.placeWalkPaths)
		PlaceWalkPaths(design, result.rooms, doorLinks);

	// Service panels for the allocated modules: side by side on the wall at
	// the low-y end of the first room, each at most 8 by 6 ft and never
	// wider or taller than the wall leaves room for (in a fighter's cockpit
	// all of them were 8 ft wide, on top of each other, and stuck out of
	// the hull).
	if(opts.placeServicePanels && opts.placeSystems && !result.rooms.isEmpty())
	{
		QList<AuScPart::PART_TYPE> panelTypes;
		for(const char* key : kPanelTypes)
		{
			const QString k(key);
			quint32 volume = 0;
			if(k == "powerPlant")		volume = brief.modules.powerPlant;
			else if(k == "maneuverDrive")	volume = brief.modules.maneuverDrive;
			else if(k == "capacitor")		volume = brief.modules.capacitor;
			else if(k == "lifeSupport")	volume = brief.modules.lifeSupport;
			else if(k == "sensors")		volume = brief.modules.sensors;
			else if(k == "shields")		volume = brief.modules.shields;
			const AuScPart::PART_TYPE partType = volume ? PanelTypeFor(k) : AuScPart::Part_Invalid;
			if(partType != AuScPart::Part_Invalid)
				panelTypes << partType;
		}
		const PlacedRoom& r = result.rooms.first();
		const double gap = .1;
		const int n = panelTypes.size();
		const double wallWidth = r.max.x - r.min.x, wallHeight = r.max.z - r.min.z;
		const double w = n ? qMin(8.0, (wallWidth - gap * (n + 1)) / n) : 0;
		const double h = qMin(6.0, wallHeight - 2 * gap);
		for(int i = 0; i < n && w > .2 && h > .2; ++i)
		{
			AuScPart* part = AuScPart::Create(panelTypes.at(i));
			AuScScreen* screen = dynamic_cast<AuScScreen*>(part);
			if(screen)
			{
				const double x0 = r.min.x + gap + i * (w + gap), z0 = r.min.z + gap;
				AuMatrix4x4 wall = WallPlane(x0, x0 + w, r.min.y, z0, z0 + h);
				screen->MakeScreen(AuPoint(x0, r.min.y, z0), AuPoint(x0 + w, r.min.y, z0 + h), wall);
				if(screen->IsValid())
					design.Parts().AddPart(*screen);
			}
			delete part;
		}
	}

	if(opts.placeSystems)
		AllocateVolumes(design, brief, ftlParts, weaponParts, placedStationTypes, result.warnings);
	else
		result.warnings << QString("the system volumes were left out (Build page), so the game will not take the ship as it is.");
	AddTransporterPads(design, result.rooms, opts.stationKeepOut, result.warnings, opts.fixedPads, opts.autoPads);

	// Which rooms did we not manage to place?
	for(const RoomSpec& r : brief.rooms)
	{
		int placed = 0;
		for(const PlacedRoom& p : result.rooms)
		{
			if(p.type == r.type)
				++placed;
		}
		if(placed < r.count)
		{
			result.roomsMissing += r.count - placed;
			result.missingRooms << QString("%1 (wanted %2, placed %3)").arg(r.label).arg(r.count).arg(placed);
		}
	}

}

LayoutResult GenerateLayout(AuScDesign& design, const Brief& brief, const HullAnalysis& hull, const LayoutOptions& opts)
{
	LayoutResult result;
	result.strategy = opts.strategy;

	// Without a closed hull there is no inside to test against. The rooms
	// are still placed against the silhouette, but the result cannot be
	// guaranteed, so say so rather than pretending otherwise.
	{
		bool anyClosed = false;
		for(const SolidMesh& solid : hull.solids)
			if(solid.IsClosed())
				anyClosed = true;
		if(!hull.solids.isEmpty() && !anyClosed)
			result.warnings << "the hull is not a closed solid, so the rooms could not be "
					"checked exactly. The shape has holes or folded faces; repair it to be sure.";
	}


	// The layout is worked out in a "layout space" where X is the long
	// horizontal axis of the hull and Z is up. At the end the boxes are
	// mapped back into design space.
	const bool swap = (hull.longAxis == 'y');
	const double mirrorSum = hull.usableMin.y + hull.usableMax.y;
	// Keep a margin so that the rooms do not touch the hull surface. The
	// silhouette the engine uses is a projection, so rooms that reach the
	// very edge of the hull would be rejected.
	const double margin = .9;
	const double usableLength = (swap ? (hull.usableMax.y - hull.usableMin.y) : (hull.usableMax.x - hull.usableMin.x)) * margin;
	const double usableDepth = (swap ? (hull.usableMax.x - hull.usableMin.x) : (hull.usableMax.y - hull.usableMin.y)) * margin;
	const double fullHeight = (hull.usableMax.z - hull.usableMin.z) * margin;

	// A tall hull can be several solid lobes with empty space between them.
	// A layout spread over the whole height puts rooms in different lobes,
	// where they cannot share a wall and so cannot have doors. Choose the
	// horizontal band with the most solid interior and keep the rooms in it.
	AuPoint layoutOrigin = hull.usableMin;
	double usableHeight = fullHeight;
	if(fullHeight > 60.0)
	{
		bool anyClosed = false;
		for(const SolidMesh& solid : hull.solids)
			if(solid.IsClosed())
				anyClosed = true;
		if(anyClosed)
		{
			const double bandHeight = qMin(fullHeight, 40.0);
			const double spanX = hull.usableMax.x - hull.usableMin.x;
			const double spanY = hull.usableMax.y - hull.usableMin.y;
			double bestScore = -1.;
			double bestZ = hull.usableMin.z;
			for(int step = 0; step <= 12; ++step)
			{
				const double z0 = hull.usableMin.z + (fullHeight - bandHeight) * step / 12.;
				int score = 0;
				for(int ix = 0; ix <= 6; ++ix)
					for(int iy = 0; iy <= 6; ++iy)
						for(int iz = 0; iz <= 2; ++iz)
						{
							const AuPoint p(hull.usableMin.x + spanX * ix / 6.,
									hull.usableMin.y + spanY * iy / 6.,
									z0 + bandHeight * iz / 2.);
							for(const SolidMesh& solid : hull.solids)
								if(solid.IsClosed() && solid.Contains(p))
								{
									++score;
									break;
								}
						}
				if(score > bestScore)
				{
					bestScore = score;
					bestZ = z0;
				}
			}
			if(bestScore > 0)
			{
				layoutOrigin.z = bestZ;
				usableHeight = bandHeight;
			}
		}
	}

	QList<RoomSpec> rooms = brief.ExpandedRooms();

	// The important rooms are placed first, so that they get the good spots
	// when the hull cannot hold everything.
	std::stable_sort(rooms.begin(), rooms.end(),
			[](const RoomSpec& a, const RoomSpec& b) { return a.priority < b.priority; });

	// Place the rooms. The room sizes are scaled down until as many rooms as
	// possible fit inside the hull.
	QList<Box> boxes;
	double usedScale = 1.0;
	double bestScale = 1.0;
	int bestPlaced = -1;
	QList<Box> bestBoxes;
	for(int attempt = 0; attempt < 20; ++attempt)
	{
		QList<RoomSpec> scaled = rooms;
		ScaleRooms(scaled, usedScale);
		QList<Box> tryBoxes;
		PlaceRooms(scaled, usableLength, usableDepth, usableHeight, opts, tryBoxes);

		// Count how many of them end up inside the hull.
		int placed = 0;
		for(const Box& b : tryBoxes)
		{
			Box d;
			d.mn = ToDesign(b.mn, swap, layoutOrigin, mirrorSum);
			d.mx = ToDesign(b.mx, swap, layoutOrigin, mirrorSum);
			if(d.mx.x < d.mn.x) qSwap(d.mn.x, d.mx.x);
			if(d.mx.y < d.mn.y) qSwap(d.mn.y, d.mx.y);
			if(d.mx.z < d.mn.z) qSwap(d.mn.z, d.mx.z);
			if(BoxInsideHull(design, hull, d.mn, d.mx))
				++placed;
		}
		if(placed > bestPlaced)
		{
			bestPlaced = placed;
			bestBoxes = tryBoxes;
			bestScale = usedScale;
		}
		if(placed >= rooms.size())
			break;	// Every room fits.
		usedScale *= .9;
		if(usedScale < .35)
			break;	// Rooms would become too small to be useful.
	}
	if(bestScale < 1.0)
	{
		result.warnings << QString("room sizes were scaled to %1 to fit inside the hull.")
				.arg(qRound(bestScale * 100) / 100.0);
	}
	// Centre the layout in the hull. The engine's silhouette is a projection,
	// so a room that sits in the middle of the hull is far more likely to be
	// accepted than one that touches the hull surface.
	boxes = bestBoxes;
	if(!boxes.isEmpty())
	{
		double minX = 0, maxX = 0, minY = 0, maxY = 0;
		for(int i = 0; i < boxes.size(); ++i)
		{
			if(i == 0)
			{
				minX = boxes[i].mn.x;
				maxX = boxes[i].mx.x;
				minY = boxes[i].mn.y;
				maxY = boxes[i].mx.y;
			}
			else
			{
				minX = qMin(minX, boxes[i].mn.x);
				maxX = qMax(maxX, boxes[i].mx.x);
				minY = qMin(minY, boxes[i].mn.y);
				maxY = qMax(maxY, boxes[i].mx.y);
			}
		}
		const double shiftX = (usableLength - (maxX - minX)) / 2. - minX;
		const double shiftY = (usableDepth - (maxY - minY)) / 2. - minY;
		for(Box& b : boxes)
		{
			b.mn.x += shiftX;
			b.mx.x += shiftX;
			b.mn.y += shiftY;
			b.mx.y += shiftY;
		}
	}

	// Map the boxes back into design space. Rooms that do not fit inside the
	// hull are shrunk and moved around a bit, and dropped if even that does
	// not help.
	auto fitBoxes = [&](double mirror, QStringList& notes) {
		QList<Box> good;
		for(const Box& b : boxes)
		{
			Box d;
			d.type = b.type;
			d.label = b.label;
			d.mn = ToDesign(b.mn, swap, layoutOrigin, mirror);
			d.mx = ToDesign(b.mx, swap, layoutOrigin, mirror);
			if(d.mx.x < d.mn.x)
				qSwap(d.mn.x, d.mx.x);
			if(d.mx.y < d.mn.y)
				qSwap(d.mn.y, d.mx.y);
			if(d.mx.z < d.mn.z)
				qSwap(d.mn.z, d.mx.z);

			// Start with the room as planned. On a boxy hull it fits and needs no
			// adjustment at all.
			Box best = d;
			bool fitted = BoxFits(design, hull, best, good);

			// Shrink it evenly about its centre. This is the least disruptive
			// correction: the room stays where the layout put it, it just gets
			// smaller. Never go below a usable size.
			if(!fitted)
			{
				for(int step = 1; step <= 12 && !fitted; ++step)
				{
					const double scale = 1.0 - step * .05;		// 0.95 down to 0.40
					Box t = ShrinkAboutCentre(d, scale, scale, scale);
					if(t.mx.x - t.mn.x <= 0 || t.mx.y - t.mn.y <= 0 || t.mx.z - t.mn.z <= 0)
						continue;
					if(BoxFits(design, hull, t, good))
					{
						best = t;
						fitted = true;
					}
				}
			}

			// Still no luck? A sloped or curved hull may need the room to give up
			// length, depth or height on its own, and it may need to move. Sweep
			// the three axes separately, then over the usable interior.
			if(!fitted)
			{
				const double axisScales[] = { .9, .8, .7, .6, .5, .4 };
				const int axisCount = int(sizeof(axisScales) / sizeof(axisScales[0]));
				for(int ai = 0; ai < axisCount && !fitted; ++ai)
				{
					const Box candidates[3] = {
						ShrinkAboutCentre(d, axisScales[ai], 1.0, 1.0),
						ShrinkAboutCentre(d, 1.0, axisScales[ai], 1.0),
						ShrinkAboutCentre(d, 1.0, 1.0, axisScales[ai])
					};
					for(int ci = 0; ci < 3 && !fitted; ++ci)
					{
						if(BoxFits(design, hull, candidates[ci], good))
						{
							best = candidates[ci];
							fitted = true;
						}
					}
				}
			}

			if(!fitted)
			{
				// Try moving the shrunk room through the band the layout was
				// planned in. Searching the whole hull would let a room jump to
				// another solid lobe, where it has no neighbours and no door.
				Box start = ShrinkAboutCentre(d, .5, .5, .5);
				if(start.mx.x - start.mn.x <= 0 || start.mx.y - start.mn.y <= 0 || start.mx.z - start.mn.z <= 0)
					start = d;
				const double roomX = start.mx.x - start.mn.x;
				const double roomY = start.mx.y - start.mn.y;
				const double roomZ = start.mx.z - start.mn.z;
				const double regionX = swap ? usableDepth : usableLength;
				const double regionY = swap ? usableLength : usableDepth;
				const double spanX = qMax(0., regionX - roomX);
				const double spanY = qMax(0., regionY - roomY);
				const double spanZ = qMax(0., usableHeight - roomZ);
				const int stepsX = 6, stepsY = 5, stepsZ = 4;
				double bestContact = -1.;
				double bestDistance = 1e30;
				Box bestMove;
				bool haveMove = false;
				for(int xi = 0; xi <= stepsX; ++xi)
				{
					for(int yi = 0; yi <= stepsY; ++yi)
					{
						for(int zi = 0; zi <= stepsZ; ++zi)
						{
							Box t = start;
							const double nx = layoutOrigin.x + spanX * xi / stepsX;
							const double ny = layoutOrigin.y + spanY * yi / stepsY;
							const double nz = layoutOrigin.z + spanZ * zi / stepsZ;
							t.mn = AuPoint(nx, ny, nz);
							t.mx = AuPoint(nx + roomX, ny + roomY, nz + roomZ);
							if(!BoxFits(design, hull, t, good))
								continue;

							// Prefer a spot that shares a wall with a room that
							// is already there. That is what a door needs.
							const double contact = SharedWallArea(t, good);
							const double dx = nx - start.mn.x;
							const double dy = ny - start.mn.y;
							const double dz = nz - start.mn.z;
							const double distance = dx*dx + dy*dy + dz*dz;
							if(!haveMove || contact > bestContact + 1e-6
									|| (qAbs(contact - bestContact) <= 1e-6 && distance < bestDistance))
							{
								bestMove = t;
								bestContact = contact;
								bestDistance = distance;
								haveMove = true;
							}
						}
					}
				}
				if(haveMove)
				{
					best = bestMove;
					fitted = true;
				}
			}

			if(fitted)
			{
				// A room that is merely near another room has no wall in
				// common, so no door can be put between them. Try shifting it
				// flush against each room already placed, and keep the move that
				// gives the most shared wall.
				Box snapped = best;
				double snappedContact = SharedWallArea(best, good);
				const double w = best.mx.x - best.mn.x;
				const double d = best.mx.y - best.mn.y;
				const double h = best.mx.z - best.mn.z;
				for(const Box& o : good)
				{
					Box candidates[4];
					// Right of the other room, and left of it.
					candidates[0] = best;
					candidates[0].mn = AuPoint(o.mx.x, (o.mn.y + o.mx.y)/2 - d/2, best.mn.z);
					candidates[0].mx = AuPoint(o.mx.x + w, candidates[0].mn.y + d, best.mx.z);
					candidates[1] = best;
					candidates[1].mn = AuPoint(o.mn.x - w, (o.mn.y + o.mx.y)/2 - d/2, best.mn.z);
					candidates[1].mx = AuPoint(o.mn.x, candidates[1].mn.y + d, best.mx.z);
					// In front of the other room, and behind it.
					candidates[2] = best;
					candidates[2].mn = AuPoint((o.mn.x + o.mx.x)/2 - w/2, o.mx.y, best.mn.z);
					candidates[2].mx = AuPoint(candidates[2].mn.x + w, o.mx.y + d, best.mx.z);
					candidates[3] = best;
					candidates[3].mn = AuPoint((o.mn.x + o.mx.x)/2 - w/2, o.mn.y - d, best.mn.z);
					candidates[3].mx = AuPoint(candidates[3].mn.x + w, o.mn.y, best.mx.z);

					for(int ci = 0; ci < 4; ++ci)
					{
						Box t = candidates[ci];
						// Keep the height inside the band. A door also needs
						// the two rooms to overlap vertically, so try lining
						// the floor up with the neighbour as well.
						double zs[4] = { best.mn.z, o.mn.z,
								o.mx.z - h, (o.mn.z + o.mx.z)/2 - h/2 };
						for(int zi = 0; zi < 4; ++zi)
						{
							if(h <= usableHeight)
							{
								t.mn.z = qBound(layoutOrigin.z, zs[zi],
										layoutOrigin.z + usableHeight - h);
								t.mx.z = t.mn.z + h;
							}
							if(t.mx.z > layoutOrigin.z + usableHeight + 1e-6)
								continue;
							if(!BoxFits(design, hull, t, good))
								continue;
							const double contact = SharedWallArea(t, good);
							if(contact > snappedContact + 1e-6)
							{
								snapped = t;
								snappedContact = contact;
							}
						}
					}
				}
				good << snapped;
			}
			else
				notes << QString("room '%1' does not fit inside the hull at a usable size and was skipped.")
						.arg(b.label);
		}
		return good;
	};

	// Rooms that share no wall with any other cannot get a door.
	auto alone = [](const QList<Box>& list) {
		int count = 0;
		for(int i = 0; i < list.size(); ++i)
		{
			QList<Box> others = list;
			others.removeAt(i);
			if(SharedWallArea(list[i], others) <= 0)
				++count;
		}
		return count;
	};
	QStringList fitNotes;
	QList<Box> good = fitBoxes(mirrorSum, fitNotes);
	if(swap)
	{
		// The bridge end goes to the bow, unless the hull, which need not be
		// the same at both ends, takes the rooms better the other way round.
		QStringList plainNotes;
		const QList<Box> plain = fitBoxes(qQNaN(), plainNotes);
		if(plain.size() > good.size() || (plain.size() == good.size() && alone(plain) < alone(good)))
		{
			good = plain;
			fitNotes = plainNotes;
			fitNotes << QString("the rooms fit the hull better with the bridge end of the layout aft.");
		}
	}
	result.warnings << fitNotes;
	boxes = good;

	// Create the room parts.
	QMap<QString, QList<PlacedRoom> > byType;
	for(const Box& b : boxes)
	{
		quint16 voidId = 0, meshId = 0;
		if(!MakeRoomParts(design, b.label, b.mn, b.mx, voidId, meshId))
		{
			result.warnings << QString("could not build room '%1'.").arg(b.label);
			continue;
		}
		SetRoomUse(design, voidId, b.type);
		PlacedRoom pr;
		pr.type = b.type;
		pr.label = b.label;
		pr.min = b.mn;
		pr.max = b.mx;
		pr.voidId = voidId;
		pr.meshId = meshId;
		pr.volumeM3 = (b.mx.x - b.mn.x) * (b.mx.y - b.mn.y) * (b.mx.z - b.mn.z) * 0.0283168466;
		byType[b.type] << pr;
		result.rooms << pr;
		++result.roomsPlaced;
	}

	{
		QList<QPair<quint16, QString> > voids;
		for(const PlacedRoom& pr : result.rooms)
			voids << qMakePair(pr.voidId, pr.type);
		if(const int given = GiveCargoAccess(design, voids))
			result.warnings << QString("No room is a hold, so the cargo is loaded and unloaded in the %1 room(s) "
					"that have no other use (halls, corridors).").arg(given);
	}

	// Final safety net. The boxes above are tested against the hull-only
	// silhouette, but the engine can rasterise the same hull slightly
	// differently. Check the actual room vertices against the hull-only
	// silhouette again, and against the engine only as a last resort. A room
	// that cannot be made to fit is removed rather than left sticking out of
	// the ship.
	{
		design.RefreshObstructionsCache();
		QList<PlacedRoom> kept;
		for(const PlacedRoom& pr : result.rooms)
		{
			const bool inside = BoxInsideHull(design, hull, pr.min, pr.max);
			if(inside)
			{
				kept << pr;
			}
			else
			{
				AuScParts deleted;
				design.Parts().DeletePart(pr.voidId, deleted);
				design.Parts().DeletePart(pr.meshId, deleted);
				result.warnings << QString("room '%1' was outside the hull and was removed.").arg(pr.label);
			}
		}
		result.rooms = kept;
		result.roomsPlaced = kept.size();
		// Rebuild the lookup table of rooms by type.
		byType.clear();
		for(const PlacedRoom& pr : kept)
			byType[pr.type] << pr;
	}

	// Doors. Rooms that share a wall get a door, and one hull door lets the
	// crew in from outside. Each door gets the room void of its open state,
	// which is what the engine needs in order to make it work.
	QList<DoorLink> doorLinks;
	result.doorsPlaced = PlaceDoors(design, hull, result.rooms, doorLinks, result.warnings);
	for(const DoorLink& link : doorLinks)
		if(link.ramp)
			++result.rampsPlaced;
	if(opts.placeWindows)
		result.windowsPlaced = PlaceWindows(design, result.rooms, result.warnings);

	FurnishLayout(design, brief, hull, opts, result, doorLinks);
	return result;
}

LayoutResult FurnishRooms(AuScDesign& design, const Brief& brief, const HullAnalysis& hull, const LayoutOptions& opts,
		const QList<PlacedRoom>& rooms)
{
	LayoutResult result;
	result.strategy = "drawn";
	result.description = "rooms drawn by hand";
	result.rooms = rooms;
	result.roomsPlaced = rooms.size();
	FurnishLayout(design, brief, hull, opts, result, QList<DoorLink>());
	return result;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Validation.

bool ValidateDesign(AuScDesign& design, QStringList& errors, QStringList& warnings)
{
	// The hull silhouette (obstruction cache) has to be built before the
	// engine can work out the hull volume and check the rooms.
	design.RefreshObstructionsCache();

	AuScReport report;
	design.ReportDesign(report);

	QStringList msgs;
	report.OkToManufacture(msgs, &design);

	// The messages come back as html fragments. Split them up and classify.
	for(const QString& m : msgs)
	{
		QString text = m;
		text.remove(QString("<span style=\"color: rgb(255, 32, 32);\">"));
		text.remove(QString("<span style=\"color: rgb(32, 255, 32);\">"));
		text.remove(QString("<span style=\"color: rgb(255, 128, 32);\">"));
		text.remove(QString("</span>"));
		text = text.trimmed();
		if(text.isEmpty())
			continue;
		if(m.contains("rgb(255, 32, 32)"))
			errors << text;
		else if(m.contains("rgb(255, 128, 32)"))
			warnings << text;
	}
	return errors.isEmpty();
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Hull import.
// The engine has a hard limit on how big a design may be. This thins the
// hull out until it fits into the budget, unless the caller asked for a
// specific tolerance.
// Thins a model out so that it has no more faces than a blueprint can
// carry, and so that it fits in the size limit.
//
// The game copes with up to about 15,000 faces. Anything over that is asking
// for trouble, so the model is thinned down until it is under the limit. The
// number of faces goes down as the grid the model is snapped to gets bigger,
// so the grid size is searched for.
//
// Returns the grid size that was used.
// Thins the hull out.
//
// The face budget is what the game can carry. A budget of zero means that
// the faces are left alone, which is what a hull that came from an existing
// blueprint wants: the game made it, so the game is happy with it.
//
// The holes are only closed for a model that came from a file. The hull of a
// blueprint that already exists is left as it is.
// Puts a triangle mesh back together as a closed solid: duplicates and
// triangles with no area go, the windings are made to agree, holes are
// closed, and corners that are a hair apart are snapped together. A mesh
// that is not closed has no inside, so a room cannot be tested against it.
AuScTriList SolidifyMesh(AuScTriList tris)
{
	for(int pass = 0; pass < 4; ++pass)
	{
		if(tris.isEmpty())
			break;
		tris = CleanMesh(tris);
		tris = FixWindings(tris);
		tris = FillHoles(tris);
		if(AuditSolidity(tris).Closed())
			break;
		tris = SnapMesh(tris, 1e-3);		// A thousandth of a foot.
	}
	return tris;
}

static double ThinHull(AuScParts& hullParts, const QList<AuScTriList>& originals, double startTolerance,
		quint32 maxFaces, quint64 byteBudget, bool closeHoles, bool useQuadric)
{
	const bool useFaceBudget = (maxFaces > 0);
	if(maxFaces == 0)
		maxFaces = 15000;

	// The memory the built parts take up.
	auto memory = [](const AuScParts& parts) -> quint64
	{
		quint64 used = 0;
		for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
			used += (*it)->MemoryUsage();
		return used;
	};

	// How many faces the model has now.
	quint32 total = 0;
	for(const AuScTriList& tris : originals)
		total += tris.size();

	AuScParts parts;
	quint32 faces = 0;

	if(!useFaceBudget || total <= maxFaces)
	{
		// The model is small enough, so it is left as it is.
		for(const AuScTriList& tris : originals)
		{
			AuScHull hull;
			AuScFaceList faceList;
			for(const AuScTri& t : tris)
				faceList.AddFace(AuScFaceTri(t.V0(), t.V1(), t.V2()));
			OrientAsHull(faceList);
			hull.AddFaces(faceList);
			if(hull.IsValid())
				parts.AddPart(hull);
		}
	}
	else
	{
		// The model is thinned out with the quadric error method, which
		// keeps the shape as well as it can be kept. Each part gets a share
		// of the faces, in proportion to how many it has now.
		//
		// Very big models are clustered onto a grid instead, because the
		// quadric method needs a lot of memory and a lot of time.
		const bool quadric = useQuadric && total <= 2000000;

		// Each part gets a share of the faces, in proportion to how many
		// it has now. The shares never add up to more than the budget.
		QVector<int> shares;
		int left = int(maxFaces);
		for(const AuScTriList& tris : originals)
		{
			const int want = int(double(maxFaces) * double(tris.size()) / double(total));
			const int share = qBound(0, want, left);
			shares << share;
			left -= share;
		}

		for(int i = 0; i < originals.size(); ++i)
		{
			const int share = shares.at(i);
			if(share <= 0)
				continue;

			AuScTriList tris;
			if(quadric)
				tris = SimplifyMesh(originals.at(i), share, SimplifyMethod::Quadric);
			else
				tris = ClusterMesh(originals.at(i), ClusterToleranceFor(originals.at(i), share));

			// Thinning a model out can open holes in it, so they are
			// closed again afterwards.
			AuScTriList closed = tris;
			if(closeHoles)
			{
				closed = SolidifyMesh(tris);

				// If closing them has taken the number of faces over the
				// budget, thin it out a little more and close them again.
				// The model is small by now, so this is quick.
				for(int guard = 0; guard < 6 && closed.size() > share; ++guard)
				{
					const int want = qMax(4, share - (int(closed.size()) - share));
					closed = SimplifyMesh(closed, want, SimplifyMethod::Quadric);
					closed = SolidifyMesh(closed);
				}
			}

			AuScFaceList faceList;
			for(const AuScTri& t : closed)
				faceList.AddFace(AuScFaceTri(t.V0(), t.V1(), t.V2()));
			faces += faceList.size();
			AuScHull hull;
			OrientAsHull(faceList);
			hull.AddFaces(faceList);
			if(hull.IsValid())
				parts.AddPart(hull);
		}
	}

	// If the blueprint is still too big, thin it out more.
	double tolerance = 0.;
	int step = 0;
	while(memory(parts) > byteBudget && step < 12)
	{
		++step;
		AuScParts thinned;
		faces = 0;
		for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
		{
			AuScFaceList faces2;
			(*it)->Faces(faces2);
			AuScTriList tris;
			faces2.Triangles(tris);
			if(tris.size() < 8)
				continue;
			const double grid = ClusterToleranceFor(tris, qMax(4, tris.size() / 2));
			const AuScTriList reduced = SolidifyMesh(ClusterMesh(tris, grid));
			AuScFaceList faceList;
			for(const AuScTri& t : reduced)
				faceList.AddFace(AuScFaceTri(t.V0(), t.V1(), t.V2()));
			faces += faceList.size();
			AuScHull hull;
			OrientAsHull(faceList);
			hull.AddFaces(faceList);
			if(hull.IsValid())
				thinned.AddPart(hull);
		}
		if(thinned.isEmpty())
			break;
		parts = thinned;
	}

	hullParts = parts;
	return tolerance;
}

bool ImportHullModel(AuDesign& design, const QStringList& fileNames, const ModelImportOptions& opts,
		AuPoint& modelMin, AuPoint& modelMax, QString& error)
{
	modelMin = AuPoint(0, 0, 0);
	modelMax = AuPoint(0, 0, 0);

	// A primitive base can be a blueprint on its own. Without one there has
	// to be at least one model.
	if(fileNames.isEmpty() && !opts.base.IsSet())
	{
		error = "there is no model and no base shape";
		return false;
	}

	// Read every file. A model that comes in parts is several files. The
	// triangles are kept so that the model can be thinned out afterwards
	// without having to read it all again.
	QList<AuScTriList> originals;
	for(const QString& fileName : fileNames)
	{
		// The same reader as the designer's (stl, 3ds, obj, and FBX, glTF
		// and more when built with Assimp).
		AuScTriList tris;
		if(!LoadMeshFile(fileName, tris, error))
			return false;
		originals << tris;
	}

	// How big a set of triangles is.
	auto extents = [](const AuScTriList& tris, AuPoint& min, AuPoint& max) -> bool
	{
		bool have = false;
		for(const AuScTri& t : tris)
		{
			for(int i = 0; i < 3; ++i)
			{
				const AuScPoint& v = t.Vertex(i);
				if(!have)
				{
					min = AuPoint(v.x, v.y, v.z);
					max = min;
					have = true;
					continue;
				}
				min.x = qMin(min.x, v.x);
				min.y = qMin(min.y, v.y);
				min.z = qMin(min.z, v.z);
				max.x = qMax(max.x, v.x);
				max.y = qMax(max.y, v.y);
				max.z = qMax(max.z, v.z);
			}
		}
		return have;
	};
	auto combinedExtents = [&extents](const QList<AuScTriList>& lists, AuPoint& min, AuPoint& max) -> bool
	{
		bool have = false;
		for(const AuScTriList& tris : lists)
		{
			AuPoint lo, hi;
			if(!extents(tris, lo, hi))
				continue;
			if(!have)
			{
				min = lo;
				max = hi;
				have = true;
				continue;
			}
			min.x = qMin(min.x, lo.x);
			min.y = qMin(min.y, lo.y);
			min.z = qMin(min.z, lo.z);
			max.x = qMax(max.x, hi.x);
			max.y = qMax(max.y, hi.y);
			max.z = qMax(max.z, hi.z);
		}
		return have;
	};
	auto moveMesh = [](AuScTriList& tris, const AuMatrix4x4& t) -> void
	{
		if(t.IsIdentity())
			return;
		for(AuScTri& tri : tris)
		{
			AuScPoint v[3] = { tri.V0(), tri.V1(), tri.V2() };
			for(int i = 0; i < 3; ++i)
			{
				const AuPoint p = t * AuPoint(v[i].x, v[i].y, v[i].z);
				v[i].x = p.x;
				v[i].y = p.y;
				v[i].z = p.z;
			}
			tri = AuScTri(v[0], v[1], v[2]);
		}
	};

	// When a base is chosen, every file is a separate model that is put on
	// the base. Without a base the files are parts of one model, as they
	// have always been, and the first one is the anchor that the rest keep
	// their places around.
	const bool separate = opts.base.IsSet() && opts.base.separateModels;

	if(!originals.isEmpty() && separate)
	{
		// Every model is scaled and put in the middle on its own, with its
		// bottom on the ground. The grid placement happens later.
		for(AuScTriList& tris : originals)
		{
			AuPoint min, max;
			if(!extents(tris, min, max))
				continue;
			double scale = opts.scale;
			if(opts.fit > 0)
			{
				const double biggest = qMax(max.x - min.x, qMax(max.y - min.y, max.z - min.z));
				if(biggest > 0)
					scale = opts.fit / biggest;
			}
			AuMatrix4x4 t;
			t.Translate(AuPoint(-(min.x + max.x) / 2 * scale, -(min.y + max.y) / 2 * scale,
					-min.z * scale));
			if(scale != 1.0)
				t.Scale(scale, scale, scale);
			moveMesh(tris, t);
		}
	}
	else if(!originals.isEmpty())
	{
		AuPoint min, max;
		if(!combinedExtents(originals, min, max))
		{
			error = "the model has no usable faces";
			return false;
		}

		// The first model is the anchor when there is no base. With a base
		// the whole group is centred on it.
		AuPoint firstMin, firstMax;
		extents(originals.first(), firstMin, firstMax);
		const double anchorX = opts.base.IsSet() ? (min.x + max.x) / 2
				: (firstMin.x + firstMax.x) / 2;
		const double anchorY = opts.base.IsSet() ? (min.y + max.y) / 2
				: (firstMin.y + firstMax.y) / 2;
		const double lift = opts.base.IsSet() ? opts.base.height : 0.;

		double scale = opts.scale;
		if(opts.fit > 0)
		{
			const double biggest = qMax(max.x - min.x, qMax(max.y - min.y, max.z - min.z));
			if(biggest > 0)
				scale = opts.fit / biggest;
		}
		AuMatrix4x4 t;
		if(opts.centre || opts.base.IsSet())
			t.Translate(AuPoint(-anchorX * scale, -anchorY * scale, -min.z * scale + lift));
		if(scale != 1.0)
			t.Scale(scale, scale, scale);
		for(AuScTriList& tris : originals)
			moveMesh(tris, t);
	}

	// A model that is not closed cannot be measured, and the game cannot tell
	// what is inside it, so the volume comes out wrong. The windings are put
	// right and the holes are closed before anything else happens.
	if(opts.repair)
	{
		int holes = 0;
		int left = 0;
		int windings = 0;
		for(int i = 0; i < originals.size(); ++i)
		{
			const MeshReport report = InspectMesh(originals.at(i));
			if(!report.windingsAgree)
			{
				originals[i] = FixWindings(originals.at(i));
				++windings;
			}
			if(report.loose > 0)
				holes += report.loose;

			// A mesh that is not closed has no inside, so the exact room
			// test cannot use it. Clean it up and close what can be closed.
			if(!AuditSolidity(originals.at(i)).Closed())
				originals[i] = SolidifyMesh(originals.at(i));

			if(report.loose > 0)
			{
				const MeshReport after = InspectMesh(originals.at(i));
				left += after.loose;
			}
		}
		if(opts.notes)
		{
			if(windings > 0)
				opts.notes->append(QString("the model had triangles that were the wrong way round, "
						"so the shape was not solid from the outside. They have been put right."));
			if(holes > 0)
			{
				opts.notes->append(QString("the model had %1 hole(s) in it, so the volume could not be "
						"measured properly. Most of them have been closed%2.")
						.arg(holes)
						.arg(left > 0 ? QString(", but %1 are still open where the model is pinched")
								.arg(left) : QString()));
			}
		}
	}

	// Change the shape of the model before it becomes the hull. Models that
	// are put on a base are changed on their own, so that each one keeps its
	// own proportions.
	if(opts.shape.AnyChange())
	{
		if(separate)
		{
			for(AuScTriList& tris : originals)
			{
				QList<AuScTriList> one;
				one << tris;
				const QList<AuScTriList> changed = ApplyModelShape(one, opts.shape);
				if(!changed.isEmpty())
					tris = changed.first();
			}
		}
		else
			originals = ApplyModelShape(originals, opts.shape);
	}

	// Put the models on the base in a grid, along the shape of the base. The
	// first model is the anchor when there is no base, so this only happens
	// when there is one.
	if(separate && !originals.isEmpty())
	{
		const int count = originals.size();
		double maxWidth = 1.;
		double maxDepth = 1.;
		for(const AuScTriList& tris : originals)
		{
			AuPoint min, max;
			if(!extents(tris, min, max))
				continue;
			maxWidth = qMax(maxWidth, max.x - min.x);
			maxDepth = qMax(maxDepth, max.y - min.y);
		}

		// As square a grid as the shape of the base allows.
		int columns = 1;
		if(count > 1)
		{
			const double aspect = opts.base.depth > 0
					? qMax(0.05, opts.base.width / opts.base.depth) : 1.;
			columns = qBound(1, int(ceil(sqrt(double(count) * aspect))), count);
		}
		const int rows = (count + columns - 1) / columns;

		// A little space between the models, so that they do not touch.
		const double cellWidth = qMax(maxWidth * 1.10, opts.base.width / double(columns));
		const double cellDepth = qMax(maxDepth * 1.10, opts.base.depth / double(rows));
		if(cellWidth * columns > opts.base.width * 1.001 ||
				cellDepth * rows > opts.base.depth * 1.001)
		{
			if(opts.notes)
				opts.notes->append(QString("the models are laid out in a grid of %1 by %2 and are wider "
						"than the base, so they hang over its edge. Make the base bigger or the models "
						"smaller.").arg(columns).arg(rows));
		}

		for(int i = 0; i < count; ++i)
		{
			const int row = i / columns;
			const int column = i % columns;
			const int inRow = qMin(columns, count - row * columns);
			const double x = (column - (inRow - 1) / 2.0) * cellWidth;
			const double y = (row - (rows - 1) / 2.0) * cellDepth;

			AuPoint min, max;
			if(!extents(originals.at(i), min, max))
				continue;
			const double dx = x - (min.x + max.x) / 2;
			const double dy = y - (min.y + max.y) / 2;
			const double dz = opts.base.height - min.z;

			AuMatrix4x4 t;
			t.Translate(AuPoint(dx, dy, dz));
			moveMesh(originals[i], t);
		}
	}

	// The base is a closed solid of its own. It is added after the models are
	// thinned out, so that its few faces are never mistaken for a model that
	// can be thinned away altogether.
	const AuScTriList baseTris = MakeBaseMesh(opts.base);

	// The game has a limit on how many faces a blueprint may have, and on how
	// big it may be. Thin the models out until they and the base fit.
	AuScParts hullParts;
	quint32 modelFaces = opts.maxFaces;
	if(!baseTris.isEmpty() && opts.maxFaces > 0)
	{
		modelFaces = opts.maxFaces > quint32(baseTris.size())
				? opts.maxFaces - quint32(baseTris.size()) : 4;
	}
	const quint64 limit = qMin(AuScReport::SizeLimit(), AuBdReport::SizeLimit());
	const quint64 budget = limit * 70 / 100;
	if(!originals.isEmpty())
		ThinHull(hullParts, originals, opts.decimate > 0 ? opts.decimate : 0.25, modelFaces,
				budget, true, true);

	if(!baseTris.isEmpty())
	{
		AuScFaceList faceList;
		for(const AuScTri& t : baseTris)
			faceList.AddFace(AuScFaceTri(t.V0(), t.V1(), t.V2()));
		AuScHull hull;
		OrientAsHull(faceList);
		hull.AddFaces(faceList);
		if(hull.IsValid())
			hullParts.AddPart(hull);
	}

	// What the caller gets back is the size of everything that was made,
	// base included.
	{
		QList<AuScTriList> all = originals;
		if(!baseTris.isEmpty())
			all << baseTris;
		AuPoint min, max;
		if(combinedExtents(all, min, max))
		{
			modelMin = min;
			modelMax = max;
		}
	}

	// A hull part that is still open cannot answer "is this room inside".
	// Say so once, at the end, rather than pretending the shape is solid.
	if(opts.notes)
	{
		int open = 0;
		quint32 openFaces = 0;
		for(AuScParts::const_iterator it = hullParts.constBegin(); it != hullParts.constEnd(); ++it)
		{
			AuScFaceList faces;
			(*it)->Faces(faces);
			AuScTriList triangles;
			faces.Triangles(triangles);
			if(triangles.isEmpty() || AuditSolidity(triangles).Closed())
				continue;
			++open;
			openFaces += triangles.size();
		}
		if(open > 0)
		{
			opts.notes->append(QString("the hull still has %1 part(s) (%2 faces) that are not closed. "
					"Rooms cannot be guaranteed to be inside such a shape; the silhouette was used "
					"instead. Repair the model first if the result matters.")
					.arg(open).arg(openFaces));
		}
	}

	for(AuScParts::const_iterator it = hullParts.constBegin(); it != hullParts.constEnd(); ++it)
		design.Parts().AddPart(*it.value());

	// Put the textures on last, so that they are on the hull as it ends up,
	// base included, and so that thinning the model out cannot lose them.
	if(opts.paint.IsSet())
	{
		QString paintError;
		if(!PaintDesign(design, opts.paint, opts.notes, paintError))
		{
			error = paintError;
			return false;
		}
	}

	return true;
}

bool CopyHull(AuScDesign& from, AuScDesign& to, QString& error, bool allowOpenHull)
{
	// The palette belongs to the design, not to a part, so it has to be
	// carried over as well or the copied hull would lose its textures.
	{
		AuDesignTex palette[SCTEXI_Count];
		from.TexturePalette(palette);
		to.SetTexturePalette(palette);
	}

	int copied = 0;
	AuScParts hullParts;
	const AuScParts& parts = from.Parts();
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p || !p->IsHullMesh())
			continue;
		AuScPart* clone = p->Clone();
		if(!clone)
			continue;
		hullParts.AddPart(*clone);
		delete clone;
		++copied;
	}
	if(!copied)
	{
		error = "the source design has no hull";
		return false;
	}
	// The game has a limit on how many faces a blueprint may have. Thin the
	// hull out until it fits.
	QList<AuScTriList> originals;
	QStringList openParts;
	for(AuScParts::const_iterator it = hullParts.constBegin(); it != hullParts.constEnd(); ++it)
	{
		AuScFaceList faces;
		(*it)->Faces(faces);
		AuScTriList tris;
		faces.Triangles(tris);

		// An existing hull is normally left exactly as it was. One that is
		// not closed, though, has no inside: a room cannot be tested
		// against it, and the game cannot tell what is part of the ship.
		const MeshSolidity solid = AuditSolidity(tris);
		if(!tris.isEmpty() && !solid.Closed())
		{
			openParts << QString("part %1 (%2 open, %3 thick, %4 folded edge(s))")
					.arg(it.key()).arg(solid.openEdges).arg(solid.thickEdges).arg(solid.foldedEdges);

			// Only repair when the caller has accepted that the result is
			// not certified; otherwise the run is refused below and the
			// repair would be wasted work.
			if(allowOpenHull)
				tris = SolidifyMesh(tris);
		}
		originals << tris;
	}
	if(!openParts.isEmpty() && !allowOpenHull)
	{
		error = QString("the hull is not a closed solid: %1. Rooms cannot be guaranteed "
				"to be inside it. Repair the mesh, or pass --allow-open-hull to place rooms "
				"against the silhouette anyway (unverified).")
				.arg(openParts.join("; "));
		return false;
	}
	const quint64 limit = qMin(AuScReport::SizeLimit(), AuBdReport::SizeLimit());
	// The hull of a blueprint that already exists is left as it is, apart
	// from making it smaller if the engine will not take it. It is put
	// back together the way it always was, so that the rooms still fit.
	ThinHull(hullParts, originals, 0.25, 15000, limit * 70 / 100, false, false);

	// A hull that is still open has no inside. Rooms can only be placed in
	// it against the silhouettes, which cannot see a concavity, so the
	// result would not be certifiable. Refuse it unless the caller has
	// explicitly accepted that risk.
	QStringList thinnedOpen;
	for(AuScParts::const_iterator it = hullParts.constBegin(); it != hullParts.constEnd(); ++it)
	{
		AuScFaceList faces;
		(*it)->Faces(faces);
		AuScTriList tris;
		faces.Triangles(tris);
		if(tris.isEmpty())
			continue;
		const MeshSolidity solid = AuditSolidity(tris);
		if(!solid.Closed())
			thinnedOpen << QString("part %1 (%2 open, %3 thick, %4 folded edge(s))")
					.arg(it.key()).arg(solid.openEdges).arg(solid.thickEdges).arg(solid.foldedEdges);
	}
	if(!thinnedOpen.isEmpty() && !allowOpenHull)
	{
		error = QString("the hull is not a closed solid after thinning: %1. Rooms cannot be "
				"guaranteed to be inside it. Repair the mesh, or pass --allow-open-hull to "
				"place rooms against the silhouette anyway (unverified).")
				.arg(thinnedOpen.join("; "));
		return false;
	}

	for(AuScParts::const_iterator it = hullParts.constBegin(); it != hullParts.constEnd(); ++it)
		to.Parts().AddPart(*it.value());
	return true;
}
