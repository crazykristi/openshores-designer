//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - turning a triangle plan into a blueprint.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "planbuild.h"

#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QQueue>
#include <QSet>
#include <cmath>
#include <algorithm>
#include <array>
#include <functional>

#include "audit.h"
#include "mesh.h"
#include "auscdesign.h"
#include "auscparts.h"
#include "generate.h"
#include "interior.h"
#include "shipgen.h"

namespace {

// A triangle mesh with shared vertices and a paint index per face.
struct IndexedMesh
{
	QVector<AuPoint>	v;
	QVector<int>		f;			// Three vertex indices per face.
	QVector<int>		paint;		// One per face.

	int		Faces() const { return f.size() / 3; }
	AuPoint	Normal(int face) const
	{
		const AuPoint& a = v[f[face*3]];
		const AuPoint& b = v[f[face*3 + 1]];
		const AuPoint& c = v[f[face*3 + 2]];
		AuPoint n = (b - a).Cross(c - a);
		const double len = n.Length();
		return len > 1e-12 ? n / len : AuPoint(0, 0, 0);
	}
};

// The vertices of the plan: grid corners and cell middles, on levels. A
// corner (ix, iy) is at half-cell (2ix, 2iy), a middle at (2cx+1, 2cy+1).
qint64 VertexKey(int hx, int hy, int level)
{
	return (qint64(hx + 2) * 4096 + qint64(hy + 2)) * 4096 + qint64(level);
}

void HalfCellCorners(const TriRef& t, int hx[3], int hy[3])
{
	static const int cxo[4] = { 0, 1, 1, 0 };
	static const int cyo[4] = { 0, 0, 1, 1 };
	hx[0] = 2 * (t.cx + cxo[t.k]);
	hy[0] = 2 * (t.cy + cyo[t.k]);
	hx[1] = 2 * (t.cx + cxo[(t.k + 1) % 4]);
	hy[1] = 2 * (t.cy + cyo[(t.k + 1) % 4]);
	hx[2] = 2 * t.cx + 1;
	hy[2] = 2 * t.cy + 1;
}

// Builds the closed surface of a set of triangle prisms. 'inside' says which
// triangles are filled; deck d covers levels d*rows to (d+1)*rows, whose
// heights 'zOf' gives. Filled prisms on decks next to each other are joined
// when 'joinDecks' is set. With 'inward' the faces wind into the solid.
IndexedMesh BuildPrisms(const TriPlan& plan, const std::function<bool(const TriRef&)>& inside,
		const std::function<double(int)>& zOf, int rows, bool joinDecks, bool inward,
		const std::function<int(const TriRef&)>& paintOf)
{
	IndexedMesh mesh;
	QHash<qint64, int> index;
	auto vertex = [&](int hx, int hy, int level) -> int {
		const qint64 key = VertexKey(hx, hy, level);
		QHash<qint64, int>::const_iterator it = index.constFind(key);
		if(it != index.constEnd())
			return it.value();
		const double s = plan.cellFeet;
		const AuPoint p((hx / 2.0 - plan.Width() / 2.0) * s, (hy / 2.0 - plan.Depth() / 2.0) * s, zOf(level));
		const int i = mesh.v.size();
		mesh.v.append(p);
		index.insert(key, i);
		return i;
	};
	auto face = [&](int a, int b, int c, int paint) {
		if(inward)
			std::swap(b, c);
		mesh.f << a << b << c;
		mesh.paint << paint;
	};

	for(int d = 0; d < plan.Decks(); ++d)
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const TriRef t(d, cx, cy, k);
					if(!inside(t))
						continue;
					const int paint = paintOf(t);
					int hx[3], hy[3];
					HalfCellCorners(t, hx, hy);
					const int top = (d + 1) * rows, bottom = d * rows;

					const TriRef above(d + 1, cx, cy, k), below(d - 1, cx, cy, k);
					if(!(joinDecks && plan.Valid(above) && inside(above)))
						face(vertex(hx[0], hy[0], top), vertex(hx[1], hy[1], top), vertex(hx[2], hy[2], top), paint);
					if(!(joinDecks && plan.Valid(below) && inside(below)))
						face(vertex(hx[0], hy[0], bottom), vertex(hx[2], hy[2], bottom), vertex(hx[1], hy[1], bottom), paint);

					// The walls, where the triangle across an edge is empty.
					for(int e = 0; e < 3; ++e)
					{
						const TriRef across = plan.Across(t, e);
						if(plan.Valid(across) && inside(across))
							continue;
						const int p = e, q = (e + 1) % 3;
						for(int r = 0; r < rows; ++r)
						{
							const int l0 = bottom + r, l1 = l0 + 1;
							const int p0 = vertex(hx[p], hy[p], l0), q0 = vertex(hx[q], hy[q], l0);
							const int p1 = vertex(hx[p], hy[p], l1), q1 = vertex(hx[q], hy[q], l1);
							face(p0, q0, q1, paint);
							face(p0, q1, p1, paint);
						}
					}
				}
	return mesh;
}

// Merges the faces of flat areas of one colour, by moving vertices that lie
// inside such an area onto a neighbour. The edges of the areas, and every
// vertex where the surface bends, stay where they are, so the shape and the
// paint edges are kept exactly.
//
// With an angle above zero, areas that are nearly flat are merged as well:
// the faces round a vertex may differ by up to that angle, and the surface
// may move by up to 'maxMove' feet. This is how the hull is brought within
// its face budget without the quadric method, which does not keep a surface
// closed and does not know about paint. Every merge keeps the surface
// closed and leaves the paint edges where they are.
void CollapseFlat(IndexedMesh& mesh, double angleDegrees = 0.0, double maxMove = 0.0)
{
	const double cosTol = angleDegrees > 0 ? cos(angleDegrees * 3.14159265358979 / 180.0) : 1.0 - 1e-9;
	const int nv = mesh.v.size();
	const int nf = mesh.Faces();
	QVector<bool> alive(nf, true);
	QVector<QVector<int> > vf(nv);
	for(int fi = 0; fi < nf; ++fi)
		for(int j = 0; j < 3; ++j)
			vf[mesh.f[fi*3 + j]] << fi;

	auto faceNormal = [&mesh](int a, int b, int c) {
		return (mesh.v[b] - mesh.v[a]).Cross(mesh.v[c] - mesh.v[a]);
	};

	for(int pass = 0; pass < 12; ++pass)
	{
		int changed = 0;
		for(int v = 0; v < nv; ++v)
		{
			QVector<int> faces;
			for(int fi : vf[v])
				if(alive[fi])
					faces << fi;
			vf[v] = faces;
			if(faces.size() < 3)
				continue;

			// All the faces round the vertex in one plane and one colour.
			const AuPoint n0 = mesh.Normal(faces.first());
			const int paint = mesh.paint[faces.first()];
			bool flat = n0.Length() > 0.5;
			for(int fi : faces)
			{
				if(mesh.paint[fi] != paint || mesh.Normal(fi).Dot(n0) < cosTol)
				{
					flat = false;
					break;
				}
			}
			if(!flat)
				continue;

			// Interior: every edge out of the vertex is shared by two faces.
			QMap<int, int> ringCount;		// Ordered, so that the result does not depend on the hash seed.
			for(int fi : faces)
				for(int j = 0; j < 3; ++j)
				{
					const int u = mesh.f[fi*3 + j];
					if(u != v)
						ringCount[u]++;
				}
			bool interior = true;
			for(QMap<int, int>::const_iterator it = ringCount.constBegin(); it != ringCount.constEnd(); ++it)
				if(it.value() != 2)
				{
					interior = false;
					break;
				}
			if(!interior)
				continue;

			for(QMap<int, int>::const_iterator it = ringCount.constBegin(); it != ringCount.constEnd(); ++it)
			{
				const int u = it.key();
				// The faces of u, to check that only the two faces on the
				// edge v-u share both of their neighbours.
				QSet<int> uRing;
				for(int fi : vf[u])
				{
					if(!alive[fi])
						continue;
					for(int j = 0; j < 3; ++j)
						uRing.insert(mesh.f[fi*3 + j]);
				}
				int common = 0;
				for(QMap<int, int>::const_iterator r = ringCount.constBegin(); r != ringCount.constEnd(); ++r)
					if(r.key() != u && uRing.contains(r.key()))
						++common;
				if(common != 2)
					continue;

				// Nothing may turn over or collapse.
				bool ok = true;
				for(int fi : faces)
				{
					int idx[3] = { mesh.f[fi*3], mesh.f[fi*3 + 1], mesh.f[fi*3 + 2] };
					if(idx[0] == u || idx[1] == u || idx[2] == u)
						continue;
					for(int j = 0; j < 3; ++j)
						if(idx[j] == v)
							idx[j] = u;
					// No slivers either: the engine drops a triangle whose
					// corners are (nearly) on one line, which opens the hull.
					const AuPoint n = faceNormal(idx[0], idx[1], idx[2]);
					double longest = 0;
					for(int j = 0; j < 3; ++j)
						longest = qMax(longest, (mesh.v[idx[j]] - mesh.v[idx[(j + 1) % 3]]).Length());
					if(n.Length() < 0.02 * longest * longest || n.Dot(n0) <= 0)
					{
						ok = false;
						break;
					}
					if(angleDegrees > 0)
					{
						// The new face must not turn far, and the vertex that
						// goes must be close to it.
						const AuPoint nu = n / n.Length();
						if(nu.Dot(mesh.Normal(fi)) < cosTol ||
								fabs((mesh.v[v] - mesh.v[idx[0]]).Dot(nu)) > maxMove)
						{
							ok = false;
							break;
						}
					}
				}
				if(!ok)
					continue;

				for(int fi : faces)
				{
					int* idx = &mesh.f[fi*3];
					if(idx[0] == u || idx[1] == u || idx[2] == u)
					{
						alive[fi] = false;
						continue;
					}
					for(int j = 0; j < 3; ++j)
						if(idx[j] == v)
							idx[j] = u;
					vf[u] << fi;
				}
				vf[v].clear();
				++changed;
				break;
			}
		}
		if(!changed)
			break;
	}

	// Compact.
	IndexedMesh out;
	QVector<int> remap(nv, -1);
	for(int fi = 0; fi < nf; ++fi)
	{
		if(!alive[fi])
			continue;
		for(int j = 0; j < 3; ++j)
		{
			const int old = mesh.f[fi*3 + j];
			if(remap[old] < 0)
			{
				remap[old] = out.v.size();
				out.v << mesh.v[old];
			}
			out.f << remap[old];
		}
		out.paint << mesh.paint[fi];
	}
	mesh = out;
}

// Splits every triangle into four at the middles of its edges. The surface
// stays closed, and the rounding then has vertices close enough together to
// follow a curve smaller than a cell.
void Subdivide(IndexedMesh& mesh)
{
	QHash<qint64, int> middles;
	auto middle = [&mesh, &middles](int a, int b) {
		const qint64 key = qint64(qMin(a, b)) * 0x100000000LL + qMax(a, b);
		QHash<qint64, int>::const_iterator it = middles.constFind(key);
		if(it != middles.constEnd())
			return it.value();
		const int i = mesh.v.size();
		mesh.v << (mesh.v[a] + mesh.v[b]) / 2.0;
		middles.insert(key, i);
		return i;
	};
	QVector<int> faces;
	QVector<int> paint;
	for(int fi = 0; fi < mesh.Faces(); ++fi)
	{
		const int a = mesh.f[fi*3], b = mesh.f[fi*3 + 1], c = mesh.f[fi*3 + 2];
		const int ab = middle(a, b), bc = middle(b, c), ca = middle(c, a);
		faces << a << ab << ca << ab << b << bc << ca << bc << c << ab << bc << ca;
		for(int j = 0; j < 4; ++j)
			paint << mesh.paint[fi];
	}
	mesh.f = faces;
	mesh.paint = paint;
}

// Rounds the hull: the surface is pushed out by the skin and then smoothed
// (Taubin's lambda/mu, which does not shrink the way plain averaging does).
// A vertex may not move further in than 'allowance' says, measured along its
// normal from where the skin put it.
void InflateAndSmooth(IndexedMesh& mesh, double skin, int iterations, const QVector<double>& allowance)
{
	const int nv = mesh.v.size();
	QVector<QVector<AuPoint> > normals(nv);
	for(int fi = 0; fi < mesh.Faces(); ++fi)
	{
		const AuPoint n = mesh.Normal(fi);
		if(n.Length() < 0.5)
			continue;
		for(int j = 0; j < 3; ++j)
		{
			QVector<AuPoint>& list = normals[mesh.f[fi*3 + j]];
			bool seen = false;
			for(const AuPoint& m : list)
				if(m.Dot(n) > 0.999)
				{
					seen = true;
					break;
				}
			if(!seen)
				list << n;
		}
	}

	QVector<AuPoint> normal(nv);
	QVector<AuPoint> inflated(nv);
	for(int i = 0; i < nv; ++i)
	{
		AuPoint n(0, 0, 0);
		for(const AuPoint& m : normals[i])
			n = n + m;
		if(n.Length() > 1e-9)
			n = n / n.Length();
		// Far enough along the averaged normal that every face the vertex
		// is on moves out by the skin, which a corner needs.
		double minDot = 1.0;
		for(const AuPoint& m : normals[i])
			minDot = qMin(minDot, n.Dot(m));
		normal[i] = n;
		inflated[i] = mesh.v[i] + n * (skin / qMax(0.4, minDot));
	}
	mesh.v = inflated;

	if(iterations <= 0)
		return;

	QVector<QVector<int> > neighbours(nv);
	{
		QVector<QSet<int> > sets(nv);
		for(int fi = 0; fi < mesh.Faces(); ++fi)
			for(int j = 0; j < 3; ++j)
			{
				const int a = mesh.f[fi*3 + j], b = mesh.f[fi*3 + (j + 1) % 3];
				sets[a].insert(b);
				sets[b].insert(a);
			}
		// Sorted, so that the sums come out the same every time.
		for(int i = 0; i < nv; ++i)
		{
			neighbours[i] = sets[i].values().toVector();
			std::sort(neighbours[i].begin(), neighbours[i].end());
		}
	}

	for(int it = 0; it < iterations * 2; ++it)
	{
		const double factor = (it % 2 == 0) ? 0.5 : -0.53;
		QVector<AuPoint> next = mesh.v;
		for(int i = 0; i < nv; ++i)
		{
			if(neighbours[i].isEmpty())
				continue;
			// The neighbours weighted by how close they are, and the move
			// taken along the normal only. The cells' fans give the vertices
			// different numbers of neighbours, and a plain average then
			// drags them sideways unevenly, which leaves a saw-tooth along
			// every rounded edge. Along the normal, a flat area stays flat.
			AuPoint avg(0, 0, 0);
			double weights = 0;
			for(int j : neighbours[i])
			{
				const double w = 1.0 / qMax(0.01, (mesh.v[j] - mesh.v[i]).Length());
				avg = avg + mesh.v[j] * w;
				weights += w;
			}
			avg = avg / weights;
			const AuPoint p = mesh.v[i] + normal[i] * ((avg - mesh.v[i]).Dot(normal[i]) * factor);
			AuPoint q = p;

			// Hold it between the allowance inwards and the skin outwards.
			const double s = (q - inflated[i]).Dot(normal[i]);
			if(s < -allowance[i])
				q = q + normal[i] * (-allowance[i] - s);
			else if(s > skin)
				q = q - normal[i] * (s - skin);
			next[i] = q;
		}
		mesh.v = next;
	}
}

AuScTriList ToTris(const IndexedMesh& mesh)
{
	AuScTriList tris;
	for(int fi = 0; fi < mesh.Faces(); ++fi)
	{
		const AuPoint n = mesh.Normal(fi);
		tris.append(AuScTri(AuScPoint(mesh.v[mesh.f[fi*3]], 0.f, 0.f, n),
				AuScPoint(mesh.v[mesh.f[fi*3 + 1]], 0.f, 0.f, n),
				AuScPoint(mesh.v[mesh.f[fi*3 + 2]], 0.f, 0.f, n)));
	}
	return tris;
}

// The z range of the room space of a deck.
void RoomHeights(const TriPlan& plan, int deck, double& z0, double& z1)
{
	z0 = plan.DeckFloor(deck) + plan.floorFeet / 2;
	z1 = plan.DeckFloor(deck) + plan.deckFeet - plan.floorFeet / 2;
}

void ApplyPaint(AuScFace& face, const TriPlan& plan, int paint)
{
	if(paint < 0 || paint >= plan.palette.size())
		paint = 0;
	const PlanPaint& p = plan.palette.at(paint);
	if(p.theme >= 0 && p.theme <= 2)
		face.SetPaint(AuScFace::COLOR_THEME(AuScFace::THEME_ExteriorPrimary + p.theme), 255);
	else
		face.SetPaint(p.colour);
}

// A room on one deck.
struct DeckRoom
{
	int			room = -1;
	int			deck = 0;
	QString		group;
	QString		type;
	quint16		voidId = 0;
	quint16		meshId = 0;
	TriRef		anchor;			// The triangle nearest the room's middle.
	AuPoint		boxMin;			// The largest box of whole cells in it, for the stations.
	AuPoint		boxMax;
	bool		hasBox = false;
};

// The largest rectangle of whole cells of a room on a deck.
bool LargestCellBox(const TriPlan& plan, int deck, int room, int& bx0, int& by0, int& bx1, int& by1)
{
	const int W = plan.Width(), D = plan.Depth();
	QVector<int> height(W, 0);
	int bestArea = 0;
	for(int cy = 0; cy < D; ++cy)
	{
		for(int cx = 0; cx < W; ++cx)
			height[cx] = plan.CellRoom(deck, cx, cy) == room ? height[cx] + 1 : 0;
		for(int cx = 0; cx < W; ++cx)
		{
			int h = height[cx];
			for(int ex = cx; ex < W && h > 0; ++ex)
			{
				h = qMin(h, height[ex]);
				const int area = h * (ex - cx + 1);
				if(area > bestArea)
				{
					bestArea = area;
					bx0 = cx;
					bx1 = ex;
					by1 = cy;
					by0 = cy - h + 1;
				}
			}
		}
	}
	return bestArea > 0;
}

// The triangles of a room, walked from 'from' to 'to' over shared edges.
// Whether a walk across a room has to keep off a triangle because of
// stairs: on the deck above, the opening over the run; on the deck below,
// the run itself, which climbs over head height, except the triangle at its
// foot, where the walk up it starts.
static bool UnderOrOverStairs(const TriPlan& plan, const TriRef& t)
{
	const int above = plan.StairAt(t.deck - 1, t.cx, t.cy);
	if(above >= 0)
		return true;
	const int below = plan.StairAt(t.deck, t.cx, t.cy);
	if(below < 0)
		return false;
	const PlanStair& stair = plan.stairs.at(below);
	return !(t.cx == stair.cx && t.cy == stair.cy && t.k == (stair.side + 2) % 4);
}

QList<TriRef> RoomWalk(const TriPlan& plan, int room, const TriRef& from, const TriRef& to)
{
	QHash<qint64, qint64> cameFrom;
	auto key = [&plan](const TriRef& t) { return (qint64(t.deck) * plan.Depth() + t.cy) * plan.Width() * 4 + t.cx * 4 + t.k; };
	QQueue<TriRef> queue;
	queue.enqueue(from);
	cameFrom.insert(key(from), -1);
	QHash<qint64, TriRef> byKey;
	byKey.insert(key(from), from);
	while(!queue.isEmpty())
	{
		const TriRef t = queue.dequeue();
		if(t == to)
			break;
		for(int e = 0; e < 3; ++e)
		{
			const TriRef n = plan.Across(t, e);
			if(plan.Room(n) != room)
				continue;
			if(!(n == to) && UnderOrOverStairs(plan, n))
				continue;
			const qint64 k = key(n);
			if(cameFrom.contains(k))
				continue;
			cameFrom.insert(k, key(t));
			byKey.insert(k, n);
			queue.enqueue(n);
		}
	}
	QList<TriRef> path;
	qint64 k = key(to);
	if(!cameFrom.contains(k))
		return path;
	while(k != -1)
	{
		path.prepend(byKey.value(k));
		k = cameFrom.value(k);
	}
	return path;
}

// Points along a walk, with the ones on a straight line left out.
QList<AuPoint> WalkPoints(const TriPlan& plan, const QList<TriRef>& walk, double z)
{
	QList<AuPoint> pts;
	for(const TriRef& t : walk)
	{
		const QPointF c = plan.TriCentre(t);
		pts << AuPoint(c.x(), c.y(), z);
	}
	QList<AuPoint> out;
	for(int i = 0; i < pts.size(); ++i)
	{
		if(i > 0 && i + 1 < pts.size())
		{
			AuPoint a = pts[i] - out.last(), b = pts[i + 1] - pts[i];
			if(a.Length() > 1e-9 && b.Length() > 1e-9 && fabs(a.Dot(b) / (a.Length() * b.Length()) - 1.0) < 1e-6)
				continue;
		}
		out << pts[i];
	}
	return out;
}

// Walk paths that are already there, so that two doors of one room do not
// lay the same path from the room's middle twice.
typedef QSet<QString> PathSet;

void AddWalkOnce(AuScDesign& design, const AuPoint& a, const AuPoint& b, quint16 doorId, PathSet& seen)
{
	auto key = [](const AuPoint& p) { return QString("%1,%2,%3").arg(p.x, 0, 'f', 2).arg(p.y, 0, 'f', 2).arg(p.z, 0, 'f', 2); };
	const QString ka = key(a), kb = key(b);
	const QString k = ka < kb ? ka + "|" + kb : kb + "|" + ka;
	if(seen.contains(k))
		return;
	seen.insert(k);
	AddWalkPath(design, a, b, doorId);
}

void AddWalk(AuScDesign& design, const QList<AuPoint>& pts, PathSet& seen)
{
	for(int i = 1; i < pts.size(); ++i)
		AddWalkOnce(design, pts[i - 1], pts[i], 0, seen);
}

bool AddPlanHullPart(AuScDesign& design, const TriPlan& plan, const PlanHull& hull)
{
	AuScFaceList faceList;
	for(int i = 0; i < hull.tris.size(); ++i)
	{
		const AuScTri& t = hull.tris.at(i);
		AuScFaceTri face(t.V0(), t.V1(), t.V2());
		ApplyPaint(face, plan, hull.paint.value(i));
		faceList.AddFace(face);
	}
	OrientAsHull(faceList);
	AuScHull part;
	part.AddFaces(faceList);
	return part.IsValid() && design.Parts().AddPart(part) != 0;
}

// The plan is drawn with fore along +x and port along +y. The game takes
// fore as +Y (auscparts.h, "Fore = +Y") and measures the length along Y
// (auscreport.cpp), so the finished blueprint is turned a quarter turn about
// Z: fore +x becomes +Y, port +y becomes -X.
void TurnToGameAxes(AuScDesign& design)
{
	design.Parts().Transform(QuarterTurnToFore());
}

// A box turned the same way.
void TurnBox(AuPoint& mn, AuPoint& mx)
{
	const AuMatrix4x4 t = QuarterTurnToFore();
	const AuPoint a = t.Project(mn), b = t.Project(mx);
	mn = a.Min(b);
	mx = a.Max(b);
}

AuScDesign* NewPlanDesign(const TriPlan& plan)
{
	AuScDesign* design = new AuScDesign;
	design->SetId(1);
	design->SetArchitectId(AuId(0));
	design->SetArchitectName("shipgen");
	design->SetName(plan.name);
	design->SetStructure(SC_Spaceship);
	design->SetStreamline(SLINE_None);
	return design;
}

}	// namespace

AuScDesign* BuildPlanHullDesign(const TriPlan& plan, const PlanHull& hull)
{
	AuScDesign* design = NewPlanDesign(plan);
	if(!hull.tris.isEmpty())
		AddPlanHullPart(*design, plan, hull);
	TurnToGameAxes(*design);
	return design;
}

PlanHull BuildPlanHull(const TriPlan& plan)
{
	PlanHull out;
	const int rows = 2;
	auto zOf = [&plan, rows](int level) { return level * plan.deckFeet / rows; };
	IndexedMesh mesh = BuildPrisms(plan, [&plan](const TriRef& t) { return plan.Hull(t); }, zOf, rows, true, false,
			[&plan](const TriRef& t) { return plan.Paint(t); });
	if(mesh.Faces() == 0)
	{
		out.notes << "There is no hull yet: draw some triangles first.";
		return out;
	}
	if(plan.smoothing > 0)
		Subdivide(mesh);

	// How far each vertex may be rounded in: the skin, and more where there
	// is no room close by.
	const double base = qMax(0.0, plan.skinFeet - 0.25);
	const double maxExtra = plan.cellFeet * 0.75;
	struct RoomPoint { double x, y, z0, z1; };
	QVector<RoomPoint> roomPoints;
	for(int d = 0; d < plan.Decks(); ++d)
	{
		double z0, z1;
		RoomHeights(plan, d, z0, z1);
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const TriRef t(d, cx, cy, k);
					if(plan.Room(t) < 0)
						continue;
					const QPointF c = plan.TriCentre(t);
					roomPoints << RoomPoint{ c.x(), c.y(), z0, z1 };
				}
	}
	QVector<double> allowance(mesh.v.size(), base + maxExtra);
	for(int i = 0; i < mesh.v.size(); ++i)
	{
		const AuPoint& p = mesh.v[i];
		double best = 1e30;
		for(const RoomPoint& r : roomPoints)
		{
			const double dx = r.x - p.x, dy = r.y - p.y;
			const double dz = p.z < r.z0 ? r.z0 - p.z : (p.z > r.z1 ? p.z - r.z1 : 0.0);
			const double horizontal = qMax(0.0, sqrt(dx*dx + dy*dy) - plan.cellFeet * 0.5);
			best = qMin(best, sqrt(horizontal * horizontal + dz * dz));
		}
		if(best < 1e29)
			allowance[i] = base + qBound(0.0, best - 0.5, maxExtra);
	}

	InflateAndSmooth(mesh, plan.skinFeet, plan.smoothing, allowance);
	out.rawTriangles = mesh.Faces();
	CollapseFlat(mesh);

	// Over the budget: merge areas that are nearly flat, a little more each
	// time, until it fits.
	const int merged = mesh.Faces();
	static const double kSteps[][2] = { { 2, 0.05 }, { 4, 0.1 }, { 7, 0.2 }, { 11, 0.3 }, { 16, 0.45 }, { 22, 0.6 }, { 30, 0.8 } };
	double usedAngle = 0;
	for(const auto& step : kSteps)
	{
		if(mesh.Faces() <= plan.maxFaces)
			break;
		CollapseFlat(mesh, step[0], step[1]);
		usedAngle = step[0];
	}
	if(usedAngle > 0)
	{
		out.thinned = true;
		out.notes << QString("The hull had %1 faces after the flat areas were merged, more than the budget of %2. "
							 "Nearly flat areas were merged too (up to %3 degrees), which leaves %4.")
						 .arg(merged).arg(plan.maxFaces).arg(usedAngle).arg(mesh.Faces());
	}
	if(mesh.Faces() > plan.maxFaces)
		out.notes << QString("The hull still has %1 faces, more than the budget of %2. Less smoothing, a bigger cell "
							 "or a smaller ship would bring it down.").arg(mesh.Faces()).arg(plan.maxFaces);
	out.tris = ToTris(mesh);
	out.paint = mesh.paint;
	const MeshSolidity solidity = AuditSolidity(out.tris);
	out.closed = solidity.Closed();
	if(!out.closed)
		out.notes << QString("The hull surface is not closed: %1 open and %2 non-manifold edges (where two "
							 "triangles meet only at a corner). Rooms near such a place cannot be checked exactly.")
						 .arg(solidity.openEdges).arg(solidity.nonManifoldEdges);

	return out;
}

QVector<QVector<bool> > RoomableTriangles(const TriPlan& plan, const PlanHull& hull)
{
	QVector<QVector<bool> > out(plan.Decks());
	const SolidMesh solid(hull.tris);
	for(int d = 0; d < plan.Decks(); ++d)
	{
		out[d].fill(false, plan.Width() * plan.Depth() * 4);
		double z0, z1;
		RoomHeights(plan, d, z0, z1);
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const TriRef t(d, cx, cy, k);
					if(!plan.Hull(t))
						continue;
					bool ok = true;
					if(solid.IsValid())
					{
						QPointF c[3];
						plan.TriCorners(t, c);
						const QPointF m = (c[0] + c[1] + c[2]) / 3.0;
						for(int j = 0; j < 4 && ok; ++j)
						{
							const QPointF q = j < 3 ? c[j] + (m - c[j]) * 0.02 : m;
							for(double z : { z0 + 0.05, z1 - 0.05 })
								if(!solid.Contains(AuPoint(q.x(), q.y(), z)))
								{
									ok = false;
									break;
								}
						}
					}
					out[d][(cy * plan.Width() + cx) * 4 + k] = ok;
				}
	}
	return out;
}

AuScDesign* BuildPlanDesign(const TriPlan& plan, const PlanBuildOptions& options, PlanBuildReport& report)
{
	report = PlanBuildReport();
	AuScDesign* design = NewPlanDesign(plan);

	// The hull.
	const PlanHull hull = BuildPlanHull(plan);
	report.notes << hull.notes;
	if(hull.tris.isEmpty())
	{
		report.errors << "The plan has no hull.";
		return design;
	}
	if(!AddPlanHullPart(*design, plan, hull))
	{
		report.errors << "The engine did not accept the hull.";
		return design;
	}
	report.hullFaces = report.shapedFaces = hull.tris.size();
	// Where the ship stands when it lands on its belly.
	double hullBottom = 1e30;
	for(const AuScTri& t : hull.tris)
		hullBottom = qMin(hullBottom, qMin(t.V0().z, qMin(t.V1().z, t.V2().z)));

	LayoutOptions layout;
	layout.centerHull = false;				// The rooms are drawn where they are.
	layout.foreAlongLongAxis = false;		// Turned on purpose, below.
	layout.placeWalkPaths = false;
	HullAnalysis analysis;
	QString error;
	if(!AnalyseHull(*design, layout, analysis, error))
		report.warnings << QString("The hull could not be measured: %1").arg(error);
	report.hullVolumeM3 = analysis.volumeM3;

	// The rooms, one per deck they are drawn on.
	const QVector<QVector<bool> > roomable = RoomableTriangles(plan, hull);
	QList<DeckRoom> deckRooms;
	QHash<qint64, int> deckRoomIndex;		// (room, deck) -> index in deckRooms
	for(int r = 0; r < plan.rooms.size(); ++r)
	{
		const PlanRoom& room = plan.rooms.at(r);
		int firstDeck = -1;
		for(int d = 0; d < plan.Decks(); ++d)
		{
			double z0, z1;
			RoomHeights(plan, d, z0, z1);
			int count = 0, outside = 0;
			double sx = 0, sy = 0;
			for(int cy = 0; cy < plan.Depth(); ++cy)
				for(int cx = 0; cx < plan.Width(); ++cx)
					for(int k = 0; k < 4; ++k)
					{
						const TriRef t(d, cx, cy, k);
						if(plan.Room(t) != r)
							continue;
						++count;
						if(!roomable[d][(cy * plan.Width() + cx) * 4 + k])
							++outside;
						const QPointF c = plan.TriCentre(t);
						sx += c.x();
						sy += c.y();
					}
			if(!count)
				continue;
			if(firstDeck < 0)
				firstDeck = d;
			if(outside)
				report.warnings << QString("%1 triangles of '%2' on deck %3 are not inside the shaped hull. "
										   "Draw the hull wider there, or the room smaller.")
										.arg(outside).arg(room.label).arg(d + 1);

			DeckRoom dr;
			dr.room = r;
			dr.deck = d;
			dr.type = room.type;
			dr.group = (d == firstDeck) ? room.label : QString("%1 deck %2").arg(room.label).arg(d + 1);

			IndexedMesh mesh = BuildPrisms(plan,
					[&plan, r, d](const TriRef& t) { return t.deck == d && plan.Room(t) == r; },
					[&plan, d, z0, z1](int level) { return level == d ? z0 : z1; }, 1, false, true,
					[](const TriRef&) { return 0; });
			CollapseFlat(mesh);
			AuScFaceList faces;
			for(int fi = 0; fi < mesh.Faces(); ++fi)
				faces.AddFace(AuScFaceTri(AuScPoint(mesh.v[mesh.f[fi*3]], 0.f, 0.f, AuPoint(0, 0, 1)),
						AuScPoint(mesh.v[mesh.f[fi*3 + 1]], 0.f, 0.f, AuPoint(0, 0, 1)),
						AuScPoint(mesh.v[mesh.f[fi*3 + 2]], 0.f, 0.f, AuPoint(0, 0, 1))));
			// The walls in a light shade of the room's colour in the plan.
			const QColor c = room.colour;
			const QColor tint(c.red() + (255 - c.red()) * 6 / 10, c.green() + (255 - c.green()) * 6 / 10,
					c.blue() + (255 - c.blue()) * 6 / 10);
			if(!AddMeshRoom(*design, dr.group, faces, dr.voidId, dr.meshId, false, tint))
			{
				report.warnings << QString("The engine did not accept room '%1'.").arg(dr.group);
				continue;
			}

			// The anchor: the room triangle nearest its middle.
			const QPointF mid(sx / count, sy / count);
			double best = 1e30;
			for(int cy = 0; cy < plan.Depth(); ++cy)
				for(int cx = 0; cx < plan.Width(); ++cx)
					for(int k = 0; k < 4; ++k)
					{
						const TriRef t(d, cx, cy, k);
						if(plan.Room(t) != r || UnderOrOverStairs(plan, t))
							continue;
						const QPointF c = plan.TriCentre(t) - mid;
						const double dist = c.x() * c.x() + c.y() * c.y();
						if(dist < best)
						{
							best = dist;
							dr.anchor = t;
						}
					}

			int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
			if(LargestCellBox(plan, d, r, bx0, by0, bx1, by1))
			{
				const QPointF lo = plan.Corner(bx0, by0), hi = plan.Corner(bx1 + 1, by1 + 1);
				dr.boxMin = AuPoint(lo.x(), lo.y(), z0);
				dr.boxMax = AuPoint(hi.x(), hi.y(), z1);
				dr.hasBox = true;
			}
			deckRoomIndex.insert(qint64(r) * 1000 + d, deckRooms.size());
			deckRooms << dr;
		}
	}
	report.rooms = deckRooms.size();

	auto deckRoomOf = [&](const TriRef& t) -> const DeckRoom* {
		const int r = plan.Room(t);
		if(r < 0)
			return 0;
		const int i = deckRoomIndex.value(qint64(r) * 1000 + t.deck, -1);
		return i >= 0 ? &deckRooms.at(i) : 0;
	};

	// The doors. Doors on neighbouring edges of one wall, between the same
	// two rooms (or a room and the outside), are one wide door: a bay door,
	// as tall as the room, for vehicles.
	PathSet paths;
	const double s = plan.cellFeet;
	struct DoorRun { PlanDoor door; int count = 1; QPointF mid; };
	QList<DoorRun> runs;
	{
		auto midOf = [&plan](const PlanDoor& d) {
			const TriRef a(d.deck, d.cx, d.cy, d.side);
			const TriRef b = plan.AcrossCellEdge(d.deck, d.cx, d.cy, d.side);
			QPointF c[3];
			plan.TriCorners(plan.Valid(a) ? a : b, c);
			return (c[0] + c[1]) / 2.0;
		};
		auto rooms = [&](const PlanDoor& d) {
			const TriRef a(d.deck, d.cx, d.cy, d.side);
			return qMakePair(deckRoomOf(a), deckRoomOf(plan.AcrossCellEdge(d.deck, d.cx, d.cy, d.side)));
		};
		QList<PlanDoor> left = plan.doors;
		while(!left.isEmpty())
		{
			QList<PlanDoor> run;
			run << left.takeFirst();
			const bool xWall = (run[0].side == TRI_E || run[0].side == TRI_W);
			bool grew = true;
			while(grew)
			{
				grew = false;
				for(int i = 0; i < left.size(); ++i)
				{
					const PlanDoor& d = left[i];
					if(d.deck != run[0].deck || d.side != run[0].side || rooms(d) != rooms(run[0])
							|| (xWall ? d.cx != run[0].cx : d.cy != run[0].cy))
						continue;
					bool next = false;
					for(const PlanDoor& r : run)
						if(abs(xWall ? d.cy - r.cy : d.cx - r.cx) == 1)
							next = true;
					if(!next)
						continue;
					run << left.takeAt(i);
					grew = true;
					break;
				}
			}
			std::sort(run.begin(), run.end(), [xWall](const PlanDoor& p, const PlanDoor& q) {
				return xWall ? p.cy < q.cy : p.cx < q.cx;
			});
			DoorRun dr;
			dr.door = run[run.size() / 2];
			dr.count = run.size();
			dr.mid = (midOf(run.first()) + midOf(run.last())) / 2.0;
			runs << dr;
		}
	}
	for(const DoorRun& run : runs)
	{
		const PlanDoor& door = run.door;
		const TriRef a(door.deck, door.cx, door.cy, door.side);
		const TriRef b = plan.AcrossCellEdge(door.deck, door.cx, door.cy, door.side);
		const DeckRoom* ra = deckRoomOf(a);
		const DeckRoom* rb = deckRoomOf(b);
		if(!ra && !rb)
			continue;
		if(ra && rb && ra == rb)
			continue;

		double z0, z1;
		RoomHeights(plan, door.deck, z0, z1);
		const bool bay = run.count > 1;
		const double doorTop = bay ? z1 - 0.5 : z0 + qMin(8.0, z1 - z0 - 0.5);
		const double width = bay ? run.count * s - 1.0 : qMin(10.0, s - 0.5);
		const QPointF mid = run.mid;
		const bool xWall = (door.side == TRI_E || door.side == TRI_W);		// The wall is at a constant x.

		if(ra && rb)
		{
			AuPoint mn, mx;
			if(xWall)
			{
				mn = AuPoint(mid.x() - .5, mid.y() - width/2, z0);
				mx = AuPoint(mid.x() + .5, mid.y() + width/2, doorTop);
			}
			else
			{
				mn = AuPoint(mid.x() - width/2, mid.y() - .5, z0);
				mx = AuPoint(mid.x() + width/2, mid.y() + .5, doorTop);
			}
			quint16 doorId = 0;
			if(!AddInteriorDoor(*design, mn, mx, xWall ? 0 : 1, ra->group, doorId, ra->meshId, rb->meshId))
			{
				report.warnings << QString("The engine did not accept the door between '%1' and '%2'.").arg(ra->group, rb->group);
				continue;
			}
			++report.doors;
			if(options.walkPaths)
			{
				const double pz = z0;
				const QPointF ca = plan.TriCentre(a), cb = plan.TriCentre(b);
				QList<AuPoint> wa = WalkPoints(plan, RoomWalk(plan, ra->room, ra->anchor, a), pz);
				QList<AuPoint> wb = WalkPoints(plan, RoomWalk(plan, rb->room, rb->anchor, b), pz);
				AddWalk(*design, wa, paths);
				AddWalk(*design, wb, paths);
				AddWalkOnce(*design, AuPoint(ca.x(), ca.y(), pz), AuPoint(cb.x(), cb.y(), pz), doorId, paths);
			}
			continue;
		}

		// A hull door: the room is on one side, and on the other is either
		// open space or hull with no room in it. Walk outwards until the
		// hull ends; the surface is the skin beyond that.
		const DeckRoom* room = ra ? ra : rb;
		const TriRef inner = ra ? a : b;
		const TriRef outer = ra ? b : a;
		const int outSide = outer.k == TRI_S ? TRI_N : (outer.k == TRI_N ? TRI_S : (outer.k == TRI_E ? TRI_W : TRI_E));
		AuPoint outward(0, 0, 0);
		if(outSide == TRI_E) outward = AuPoint(1, 0, 0);
		if(outSide == TRI_W) outward = AuPoint(-1, 0, 0);
		if(outSide == TRI_N) outward = AuPoint(0, 1, 0);
		if(outSide == TRI_S) outward = AuPoint(0, -1, 0);
		const double sgn = (outSide == TRI_E || outSide == TRI_N) ? 1.0 : -1.0;
		const double roomWall = xWall ? mid.x() : mid.y();

		// From the wall, through hull triangles, to the first one that is not hull.
		double boundary = roomWall;
		{
			TriRef t = outer;
			int guard = 0;
			while(plan.Hull(t) && guard++ < 400)
			{
				// Through this cell, out the far side.
				TriRef farSide(t.deck, t.cx, t.cy, outSide);
				if(!plan.Hull(farSide))
				{
					// The far triangle of the cell is outside: the hull ends at the cell's middle.
					boundary += sgn * s / 2;
					break;
				}
				boundary += sgn * s;
				t = plan.AcrossCellEdge(t.deck, t.cx, t.cy, outSide);
			}
		}
		const double surface = boundary + sgn * plan.skinFeet;
		const double slab = boundary + sgn * plan.skinFeet * 0.5;
		AuPoint mn, mx;
		if(xWall)
		{
			mn = AuPoint(slab - .5, mid.y() - width/2, z0);
			mx = AuPoint(slab + .5, mid.y() + width/2, doorTop);
		}
		else
		{
			mn = AuPoint(mid.x() - width/2, slab - .5, z0);
			mx = AuPoint(mid.x() + width/2, slab + .5, doorTop);
		}
		quint16 doorId = 0;
		if(!AddHullDoor(*design, mn, mx, outward, roomWall, surface, room->group, doorId, room->meshId, hullBottom))
		{
			report.warnings << QString("The engine did not accept the hull door of '%1'.").arg(room->group);
			continue;
		}
		++report.doors;
		// A ramp down to the ground where the door is low enough, as at the
		// user's aircraft factory; otherwise a ladder (below).
		AuPoint rampTop, rampFoot;
		const bool ramp = options.ramps && AddDoorRamp(*design, mn, mx, outward, surface,
				hullBottom - SHIPGEN_RAMP_BELOW_HULL, rampTop, rampFoot);
		if(ramp)
			++report.ramps;
		if(options.walkPaths)
		{
			const double pz = z0;
			const QPointF ci = plan.TriCentre(inner);
			AddWalk(*design, WalkPoints(plan, RoomWalk(plan, room->room, room->anchor, inner), pz), paths);
			const AuPoint from(ci.x(), ci.y(), pz);
			AuPoint to = from;
			// Just outside the hull, inside the doorway's voids (which reach
			// a foot beyond it), so the top of the ladder is on the doorway's
			// floor and nobody has to jump.
			if(xWall)
				to.x = surface + sgn * SHIPGEN_LADDER_OUTSIDE;
			else
				to.y = surface + sgn * SHIPGEN_LADDER_OUTSIDE;
			if(ramp)
			{
				// Out over the landing and down the ramp.
				rampTop.z = pz;
				AddWalkOnce(*design, from, rampTop, doorId, paths);
				AddWalkOnce(*design, rampTop, rampFoot, 0, paths);
			}
			else
				AddWalkOnce(*design, from, to, doorId, paths);
			// Down to the ground by a ladder, as in the working designs:
			// straight down from outside the door to a little below the
			// bottom of the hull.
			const double foot = hullBottom - SHIPGEN_LADDER_BELOW_HULL;
			if(!ramp && pz - foot > .5)
				AddLadderPath(*design, AuPoint(to.x, to.y, foot), to, outward * -1.0);
			const double depth = fabs(surface - roomWall) - plan.skinFeet;
			if(depth > s * 1.5)
				report.warnings << QString("The hull door of '%1' is %2 ft from the hull surface; the way out is a "
						"long tunnel. Put the door on a room cell at the edge of the hull.")
						.arg(room->group).arg(qRound(depth));
		}
	}

	// The windows: cut through the room wall and the hull with a window jig,
	// the way the game's designer makes them, from sill height up. With
	// joinWindows, windows side by side on one straight wall of a room are
	// one long window.
	struct WindowRun { PlanDoor window; QPointF p, q; int count = 1; };
	QList<WindowRun> windowRuns;
	{
		struct Edge { PlanDoor window; QPointF p, q, inward; const DeckRoom* room; };
		QList<Edge> edges;
		for(const PlanDoor& window : plan.windows)
		{
			const TriRef a(window.deck, window.cx, window.cy, window.side);
			const TriRef b = window.edge ? plan.Across(a, window.edge) : plan.AcrossCellEdge(window.deck, window.cx, window.cy, window.side);
			Edge e;
			e.window = window;
			e.room = deckRoomOf(a) ? deckRoomOf(a) : deckRoomOf(b);
			if(window.edge)
				plan.EdgeEnds(a, window.edge, e.p, e.q);
			else
			{
				QPointF c[3];
				plan.TriCorners(plan.Valid(a) ? a : b, c);
				e.p = c[0];
				e.q = c[1];
			}
			e.inward = plan.TriCentre(deckRoomOf(a) ? a : b) - (e.p + e.q) / 2.0;
			edges << e;
		}
		auto close = [](const QPointF& u, const QPointF& v) { return fabs(u.x() - v.x()) < 1e-6 && fabs(u.y() - v.y()) < 1e-6; };
		auto cross = [](const QPointF& u, const QPointF& v) { return u.x() * v.y() - u.y() * v.x(); };
		while(!edges.isEmpty())
		{
			QList<Edge> run;
			run << edges.takeFirst();
			const QPointF dir = run[0].q - run[0].p;
			bool grew = plan.joinWindows;
			while(grew)
			{
				grew = false;
				for(int i = 0; i < edges.size(); ++i)
				{
					const Edge& e = edges[i];
					if(e.window.deck != run[0].window.deck || e.room != run[0].room || !e.room
							|| fabs(cross(dir, e.q - e.p)) > 1e-6
							|| (QPointF::dotProduct(e.inward, run[0].inward) <= 0))
						continue;
					bool next = false;
					for(const Edge& r : run)
						if(close(e.p, r.p) || close(e.p, r.q) || close(e.q, r.p) || close(e.q, r.q))
							next = true;
					// Collinear, not just parallel.
					if(!next || fabs(cross(dir, e.p - run[0].p)) > 1e-6)
						continue;
					run << edges.takeAt(i);
					grew = true;
					break;
				}
			}
			// The ends of the run: the furthest apart along it.
			const double len = sqrt(QPointF::dotProduct(dir, dir));
			const QPointF unit = dir / len;
			double lo = 1e30, hi = -1e30;
			for(const Edge& r : run)
				for(const QPointF& pt : { r.p, r.q })
				{
					const double t = QPointF::dotProduct(pt - run[0].p, unit);
					lo = qMin(lo, t);
					hi = qMax(hi, t);
				}
			WindowRun wr;
			wr.window = run[0].window;
			wr.p = run[0].p + unit * lo;
			wr.q = run[0].p + unit * hi;
			wr.count = run.size();
			windowRuns << wr;
		}
	}
	// How high the windows are: the plan's sill and top above the floor,
	// kept inside the room.
	auto windowHeights = [&plan](int deck, double& sill, double& top) {
		double z0, z1;
		RoomHeights(plan, deck, z0, z1);
		top = qMin(z1 - .5, z0 + plan.windowTopFeet);
		sill = qMin(z0 + qMax(0.0, plan.windowSillFeet), top - 1.0);
	};
	for(const WindowRun& run : windowRuns)
	{
		const PlanDoor& window = run.window;
		if(window.edge != 0 || run.count > 1)
		{
			// On a diagonal. How far away the hull skin is comes from looking
			// out at the hull itself.
			const TriRef a(window.deck, window.cx, window.cy, window.side);
			const TriRef b = plan.Across(a, window.edge);
			const DeckRoom* ra = deckRoomOf(a);
			const DeckRoom* rb = deckRoomOf(b);
			if((ra != 0) == (rb != 0))
				continue;
			const DeckRoom* room = ra ? ra : rb;
			const QPointF p = run.p, q = run.q;
			const QPointF mid = (p + q) / 2.0;
			QPointF along = q - p;
			const double edgeLength = sqrt(QPointF::dotProduct(along, along));
			along /= edgeLength;
			QPointF out(-along.y(), along.x());
			if(QPointF::dotProduct(out, plan.TriCentre(ra ? a : b) - mid) > 0)
				out = -out;
			double sill, top;
			windowHeights(window.deck, sill, top);
			AuScFaceList hullFaces;
			for(AuScParts::const_iterator it = design->Parts().constBegin(); it != design->Parts().constEnd(); ++it)
				if(it.value() && it.value()->IsHullMesh() && it.value()->GroupName() != "Frame")
				{
					AuScFaceList partFaces;
					it.value()->Faces(partFaces);
					hullFaces.AddFaces(partFaces);
				}
			// Look out from along the window (from its middle, and every cell
			// along a long one) and cut as deep as the furthest hull.
			double gap = -1;
			AuPoint hit;
			const int looks = qMax(1, int(edgeLength / s));
			for(int i = 0; i < looks; ++i)
			{
				const QPointF at = looks == 1 ? mid : p + along * (edgeLength * (i + .5) / looks);
				const AuPoint eye(at.x() - out.x() * .5, at.y() - out.y() * .5, (sill + top) / 2);
				AuPoint h = eye + AuPoint(out.x(), out.y(), 0) * 40.0;
				if(!hullFaces.Sight(eye, h))
					continue;
				if((h - eye).Length() - .5 > gap)
				{
					gap = (h - eye).Length() - .5;
					hit = h;
				}
			}
			if(gap < 0)
			{
				report.warnings << QString("A window of '%1' looks out at no hull.").arg(room->group);
				continue;
			}
			// Less a frame's width at the ends: 0.35 ft on a diagonal, half a
			// foot on a cell edge, as for a single window.
			const double width = edgeLength - (window.edge != 0 ? .7 : 1.0);
			// The frame JigCut puts round an opening (instead of the engine's
			// lining, which overlaps itself on a curved hull) needs the wall
			// along an axis. So the hull and the room are turned until the
			// wall is at constant x with the outside towards +x, cut there,
			// and turned back with what the cut made.
			const double angle = atan2(out.y(), out.x());
			AuMatrix4x4 toWall, back;
			toWall.RotateZ(-angle);
			toWall.Translate(AuPoint(-mid.x(), -mid.y(), 0));
			back.Translate(AuPoint(mid.x(), mid.y(), 0));
			back.RotateZ(angle);
			const quint16 hullId = HullPartAt(*design, hit);
			AuScPart* hullPart = design->Parts().value(hullId);
			AuScPart* roomPart = design->Parts().value(room->meshId);
			if(!hullPart || !roomPart)
			{
				report.warnings << QString("The window of '%1' could not be cut.").arg(room->group);
				continue;
			}
			hullPart->Transform(toWall);
			roomPart->Transform(toWall);
			AuScFaceList jig = BoxFaces(AuPoint(-.5, -width / 2, sill), AuPoint(gap + 3.0, width / 2, top));
			OrientAsHull(jig);
			QList<quint16> added;
			const bool cut = JigCut(*design, AuScPart::Part_WindowJig, jig, hullId, room->meshId, &added, 0, true, 0.0);
			// The cut may have put new parts in their place.
			for(quint16 id : { hullId, room->meshId })
				if(AuScPart* part = design->Parts().value(id))
					part->Transform(back);
			for(quint16 id : added)
				if(AuScPart* part = design->Parts().value(id))
					part->Transform(back);
			if(!cut || added.isEmpty())
			{
				report.warnings << QString("The window of '%1' could not be cut.").arg(room->group);
				continue;
			}
			for(quint16 id : added)
				if(AuScPart* part = design->Parts().value(id))
					if(part->IsWindow())
						part->SetGroupName(room->group);
			++report.windows;
			continue;
		}
		const TriRef a(window.deck, window.cx, window.cy, window.side);
		const TriRef b = plan.AcrossCellEdge(window.deck, window.cx, window.cy, window.side);
		const DeckRoom* ra = deckRoomOf(a);
		const DeckRoom* rb = deckRoomOf(b);
		if((ra != 0) == (rb != 0))
			continue;
		const DeckRoom* room = ra ? ra : rb;
		const TriRef outer = ra ? b : a;
		const int outSide = outer.k == TRI_S ? TRI_N : (outer.k == TRI_N ? TRI_S : (outer.k == TRI_E ? TRI_W : TRI_E));
		const double sgn = (outSide == TRI_E || outSide == TRI_N) ? 1.0 : -1.0;
		const bool xWall = (window.side == TRI_E || window.side == TRI_W);
		QPointF c[3];
		plan.TriCorners(plan.Valid(a) ? a : b, c);
		const QPointF mid = (c[0] + c[1]) / 2.0;
		const double roomWall = xWall ? mid.x() : mid.y();
		double boundary = roomWall;
		{
			TriRef t = outer;
			int guard = 0;
			while(plan.Hull(t) && guard++ < 400)
			{
				TriRef farSide(t.deck, t.cx, t.cy, outSide);
				if(!plan.Hull(farSide))
				{
					boundary += sgn * s / 2;
					break;
				}
				boundary += sgn * s;
				t = plan.AcrossCellEdge(t.deck, t.cx, t.cy, outSide);
			}
		}
		const double surface = boundary + sgn * plan.skinFeet;
		double sill, top;
		windowHeights(window.deck, sill, top);
		const double half = (s - 1.0) / 2;
		const double inner = roomWall - sgn * .5, outerFace = surface + sgn * 3.0;
		AuPoint mn, mx;
		if(xWall)
		{
			mn = AuPoint(qMin(inner, outerFace), mid.y() - half, sill);
			mx = AuPoint(qMax(inner, outerFace), mid.y() + half, top);
		}
		else
		{
			mn = AuPoint(mid.x() - half, qMin(inner, outerFace), sill);
			mx = AuPoint(mid.x() + half, qMax(inner, outerFace), top);
		}
		AuPoint probe = (mn + mx) / 2.0;
		if(xWall)
			probe.x = surface;
		else
			probe.y = surface;
		AuScFaceList jig = BoxFaces(mn, mx);
		OrientAsHull(jig);
		QList<quint16> added;
		const quint16 hullId = HullPartAt(*design, probe);
		if(!hullId || !JigCut(*design, AuScPart::Part_WindowJig, jig, hullId, room->meshId, &added, xWall ? 0 : 1) || added.isEmpty())
		{
			report.warnings << QString("The window of '%1' could not be cut.").arg(room->group);
			continue;
		}
		for(quint16 id : added)
			if(AuScPart* part = design->Parts().value(id))
				if(part->IsWindow())
					part->SetGroupName(room->group);		// So that the room's light falls on it.
		++report.windows;
		if(fabs(surface - roomWall) - plan.skinFeet > s * 1.5)
			report.warnings << QString("A window of '%1' is %2 ft from the hull surface; it looks down a tunnel.")
					.arg(room->group).arg(qRound(fabs(surface - roomWall) - plan.skinFeet));
	}

	// The ladders: a shaft through the floor between the two rooms, and a
	// ladder path up it.
	for(const PlanLadder& ladder : plan.ladders)
	{
		const DeckRoom* lower = deckRoomOf(TriRef(ladder.deck, ladder.cx, ladder.cy, 0));
		const DeckRoom* upper = deckRoomOf(TriRef(ladder.deck + 1, ladder.cx, ladder.cy, 0));
		if(!lower || !upper)
			continue;
		double lz0, lz1, uz0, uz1;
		RoomHeights(plan, ladder.deck, lz0, lz1);
		RoomHeights(plan, ladder.deck + 1, uz0, uz1);
		const QPointF lo = plan.Corner(ladder.cx, ladder.cy), hi = plan.Corner(ladder.cx + 1, ladder.cy + 1);
		const double inset = qMin(0.5, s / 4);
		quint16 shaftVoid = 0, shaftMesh = 0;
		const AuPoint mn(lo.x() + inset, lo.y() + inset, lz1 - 1.0), mx(hi.x() - inset, hi.y() - inset, uz0 + 1.0);
		if(!AddShaft(*design, QString("%1 %2 to %3").arg(LadderGroupPrefix(), lower->group, upper->group), mn, mx, shaftVoid))
		{
			report.warnings << QString("The engine did not accept the ladder from '%1'.").arg(lower->group);
			continue;
		}
		Q_UNUSED(shaftMesh);
		const QPointF c = plan.CellCentre(ladder.cx, ladder.cy);
		AddLadderPath(*design, AuPoint(c.x(), c.y(), lz0), AuPoint(c.x(), c.y(), uz0), AuPoint(1, 0, 0));
		// So that it can be seen: the opening through the ceiling and the
		// floor, and a ladder.
		const QString ladderGroup = QString("%1 %2 to %3").arg(LadderGroupPrefix(), lower->group, upper->group);
		if(!CutLadderOpening(*design, mn, mx, lower->meshId, upper->meshId))
			report.warnings << QString("The opening for the ladder from '%1' could not be cut.").arg(lower->group);
		AddLadderMesh(*design, AuPoint(c.x(), c.y(), lz0), AuPoint(c.x(), c.y(), uz0), AuPoint(1, 0, 0), ladderGroup);
		if(options.walkPaths)
		{
			AddWalk(*design, WalkPoints(plan, RoomWalk(plan, lower->room, lower->anchor,
					TriRef(ladder.deck, ladder.cx, ladder.cy, 0)), lz0), paths);
			AddWalk(*design, WalkPoints(plan, RoomWalk(plan, upper->room, upper->anchor,
					TriRef(ladder.deck + 1, ladder.cx, ladder.cy, 0)), uz0), paths);
		}
		++report.ladders;
	}

	// The stairs: like a ladder, an opening through the ceiling below and the
	// floor above with a void through it, but over the whole run, with a
	// sloped barrier to walk on and steps to see (as in the user's aircraft
	// factory), a walk path up it (which the factory's lacks), and a rail
	// round the opening on the deck above.
	for(const PlanStair& stair : plan.stairs)
	{
		const int n = plan.StairCells();
		int x0, y0, xl, yl;
		plan.StairCell(stair, 0, x0, y0);
		plan.StairCell(stair, n, xl, yl);
		const DeckRoom* lower = deckRoomOf(TriRef(stair.deck, x0, y0, 0));
		const DeckRoom* upper = deckRoomOf(TriRef(stair.deck + 1, xl, yl, 0));
		if(!lower || !upper || !plan.StairFits(stair))
			continue;
		double lz0, lz1, uz0, uz1;
		RoomHeights(plan, stair.deck, lz0, lz1);
		RoomHeights(plan, stair.deck + 1, uz0, uz1);
		const AuPoint dir(stair.side == TRI_E ? 1 : stair.side == TRI_W ? -1 : 0,
				stair.side == TRI_N ? 1 : stair.side == TRI_S ? -1 : 0, 0);
		const bool alongX = stair.side == TRI_E || stair.side == TRI_W;
		const double run = n * s, rise = uz0 - lz0, inset = qMin(0.5, s / 4);
		const QPointF c0 = plan.CellCentre(x0, y0);
		// The middle of the run's back edge, where it starts.
		const AuPoint foot(c0.x() - dir.x * s / 2, c0.y() - dir.y * s / 2, lz0);
		const AuPoint top = foot + dir * run;
		// The opening: over the run, the width of a cell less an inset, from
		// an inset in from the foot to the edge of the landing.
		AuPoint a = foot + dir * inset, b = foot + dir * run;
		if(alongX)
		{
			a.y -= s / 2 - inset;
			b.y += s / 2 - inset;
		}
		else
		{
			a.x -= s / 2 - inset;
			b.x += s / 2 - inset;
		}
		a.z = lz1 - 1.0;
		b.z = uz0 + 1.0;
		const AuPoint mn = a.Min(b), mx = a.Max(b);
		const QString group = QString("%1 (stairs) %2 to %3").arg(LadderGroupPrefix(), lower->group, upper->group);
		quint16 shaftVoid = 0;
		if(!AddShaft(*design, group, mn, mx, shaftVoid))
		{
			report.warnings << QString("The engine did not accept the stairs from '%1'.").arg(lower->group);
			continue;
		}
		if(!CutLadderOpening(*design, mn, mx, lower->meshId, upper->meshId))
			report.warnings << QString("The opening for the stairs from '%1' could not be cut.").arg(lower->group);
		if(!AddStairFlight(*design, foot, dir, s - 2 * inset, run, rise, group))
			report.warnings << QString("The engine did not accept the steps of the stairs from '%1'.").arg(lower->group);
		// The rail round the opening on the deck above: along both sides and
		// across the foot end; the landing end is the way off.
		{
			const double r = .2;		// Just outside the opening.
			AuPoint p0(mn.x - r, mn.y - r, uz0), p1(mx.x + r, mn.y - r, uz0), p2(mx.x + r, mx.y + r, uz0), p3(mn.x - r, mx.y + r, uz0);
			QList<QPair<AuPoint, AuPoint> > sides;
			sides << qMakePair(p0, p1) << qMakePair(p1, p2) << qMakePair(p2, p3) << qMakePair(p3, p0);
			for(const QPair<AuPoint, AuPoint>& side : sides)
			{
				// Skip the side at the landing: the one whose middle is furthest along 'dir'.
				const AuPoint middle = (side.first + side.second) / 2.0;
				const AuPoint centre = (mn + mx) / 2.0;
				const double along = (middle - centre).Dot(dir);
				const double half = alongX ? (mx.x - mn.x) / 2 : (mx.y - mn.y) / 2;
				if(along > half * .5)
					continue;
				AddRail(*design, side.first, side.second, group);
			}
		}
		if(options.walkPaths)
		{
			// Up from the room below to the foot, up the slope, and on to the
			// landing and the room above.
			const int back = (stair.side + 2) % 4;
			const TriRef start(stair.deck, x0, y0, back), landing(stair.deck + 1, xl, yl, back);
			AddWalk(*design, WalkPoints(plan, RoomWalk(plan, lower->room, lower->anchor, start), lz0), paths);
			AddWalk(*design, WalkPoints(plan, RoomWalk(plan, upper->room, upper->anchor, landing), uz0), paths);
			const QPointF sc = plan.TriCentre(start), lc = plan.TriCentre(landing);
			const AuPoint bottom(sc.x(), sc.y(), lz0), up(top.x, top.y, uz0), off(lc.x(), lc.y(), uz0);
			AddWalkOnce(*design, bottom, up, 0, paths);
			AddWalkOnce(*design, up, off, 0, paths);
		}
		++report.stairs;
	}

	// Vehicle parking spots: the game's flat icons on the floor of the room,
	// a little above it as in the working designs (the Atlantis' hold).
	for(const PlanParking& spot : plan.parking)
	{
		if(!plan.ParkingFits(spot))
		{
			report.warnings << QString("A parking spot on deck %1 does not lie on the floor of one room.").arg(spot.deck + 1);
			continue;
		}
		double z0, z1;
		RoomHeights(plan, spot.deck, z0, z1);
		const QPointF c = plan.Corner(spot.cx, spot.cy);
		AuScPart* part = AuScPart::Create(spot.space ? AuScPart::Part_SpaceVehicleParkingSpot
				: AuScPart::Part_GroundVehicleParkingSpot);
		AuScIcon* icon = dynamic_cast<AuScIcon*>(part);
		if(icon)
		{
			// The icon's long side is its own y; along the plan's x unless across.
			icon->MakeIcon(AuPoint(c.x(), c.y(), z0 + .3), spot.across ? 0.f : float(PI / 2), AuMatrix4x4());
			const DeckRoom* room = deckRoomOf(TriRef(spot.deck, qMin(spot.cx, plan.Width() - 1), qMin(spot.cy, plan.Depth() - 1), 0));
			if(room)
				icon->SetGroupName(room->group);
			if(icon->IsValid() && design->Parts().AddPart(*icon))
				++report.parking;
		}
		delete part;
	}

	// Everything so far was laid out in plan space. Turn it to the game's
	// axes before the furnishing, because the engine's hull volume, which
	// the system volumes are worked out from, depends on the orientation.
	TurnToGameAxes(*design);
	if(!AnalyseHull(*design, layout, analysis, error))
		report.warnings << QString("The turned hull could not be measured: %1").arg(error);
	report.hullVolumeM3 = analysis.volumeM3;

	// Stations, berths, panels and the system volumes, by the template.
	if(options.furnish)
	{
		Brief brief;
		brief.name = plan.name;
		ApplyTemplate(brief, plan.templateKey);
		brief.ftlDrive = plan.ftlDrive;
		// Stations left out, and with them what they run (the generator
		// fits no shields without a shield station and no weapon bay
		// without fire control).
		for(int i = brief.stations.size() - 1; i >= 0; --i)
			if(plan.leaveOut.contains(brief.stations.at(i).type))
				brief.stations.removeAt(i);
		if(plan.leaveOut.contains("shield"))
			brief.modules.shields = 0;
		QList<PlacedRoom> placed;
		for(const DeckRoom& dr : deckRooms)
		{
			if(!dr.hasBox)
			{
				report.notes << QString("'%1' has no whole cell, so nothing was put in it.").arg(dr.group);
				continue;
			}
			PlacedRoom pr;
			pr.type = dr.type;
			pr.label = dr.group;
			pr.min = dr.boxMin;
			pr.max = dr.boxMax;
			TurnBox(pr.min, pr.max);
			pr.voidId = dr.voidId;
			pr.meshId = dr.meshId;
			pr.volumeM3 = (dr.boxMax.x - dr.boxMin.x) * (dr.boxMax.y - dr.boxMin.y) * (dr.boxMax.z - dr.boxMin.z) * 0.0283168466;
			placed << pr;
		}
		const LayoutResult furnished = FurnishRooms(*design, brief, analysis, layout, placed);
		report.stations = furnished.stationsPlaced;
		report.berths = furnished.berthsPlaced;
		report.notes << furnished.warnings;
	}

	// The cuts for doors and windows add faces to the hull.
	report.hullFaces = 0;
	for(AuScParts::const_iterator it = design->Parts().constBegin(); it != design->Parts().constEnd(); ++it)
		if(it.value() && it.value()->IsHullMesh())
		{
			AuScFaceList faces;
			it.value()->Faces(faces);
			report.hullFaces += faces.size();
		}

	ValidateDesign(*design, report.errors, report.engineWarnings);
	if(design->HullVolume() > 0)
		report.hullVolumeM3 = design->HullVolume();		// The engine's own measure.
	report.ok = report.errors.isEmpty();
	return design;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// A plan from a 3D model.

// A triangle from three corners, with its face normal.
static AuScTri TriOf(const AuPoint& a, const AuPoint& b, const AuPoint& c)
{
	AuPoint n = (b - a).Cross(c - a);
	if(n.Length() > 1e-12)
		n.Unit();
	else
		n = AuPoint(0, 0, 1);
	return AuScTri(AuScPoint(a, 0.f, 0.f, n), AuScPoint(b, 0.f, 0.f, n), AuScPoint(c, 0.f, 0.f, n));
}

bool PlanFromModel(const QString& modelFile, const PlanFromModelOptions& options, TriPlan& plan,
		QStringList& notes, QString& error)
{
	AuScTriList tris;
	if(!LoadMeshFile(modelFile, tris, error))
		return false;
	if(tris.isEmpty())
	{
		error = QString("%1 has no triangles.").arg(modelFile);
		return false;
	}
	// A big model is thinned out first, the way shipgen reduce does it, and
	// the holes the thinning opens are closed again.
	if(options.maxFaces > 0 && tris.size() > options.maxFaces)
	{
		const int before = tris.size();
		tris = SimplifyMesh(tris, options.maxFaces, SimplifyMethod::Quadric);
		if(InspectMesh(tris).loose > 0)
			tris = FillHoles(tris);
		notes << QString("The model was thinned from %1 to %2 faces.").arg(before).arg(tris.size());
	}

	// Repair it only when it needs it, and keep the repair only when it helps:
	// on some closed models the repair does more harm than good.
	auto problems = [](const MeshSolidity& m) { return m.openEdges + m.foldedEdges + m.thickEdges - m.pinchedEdges; };
	{
		const AuScTriList windings = FixWindings(CleanMesh(tris));
		if(problems(AuditSolidity(windings)) <= problems(AuditSolidity(tris)))
			tris = windings;
		if(!AuditSolidity(tris).Closed())
		{
			const AuScTriList repaired = SolidifyMesh(tris);
			if(problems(AuditSolidity(repaired)) < problems(AuditSolidity(tris)))
				tris = repaired;
		}
	}
	const MeshSolidity solidity = AuditSolidity(tris);
	if(!solidity.Closed() && !solidity.ClosedButForOpenings())
		notes << QString("The model is not closed (%1 open, %2 thick, %3 folded edges after repair), so its inside "
				"cannot be told. Repair it first, for example with shipgen reduce.")
				.arg(solidity.openEdges).arg(solidity.thickEdges).arg(solidity.foldedEdges);
	else if(!solidity.Closed())
		notes << QString("The model is not quite closed (%1 open edges after repair); the inside is found by a "
				"vote of rays, so look the grid over.").arg(solidity.openEdges);

	// Turn it so that the longest horizontal side is x, and scale it.
	auto transform = [&tris](const AuMatrix4x4& m) {
		AuScTriList turned;
		for(const AuScTri& t : tris)
			turned.append(TriOf(m.Project(AuPoint(t.V0())), m.Project(AuPoint(t.V1())), m.Project(AuPoint(t.V2()))));
		tris = turned;
	};
	if(options.yUp)
	{
		AuMatrix4x4 up;
		up.RotateX(PI / 2);		// y becomes z.
		transform(up);
	}
	AuPoint mn, mx;
	MeshExtents(tris, mn, mx);
	AuMatrix4x4 m;
	if(mx.y - mn.y > mx.x - mn.x)
		m.RotateZ(PI / 2);
	if(options.flip)
		m.RotateZ(PI);
	transform(m);
	MeshExtents(tris, mn, mx);
	const double length = mx.x - mn.x;
	if(length <= 0)
	{
		error = QString("%1 has no length.").arg(modelFile);
		return false;
	}
	const double scale = options.lengthFeet / length;
	const AuPoint centre((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z);
	{
		AuScTriList placed;
		for(const AuScTri& t : tris)
		{
			const AuPoint p[3] = { AuPoint(t.V0()), AuPoint(t.V1()), AuPoint(t.V2()) };
			placed.append(TriOf((p[0] - centre) * scale, (p[1] - centre) * scale, (p[2] - centre) * scale));
		}
		tris = placed;
	}
	MeshExtents(tris, mn, mx);
	// A model made of several pieces that overlap (a crystal and the rings
	// round it) has no single inside: counting crossings goes wrong where
	// they overlap. Each piece is tested on its own, and a point is inside
	// the model when it is inside any of them.
	QList<SolidMesh> pieces;
	{
		QHash<QString, int> vertexIds;
		auto vid = [&vertexIds](const AuPoint& p) {
			const QString key = QString("%1,%2,%3").arg(p.x, 0, 'f', 4).arg(p.y, 0, 'f', 4).arg(p.z, 0, 'f', 4);
			auto it = vertexIds.find(key);
			if(it == vertexIds.end())
				it = vertexIds.insert(key, vertexIds.size());
			return it.value();
		};
		QVector<int> parent;
		std::function<int(int)> root = [&parent, &root](int i) { return parent[i] == i ? i : (parent[i] = root(parent[i])); };
		QVector<std::array<int, 3> > corners;
		for(const AuScTri& t : tris)
		{
			const std::array<int, 3> c = { vid(AuPoint(t.V0())), vid(AuPoint(t.V1())), vid(AuPoint(t.V2())) };
			while(parent.size() < vertexIds.size())
				parent << parent.size();
			parent[root(c[1])] = root(c[0]);
			parent[root(c[2])] = root(c[0]);
			corners << c;
		}
		QMap<int, AuScTriList> byPiece;
		for(int i = 0; i < tris.size(); ++i)
			byPiece[root(corners[i][0])].append(tris.at(i));
		for(const AuScTriList& piece : byPiece)
		{
			SolidMesh solid(piece);
			if(solid.HasInside())
				pieces << solid;
		}
		if(byPiece.size() > 1)
			notes << QString("The model has %1 separate pieces; %2 of them are closed and count as hull.")
					.arg(byPiece.size()).arg(pieces.size());
	}
	auto inModel = [&pieces](const AuPoint& p) {
		for(const SolidMesh& piece : pieces)
			if(piece.Contains(p))
				return true;
		return false;
	};

	// The grid: a cell of room round the model, and enough decks.
	plan = TriPlan();
	plan.name = QFileInfo(modelFile).completeBaseName();
	plan.cellFeet = options.cellFeet;
	plan.deckFeet = options.deckFeet;
	const int width = int(ceil((mx.x - mn.x) / options.cellFeet)) + 2;
	int depth = int(ceil(qMax(fabs(mn.y), fabs(mx.y)) * 2 / options.cellFeet)) + 2;
	depth += depth % 2;		// Even, so that the mirror line is a cell edge.
	plan.Resize(width, depth);
	const int decks = qMax(1, int(ceil(mx.z / options.deckFeet - .2)));
	while(plan.Decks() < decks)
		plan.AddDeck();

	int hull = 0;
	for(int d = 0; d < plan.Decks(); ++d)
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const TriRef t(d, cx, cy, k);
					QPointF c[3];
					plan.TriCorners(t, c);
					const QPointF middle = (c[0] + c[1] + c[2]) / 3.0;
					QPointF at[4] = { middle, middle, middle, middle };
					for(int i = 0; i < 3; ++i)
						at[i + 1] = middle + (c[i] - middle) * .6;
					int inside = 0, samples = 0;
					for(const QPointF& q : at)
						for(double h : { .2, .5, .8 })
						{
							++samples;
							if(inModel(AuPoint(q.x(), q.y(), plan.DeckFloor(d) + plan.deckFeet * h)))
								++inside;
						}
					if(inside >= options.threshold * samples && inside > 0)
					{
						plan.SetHull(t, true);
						++hull;
					}
				}
	if(!hull)
	{
		error = QString("No part of %1 is thick enough for a %2 ft cell. Make it longer, or the cells smaller.")
				.arg(modelFile).arg(options.cellFeet);
		return false;
	}
	notes << QString("%1 by %2 cells of %3 ft, %4 decks; %5 hull triangles.").arg(plan.Width()).arg(plan.Depth())
			.arg(options.cellFeet).arg(plan.Decks()).arg(hull);
	return true;
}
