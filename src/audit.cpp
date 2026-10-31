//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - exact containment checks against the hull triangles.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "audit.h"

#include <QHash>
#include <QPair>
#include <QSet>
#include <algorithm>
#include <cmath>

#include "auscparts.h"

namespace {

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// A vertex rounded to 0.0001 feet, so that the same corner coming from two
// triangles is recognised as one point.

struct VertexKey
{
	qint64	x = 0;
	qint64	y = 0;
	qint64	z = 0;
};

inline bool operator==(const VertexKey& a, const VertexKey& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline uint qHash(const VertexKey& k, uint seed = 0)
{
	return ::qHash(k.x, seed) ^ (::qHash(k.y, seed + 1) << 1) ^ (::qHash(k.z, seed + 2) << 2);
}

VertexKey KeyOf(const AuPoint& p)
{
	VertexKey k;
	k.x = qint64(qRound64(p.x * 1000000.));
	k.y = qint64(qRound64(p.y * 1000000.));
	k.z = qint64(qRound64(p.z * 1000000.));
	return k;
}

double Axis(const AuPoint& p, int axis)
{
	return axis == 0 ? p.x : axis == 1 ? p.y : p.z;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Ray / triangle intersection. The triangle is hit when the ray passes
// through its inside; a hit exactly at the origin is reported separately so
// that the caller can avoid trusting a parity count there.

bool RayHitsTriangle(const AuPoint& origin, const AuPoint& direction,
		const AuPoint& a, const AuPoint& b, const AuPoint& c,
		bool& onSurface)
{
	const AuPoint edge1 = b - a;
	const AuPoint edge2 = c - a;
	const AuPoint pvec = direction.Cross(edge2);
	const double det = edge1.Dot(pvec);
	if(std::fabs(det) < 1e-12)
		return false;						// Parallel.

	const double inv = 1. / det;
	const AuPoint tvec = origin - a;
	const double u = tvec.Dot(pvec) * inv;
	if(u < 0. || u > 1.)
		return false;

	const AuPoint qvec = tvec.Cross(edge1);
	const double v = direction.Dot(qvec) * inv;
	if(v < 0. || u + v > 1.)
		return false;

	const double t = edge2.Dot(qvec) * inv;
	if(std::fabs(t) < 1e-9)
	{
		onSurface = true;					// The origin is on the triangle.
		return false;
	}
	return t > 0.;							// Behind the ray does not count.
}

// A handful of directions with no axis or half-axis in common, so that a ray
// is very unlikely to run exactly along an edge of the model.
const double kDirections[7][3] =
{
	{ 0.3420201433, 0.8660254038, 0.3665012260 },
	{ -0.6427876097, 0.4226182617, 0.6502878402 },
	{ 0.7071067812, 0.3, 0.6390096504 },
	{ -0.3, 0.8, 0.5196152423 },
	{ 0.5, -0.5, 0.7071067812 },
	{ 0.123456789, 0.987654321, 0.098765432 },
	{ -0.7654321, 0.1234567, 0.6324555 }
};

// Separating axis test between a triangle and an axis aligned box. This is
// the test from Akenine-Moller, "Fast 3D Triangle-Box Overlap Testing". When
// no separating axis exists the two overlap.
bool TriangleHitsBox(const AuPoint& a, const AuPoint& b, const AuPoint& c,
		const AuPoint& mn, const AuPoint& mx)
{
	const AuPoint centre((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, (mn.z + mx.z) / 2);
	const AuPoint half((mx.x - mn.x) / 2, (mx.y - mn.y) / 2, (mx.z - mn.z) / 2);
	if(half.x <= 0 || half.y <= 0 || half.z <= 0)
		return false;

	const AuPoint v[3] = { a - centre, b - centre, c - centre };

	// The three box axes.
	for(int axis = 0; axis < 3; ++axis)
	{
		const double lo = qMin(Axis(v[0], axis), qMin(Axis(v[1], axis), Axis(v[2], axis)));
		const double hi = qMax(Axis(v[0], axis), qMax(Axis(v[1], axis), Axis(v[2], axis)));
		if(lo > Axis(half, axis) || hi < -Axis(half, axis))
			return false;
	}

	// The triangle's own plane.
	const AuPoint e0 = v[1] - v[0];
	const AuPoint e1 = v[2] - v[0];
	const AuPoint normal = e0.Cross(e1);
	const double d = normal.Dot(v[0]);
	const double r = half.x * std::fabs(normal.x) + half.y * std::fabs(normal.y) +
			half.z * std::fabs(normal.z);
	if(std::fabs(d) > r)
		return false;

	// Nine cross products of a triangle edge and a box axis.
	const AuPoint edges[3] = { e0, v[2] - v[1], v[0] - v[2] };
	const AuPoint axes[3] = { AuPoint(1, 0, 0), AuPoint(0, 1, 0), AuPoint(0, 0, 1) };
	for(int e = 0; e < 3; ++e)
	{
		for(int axis = 0; axis < 3; ++axis)
		{
			const AuPoint sep = edges[e].Cross(axes[axis]);
			double lo = sep.Dot(v[0]);
			double hi = lo;
			for(int i = 1; i < 3; ++i)
			{
				const double p = sep.Dot(v[i]);
				lo = qMin(lo, p);
				hi = qMax(hi, p);
			}
			const double rad = half.x * std::fabs(sep.x) + half.y * std::fabs(sep.y) +
					half.z * std::fabs(sep.z);
			if(lo > rad || hi < -rad)
				return false;
		}
	}
	return true;
}

// Does a ray touch an axis aligned box?
bool RayHitsBox(const AuPoint& origin, const AuPoint& direction,
		const AuPoint& mn, const AuPoint& mx)
{
	double tmin = 0.;
	double tmax = 1e30;
	for(int axis = 0; axis < 3; ++axis)
	{
		const double o = Axis(origin, axis);
		const double d = Axis(direction, axis);
		const double lo = Axis(mn, axis);
		const double hi = Axis(mx, axis);
		if(std::fabs(d) < 1e-12)
		{
			if(o < lo || o > hi)
				return false;
			continue;
		}
		double t0 = (lo - o) / d;
		double t1 = (hi - o) / d;
		if(t0 > t1)
			qSwap(t0, t1);
		tmin = qMax(tmin, t0);
		tmax = qMin(tmax, t1);
		if(tmin > tmax)
			return false;
	}
	return true;
}

// Do two axis aligned boxes touch?
bool BoxesTouch(const AuPoint& amn, const AuPoint& amx, const AuPoint& bmn, const AuPoint& bmx)
{
	return amn.x <= bmx.x && amx.x >= bmn.x
			&& amn.y <= bmx.y && amx.y >= bmn.y
			&& amn.z <= bmx.z && amx.z >= bmn.z;
}

} // namespace

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

MeshSolidity AuditSolidity(const AuScTriList& triangles)
{
	MeshSolidity report;
	report.triangles = triangles.size();
	if(triangles.isEmpty())
		return report;

	QHash<VertexKey, int> ids;
	QHash<QPair<int, int>, int> undirected;
	QHash<QPair<int, int>, int> directed;

	for(const AuScTri& t : triangles)
	{
		int index[3];
		for(int i = 0; i < 3; ++i)
		{
			const AuScPoint& v = t.Vertex(i);
			const VertexKey key = KeyOf(AuPoint(v.x, v.y, v.z));
			QHash<VertexKey, int>::const_iterator it = ids.constFind(key);
			if(it == ids.constEnd())
			{
				const int id = ids.size();
				ids.insert(key, id);
				index[i] = id;
			}
			else
				index[i] = it.value();
		}
		for(int i = 0; i < 3; ++i)
		{
			const int a = index[i];
			const int b = index[(i + 1) % 3];
			if(a == b)
				continue;
			++directed[qMakePair(a, b)];
			++undirected[qMakePair(qMin(a, b), qMax(a, b))];
		}
	}
	report.vertices = ids.size();

	for(QHash<QPair<int, int>, int>::const_iterator it = undirected.constBegin();
			it != undirected.constEnd(); ++it)
	{
		if(it.value() == 1)
			++report.openEdges;
		else if(it.value() > 2)
		{
			++report.thickEdges;
			const int a = it.key().first;
			const int b = it.key().second;
			if(it.value() % 2 == 0 && directed.value(qMakePair(a, b)) == directed.value(qMakePair(b, a)))
				++report.pinchedEdges;
		}
		else
		{
			// Exactly two triangles share it. They have to go round in
			// opposite directions, otherwise the surface is folded.
			const int a = it.key().first;
			const int b = it.key().second;
			if(directed.value(qMakePair(a, b)) != 1 || directed.value(qMakePair(b, a)) != 1)
				++report.foldedEdges;
		}
	}
	report.nonManifoldEdges = report.thickEdges + report.foldedEdges;
	return report;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

SolidMesh::SolidMesh(const AuScTriList& triangles)
{
	m_triangles.reserve(triangles.size());
	for(const AuScTri& t : triangles)
	{
		const AuScPoint& v0 = t.Vertex(0);
		const AuScPoint& v1 = t.Vertex(1);
		const AuScPoint& v2 = t.Vertex(2);
		Triangle tri;
		tri.a = AuPoint(v0.x, v0.y, v0.z);
		tri.b = AuPoint(v1.x, v1.y, v1.z);
		tri.c = AuPoint(v2.x, v2.y, v2.z);
		tri.centroid = (tri.a + tri.b + tri.c) / 3.;
		m_triangles << tri;
	}
	m_solidity = AuditSolidity(triangles);
	if(!m_triangles.isEmpty())
		BuildNode(0, m_triangles.size());
}

int SolidMesh::BuildNode(int start, int count)
{
	AuPoint mn(1e30, 1e30, 1e30);
	AuPoint mx(-1e30, -1e30, -1e30);
	AuPoint cmin = mn, cmax = mx;
	for(int i = start; i < start + count; ++i)
	{
		const Triangle& t = m_triangles.at(i);
		for(int v = 0; v < 3; ++v)
		{
			const AuPoint& p = v == 0 ? t.a : v == 1 ? t.b : t.c;
			mn = mn.Min(p);
			mx = mx.Max(p);
		}
		cmin = cmin.Min(t.centroid);
		cmax = cmax.Max(t.centroid);
	}

	const int index = m_nodes.size();
	Node node;
	node.mn = mn;
	node.mx = mx;
	node.start = start;
	node.count = count;
	m_nodes << node;

	if(count <= 8)
		return index;

	// Split down the middle of the widest spread of the triangle centres.
	const AuPoint spread = cmax - cmin;
	int axis = 0;
	if(spread.y > spread.x) axis = 1;
	if(spread.z > (axis == 0 ? spread.x : spread.y)) axis = 2;

	const int mid = start + count / 2;
	std::nth_element(m_triangles.begin() + start, m_triangles.begin() + mid,
			m_triangles.begin() + start + count,
			[axis](const Triangle& a, const Triangle& b)
			{
				return Axis(a.centroid, axis) < Axis(b.centroid, axis);
			});

	const int left = BuildNode(start, mid - start);
	const int right = BuildNode(mid, start + count - mid);
	m_nodes[index].left = left;
	m_nodes[index].right = right;
	m_nodes[index].count = 0;
	return index;
}

int SolidMesh::CountCrossings(const AuPoint& origin, const AuPoint& direction, bool& surface) const
{
	if(m_nodes.isEmpty())
		return 0;

	int crossings = 0;
	int stack[64];
	int top = 0;
	stack[top++] = 0;
	while(top > 0)
	{
		const Node& node = m_nodes.at(stack[--top]);
		const double pad = 1e-6;
		const AuPoint mn(node.mn.x - pad, node.mn.y - pad, node.mn.z - pad);
		const AuPoint mx(node.mx.x + pad, node.mx.y + pad, node.mx.z + pad);
		if(!RayHitsBox(origin, direction, mn, mx))
			continue;

		if(node.left < 0 && node.right < 0)
		{
			for(int i = node.start; i < node.start + node.count; ++i)
			{
				const Triangle& t = m_triangles.at(i);
				if(RayHitsTriangle(origin, direction, t.a, t.b, t.c, surface))
					++crossings;
			}
			continue;
		}
		if(top + 2 > 64)
			continue;						// Cannot happen with a sane tree.
		stack[top++] = node.left;
		stack[top++] = node.right;
	}
	return crossings;
}

bool SolidMesh::Contains(const AuPoint& point, bool* onSurface) const
{
	if(onSurface)
		*onSurface = false;
	if(!HasInside())
		return false;
	const bool closed = IsClosed();

	int insideVotes = 0;
	int outsideVotes = 0;
	bool touchedSurface = false;

	for(int d = 0; d < 7; ++d)
	{
		AuPoint direction(kDirections[d][0], kDirections[d][1], kDirections[d][2]);
		const double length = direction.Length();
		if(length <= 0)
			continue;
		direction /= length;

		bool surface = false;
		const int crossings = CountCrossings(point, direction, surface);
		touchedSurface = touchedSurface || surface;

		if(crossings % 2)
			++insideVotes;
		else
			++outsideVotes;

		// Three directions that agree are enough; a direction that runs
		// along an edge is outvoted by the others. With openings in the
		// hull, a ray may leave through one, so all seven vote.
		if(closed && d >= 2 && (insideVotes == 3 || outsideVotes == 3))
			break;
	}
	if(!closed)
		return !touchedSurface && insideVotes >= 5;

	// A point exactly on the surface is neither in nor out.
	if(touchedSurface && qAbs(insideVotes - outsideVotes) <= 1)
	{
		if(onSurface)
			*onSurface = true;
		return false;
	}
	return insideVotes > outsideVotes;
}

void SolidMesh::CollectTriangles(int node, const AuPoint& mn, const AuPoint& mx, QVector<int>& out) const
{
	const Node& n = m_nodes.at(node);
	if(!BoxesTouch(n.mn, n.mx, mn, mx))
		return;
	if(n.left < 0 && n.right < 0)
	{
		for(int i = n.start; i < n.start + n.count; ++i)
			out << i;
		return;
	}
	CollectTriangles(n.left, mn, mx, out);
	CollectTriangles(n.right, mn, mx, out);
}

bool SolidMesh::BoxHitsSurface(const AuPoint& mn, const AuPoint& mx) const
{
	if(m_nodes.isEmpty())
		return false;
	QVector<int> candidates;
	CollectTriangles(0, mn, mx, candidates);
	for(int index : candidates)
	{
		const Triangle& t = m_triangles.at(index);
		if(TriangleHitsBox(t.a, t.b, t.c, mn, mx))
			return true;
	}
	return false;
}

bool SolidMesh::BoxInside(const AuPoint& mn, const AuPoint& mx) const
{
	if(!HasInside() || mn.x > mx.x || mn.y > mx.y || mn.z > mx.z)
		return false;
	if(!IsClosed())
	{
		// A door or window opening ends at the room's wall, so a point right
		// on the wall can be in the opening. Test the room a little inside
		// its walls.
		AuPoint inMn = mn, inMx = mx;
		const double m[3] = { qMin(.25, (mx.x - mn.x) * .05), qMin(.25, (mx.y - mn.y) * .05), qMin(.25, (mx.z - mn.z) * .05) };
		inMn += AuPoint(m[0], m[1], m[2]);
		inMx -= AuPoint(m[0], m[1], m[2]);
		return BoxInsideSampled(inMn, inMx);
	}
	return BoxInsideSampled(mn, mx);
}

bool SolidMesh::BoxInsideSampled(const AuPoint& mn, const AuPoint& mx) const
{
	// Every point of the sample grid has to be inside. A box is convex, but
	// the hull is not, so the interior is sampled as well as the boundary.
	// In a hull with door and window openings, a point by an opening may be
	// called outside because the rays leave through it; up to three of the
	// 27 points may fail there. A room that really is outside fails at most
	// of them.
	const int allowed = IsClosed() ? 0 : 3;
	int failed = 0;
	for(int ix = 0; ix <= 2; ++ix)
		for(int iy = 0; iy <= 2; ++iy)
			for(int iz = 0; iz <= 2; ++iz)
			{
				const AuPoint p(mn.x + (mx.x - mn.x) * ix / 2.,
						mn.y + (mx.y - mn.y) * iy / 2.,
						mn.z + (mx.z - mn.z) * iz / 2.);
				bool surface = false;
				if(!Contains(p, &surface) || surface)
					if(++failed > allowed)
						return false;
			}

	// No part of the surface may pass through the box. The box is shrunk a
	// little so that a wall the room is built against does not count.
	const double e = 1e-3;
	const AuPoint lo(mn.x + e, mn.y + e, mn.z + e);
	const AuPoint hi(mx.x - e, mx.y - e, mx.z - e);
	if(lo.x >= hi.x || lo.y >= hi.y || lo.z >= hi.z)
		return false;
	return !BoxHitsSurface(lo, hi);
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

namespace {

// A room that is a box: eight corners. Its bounds are the room.
bool BoxRoom(const AuScPart* part)
{
	QSet<quint32> ids;
	part->VertexIds(ids);
	return ids.size() <= 8;
}

// Any other room (one drawn on triangles, with slanted walls) is inside
// when the air in it is: points on a grid through its bounds that are inside
// the room must be inside the hull. Its bounding box may reach outside a
// tapering hull while the room does not, and its surface touches the cuts of
// its doors and windows, where inside and outside cannot be told.
bool MeshInside(const SolidMesh& hull, const AuScPart* part, const AuPoint& mn, const AuPoint& mx)
{
	AuScFaceList faces;
	part->Faces(faces);
	AuScTriList tris;
	faces.Triangles(tris);
	SolidMesh room(tris);
	room.SetOpenings(64);		// Its doors and windows are cut through it too.
	if(!room.HasInside())
		return hull.BoxInside(mn, mx);
	const AuPoint size = mx - mn;
	const double step = qMax(1.5, qMax(size.x, qMax(size.y, size.z)) / 24);
	// Through a hull with openings the rays that decide a point's side can
	// leave by a window, so a point by a window may come out wrong; a few
	// of those are allowed (without windows none of the Wedge bridge's
	// points failed; with 33 windows 1 to 2 percent did).
	int samples = 0, outside = 0;
	for(double x = mn.x + step / 2; x < mx.x; x += step)
		for(double y = mn.y + step / 2; y < mx.y; y += step)
			for(double z = mn.z + step / 2; z < mx.z; z += step)
			{
				const AuPoint p(x, y, z);
				bool on = false;
				if(!room.Contains(p, &on) || on)
					continue;
				++samples;
				if(!hull.Contains(p, &on) && !on)
				{
					if(hull.IsClosed())
						return false;
					++outside;
				}
			}
	return samples > 0 && outside <= qMax(2, samples * 3 / 100);
}

bool PartBounds(const AuScPart* part, AuPoint& mn, AuPoint& mx)
{
	if(!part)
		return false;
	QSet<quint32> ids;
	part->VertexIds(ids);
	if(ids.isEmpty())
		return false;
	mn = AuPoint(1e30, 1e30, 1e30);
	mx = AuPoint(-1e30, -1e30, -1e30);
	for(QSet<quint32>::const_iterator it = ids.constBegin(); it != ids.constEnd(); ++it)
	{
		const AuPoint p = part->VertexLoc(*it);
		mn = mn.Min(p);
		mx = mx.Max(p);
	}
	return true;
}

struct HullSolid
{
	quint16		id = 0;
	SolidMesh	mesh;
};

}

void AuditDesign(const AuScParts& parts, QList<HullSolidity>& hulls,
		QList<RoomContainment>& rooms, QStringList& notes)
{
	hulls.clear();
	rooms.clear();
	notes.clear();

	int openings = 0;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
		if(it.value() && (it.value()->IsDoor() || it.value()->IsWindow()))
			++openings;

	QList<HullSolid> solids;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* part = it.value();
		if(!part || part->IsOmitted() || !part->IsHullMesh())
			continue;
		AuScFaceList faces;
		part->Faces(faces);
		AuScTriList triangles;
		faces.Triangles(triangles);

		HullSolid solid;
		solid.id = it.key();
		solid.mesh = SolidMesh(triangles);
		solid.mesh.SetOpenings(openings);
		solids << solid;

		HullSolidity entry;
		entry.id = solid.id;
		entry.mesh = solid.mesh.Solidity();
		hulls << entry;
	}

	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* part = it.value();
		if(!part || part->IsOmitted())
			continue;
		if(!(part->IsRoomVoid() || part->IsRoomMesh()))
			continue;
		if(part->AssociatedPartId())
			continue;		// A doorway: it reaches through the wall, or out through the hull, on purpose.

		RoomContainment room;
		room.partId = it.key();
		room.type = part->TypeName();

		AuPoint mn, mx;
		if(!PartBounds(part, mn, mx))
		{
			room.note = "the room has no vertices";
			rooms << room;
			continue;
		}

		// A closed hull part is the only kind whose inside can be decided.
		// Try every closed part; a room in a model that was made in pieces
		// can be held by any one of them.
		bool sawOpenHull = false;
		for(const HullSolid& hull : solids)
		{
			if(!hull.mesh.HasInside())
			{
				sawOpenHull = true;
				continue;
			}
			room.checked = true;
			if(hull.mesh.BoxInside(mn, mx) || (!BoxRoom(part) && MeshInside(hull.mesh, part, mn, mx)))
			{
				room.inside = true;
				room.hullId = hull.id;
				room.throughOpenings = !hull.mesh.IsClosed();
				break;
			}
		}
		if(!room.checked)
			room.note = sawOpenHull ? "the hull is not a closed solid"
					: "there is no hull mesh";
		else if(!room.inside)
			room.note = "outside every closed hull part";
		rooms << room;
	}

	for(const HullSolid& hull : solids)
	{
		if(hull.mesh.HasInside())
			continue;
		const MeshSolidity& s = hull.mesh.Solidity();
		notes << QString("hull part %1 is not closed: %2 open edge(s), "
				"%3 non-manifold edge(s); this hull cannot be checked exactly.")
				.arg(hull.id).arg(s.openEdges).arg(s.nonManifoldEdges);
	}
}
