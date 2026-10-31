//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - turning a triangle plan into a blueprint.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "planbuild.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QHash>
#include <QLineF>
#include <QMap>
#include <QQueue>
#include <QSet>
#include <cmath>
#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <numeric>
#include <queue>

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

// The drawn hull triangles, with the gaps filled where two parts of the
// hull would touch only along an edge: round a point of a deck, the filled
// triangles must make one wedge, not two or more (a checkerboard corner);
// and a triangle may not meet the triangle across its edge on the deck
// above (or below) when neither the one above it nor the one across on its
// own deck is filled. Such an edge would have four faces, which the engine's
// door and window cutting tears open (the Omega: 76 edges, then 751 open
// ones, and the game hung loading it). Only fills, so every drawn triangle
// stays inside; 'added' counts the triangles filled in.
QVector<bool> ManifoldHull(const TriPlan& plan, int& added)
{
	const int W = plan.Width(), D = plan.Depth(), decks = plan.Decks();
	auto id = [W, D](const TriRef& t) { return ((t.deck * D + t.cy) * W + t.cx) * 4 + t.k; };
	QVector<bool> filled(decks * D * W * 4, false);
	for(int d = 0; d < decks; ++d)
		for(int cy = 0; cy < D; ++cy)
			for(int cx = 0; cx < W; ++cx)
				for(int k = 0; k < 4; ++k)
					filled[id(TriRef(d, cx, cy, k))] = plan.Hull(TriRef(d, cx, cy, k));
	auto isFilled = [&](const TriRef& t) { return plan.Valid(t) && filled[id(t)]; };
	added = 0;

	for(int pass = 0; pass < 20; ++pass)
	{
		int changed = 0;
		for(int d = 0; d < decks; ++d)
		{
			// Round each point of the deck: its triangles by angle, with a
			// gap that may not be filled where the fan leaves the grid.
			for(int hy = 0; hy <= 2 * D; ++hy)
				for(int hx = 0; hx <= 2 * W; ++hx)
				{
					const bool middle = (hx % 2) && (hy % 2);
					if((hx % 2) != (hy % 2))
						continue;
					struct Item { double angle; TriRef t; };
					QVector<Item> fan;
					for(int cy = (hy - 1) / 2; cy <= hy / 2; ++cy)
						for(int cx = (hx - 1) / 2; cx <= hx / 2; ++cx)
						{
							if(cx < 0 || cy < 0 || cx >= W || cy >= D)
								continue;
							for(int k = 0; k < 4; ++k)
							{
								const TriRef t(d, cx, cy, k);
								int tx[3], ty[3];
								HalfCellCorners(t, tx, ty);
								bool touches = false;
								for(int i = 0; i < 3; ++i)
									touches = touches || (tx[i] == hx && ty[i] == hy);
								if(!touches)
									continue;
								const double mx = (tx[0] + tx[1] + tx[2]) / 3.0 - hx, my = (ty[0] + ty[1] + ty[2]) / 3.0 - hy;
								fan << Item{ atan2(my, mx), t };
							}
						}
					const int full = middle ? 4 : 8;
					if(fan.size() < 2)
						continue;
					std::sort(fan.begin(), fan.end(), [](const Item& a, const Item& b) { return a.angle < b.angle; });
					// A fan cut by the grid's edge: the gap is where the angle jumps.
					int start = 0;
					if(fan.size() < full)
					{
						double widest = -1;
						for(int i = 0; i < fan.size(); ++i)
						{
							const double next = i + 1 < fan.size() ? fan[i + 1].angle : fan[0].angle + 2 * M_PI;
							if(next - fan[i].angle > widest)
							{
								widest = next - fan[i].angle;
								start = (i + 1) % fan.size();
							}
						}
					}
					// The empty gaps between filled wedges.
					const int n = fan.size();
					QVector<QPair<int, int> > gaps;	// first index (from start), length
					int filledCount = 0;
					for(int i = 0; i < n; ++i)
						filledCount += isFilled(fan[(start + i) % n].t) ? 1 : 0;
					if(filledCount == 0 || filledCount == n)
						continue;
					const bool closed = fan.size() == full;
					for(int i = 0; i < n; )
					{
						if(isFilled(fan[(start + i) % n].t)) { ++i; continue; }
						int j = i;
						while(j < n && !isFilled(fan[(start + j) % n].t))
							++j;
						gaps << qMakePair(i, j - i);
						i = j;
					}
					// In a closed fan the first and last gap may be one.
					if(closed && gaps.size() > 1 && gaps.first().first == 0 && gaps.last().first + gaps.last().second == n)
					{
						gaps.first().first = gaps.last().first;
						gaps.first().second += gaps.last().second;
						gaps.removeLast();
					}
					// Runs of filled triangles: as many as gaps in a closed fan;
					// in an open one, a gap at either end does not part two runs.
					int inner = 0;
					for(const QPair<int, int>& g : gaps)
						if(closed || (g.first > 0 && g.first + g.second < n))
							++inner;
					const int runs = closed ? gaps.size() : inner + 1;
					if(runs < 2)
						continue;
					// Keep the widest gap (or the grid's edge) and fill the rest.
					int keep = -1;
					if(closed)
					{
						for(int g = 0; g < gaps.size(); ++g)
							if(keep < 0 || gaps[g].second > gaps[keep].second)
								keep = g;
					}
					for(int g = 0; g < gaps.size(); ++g)
					{
						if(g == keep)
							continue;
						if(!closed && (gaps[g].first == 0 || gaps[g].first + gaps[g].second == n))
							continue;
						for(int i = 0; i < gaps[g].second; ++i)
						{
							const TriRef& t = fan[(start + gaps[g].first + i) % n].t;
							filled[id(t)] = true;
							++changed;
						}
					}
				}

			// Between this deck and the one above: no two triangles meeting
			// only along an edge.
			if(d + 1 >= decks)
				continue;
			for(int cy = 0; cy < D; ++cy)
				for(int cx = 0; cx < W; ++cx)
					for(int k = 0; k < 4; ++k)
					{
						const TriRef t(d, cx, cy, k), up(d + 1, cx, cy, k);
						for(int e = 0; e < 3; ++e)
						{
							const TriRef across = plan.Across(t, e);
							if(!plan.Valid(across))
								continue;
							const TriRef acrossUp(d + 1, across.cx, across.cy, across.k);
							if(isFilled(t) && isFilled(acrossUp) && !isFilled(up) && !isFilled(across))
							{
								filled[id(up)] = true;
								++changed;
							}
							else if(!isFilled(t) && !isFilled(acrossUp) && isFilled(up) && isFilled(across))
							{
								filled[id(t)] = true;
								++changed;
							}
						}
					}
		}
		added += changed;
		if(changed == 0)
			break;
	}
	return filled;
}

// The plan distance from a point to a triangle of the plan (0 inside it).
double PlanDistance(const TriPlan& plan, const QPointF& q, const TriRef& t)
{
	QPointF c[3];
	plan.TriCorners(t, c);
	auto cross = [](const QPointF& a, const QPointF& b) { return a.x() * b.y() - a.y() * b.x(); };
	const double s0 = cross(c[1] - c[0], q - c[0]), s1 = cross(c[2] - c[1], q - c[1]), s2 = cross(c[0] - c[2], q - c[2]);
	if((s0 >= -1e-9 && s1 >= -1e-9 && s2 >= -1e-9) || (s0 <= 1e-9 && s1 <= 1e-9 && s2 <= 1e-9))
		return 0;
	double best = 1e30;
	for(int i = 0; i < 3; ++i)
	{
		const QPointF a = c[i], b = c[(i + 1) % 3], ab = b - a;
		const double len2 = QPointF::dotProduct(ab, ab);
		const double u = len2 > 0 ? qBound(0.0, QPointF::dotProduct(q - a, ab) / len2, 1.0) : 0.0;
		const QPointF d = q - (a + ab * u);
		best = qMin(best, sqrt(QPointF::dotProduct(d, d)));
	}
	return best;
}

// Slopes the hull between decks instead of stepping it (TriPlan::
// slopedDecks). Where the deck above is narrower, the ledge rises from the
// lower deck's edge to the top of the deck above; where the deck above is
// wider, its underside falls to the bottom of the deck below. Points only
// move outward from the solid, so every drawn triangle stays inside the
// hull (the user: the drawn hull is the least the hull may be). Returns
// false, leaving 'mesh' as it was, if the result is not a closed surface.
bool SlopeBetweenDecks(const TriPlan& plan, IndexedMesh& mesh, const std::function<double(int)>& zOf, int rows)
{
	const double eps = 1e-6;
	IndexedMesh out = mesh;
	// The plan distance from a point to the hull of a deck, and to what is
	// not hull of it (the grid's border counts as outside).
	auto distTo = [&plan](const QPointF& q, int deck, bool hull) {
		double best = 1e30;
		if(!hull)
		{
			const QPointF lo = plan.Corner(0, 0), hi = plan.Corner(plan.Width(), plan.Depth());
			best = qMin(qMin(q.x() - lo.x(), hi.x() - q.x()), qMin(q.y() - lo.y(), hi.y() - q.y()));
		}
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const TriRef t(deck, cx, cy, k);
					if(plan.Hull(t) != hull)
						continue;
					const QPointF c = plan.TriCentre(t);
					if(fabs(c.x() - q.x()) > best + plan.cellFeet || fabs(c.y() - q.y()) > best + plan.cellFeet)
						continue;
					best = qMin(best, PlanDistance(plan, q, t));
				}
		return best;
	};
	for(int d = 0; d + 1 < plan.Decks(); ++d)
	{
		const double zb = zOf((d + 1) * rows), up = zOf((d + 2) * rows) - zb, down = zb - zOf(d * rows);
		for(AuPoint& v : out.v)
		{
			if(v.z < zb - down - eps || v.z > zb + up + eps)
				continue;
			const QPointF q(v.x, v.y);
			const double inLower = distTo(q, d, true), outLower = distTo(q, d, false);
			const double inUpper = distTo(q, d + 1, true), outUpper = distTo(q, d + 1, false);
			// A ledge: on the lower deck's hull, not inside the upper's.
			if(v.z >= zb - eps && inLower < eps && outUpper < eps && (outLower > eps || inUpper > eps))
				v.z = qMax(v.z, zb + up * outLower / (outLower + inUpper));
			// An overhang: on the upper deck's hull, not inside the lower's.
			if(v.z <= zb + eps && inUpper < eps && outLower < eps && (outUpper > eps || inLower > eps))
				v.z = qMin(v.z, zb - down * outUpper / (outUpper + inLower));
		}
	}

	// Walls that were raised or lowered onto the slope have collapsed: weld
	// the points that now coincide and drop the faces with no area left.
	QHash<QString, int> byPosition;
	QVector<int> remap(out.v.size());
	QVector<AuPoint> welded;
	for(int i = 0; i < out.v.size(); ++i)
	{
		const AuPoint& p = out.v[i];
		const QString key = QString("%1,%2,%3").arg(qRound64(p.x * 1e4)).arg(qRound64(p.y * 1e4)).arg(qRound64(p.z * 1e4));
		QHash<QString, int>::const_iterator it = byPosition.constFind(key);
		if(it == byPosition.constEnd())
		{
			byPosition.insert(key, welded.size());
			remap[i] = welded.size();
			welded << p;
		}
		else
			remap[i] = it.value();
	}
	IndexedMesh result;
	result.v = welded;
	for(int fi = 0; fi < out.Faces(); ++fi)
	{
		const int a = remap[out.f[fi*3]], b = remap[out.f[fi*3 + 1]], c = remap[out.f[fi*3 + 2]];
		if(a == b || b == c || a == c)
			continue;
		if((welded[b] - welded[a]).Cross(welded[c] - welded[a]).Length() < 1e-9)
			continue;
		result.f << a << b << c;
		result.paint << out.paint[fi];
	}
	// Faces that now lie on each other back to back (a wall squeezed flat
	// between two slopes) cancel out.
	{
		QHash<QString, int> seen;
		QVector<bool> drop(result.Faces(), false);
		for(int fi = 0; fi < result.Faces(); ++fi)
		{
			int k[3] = { result.f[fi*3], result.f[fi*3 + 1], result.f[fi*3 + 2] };
			std::sort(k, k + 3);
			const QString key = QString("%1,%2,%3").arg(k[0]).arg(k[1]).arg(k[2]);
			QHash<QString, int>::const_iterator it = seen.constFind(key);
			if(it != seen.constEnd() && !drop[it.value()])
			{
				drop[fi] = drop[it.value()] = true;
				continue;
			}
			seen.insert(key, fi);
		}
		IndexedMesh kept;
		kept.v = result.v;
		for(int fi = 0; fi < result.Faces(); ++fi)
			if(!drop[fi])
			{
				kept.f << result.f[fi*3] << result.f[fi*3 + 1] << result.f[fi*3 + 2];
				kept.paint << result.paint[fi];
			}
		result = kept;
	}
	AuScTriList tris;
	for(int fi = 0; fi < result.Faces(); ++fi)
	{
		const AuPoint& a = result.v[result.f[fi*3]];
		const AuPoint& b = result.v[result.f[fi*3 + 1]];
		const AuPoint& c = result.v[result.f[fi*3 + 2]];
		AuPoint n = (b - a).Cross(c - a);
		n.Unit();
		tris.append(AuScTri(AuScPoint(a, 0.f, 0.f, n), AuScPoint(b, 0.f, 0.f, n), AuScPoint(c, 0.f, 0.f, n)));
	}
	if(!AuditSolidity(tris).Closed())
		return false;
	mesh = result;
	return true;
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

// The thinnest triangle the simplifier may leave: twice its area at least
// this times its longest side squared (about 1 in 20). The engine drops a
// triangle whose corners are on one line, but it keeps much thinner ones
// than this (the Cyclops frigate's hull, saved and read back, stayed closed
// down to 0.0005). What thin triangles do break is the cutting of doors and
// windows (AuScFaceList::JigCut): at 0.01 the same plan came out with a
// slightly different hull from run to run; at 0.05 it is the same each time.
double SliverRatio()
{
	return 0.05;
}

// Brings a closed hull down to 'targetFaces' by collapsing edges, the
// cheapest first by the quadric error of Garland and Heckbert, onto one of
// their own ends (a half-edge collapse: no vertex moves, so the hull keeps
// points of the shaped surface). What the flat merging cannot do, this can:
// a rounded hull has few flat areas, but most of it bends so gently that far
// fewer faces describe it.
//
// It keeps what the hull needs:
//  * closed: an edge is only collapsed when exactly two faces share it and
//    its ends have exactly two neighbours in common (the link condition), and
//    no face may turn over or become a sliver (the engine drops a triangle
//    whose corners are nearly on one line, which opens the hull);
//  * the paint: a vertex on the edge between two colours may only slide
//    along that edge, and the edges between colours carry extra planes in
//    their quadrics, so they stay straight where they were;
//  * the rooms: every vertex taken away is kept as a witness on the face over
//    it. No witness may end up further outside the new surface (the surface
//    moved in) than 'inward' allows for it, nor further inside (the surface
//    moved out) or further from it than the move limit.
// The move limits are tried in turn: when no edge can go within one, the
// next (larger) one is taken, and every edge is tried again. The witnesses
// keep the original surface, so a later limit is still measured from it.
// Returns the faces left; 'usedMove' is the limit it ended on.
// 'planeN' and 'planeD', when given, are the plane each face's quadric is
// made of instead of its own (the panel's, for flat panels).
// With 'regions' above 0 the paint index holds paint * regions + panel: the
// borders between panels of one paint may straighten (the panels' planes
// lead them to where the planes meet), only paint edges are held in place.
int DecimateHull(IndexedMesh& mesh, int targetFaces, const QVector<double>& inward, const QVector<double>& moves,
		double* usedMove = 0, const QVector<AuPoint>* planeN = 0, const QVector<double>* planeD = 0, int regions = 0)
{
	int stage = 0;
	double maxMove = moves.isEmpty() ? 0.0 : moves.first();
	if(usedMove)
		*usedMove = maxMove;
	const int nv = mesh.v.size();
	const int nf = mesh.Faces();
	if(nf <= targetFaces)
		return nf;

	// A symmetric 4x4 quadric in ten numbers.
	struct Quadric
	{
		double q[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		void AddPlane(const AuPoint& n, double d, double w)
		{
			const double a = n.x, b = n.y, c = n.z;
			q[0] += w*a*a; q[1] += w*a*b; q[2] += w*a*c; q[3] += w*a*d;
			q[4] += w*b*b; q[5] += w*b*c; q[6] += w*b*d;
			q[7] += w*c*c; q[8] += w*c*d; q[9] += w*d*d;
		}
		void Add(const Quadric& o) { for(int i = 0; i < 10; ++i) q[i] += o.q[i]; }
		double At(const AuPoint& p) const
		{
			const double x = p.x, y = p.y, z = p.z;
			return q[0]*x*x + 2*q[1]*x*y + 2*q[2]*x*z + 2*q[3]*x
					+ q[4]*y*y + 2*q[5]*y*z + 2*q[6]*y
					+ q[7]*z*z + 2*q[8]*z + q[9];
		}
	};

	QVector<bool> alive(nf, true);
	QVector<bool> gone(nv, false);
	QVector<int> stamp(nv, 0);
	QVector<QVector<int> > vf(nv);
	for(int fi = 0; fi < nf; ++fi)
		for(int j = 0; j < 3; ++j)
			vf[mesh.f[fi*3 + j]] << fi;
	auto facesOf = [&](int v) -> const QVector<int>& {
		QVector<int>& list = vf[v];
		QVector<int> live;
		live.reserve(list.size());
		for(int fi : list)
			if(alive[fi])
				live << fi;
		list = live;
		return list;
	};
	auto has = [&mesh](int fi, int v) {
		return mesh.f[fi*3] == v || mesh.f[fi*3 + 1] == v || mesh.f[fi*3 + 2] == v;
	};
	auto rawNormal = [&mesh](int a, int b, int c) {
		return (mesh.v[b] - mesh.v[a]).Cross(mesh.v[c] - mesh.v[a]);
	};

	// The quadrics: each face's plane, weighted by its area, and for each edge
	// between two colours a plane standing up through it, weighted heavily.
	QVector<Quadric> quadric(nv);
	for(int fi = 0; fi < nf; ++fi)
	{
		const int* idx = &mesh.f[fi*3];
		AuPoint n = rawNormal(idx[0], idx[1], idx[2]);
		const double area2 = n.Length();
		if(area2 < 1e-12)
			continue;
		n = n / area2;
		double d = -n.Dot(mesh.v[idx[0]]);
		if(planeN && planeD)
		{
			n = (*planeN)[fi];
			d = (*planeD)[fi];
		}
		for(int j = 0; j < 3; ++j)
			quadric[idx[j]].AddPlane(n, d, area2 / 2);
	}
	{
		QHash<qint64, int> firstFace;
		for(int fi = 0; fi < nf; ++fi)
			for(int j = 0; j < 3; ++j)
			{
				const int a = mesh.f[fi*3 + j], b = mesh.f[fi*3 + (j + 1) % 3];
				const qint64 key = qint64(qMin(a, b)) * nv + qMax(a, b);
				QHash<qint64, int>::const_iterator it = firstFace.constFind(key);
				if(it == firstFace.constEnd())
				{
					firstFace.insert(key, fi);
					continue;
				}
				if(mesh.paint[it.value()] == mesh.paint[fi] ||
						(regions > 0 && mesh.paint[it.value()] / regions == mesh.paint[fi] / regions))
					continue;
				AuPoint e = mesh.v[b] - mesh.v[a];
				const double len = e.Length();
				if(len < 1e-9)
					continue;
				AuPoint n = e.Cross(mesh.Normal(fi));
				if(n.Length() < 1e-9)
					continue;
				n = n / n.Length();
				const double d = -n.Dot(mesh.v[a]);
				quadric[a].AddPlane(n, d, 10 * len * len);
				quadric[b].AddPlane(n, d, 10 * len * len);
			}
	}

	// The witnesses: the vertices taken away, on the face over them.
	struct Witness { AuPoint p; double in; };
	QVector<Witness> witnesses;
	QVector<QVector<int> > onFace(nf);

	// Closest point on triangle abc to p (Ericson, Real-Time Collision Detection 5.1.5).
	auto closest = [](const AuPoint& p, const AuPoint& a, const AuPoint& b, const AuPoint& c) {
		const AuPoint ab = b - a, ac = c - a, ap = p - a;
		const double d1 = ab.Dot(ap), d2 = ac.Dot(ap);
		if(d1 <= 0 && d2 <= 0) return a;
		const AuPoint bp = p - b;
		const double d3 = ab.Dot(bp), d4 = ac.Dot(bp);
		if(d3 >= 0 && d4 <= d3) return b;
		const double vc = d1*d4 - d3*d2;
		if(vc <= 0 && d1 >= 0 && d3 <= 0) return a + ab * (d1 / (d1 - d3));
		const AuPoint cp = p - c;
		const double d5 = ab.Dot(cp), d6 = ac.Dot(cp);
		if(d6 >= 0 && d5 <= d6) return c;
		const double vb = d5*d2 - d1*d6;
		if(vb <= 0 && d2 >= 0 && d6 <= 0) return a + ac * (d2 / (d2 - d6));
		const double va = d3*d6 - d5*d4;
		if(va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
			return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
		const double denom = 1.0 / (va + vb + vc);
		return a + ab * (vb * denom) + ac * (vc * denom);
	};

	struct Candidate
	{
		double	cost;
		int		v, u, sv, su;
		bool operator<(const Candidate& o) const { return cost > o.cost; }		// Cheapest on top.
	};
	std::priority_queue<Candidate> queue;
	auto push = [&](int v, int u) {
		if(gone[v] || gone[u] || v == u)
			return;
		Quadric q = quadric[v];
		q.Add(quadric[u]);
		queue.push(Candidate{ qMax(0.0, q.At(mesh.v[u])), v, u, stamp[v], stamp[u] });
	};
	// In order, so that the same hull always simplifies the same way (a
	// QSet's order changes with Qt's hash seed from run to run).
	auto neighbours = [&](int v) {
		QVector<int> ring;
		for(int fi : facesOf(v))
			for(int j = 0; j < 3; ++j)
				if(mesh.f[fi*3 + j] != v)
					ring << mesh.f[fi*3 + j];
		std::sort(ring.begin(), ring.end());
		ring.erase(std::unique(ring.begin(), ring.end()), ring.end());
		return ring;
	};
	for(int v = 0; v < nv; ++v)
		for(int u : neighbours(v))
			push(v, u);

	int faces = nf;
	int dbg[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	struct Placement { int witness; int face; };
	for(;;)
	{
	if(queue.empty() && faces > targetFaces && stage + 1 < moves.size())
	{
		// Stuck at this limit: the next one, and every edge again.
		maxMove = moves[++stage];
		if(usedMove)
			*usedMove = maxMove;
		for(int v = 0; v < nv; ++v)
			if(!gone[v])
				for(int u : neighbours(v))
					push(v, u);
	}
	if(faces <= targetFaces || queue.empty())
		break;
	{
		const Candidate c = queue.top();
		queue.pop();
		const int v = c.v, u = c.u;
		if(gone[v] || gone[u] || c.sv != stamp[v] || c.su != stamp[u])
			continue;
		const QVector<int> star = facesOf(v);
		if(star.size() < 3)
			continue;

		// The two faces on the edge, and the link condition.
		QVector<int> onEdge;
		for(int fi : star)
			if(has(fi, u))
				onEdge << fi;
		if(onEdge.size() != 2)
			{ ++dbg[0]; continue; }
		const QVector<int> ringV = neighbours(v), ringU = neighbours(u);
		int common = 0;
		for(int w : ringV)
			if(w != u && std::binary_search(ringU.begin(), ringU.end(), w))
				++common;
		if(common != 2)
			{ ++dbg[1]; continue; }

		// The paint: within one colour anything goes; on the edge between
		// colours only along that edge, and only where it is not a corner of
		// three colours or a bend in the colour edge.
		bool oneColour = true;
		for(int fi : star)
			if(mesh.paint[fi] != mesh.paint[star.first()])
				oneColour = false;
		if(!oneColour)
		{
			if(mesh.paint[onEdge[0]] == mesh.paint[onEdge[1]])
				{ ++dbg[2]; continue; }
			QSet<int> colours;
			for(int fi : star)
				colours.insert(mesh.paint[fi]);
			if(colours.size() != 2)
				{ ++dbg[3]; continue; }
			int colourEdges = 0;
			for(int w : ringV)
			{
				int a = -1, b = -1;
				for(int fi : star)
					if(has(fi, w))
						(a < 0 ? a : b) = fi;
				if(a >= 0 && b >= 0 && mesh.paint[a] != mesh.paint[b])
					++colourEdges;
			}
			if(colourEdges != 2)
				{ ++dbg[4]; continue; }
		}

		// The faces after: nothing turns over, nothing becomes a sliver.
		bool ok = true;
		QVector<int> after;
		for(int fi : star)
		{
			if(onEdge.contains(fi))
				continue;
			int idx[3] = { mesh.f[fi*3], mesh.f[fi*3 + 1], mesh.f[fi*3 + 2] };
			for(int j = 0; j < 3; ++j)
				if(idx[j] == v)
					idx[j] = u;
			const AuPoint n = rawNormal(idx[0], idx[1], idx[2]);
			double longest = 0;
			for(int j = 0; j < 3; ++j)
				longest = qMax(longest, (mesh.v[idx[j]] - mesh.v[idx[(j + 1) % 3]]).Length());
			if(n.Length() < SliverRatio() * longest * longest || (n / n.Length()).Dot(mesh.Normal(fi)) < 0.5 ||
					// A panel's faces face its way, or no further from it than before.
					(planeN && (n / n.Length()).Dot((*planeN)[fi]) < qMin(0.7, mesh.Normal(fi).Dot((*planeN)[fi]) - 0.05)))
			{
				ok = false;
				break;
			}
			after << fi;
		}
		if(!ok || after.isEmpty())
			{ ++dbg[5]; continue; }

		// The witnesses of the faces round v, and v itself, against the
		// faces after.
		auto newCorners = [&](int fi, AuPoint p[3]) {
			for(int j = 0; j < 3; ++j)
			{
				const int idx = mesh.f[fi*3 + j];
				p[j] = mesh.v[idx == v ? u : idx];
			}
		};
		QVector<int> toPlace;
		for(int fi : star)
			toPlace += onFace[fi];
		const int own = witnesses.size();
		witnesses << Witness{ mesh.v[v], inward[v] };
		toPlace << own;
		QVector<Placement> placed;
		for(int w : toPlace)
		{
			const Witness& wit = witnesses[w];
			double best = 1e300, signedBest = 0;
			int bestFace = -1;
			for(int fi : after)
			{
				AuPoint p[3];
				newCorners(fi, p);
				const AuPoint q = closest(wit.p, p[0], p[1], p[2]);
				const double dist = (wit.p - q).Length();
				if(dist < best)
				{
					best = dist;
					bestFace = fi;
					AuPoint n = (p[1] - p[0]).Cross(p[2] - p[0]);
					n = n / n.Length();
					signedBest = (wit.p - p[0]).Dot(n);
				}
			}
			// Outside the new surface: it moved in there; inside: it moved out.
			if(best > maxMove + 1e-9 || (signedBest > 0 && signedBest > qMin(wit.in, maxMove) + 1e-9))
			{
				ok = false;
				break;
			}
			placed << Placement{ w, bestFace };
		}
		if(!ok)
		{
			++dbg[6];
			witnesses.removeLast();
			continue;
		}

		// Collapse.
		for(int fi : onEdge)
		{
			alive[fi] = false;
			onFace[fi].clear();
			--faces;
		}
		for(int fi : after)
		{
			onFace[fi].clear();
			for(int j = 0; j < 3; ++j)
				if(mesh.f[fi*3 + j] == v)
					mesh.f[fi*3 + j] = u;
			vf[u] << fi;
		}
		for(const Placement& p : placed)
			onFace[p.face] << p.witness;
		vf[v].clear();
		gone[v] = true;
		quadric[u].Add(quadric[v]);
		++stamp[u];
		const QVector<int> ring = neighbours(u);
		for(int w : ring)
			++stamp[w];
		// Every edge of the vertices whose stamps changed goes back on the
		// queue, with its new cost (the stamps dropped the old entries).
		for(int w : ring)
		{
			push(u, w);
			for(int x : neighbours(w))
			{
				push(w, x);
				push(x, w);
			}
		}
	}
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
	if(!qgetenv("SHIPGEN_DEBUG").isEmpty())
		fprintf(stderr, "decimate: faces %d; rejected edge %d link %d paint-across %d colours %d colour-edges %d flip/sliver %d witness %d\n",
				mesh.Faces(), dbg[0], dbg[1], dbg[2], dbg[3], dbg[4], dbg[5], dbg[6]);
	return mesh.Faces();
}

// Groups the faces of a hull into about 'panels' panels that each face one
// way: variational shape approximation (Cohen-Steiner, Alliez and Desbrun,
// 2004) with its L2,1 measure, the area of a face times how far its normal
// is from the panel's. A panel grows from its seed face over neighbouring
// faces of the same paint, the best fitting first; then each panel's
// direction and middle are fitted to its faces and its seed moved to its
// best fitting face, and again. Panels are added where the fit is worst
// until there are enough. Every piece of one paint gets at least one.
// Returns the number of panels; 'region' is each face's panel, 'pn' and 'pd'
// each panel's plane (n.p + d = 0).
int PanelPartition(const IndexedMesh& mesh, int panels, QVector<int>& region, QVector<AuPoint>& pn, QVector<double>& pd)
{
	const int nf = mesh.Faces();
	QVector<double> area(nf);
	QVector<AuPoint> normal(nf), middle(nf);
	for(int fi = 0; fi < nf; ++fi)
	{
		const AuPoint& a = mesh.v[mesh.f[fi*3]];
		const AuPoint& b = mesh.v[mesh.f[fi*3 + 1]];
		const AuPoint& c = mesh.v[mesh.f[fi*3 + 2]];
		const AuPoint n = (b - a).Cross(c - a);
		area[fi] = n.Length() / 2;
		normal[fi] = mesh.Normal(fi);
		middle[fi] = (a + b + c) / 3.0;
	}
	// Neighbours across edges, within one paint.
	QVector<QVector<int> > next(nf);
	{
		QHash<qint64, int> firstFace;
		const qint64 nv = mesh.v.size();
		for(int fi = 0; fi < nf; ++fi)
			for(int j = 0; j < 3; ++j)
			{
				const int a = mesh.f[fi*3 + j], b = mesh.f[fi*3 + (j + 1) % 3];
				const qint64 key = qint64(qMin(a, b)) * nv + qMax(a, b);
				QHash<qint64, int>::iterator it = firstFace.find(key);
				if(it == firstFace.end())
					firstFace.insert(key, fi);
				else if(mesh.paint[it.value()] == mesh.paint[fi])
				{
					next[fi] << it.value();
					next[it.value()] << fi;
				}
			}
	}
	auto error = [&](int fi, const AuPoint& n) {
		const AuPoint d = normal[fi] - n;
		return area[fi] * d.Dot(d);
	};

	// A seed in every piece of one paint: its biggest face.
	QVector<int> seeds;
	{
		QVector<bool> seen(nf, false);
		for(int start = 0; start < nf; ++start)
		{
			if(seen[start])
				continue;
			int biggest = start;
			QVector<int> stack;
			stack << start;
			seen[start] = true;
			while(!stack.isEmpty())
			{
				const int fi = stack.takeLast();
				if(area[fi] > area[biggest])
					biggest = fi;
				for(int n : next[fi])
					if(!seen[n])
					{
						seen[n] = true;
						stack << n;
					}
			}
			seeds << biggest;
		}
	}
	QVector<AuPoint> dir;
	for(int s : seeds)
		dir << normal[s];

	struct Entry
	{
		double	e;
		int		face, proxy;
		bool operator<(const Entry& o) const { return e > o.e; }
	};
	auto grow = [&]() {
		region.fill(-1, nf);
		std::priority_queue<Entry> queue;
		for(int p = 0; p < seeds.size(); ++p)
		{
			region[seeds[p]] = p;
			for(int n : next[seeds[p]])
				queue.push(Entry{ error(n, dir[p]), n, p });
		}
		while(!queue.empty())
		{
			const Entry e = queue.top();
			queue.pop();
			if(region[e.face] >= 0)
				continue;
			region[e.face] = e.proxy;
			for(int n : next[e.face])
				if(region[n] < 0)
					queue.push(Entry{ error(n, dir[e.proxy]), n, e.proxy });
		}
	};
	auto fit = [&]() {
		QVector<AuPoint> sum(seeds.size(), AuPoint(0, 0, 0));
		for(int fi = 0; fi < nf; ++fi)
			if(region[fi] >= 0)
				sum[region[fi]] = sum[region[fi]] + normal[fi] * area[fi];
		for(int p = 0; p < seeds.size(); ++p)
			if(sum[p].Length() > 1e-12)
				dir[p] = sum[p] / sum[p].Length();
		// Each seed moves to the face of its panel that fits it best.
		QVector<double> best(seeds.size(), 1e300);
		for(int fi = 0; fi < nf; ++fi)
		{
			const int p = region[fi];
			if(p < 0)
				continue;
			const double e = error(fi, dir[p]) / qMax(area[fi], 1e-12);
			if(e < best[p])
			{
				best[p] = e;
				seeds[p] = fi;
			}
		}
	};
	auto lloyd = [&](int rounds) {
		for(int i = 0; i < rounds; ++i)
		{
			grow();
			fit();
		}
		grow();
	};

	lloyd(3);
	while(seeds.size() < panels)
	{
		// New panels where the fit is worst: in each of the worst panels, at
		// its worst face; up to a quarter more at a time.
		QVector<double> total(seeds.size(), 0.0);
		QVector<int> worst(seeds.size(), -1);
		for(int fi = 0; fi < nf; ++fi)
		{
			const int p = region[fi];
			if(p < 0)
				continue;
			const double e = error(fi, dir[p]);
			total[p] += e;
			if(worst[p] < 0 || e > error(worst[p], dir[p]))
				worst[p] = fi;
		}
		QVector<int> order(seeds.size());
		for(int p = 0; p < order.size(); ++p)
			order[p] = p;
		std::sort(order.begin(), order.end(), [&total](int a, int b) { return total[a] > total[b]; });
		const int add = qMin(panels - seeds.size(), qMax(1, seeds.size() / 4));
		int added = 0;
		for(int p : order)
		{
			if(added >= add || total[p] <= 1e-12 || worst[p] < 0 || worst[p] == seeds[p])
				break;
			seeds << worst[p];
			dir << normal[worst[p]];
			++added;
		}
		if(!added)
			break;		// Every panel is already flat.
		lloyd(2);
	}
	lloyd(6);

	// Each panel's plane through the area-weighted middle of its faces.
	pn = dir;
	pd.fill(0.0, seeds.size());
	QVector<AuPoint> centre(seeds.size(), AuPoint(0, 0, 0));
	QVector<double> weight(seeds.size(), 0.0);
	for(int fi = 0; fi < nf; ++fi)
	{
		if(region[fi] < 0)
			region[fi] = 0;		// Cannot happen: every piece has a seed.
		centre[region[fi]] = centre[region[fi]] + middle[fi] * area[fi];
		weight[region[fi]] += area[fi];
	}
	for(int p = 0; p < seeds.size(); ++p)
		pd[p] = weight[p] > 0 ? -pn[p].Dot(centre[p] / weight[p]) : 0.0;
	return seeds.size();
}

// Presses the panels of a hull flat: each vertex goes onto the plane of its
// panel, or where the planes of the panels round it meet (least squares,
// with a little pull back to where it was, so that two nearly parallel
// planes do not send it far). It moves at most 'maxMove', and in at most what
// 'inward' allows for it (the rooms); a move that would turn a face over or
// make a sliver is taken back. 'paint' holds paint * regions + panel.
void FlattenPanels(IndexedMesh& mesh, int regions, const QVector<AuPoint>& pn, const QVector<double>& pd,
		const QVector<double>& inward, double maxMove)
{
	const int nv = mesh.v.size(), nf = mesh.Faces();
	QVector<QSet<int> > around(nv);
	QVector<AuPoint> vnormal(nv, AuPoint(0, 0, 0));
	for(int fi = 0; fi < nf; ++fi)
	{
		const int* idx = &mesh.f[fi*3];
		const AuPoint n = (mesh.v[idx[1]] - mesh.v[idx[0]]).Cross(mesh.v[idx[2]] - mesh.v[idx[0]]);
		for(int j = 0; j < 3; ++j)
		{
			around[idx[j]].insert(mesh.paint[fi] % regions);
			vnormal[idx[j]] = vnormal[idx[j]] + n;
		}
	}
	const QVector<AuPoint> original = mesh.v;
	QVector<AuPoint> oldNormal(nf);
	for(int fi = 0; fi < nf; ++fi)
		oldNormal[fi] = mesh.Normal(fi);
	for(int i = 0; i < nv; ++i)
	{
		if(around[i].isEmpty())
			continue;
		// (sum n n^T + eps I) x = sum n (-d) + eps p; eps pulls a corner of
		// nearly parallel panels back towards where it was, instead of far
		// along the line where they (almost) meet.
		const double eps = 0.1;
		double m[3][3] = { { eps, 0, 0 }, { 0, eps, 0 }, { 0, 0, eps } };
		const AuPoint& p = original[i];
		double r[3] = { eps * p.x, eps * p.y, eps * p.z };
		QList<int> panelsHere = around[i].values();
		std::sort(panelsHere.begin(), panelsHere.end());		// The same sum every run.
		for(int g : panelsHere)
		{
			const double n[3] = { pn[g].x, pn[g].y, pn[g].z };
			for(int a = 0; a < 3; ++a)
			{
				for(int b = 0; b < 3; ++b)
					m[a][b] += n[a] * n[b];
				r[a] += n[a] * -pd[g];
			}
		}
		const double det = m[0][0]*(m[1][1]*m[2][2] - m[1][2]*m[2][1]) - m[0][1]*(m[1][0]*m[2][2] - m[1][2]*m[2][0])
				+ m[0][2]*(m[1][0]*m[2][1] - m[1][1]*m[2][0]);
		if(fabs(det) < 1e-12)
			continue;
		auto solve = [&](int col) {
			double t[3][3];
			for(int a = 0; a < 3; ++a)
				for(int b = 0; b < 3; ++b)
					t[a][b] = b == col ? r[a] : m[a][b];
			return (t[0][0]*(t[1][1]*t[2][2] - t[1][2]*t[2][1]) - t[0][1]*(t[1][0]*t[2][2] - t[1][2]*t[2][0])
					+ t[0][2]*(t[1][0]*t[2][1] - t[1][1]*t[2][0])) / det;
		};
		AuPoint move = AuPoint(solve(0), solve(1), solve(2)) - p;
		const double length = move.Length();
		if(length > maxMove)
			move = move * (maxMove / length);
		// Inwards no further than the rooms allow.
		AuPoint n = vnormal[i];
		if(n.Length() > 1e-12)
		{
			n = n / n.Length();
			const double in = -move.Dot(n);
			if(in > inward[i])
				move = move + n * (in - inward[i]);
		}
		mesh.v[i] = p + move;
	}
	// Take back what turns a face over or makes a sliver, until nothing does.
	for(int pass = 0; pass < 8; ++pass)
	{
		int undone = 0;
		for(int fi = 0; fi < nf; ++fi)
		{
			const int* idx = &mesh.f[fi*3];
			const AuPoint n = (mesh.v[idx[1]] - mesh.v[idx[0]]).Cross(mesh.v[idx[2]] - mesh.v[idx[0]]);
			double longest = 0;
			for(int j = 0; j < 3; ++j)
				longest = qMax(longest, (mesh.v[idx[j]] - mesh.v[idx[(j + 1) % 3]]).Length());
			if(n.Length() >= SliverRatio() * longest * longest && (n / n.Length()).Dot(oldNormal[fi]) > 0.3 &&
					(n / n.Length()).Dot(pn[mesh.paint[fi] % regions]) >=
						qMin(0.7, oldNormal[fi].Dot(pn[mesh.paint[fi] % regions]) - 0.05))
				continue;
			for(int j = 0; j < 3; ++j)
				if(!(mesh.v[idx[j]] == original[idx[j]]))
				{
					mesh.v[idx[j]] = original[idx[j]];
					++undone;
				}
		}
		if(!undone)
			break;
	}
	if(!qgetenv("SHIPGEN_DEBUG").isEmpty())
	{
		int moved = 0;
		for(int i = 0; i < nv; ++i)
			if(!(mesh.v[i] == original[i]))
				++moved;
		fprintf(stderr, "flatten: %d of %d vertices moved\n", moved, nv);
	}
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

// Pushes the surface out by the skin with mitred corners (the angular hull
// style): every face moves out by exactly the skin, and every vertex goes
// to where the planes of its faces meet again (least squares over its
// distinct face planes, pulled gently towards the plain push for the
// directions they leave free). A flat area stays flat and an edge stays a
// sharp edge. A vertex moves no more than three skins.
void OffsetMitred(IndexedMesh& mesh, double skin)
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
				if(m.Dot(n) > 0.9999)
				{
					seen = true;
					break;
				}
			if(!seen)
				list << n;
		}
	}
	auto det3 = [](const double m[3][3]) {
		return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
			+ m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
	};
	QVector<AuPoint> out(nv);
	for(int i = 0; i < nv; ++i)
	{
		const AuPoint v = mesh.v[i];
		AuPoint avg(0, 0, 0);
		for(const AuPoint& m : normals[i])
			avg = avg + m;
		if(avg.Length() < 1e-9)
		{
			out[i] = v;
			continue;
		}
		avg = avg / avg.Length();
		double minDot = 1.0;
		for(const AuPoint& m : normals[i])
			minDot = qMin(minDot, avg.Dot(m));
		const AuPoint guess = v + avg * (skin / qMax(0.4, minDot));
		// Normal equations of: sum (n.x - (n.v + skin))^2 + w |x - guess|^2.
		const double w = 1e-4;
		double A[3][3] = { { w, 0, 0 }, { 0, w, 0 }, { 0, 0, w } };
		double b[3] = { w * guess.x, w * guess.y, w * guess.z };
		for(const AuPoint& m : normals[i])
		{
			const double nn[3] = { m.x, m.y, m.z };
			const double d = m.Dot(v) + skin;
			for(int r = 0; r < 3; ++r)
			{
				for(int c = 0; c < 3; ++c)
					A[r][c] += nn[r] * nn[c];
				b[r] += nn[r] * d;
			}
		}
		const double det = det3(A);
		AuPoint x = guess;
		if(fabs(det) > 1e-12)
		{
			double r[3];
			for(int c = 0; c < 3; ++c)
			{
				double M[3][3];
				for(int rr = 0; rr < 3; ++rr)
					for(int cc = 0; cc < 3; ++cc)
						M[rr][cc] = cc == c ? b[rr] : A[rr][cc];
				r[c] = det3(M) / det;
			}
			x = AuPoint(r[0], r[1], r[2]);
		}
		const AuPoint move = x - v;
		if(move.Length() > 3 * skin)
			x = v + move * (3 * skin / move.Length());
		out[i] = x;
	}
	mesh.v = out;
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

// How far the design has to move so that its origin lies within the hull's
// extents on every axis: the engine reports "CG outside design extents"
// otherwise (auscreport.cpp), and the game hung loading such a design (the
// Omega, whose lowest decks were empty, so its hull began 17 ft above the
// origin). Onto the nearest side, with a little room (a tenth of the size,
// at most two feet), so the origin is inside, not on the skin.
AuPoint OriginShift(const AuScDesign& design)
{
	AuPoint mn(1e30, 1e30, 1e30), mx(-1e30, -1e30, -1e30);
	for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
	{
		const AuScPart* part = it.value();
		if(!part || !part->IsHullMesh())
			continue;
		QSet<quint32> ids;
		part->VertexIds(ids);
		for(QSet<quint32>::const_iterator v = ids.constBegin(); v != ids.constEnd(); ++v)
		{
			const AuPoint p = part->VertexLoc(*v);
			mn = mn.Min(p);
			mx = mx.Max(p);
		}
	}
	if(mn.x > mx.x)
		return AuPoint(0, 0, 0);
	auto shiftFor = [](double lo, double hi) {
		const double margin = qMin(2.0, (hi - lo) * 0.1);
		if(lo > 0)
			return -(lo + margin);
		if(hi < 0)
			return -(hi - margin);
		return 0.0;
	};
	return AuPoint(shiftFor(mn.x, mx.x), shiftFor(mn.y, mx.y), shiftFor(mn.z, mx.z));
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

// The inside of a model found slice by slice, which needs no closed surface.
//
// The model is cut into thin horizontal slabs. In each slab every square of
// a fine grid that a triangle of the model passes through is marked as
// surface; then the outside is flooded in from the edge of the grid, round
// the marks. What the flood cannot reach is inside. Models from the internet
// are mostly open panels laid against each other, which the closed-solid test
// throws away; here a gap only matters if the outside can flow through it
// sideways, and gaps narrower than 'seal' squares are closed off first (the
// marks are grown by that much for the flood, and the outside grown back
// after). A slab is flooded on its own, so a missing top or bottom does not
// matter at all. Every triangle is visited once, so a model of millions of
// faces needs no thinning.
class SliceSolid
{
public:
	// The grid covers mn to mx with squares of 'step' and slabs as thick.
	// Returns false when stopped.
	// 'progress' is told the phase (0 marking the faces, 1 flooding the
	// slices), how far it has got and how far it goes; false stops it.
	bool	Build(const AuScTriList& tris, const AuPoint& mn, const AuPoint& mx, double step, int seal,
			const std::function<bool(int phase, int done, int total)>& progress);
	bool	Inside(const AuPoint& p) const
	{
		const int ix = int(floor((p.x - m_origin.x) / m_step));
		const int iy = int(floor((p.y - m_origin.y) / m_step));
		const int iz = int(floor((p.z - m_origin.z) / m_step));
		if(ix < 0 || iy < 0 || iz < 0 || ix >= m_nx || iy >= m_ny || iz >= m_nz)
			return false;
		return m_cells[(qint64(iz) * m_ny + iy) * m_nx + ix] != CELL_OUT;
	}
	double	Step() const { return m_step; }
	// The box the grid covers.
	void	Extents(AuPoint& lo, AuPoint& hi) const
	{
		lo = m_origin;
		hi = m_origin + AuPoint(m_nx, m_ny, m_nz) * m_step;
	}
	qint64	SurfaceCells() const { return m_surface; }
	qint64	InsideCells() const { return m_inside; }

private:
	enum { CELL_OUT = 0, CELL_SURFACE = 1, CELL_IN = 2 };	// Not OUT, IN: the Windows headers define those as nothing.
	void	Mark(int iz, const QVector<QPointF>& polygon);
	void	Flood(int iz, int seal);

	AuPoint				m_origin;
	double				m_step = 1;
	int					m_nx = 0, m_ny = 0, m_nz = 0;
	QVector<quint8>		m_cells;
	qint64				m_surface = 0, m_inside = 0;
};

// Marks every square of slab 'iz' that the convex polygon (a triangle cut to
// the slab, seen from above; it may have collapsed to a segment or a point)
// touches. Each row of squares takes the x range of the polygon's edges
// inside the row's strip, which is exact for a convex polygon.
void SliceSolid::Mark(int iz, const QVector<QPointF>& polygon)
{
	if(polygon.isEmpty())
		return;
	double ylo = polygon[0].y(), yhi = ylo;
	for(const QPointF& p : polygon)
	{
		ylo = qMin(ylo, p.y());
		yhi = qMax(yhi, p.y());
	}
	const int r0 = qMax(0, int(floor((ylo - m_origin.y) / m_step)));
	const int r1 = qMin(m_ny - 1, int(floor((yhi - m_origin.y) / m_step)));
	quint8* slab = m_cells.data() + qint64(iz) * m_ny * m_nx;
	for(int iy = r0; iy <= r1; ++iy)
	{
		const double y0 = m_origin.y + iy * m_step, y1 = y0 + m_step;
		double xlo = 1e300, xhi = -1e300;
		auto take = [&](double x) { xlo = qMin(xlo, x); xhi = qMax(xhi, x); };
		for(int i = 0; i < polygon.size(); ++i)
		{
			const QPointF& a = polygon[i];
			const QPointF& b = polygon[(i + 1) % polygon.size()];
			if(a.y() >= y0 && a.y() <= y1)
				take(a.x());
			const double dy = b.y() - a.y();
			if(fabs(dy) < 1e-12)
				continue;
			for(double yy : { y0, y1 })
			{
				const double t = (yy - a.y()) / dy;
				if(t > 0 && t < 1)
					take(a.x() + (b.x() - a.x()) * t);
			}
		}
		if(xlo > xhi)
			continue;
		const int c0 = qMax(0, int(floor((xlo - m_origin.x) / m_step)));
		const int c1 = qMin(m_nx - 1, int(floor((xhi - m_origin.x) / m_step)));
		quint8* row = slab + qint64(iy) * m_nx;
		for(int ix = c0; ix <= c1; ++ix)
			row[ix] = CELL_SURFACE;
	}
}

// Floods the outside of slab 'iz' in from the edge of the grid; every square
// it does not reach and that is not surface becomes inside.
void SliceSolid::Flood(int iz, int seal)
{
	const int nx = m_nx, ny = m_ny;
	quint8* slab = m_cells.data() + qint64(iz) * ny * nx;
	// Grows a mask by 'r' squares (a square of 2r+1 round each), a row pass
	// and a column pass.
	auto grow = [nx, ny](QVector<quint8>& mask, int r) {
		if(r <= 0)
			return;
		QVector<quint8> rows(mask.size(), 0);
		for(int y = 0; y < ny; ++y)
		{
			const quint8* in = mask.constData() + qint64(y) * nx;
			quint8* out = rows.data() + qint64(y) * nx;
			int last = -1000000;
			for(int x = 0; x < nx; ++x)		// Distance to the last mark on the left...
			{
				if(in[x])
					last = x;
				if(x - last <= r)
					out[x] = 1;
			}
			last = 1000000;
			for(int x = nx - 1; x >= 0; --x)	// ...and on the right.
			{
				if(in[x])
					last = x;
				if(last - x <= r)
					out[x] = 1;
			}
		}
		for(int x = 0; x < nx; ++x)
		{
			int last = -1000000;
			for(int y = 0; y < ny; ++y)
			{
				if(rows[qint64(y) * nx + x])
					last = y;
				if(y - last <= r)
					mask[qint64(y) * nx + x] = 1;
			}
			last = 1000000;
			for(int y = ny - 1; y >= 0; --y)
			{
				if(rows[qint64(y) * nx + x])
					last = y;
				if(last - y <= r)
					mask[qint64(y) * nx + x] = 1;
			}
		}
	};
	QVector<quint8> wall(nx * ny);
	for(int i = 0; i < nx * ny; ++i)
		wall[i] = slab[i] == CELL_SURFACE;
	grow(wall, seal);
	// The flood, from every square on the edge of the grid (which is kept
	// clear of the model by the padding in Build).
	QVector<quint8> outside(nx * ny, 0);
	QVector<int> queue;
	queue.reserve(nx * ny / 4);
	auto push = [&](int x, int y) {
		const int i = y * nx + x;
		if(!wall[i] && !outside[i])
		{
			outside[i] = 1;
			queue << i;
		}
	};
	for(int x = 0; x < nx; ++x)
	{
		push(x, 0);
		push(x, ny - 1);
	}
	for(int y = 0; y < ny; ++y)
	{
		push(0, y);
		push(nx - 1, y);
	}
	for(int q = 0; q < queue.size(); ++q)
	{
		const int x = queue[q] % nx, y = queue[q] / nx;
		if(x > 0)		push(x - 1, y);
		if(x < nx - 1)	push(x + 1, y);
		if(y > 0)		push(x, y - 1);
		if(y < ny - 1)	push(x, y + 1);
	}
	// The outside grown back by the seal, so the sealing only closes gaps and
	// does not thicken the model.
	grow(outside, seal);
	for(int i = 0; i < nx * ny; ++i)
	{
		if(slab[i] == CELL_SURFACE)
			++m_surface;
		else if(!outside[i])
		{
			slab[i] = CELL_IN;
			++m_inside;
		}
	}
}

bool SliceSolid::Build(const AuScTriList& tris, const AuPoint& mn, const AuPoint& mx, double step, int seal,
		const std::function<bool(int phase, int done, int total)>& progress)
{
	// A margin round the model, so that the flood can go all the way round it.
	const int pad = seal + 2;
	m_step = step;
	m_origin = AuPoint(mn.x - pad * step, mn.y - pad * step, mn.z);
	m_nx = int(ceil((mx.x - mn.x) / step)) + 2 * pad;
	m_ny = int(ceil((mx.y - mn.y) / step)) + 2 * pad;
	m_nz = qMax(1, int(ceil((mx.z - mn.z) / step)));
	m_cells.fill(CELL_OUT, qint64(m_nx) * m_ny * m_nz);
	m_surface = m_inside = 0;

	// Each triangle is cut to every slab it crosses: the part of it between
	// the slab's floor and ceiling, seen from above.
	auto clip = [](const QVector<AuPoint>& in, double z, bool keepAbove) {
		QVector<AuPoint> out;
		for(int i = 0; i < in.size(); ++i)
		{
			const AuPoint& a = in[i];
			const AuPoint& b = in[(i + 1) % in.size()];
			const bool aIn = keepAbove ? a.z >= z : a.z <= z;
			const bool bIn = keepAbove ? b.z >= z : b.z <= z;
			if(aIn)
				out << a;
			if(aIn != bIn)
			{
				const double t = (z - a.z) / (b.z - a.z);
				out << a + (b - a) * t;
			}
		}
		return out;
	};
	for(int n = 0; n < tris.size(); ++n)
	{
		if(n % 200000 == 0 && progress && !progress(0, n, tris.size()))
			return false;
		const AuScTri& t = tris.at(n);
		const QVector<AuPoint> corners = { AuPoint(t.V0()), AuPoint(t.V1()), AuPoint(t.V2()) };
		const double zlo = qMin(corners[0].z, qMin(corners[1].z, corners[2].z));
		const double zhi = qMax(corners[0].z, qMax(corners[1].z, corners[2].z));
		const int k0 = qMax(0, int(floor((zlo - m_origin.z) / step)));
		const int k1 = qMin(m_nz - 1, int(floor((zhi - m_origin.z) / step)));
		for(int k = k0; k <= k1; ++k)
		{
			const double z0 = m_origin.z + k * step, z1 = z0 + step;
			QVector<AuPoint> part = corners;
			if(zlo < z0)
				part = clip(part, z0, true);
			if(zhi > z1 && !part.isEmpty())
				part = clip(part, z1, false);
			QVector<QPointF> flat;
			for(const AuPoint& p : part)
				flat << QPointF(p.x, p.y);
			Mark(k, flat);
		}
	}
	for(int k = 0; k < m_nz; ++k)
	{
		if(k % 64 == 0 && progress && !progress(1, k, m_nz))
			return false;
		Flood(k, seal);
	}
	return true;
}

// Turns, moves and scales a model's triangles the way PlanFromModel placed
// it (TriPlan::model).
static void PlaceModel(AuScTriList& tris, const PlanModel& model)
{
	auto transform = [&tris](const AuMatrix4x4& m) {
		AuScTriList turned;
		turned.reserve(tris.size());
		for(const AuScTri& t : tris)
		{
			const AuPoint a = m.Project(AuPoint(t.V0())), b = m.Project(AuPoint(t.V1())), c = m.Project(AuPoint(t.V2()));
			AuPoint n = (b - a).Cross(c - a);
			n = n.Length() > 1e-12 ? n / n.Length() : AuPoint(0, 0, 1);
			turned.append(AuScTri(AuScPoint(a, 0.f, 0.f, n), AuScPoint(b, 0.f, 0.f, n), AuScPoint(c, 0.f, 0.f, n)));
		}
		tris = turned;
	};
	if(model.yUp)
	{
		AuMatrix4x4 up;
		up.RotateX(PI / 2);
		transform(up);
	}
	AuMatrix4x4 m;
	if(model.turn)
		m.RotateZ(PI / 2);
	if(model.flip)
		m.RotateZ(PI);
	transform(m);
	const AuPoint centre(model.centre[0], model.centre[1], model.centre[2]);
	AuScTriList placed;
	placed.reserve(tris.size());
	// Flipped with the plan after (TriPlan::Flip): mirrored about the grid's
	// centre, and z about half the decks' height.
	auto flip = [&model](AuPoint p) {
		if(model.mirrorX) p.x = -p.x;
		if(model.mirrorY) p.y = -p.y;
		if(model.mirrorZ) p.z = model.height - p.z;
		return p;
	};
	for(const AuScTri& t : tris)
	{
		const AuPoint a = flip((AuPoint(t.V0()) - centre) * model.scale);
		const AuPoint b = flip((AuPoint(t.V1()) - centre) * model.scale);
		const AuPoint c = flip((AuPoint(t.V2()) - centre) * model.scale);
		AuPoint n = (b - a).Cross(c - a);
		n = n.Length() > 1e-12 ? n / n.Length() : AuPoint(0, 0, 1);
		placed.append(AuScTri(AuScPoint(a, 0.f, 0.f, n), AuScPoint(b, 0.f, 0.f, n), AuScPoint(c, 0.f, 0.f, n)));
	}
	tris = placed;
}

// The sliced solids of models, by file and placement: the import leaves its
// own here, so that the designer does not read the model again for the hull
// (a big model takes a minute to read).
static QString ModelKey(const PlanModel& model, double cellFeet)
{
	return QString("%1|%2%3%4%11%12%13:%14|%5,%6,%7|%8|%9|%10").arg(QFileInfo(model.file).absoluteFilePath())
			.arg(model.yUp).arg(model.turn).arg(model.flip)
			.arg(model.centre[0], 0, 'g', 12).arg(model.centre[1], 0, 'g', 12).arg(model.centre[2], 0, 'g', 12)
			.arg(model.scale, 0, 'g', 12).arg(model.sealFeet).arg(cellFeet)
			.arg(model.mirrorX).arg(model.mirrorY).arg(model.mirrorZ).arg(model.height, 0, 'g', 12);
}
static QHash<QString, std::shared_ptr<SliceSolid> >& ModelSolids()
{
	static QHash<QString, std::shared_ptr<SliceSolid> > solids;
	return solids;
}

// The grid of the slices for a model of this size: an eighth of a cell,
// coarser only for a model too big for 150 million squares.
static double SliceStep(const AuPoint& mn, const AuPoint& mx, double cellFeet)
{
	double fine = cellFeet / 8;
	const AuPoint size = mx - mn;
	const double cells = (size.x / fine + 8) * (size.y / fine + 8) * (size.z / fine + 1);
	if(cells > 1.5e8)
		fine *= pow(cells / 1.5e8, 1.0 / 3);
	return fine;
}

// SHIPGEN_DUMP_HULL=<prefix>: the hull at each stage of BuildPlanHull as
// <prefix>-<stage>.obj, for looking at it in a viewer (debugging only).
static void DumpHull(const IndexedMesh& mesh, const char* stage)
{
	const QByteArray prefix = qgetenv("SHIPGEN_DUMP_HULL");
	if(prefix.isEmpty())
		return;
	QFile file(QString::fromLocal8Bit(prefix) + "-" + stage + ".obj");
	if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return;
	QTextStream out(&file);
	for(const AuPoint& p : mesh.v)
		out << "v " << p.x << " " << p.y << " " << p.z << "\n";
	for(int i = 0; i < mesh.f.size(); i += 3)
		out << "f " << mesh.f[i] + 1 << " " << mesh.f[i + 1] + 1 << " " << mesh.f[i + 2] + 1 << "\n";
}

// The sliced solid of a plan's model: from the cache, or read and sliced.
// The model's triangles where the plan puts them, by the same key.
static QHash<QString, std::shared_ptr<AuScTriList> >& ModelTris()
{
	static QHash<QString, std::shared_ptr<AuScTriList> > tris;
	return tris;
}

static std::shared_ptr<SliceSolid> ModelSolidFor(const TriPlan& plan, QString& error)
{
	const QString key = ModelKey(plan.model, plan.cellFeet);
	if(ModelSolids().contains(key))
		return ModelSolids().value(key);
	AuScTriList tris;
	if(!LoadMeshFile(plan.model.file, tris, error))
		return std::shared_ptr<SliceSolid>();
	PlaceModel(tris, plan.model);
	AuPoint mn, mx;
	MeshExtents(tris, mn, mx);
	const double fine = SliceStep(mn, mx, plan.cellFeet);
	std::shared_ptr<SliceSolid> solid = std::make_shared<SliceSolid>();
	solid->Build(tris, mn, mx, fine, qMax(0, int(ceil(plan.model.sealFeet / fine / 2 - 1e-9))), 0);
	ModelSolids().insert(key, solid);
	ModelTris().insert(key, std::make_shared<AuScTriList>(tris));
	return solid;
}

// The model's triangles where the plan puts them (loaded again when only the
// solid was kept, as PlanFromModel keeps it).
static std::shared_ptr<AuScTriList> ModelTrisFor(const TriPlan& plan)
{
	const QString key = ModelKey(plan.model, plan.cellFeet);
	if(ModelTris().contains(key))
		return ModelTris().value(key);
	AuScTriList tris;
	QString error;
	if(!LoadMeshFile(plan.model.file, tris, error))
		return std::shared_ptr<AuScTriList>();
	PlaceModel(tris, plan.model);
	std::shared_ptr<AuScTriList> shared = std::make_shared<AuScTriList>(tris);
	ModelTris().insert(key, shared);
	return shared;
}

// The hull of a plan made from a model (since 31 October 2026, smooth): the
// model's sliced solid together with the rooms grown by the skin (so a room
// drawn out past the model still has its wall), on a grid of cubes a quarter
// of a cell across. Hull triangles drawn or rubbed out do not change it;
// unticking the model as the hull goes back to the drawing.
//
// The cubes' own surface steps (the first version used it, and the shaping
// after could only round the steps into jagged facets: the user's Omega
// destroyer). Instead the grid is blurred a little ([1 2 1] each way), the
// cubes over the half level make the surface, and its corners are moved onto
// the level (surface nets), which keeps the cube surface's square faces (and
// so its closedness) without the steps. Marching tetrahedra, tried first,
// left so many slivers that the simplifying could not get far (the Cyclops
// stopped at 16,674 faces). Slopes come out smooth; edges are rounded by about a cube; parts thinner than about two
// cubes (antennas) go. Each face takes the paint of the plan triangle it
// covers.
//
// Tried and left out: drawing each point onto the nearest point of the
// model's own triangles. Point by point it followed every little detail of
// the model, smaller than the cubes, and the hull came out crumpled; with the
// moves averaged over the neighbours it was still noisier than without.
// The point nearest to all the planes (point, normal) round a corner of the
// cubes, a little drawn to their middle for the directions the planes leave
// free, and kept within 'lim' of the corner on each axis (dual contouring).
static AuPoint CornerOnPlanes(const QVector<AuPoint>& pts, const QVector<AuPoint>& normals, const AuPoint& corner, double lim)
{
	double A[3][3] = { { 0 } }, b[3] = { 0, 0, 0 };
	AuPoint mass(0, 0, 0);
	for(int i = 0; i < pts.size(); ++i)
	{
		const AuPoint& n = normals[i];
		const double nv[3] = { n.x, n.y, n.z };
		const double np = n.Dot(pts[i]);
		for(int r = 0; r < 3; ++r)
		{
			for(int q = 0; q < 3; ++q)
				A[r][q] += nv[r] * nv[q];
			b[r] += nv[r] * np;
		}
		mass = mass + pts[i];
	}
	if(pts.isEmpty())
		return corner;
	mass = mass / pts.size();
	const double lambda = 0.05, mv[3] = { mass.x, mass.y, mass.z };
	for(int r = 0; r < 3; ++r)
	{
		A[r][r] += lambda;
		b[r] += lambda * mv[r];
	}
	auto det3 = [](const double m[3][3]) {
		return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
				+ m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
	};
	const double det = det3(A);
	AuPoint x = mass;
	if(fabs(det) > 1e-12)
	{
		double sol[3];
		for(int col = 0; col < 3; ++col)
		{
			double m[3][3];
			for(int r = 0; r < 3; ++r)
				for(int q = 0; q < 3; ++q)
					m[r][q] = q == col ? b[r] : A[r][q];
			sol[col] = det3(m) / det;
		}
		x = AuPoint(sol[0], sol[1], sol[2]);
	}
	return AuPoint(qBound(corner.x - lim, x.x, corner.x + lim), qBound(corner.y - lim, x.y, corner.y + lim),
				   qBound(corner.z - lim, x.z, corner.z + lim));
}

// With 'tris' (the model's own triangles, where the plan puts them) the hull
// keeps the model's edges sharp and its flat faces flat: the grid is not
// blurred, and every corner is put where the planes round it meet, the
// model's own planes where the surface is the model's, the rooms' walls
// where it is theirs (dual contouring, as AngularHullMesh). Without, as
// below (smooth).
static IndexedMesh ModelHullMesh(const TriPlan& plan, const SliceSolid& solid, const AuScTriList* tris = 0)
{
	const double h = plan.cellFeet / 4;
	AuPoint lo, hi;
	solid.Extents(lo, hi);
	const QPointF g0 = plan.Corner(0, 0), g1 = plan.Corner(plan.Width(), plan.Depth());
	const double skin = qMax(0.0, plan.skinFeet);
	lo = AuPoint(qMin(lo.x, qMin(g0.x(), g1.x())), qMin(lo.y, qMin(g0.y(), g1.y())), qMin(lo.z, 0.0)) - AuPoint(skin, skin, skin);
	hi = AuPoint(qMax(hi.x, qMax(g0.x(), g1.x())), qMax(hi.y, qMax(g0.y(), g1.y())),
			qMax(hi.z, plan.Decks() * plan.deckFeet)) + AuPoint(skin, skin, skin);
	// Two empty cubes all round, so the blurred grid is empty at its edge.
	const AuPoint origin = lo - AuPoint(2 * h, 2 * h, 2 * h);
	const int nx = int(ceil((hi.x - lo.x) / h)) + 4, ny = int(ceil((hi.y - lo.y) / h)) + 4,
			nz = int(ceil((hi.z - lo.z) / h)) + 4;
	const qint64 count = qint64(nx) * ny * nz;
	auto index = [nx, ny](int i, int j, int k) { return (qint64(k) * ny + j) * nx + i; };
	auto centreOf = [&](int i, int j, int k) { return origin + AuPoint(i + .5, j + .5, k + .5) * h; };
	// The plan triangle a point is in (an invalid one off the grid).
	auto triangleAt = [&plan](const AuPoint& p) {
		const QPointF c0 = plan.Corner(0, 0);
		const int cx = int(floor((p.x - c0.x()) / plan.cellFeet)), cy = int(floor((p.y - c0.y()) / plan.cellFeet));
		const QPointF c = plan.CellCentre(cx, cy);
		const double dx = p.x - c.x(), dy = p.y - c.y();
		const int k = fabs(dx) >= fabs(dy) ? (dx > 0 ? TRI_E : TRI_W) : (dy > 0 ? TRI_N : TRI_S);
		return TriRef(int(floor(p.z / plan.deckFeet)), cx, cy, k);
	};
	// A room: the plan triangle's prism between its deck's floor and ceiling.
	// Only the rooms join the model, not every drawn hull triangle: those
	// come from the model already, and their zigzag edges, grown by the skin,
	// stood out of its sides as teeth.
	auto drawn = [&](const AuPoint& p) {
		const TriRef t = triangleAt(p);
		if(p.z < 0 || !plan.Valid(t) || plan.Room(t) < 0)
			return false;
		double z0, z1;
		RoomHeights(plan, t.deck, z0, z1);
		return p.z >= z0 && p.z <= z1;
	};
	// The rooms grown by the skin: their wall.
	auto drawnWithSkin = [&](const AuPoint& p) {
		if(drawn(p))
			return true;
		if(skin <= 0)
			return false;
		static const int kDirs[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		for(const auto& d : kDirs)
			if(drawn(p + AuPoint(d[0], d[1], d[2]) * skin))
				return true;
		return false;
	};

	// The grid, then blurred (not when sharp).
	QVector<float> field(count, 0.f);
	for(int k = 2; k < nz - 2; ++k)
		for(int j = 2; j < ny - 2; ++j)
			for(int i = 2; i < nx - 2; ++i)
			{
				const AuPoint p = centreOf(i, j, k);
				if(solid.Inside(p) || drawnWithSkin(p))
					field[index(i, j, k)] = 1.f;
			}
	if(!tris)
	{
		QVector<float> blurred(count, 0.f);
		const qint64 strides[3] = { 1, nx, qint64(nx) * ny };
		const int sizes[3] = { nx, ny, nz };
		for(int axis = 0; axis < 3; ++axis)
		{
			const qint64 st = strides[axis];
			for(int k = 0; k < nz; ++k)
				for(int j = 0; j < ny; ++j)
					for(int i = 0; i < nx; ++i)
					{
						const int at = axis == 0 ? i : (axis == 1 ? j : k);
						const qint64 c = index(i, j, k);
						const float before = at > 0 ? field[c - st] : 0.f;
						const float after = at < sizes[axis] - 1 ? field[c + st] : 0.f;
						blurred[c] = 0.25f * before + 0.5f * field[c] + 0.25f * after;
					}
			field.swap(blurred);
		}
	}

	// The surface of the cubes over the half level, with every corner then
	// moved onto the smooth surface (surface nets: the mean of the points
	// where the blurred grid crosses the level on the edges between the
	// eight cube centres round the corner). The level is just over a half,
	// so that no grid value (a multiple of 1/64) is ever on it.
	const float level = 0.5f + 1.f / 256;
	auto at = [&](int i, int j, int k) -> bool {
		if(i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz)
			return false;
		return field[index(i, j, k)] > level;
	};
	IndexedMesh mesh;
	QHash<qint64, int> cornerId;
	QVector<std::array<int, 3> > cornerAt;
	auto vertex = [&](int i, int j, int k) {
		const qint64 key = (qint64(k) * (ny + 1) + j) * (nx + 1) + i;
		QHash<qint64, int>::const_iterator it = cornerId.constFind(key);
		if(it != cornerId.constEnd())
			return it.value();
		const int id = mesh.v.size();
		mesh.v << origin + AuPoint(i, j, k) * h;
		cornerAt << std::array<int, 3>{ { i, j, k } };
		cornerId.insert(key, id);
		return id;
	};
	auto paintAt = [&](const AuPoint& p) {
		TriRef t = triangleAt(p);
		t.deck = qBound(0, t.deck, plan.Decks() - 1);
		return plan.Valid(t) ? plan.Paint(t) : 0;
	};
	auto quad = [&](int a, int b, int c, int d, int paint) {
		mesh.f << a << b << c << a << c << d;
		mesh.paint << paint << paint;
	};
	for(int k = 0; k < nz; ++k)
		for(int j = 0; j < ny; ++j)
			for(int i = 0; i < nx; ++i)
			{
				if(!at(i, j, k))
					continue;
				const int paint = paintAt(centreOf(i, j, k));
				if(!at(i + 1, j, k)) quad(vertex(i+1, j, k), vertex(i+1, j+1, k), vertex(i+1, j+1, k+1), vertex(i+1, j, k+1), paint);
				if(!at(i - 1, j, k)) quad(vertex(i, j, k), vertex(i, j, k+1), vertex(i, j+1, k+1), vertex(i, j+1, k), paint);
				if(!at(i, j + 1, k)) quad(vertex(i, j+1, k), vertex(i, j+1, k+1), vertex(i+1, j+1, k+1), vertex(i+1, j+1, k), paint);
				if(!at(i, j - 1, k)) quad(vertex(i, j, k), vertex(i+1, j, k), vertex(i+1, j, k+1), vertex(i, j, k+1), paint);
				if(!at(i, j, k + 1)) quad(vertex(i, j, k+1), vertex(i+1, j, k+1), vertex(i+1, j+1, k+1), vertex(i, j+1, k+1), paint);
				if(!at(i, j, k - 1)) quad(vertex(i, j, k), vertex(i, j+1, k), vertex(i+1, j+1, k), vertex(i+1, j, k), paint);
			}
	// Corner (i, j, k) is surrounded by the cube centres (i-1..i, j-1..j,
	// k-1..k); every corner of the surface has a crossing among them.
	auto value = [&](int i, int j, int k) -> float {
		if(i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz)
			return 0.f;
		return field[index(i, j, k)];
	};
	if(tris)
	{
		// The model's triangles in buckets of four cubes, for finding where an
		// edge between two cube centres goes through the model.
		const double bs = 4 * h;
		const int bx = nx / 4 + 2, by = ny / 4 + 2, bz = nz / 4 + 2;
		QVector<QVector<int> > bucket(qint64(bx) * by * bz);
		auto bucketOf = [&](const AuPoint& p, int& i, int& j, int& k) {
			i = qBound(0, int(floor((p.x - origin.x) / bs)), bx - 1);
			j = qBound(0, int(floor((p.y - origin.y) / bs)), by - 1);
			k = qBound(0, int(floor((p.z - origin.z) / bs)), bz - 1);
		};
		for(int t = 0; t < tris->size(); ++t)
		{
			const AuScTri& tri = tris->at(t);
			const AuPoint a(tri.V0()), b(tri.V1()), c(tri.V2());
			int i0, j0, k0, i1, j1, k1;
			bucketOf(a.Min(b).Min(c), i0, j0, k0);
			bucketOf(a.Max(b).Max(c), i1, j1, k1);
			for(int k = k0; k <= k1; ++k)
				for(int j = j0; j <= j1; ++j)
					for(int i = i0; i <= i1; ++i)
						bucket[(qint64(k) * by + j) * bx + i] << t;
		}
		// Where the segment from 'in' to 'out' first leaves through the model,
		// and the model's normal there (out of the solid, toward 'out').
		// The sliced solid is not exactly the model (its own grid, the
		// sealing), so the model is looked for a cube and a half either way.
		const double reachOut = 1.5;
		auto hitModel = [&](const AuPoint& in, const AuPoint& out, AuPoint& at, AuPoint& normal) {
			const AuPoint dir = out - in;
			int i0, j0, k0, i1, j1, k1;
			bucketOf((in - dir * reachOut).Min(out + dir * reachOut), i0, j0, k0);
			bucketOf((in - dir * reachOut).Max(out + dir * reachOut), i1, j1, k1);
			double best = 1e9;
			for(int k = k0; k <= k1; ++k)
				for(int j = j0; j <= j1; ++j)
					for(int i = i0; i <= i1; ++i)
						for(int t : bucket[(qint64(k) * by + j) * bx + i])
						{
							const AuScTri& tri = tris->at(t);
							const AuPoint a(tri.V0()), b(tri.V1()), c(tri.V2());
							const AuPoint e1 = b - a, e2 = c - a, pv = dir.Cross(e2);
							const double det = e1.Dot(pv);
							if(fabs(det) < 1e-12)
								continue;
							const AuPoint tv = in - a;
							const double u = tv.Dot(pv) / det;
							if(u < -1e-6 || u > 1 + 1e-6)
								continue;
							const AuPoint qv = tv.Cross(e1);
							const double v = dir.Dot(qv) / det;
							if(v < -1e-6 || u + v > 1 + 1e-6)
								continue;
							const double s = e2.Dot(qv) / det;
							if(s < -reachOut || s > 1 + reachOut || fabs(s - 0.5) >= fabs(best - 0.5))
								continue;
							best = s;
							AuPoint n = e1.Cross(e2);
							n = n / qMax(n.Length(), 1e-12);
							normal = n.Dot(dir) < 0 ? n * -1.0 : n;
							at = in + dir * s;
						}
			return best < 1e8;
		};
		int fromModel = 0, fromGrid = 0;
		auto inside = [&](const AuPoint& p) { return solid.Inside(p) || drawnWithSkin(p); };
		static const AuPoint kAxis[3] = { AuPoint(1, 0, 0), AuPoint(0, 1, 0), AuPoint(0, 0, 1) };
		for(int vi = 0; vi < mesh.v.size(); ++vi)
		{
			const int ci = cornerAt[vi][0], cj = cornerAt[vi][1], ck = cornerAt[vi][2];
			QVector<AuPoint> pts, normals;
			for(int c = 0; c < 8; ++c)
				for(int axis = 0; axis < 3; ++axis)
				{
					const int o = c | (1 << axis);
					if(o == c)
						continue;
					const int ic = ci - 1 + (c & 1), jc = cj - 1 + ((c >> 1) & 1), kc = ck - 1 + ((c >> 2) & 1);
					const int io = ci - 1 + (o & 1), jo = cj - 1 + ((o >> 1) & 1), ko = ck - 1 + ((o >> 2) & 1);
					const bool inC = at(ic, jc, kc), inO = at(io, jo, ko);
					if(inC == inO)
						continue;
					AuPoint pin = centreOf(ic, jc, kc), pout = centreOf(io, jo, ko);
					if(!inC)
						std::swap(pin, pout);
					AuPoint p, n;
					// The model's own surface when the solid ends there; else the
					// edge of the room's wall (along the grid), found by halving.
					if(!(solid.Inside(pin) && !drawnWithSkin(pin) && hitModel(pin, pout, p, n)))
					{
						AuPoint a = pin, b = pout;
						for(int it = 0; it < 10; ++it)
						{
							const AuPoint m = (a + b) * 0.5;
							if(inside(m)) a = m; else b = m;
						}
						p = (a + b) * 0.5;
						n = kAxis[axis] * ((pout - pin).Dot(kAxis[axis]) > 0 ? 1.0 : -1.0);
						++fromGrid;
					}
					else
						++fromModel;
					pts << p;
					normals << n;
				}
			if(!pts.isEmpty())
				mesh.v[vi] = CornerOnPlanes(pts, normals, origin + AuPoint(ci, cj, ck) * h, h * 1.5);
		}
		if(!qgetenv("SHIPGEN_DEBUG").isEmpty())
			fprintf(stderr, "model hull crossings: %d on the model, %d on the grid\n", fromModel, fromGrid);
		return mesh;
	}
	for(int vi = 0; vi < mesh.v.size(); ++vi)
	{
		const int ci = cornerAt[vi][0], cj = cornerAt[vi][1], ck = cornerAt[vi][2];
		float f[8];
		for(int c = 0; c < 8; ++c)
			f[c] = value(ci - 1 + (c & 1), cj - 1 + ((c >> 1) & 1), ck - 1 + ((c >> 2) & 1));
		AuPoint sum(0, 0, 0);
		int crossings = 0;
		for(int c = 0; c < 8; ++c)
			for(int axis = 0; axis < 3; ++axis)
			{
				const int o = c | (1 << axis);
				if(o == c)
					continue;
				if((f[c] > level) == (f[o] > level))
					continue;
				const double t = (level - f[c]) / (f[o] - f[c]);
				const AuPoint a((c & 1) - .5, ((c >> 1) & 1) - .5, ((c >> 2) & 1) - .5);
				const AuPoint b((o & 1) - .5, ((o >> 1) & 1) - .5, ((o >> 2) & 1) - .5);
				sum = sum + a + (b - a) * t;
				++crossings;
			}
		if(crossings > 0)
			mesh.v[vi] = mesh.v[vi] + sum / crossings * h;
	}
	return mesh;
}

// The angular hull (TriPlan::hullStyle "angular"), made from a field rather
// than from the prisms of the triangles, so that a slope that does not change
// comes out as one flat panel instead of a sawtooth.
//
// Each deck that has hull gets the signed distance to its drawn outline in
// the plan (negative inside). The hull is everything within the skin of:
//   - each deck's outline, straight up from its floor to its ceiling (the
//     drawn hull is the least the hull may be), and
//   - between two decks next to each other that both have hull, the outline
//     blended from one deck's to the other's over the height of each of the
//     two decks: where the deck above is narrower the ledge rises from the
//     lower deck's edge to the top of the deck above, where it is wider its
//     underside falls to the bottom of the deck below (as SlopeBetweenDecks).
// An empty deck between two others is left a gap (the user: such gaps are
// meant). Two straight edges, one above the other, blend into a plane.
//
// The surface is taken from a grid of cubes, the faces between the cubes in
// and out of the hull, and every corner of those faces is put where the
// planes of the surface round it meet (dual contouring), so that the edges
// and corners stay sharp and the flat panels flat; then the flat areas are
// merged (DecimateHull with a small limit, in BuildPlanHull).
static IndexedMesh AngularHullMesh(const TriPlan& plan, const QVector<bool>& hullTris)
{
	const int W = plan.Width(), D = plan.Depth(), decks = plan.Decks();
	const double cell = plan.cellFeet, deckH = plan.deckFeet;
	// The skin: the field less the skin. The plan distance is in the octagon
	// norm, so the corners grown by it are cut off at 45 degrees, not round;
	// above and below it is the band, so flat. (Grown afterwards instead
	// (OffsetMitred), the many nearly flat faces of the grid's surface
	// crumpled it.)
	const double skin = qMax(0.0, plan.skinFeet);
	// How far apart two edges, one on each deck, may be for the hull to
	// slope from one to the other: further apart, it steps. Without a limit
	// every part of a deck blended with every part of the next, which made
	// curved fillets between parts (a wing and the body beside it).
	const double reach = cell * 1.5;
	const double h = cell / 4;
	auto isHull = [&](const TriRef& t) {
		return plan.Valid(t) && hullTris[((t.deck * D + t.cy) * W + t.cx) * 4 + t.k];
	};

	// The plan's distance fields, one per deck, on a grid of half the cube.
	const QPointF c0 = plan.Corner(0, 0), c1 = plan.Corner(W, D);
	const double s = h / 2, margin = skin + 4 * h;
	const double gx0 = qMin(c0.x(), c1.x()) - margin, gy0 = qMin(c0.y(), c1.y()) - margin;
	const int gnx = int(ceil((fabs(c1.x() - c0.x()) + 2 * margin) / s)) + 1;
	const int gny = int(ceil((fabs(c1.y() - c0.y()) + 2 * margin) / s)) + 1;
	QVector<bool> hasHull(decks, false);
	for(int d = 0; d < decks; ++d)
		for(int i = 0; i < W * D * 4 && !hasHull[d]; ++i)
			hasHull[d] = hullTris[d * W * D * 4 + i];
	// Whether a point of the plan is in the hull of decks d0..d1.
	auto inDecks = [&](int d0, int d1, const QPointF& q) {
		const int cx = int(floor((q.x() - c0.x()) / cell)), cy = int(floor((q.y() - c0.y()) / cell));
		if(cx < 0 || cy < 0 || cx >= W || cy >= D)
			return false;
		const QPointF c = plan.CellCentre(cx, cy);
		const double dx = q.x() - c.x(), dy = q.y() - c.y();
		const int k = fabs(dx) >= fabs(dy) ? (dx > 0 ? TRI_E : TRI_W) : (dy > 0 ? TRI_N : TRI_S);
		for(int d = d0; d <= d1; ++d)
			if(isHull(TriRef(d, cx, cy, k)))
				return true;
		return false;
	};
	// The outline of the hull of decks d0..d1 (one deck, or two seen from
	// above as one): the triangle edges with no hull across them, each with
	// the hull on its left; in buckets a cell wide, searched in rings outward.
	struct Outline
	{
		QVector<QLineF> edges;
		QVector<QVector<int> > bucket;
		int bnx = 0, bny = 0;
	};
	const int bnx = int(ceil(gnx * s / cell)) + 1, bny = int(ceil(gny * s / cell)) + 1;
	auto outline = [&](int d0, int d1) {
		auto inRegion = [&](const TriRef& t) {
			if(!plan.Valid(t))
				return false;
			for(int d = d0; d <= d1; ++d)
				if(isHull(TriRef(d, t.cx, t.cy, t.k)))
					return true;
			return false;
		};
		Outline o;
		for(int cy = 0; cy < D; ++cy)
			for(int cx = 0; cx < W; ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const TriRef t(d0, cx, cy, k);
					if(!inRegion(t))
						continue;
					QPointF c[3];
					plan.TriCorners(t, c);
					for(int e = 0; e < 3; ++e)
						if(!inRegion(plan.Across(t, e)))
							o.edges << QLineF(c[e], c[(e + 1) % 3]);
				}
		o.bnx = bnx;
		o.bny = bny;
		o.bucket.resize(bnx * bny);
		for(int e = 0; e < o.edges.size(); ++e)
		{
			const QLineF& l = o.edges[e];
			const int i0 = int(floor((qMin(l.x1(), l.x2()) - gx0) / cell)), i1 = int(floor((qMax(l.x1(), l.x2()) - gx0) / cell));
			const int j0 = int(floor((qMin(l.y1(), l.y2()) - gy0) / cell)), j1 = int(floor((qMax(l.y1(), l.y2()) - gy0) / cell));
			for(int j = qMax(0, j0); j <= qMin(bny - 1, j1); ++j)
				for(int i = qMax(0, i0); i <= qMin(bnx - 1, i1); ++i)
					o.bucket[j * bnx + i] << e;
		}
		return o;
	};
	// The edges near a point, ring by ring, until 'done' says the rest are
	// further than the ring.
	auto nearEdges = [&](const Outline& o, const QPointF& q, const std::function<void(const QLineF&)>& visit,
						 const std::function<double()>& best) {
		const int bi = int(floor((q.x() - gx0) / cell)), bj = int(floor((q.y() - gy0) / cell));
		for(int r = 0; r < qMax(o.bnx, o.bny); ++r)
		{
			for(int y = bj - r; y <= bj + r; ++y)
				for(int x = bi - r; x <= bi + r; ++x)
				{
					if(qMax(abs(x - bi), abs(y - bj)) != r || x < 0 || y < 0 || x >= o.bnx || y >= o.bny)
						continue;
					for(int e : o.bucket[y * o.bnx + x])
						visit(o.edges[e]);
				}
			if(best() <= r * cell)
				break;
		}
	};
	// The distance in the octagon norm, not the round one: the plan's edges
	// go only along the grid and its diagonals, so in this norm the distance
	// is flat in pieces everywhere, round corners too (they come out cut off
	// at 45 degrees), and so is every blend of two decks; with round
	// distances the corners of a slope twisted.
	auto norm = [](double x, double y) {
		x = fabs(x); y = fabs(y);
		return qMax(qMax(x, y), (x + y) * M_SQRT1_2);
	};
	auto segDist = [&norm](const QPointF& q, const QLineF& l) {
		// Convex and flat in pieces along the segment: smallest at an end or
		// where a part of the norm changes over.
		const QPointF a = l.p1(), ab = l.p2() - l.p1(), aq = q - a;
		double best = qMin(norm(aq.x(), aq.y()), norm(q.x() - l.x2(), q.y() - l.y2()));
		const double us[4] = { ab.x() != 0 ? aq.x() / ab.x() : -1, ab.y() != 0 ? aq.y() / ab.y() : -1,
							   ab.x() + ab.y() != 0 ? (aq.x() + aq.y()) / (ab.x() + ab.y()) : -1,
							   ab.x() - ab.y() != 0 ? (aq.x() - aq.y()) / (ab.x() - ab.y()) : -1 };
		for(double u : us)
			if(u > 0 && u < 1)
				best = qMin(best, norm(aq.x() - ab.x() * u, aq.y() - ab.y() * u));
		return best;
	};
	// The signed distance to the outline of decks d0..d1 (negative inside).
	//
	// With 'away' a deck: in the part of the outline that is not that deck,
	// the distance is measured straight away from that deck instead, to
	// where the outline ends. That is how wide the slope between the two is
	// there: the nearest edge may be another one (beside the end of a bar
	// that moves over each deck, it was the bar's end, and the slope came
	// out as stairs there).
	auto planField = [&](int d0, int d1, int away) {
		const Outline o = outline(d0, d1);
		Outline from;
		if(away >= 0)
			from = outline(away, away);
		QVector<float> field(gnx * gny);
		for(int j = 0; j < gny; ++j)
			for(int i = 0; i < gnx; ++i)
			{
				const QPointF q(gx0 + i * s, gy0 + j * s);
				const bool inside = inDecks(d0, d1, q);
				if(inside && away >= 0 && !inDecks(away, away, q))
				{
					// The way away from the deck: from the nearest point of its
					// outline (round distance), or out across the nearest edge
					// where the point is on it.
					double bestD = 1e30;
					QPointF dir(0, 0);
					nearEdges(from, q, [&](const QLineF& l) {
						const QPointF a = l.p1(), ab = l.p2() - l.p1();
						const double len2 = QPointF::dotProduct(ab, ab);
						const double u = len2 > 0 ? qBound(0.0, QPointF::dotProduct(q - a, ab) / len2, 1.0) : 0.0;
						const QPointF dd = q - (a + ab * u);
						const double dist = sqrt(QPointF::dotProduct(dd, dd));
						if(dist < bestD - 1e-9)
						{
							bestD = dist;
							const double len = sqrt(len2);
							// Out of the hull: the right of the edge.
							dir = dist > 1e-6 && (u <= 0 || u >= 1) ? dd / dist : QPointF(ab.y(), -ab.x()) / qMax(len, 1e-12);
						}
					}, [&]() { return bestD; });
					if(bestD < 1e29)
					{
						double run = 0;
						const double step = s / 4;
						while(run < 4 * reach && inDecks(d0, d1, q + dir * (run + step)))
							run += step;
						field[j * gnx + i] = float(-(run + step * 0.5));
						continue;
					}
				}
				double best = 1e30;
				nearEdges(o, q, [&](const QLineF& l) { best = qMin(best, segDist(q, l)); }, [&]() { return best; });
				field[j * gnx + i] = float(inside ? -best : best);
			}
		return field;
	};
	// Each deck's own outline (g), and each deck's together with the one
	// above, seen from each of the two (pairDown: in the part that is the
	// deck above only, measured away from the deck below; pairUp the other
	// way round). The slopes blend a deck's outline with the pair's, not with
	// the other deck's: a point of the deck above that is not over the deck
	// below is then measured to the edge the slope runs to, not to the
	// nearest edge of its own deck, which may be the one over the deck
	// below's edge (a strut that moves over by its own width each deck: the
	// two decks' distances were equal and opposite, and the slope crumpled).
	QVector<QVector<float> > g(decks), pairDown(decks), pairUp(decks);
	for(int d = 0; d < decks; ++d)
	{
		if(hasHull[d])
			g[d] = planField(d, d, -1);
		if(hasHull[d] && d + 1 < decks && hasHull[d + 1])
		{
			pairDown[d] = planField(d, d + 1, d);
			pairUp[d] = planField(d, d + 1, d + 1);
		}
	}
	auto sample = [&](const QVector<float>& field, double x, double y) -> double {
		const double fx = (x - gx0) / s, fy = (y - gy0) / s;
		if(fx < 0 || fy < 0 || fx >= gnx - 1 || fy >= gny - 1)
			return 1e4;
		const int i = int(fx), j = int(fy);
		const double u = fx - i, v = fy - j;
		const float* f = field.constData() + j * gnx + i;
		return (f[0] * (1 - u) + f[1] * u) * (1 - v) + (f[gnx] * (1 - u) + f[gnx + 1] * u) * v;
	};
	auto G = [&](int d, double x, double y) { return sample(g[d], x, y); };
	// How far z is outside deck d's height (negative inside).
	auto band = [&](int d, double z) { return qMax(d * deckH - z, z - (d + 1) * deckH); };
	// The skin: on the plan distances, and above and below a deck where no
	// slope goes on from it (the slope's own skin covers it there: a deck's
	// box grown up by the skin stood out of the slope as a step at every
	// deck).
	auto sloping = [&](int d, int other, double x, double y) {
		return other >= 0 && other < decks && hasHull[other] && G(other, x, y) <= reach && G(d, x, y) <= reach;
	};
	auto F = [&](const AuPoint& p) -> double {
		const int dlo = qMax(0, int(floor((p.z - skin - 2 * h) / deckH)) - 1);
		const int dhi = qMin(decks - 1, int(floor((p.z + skin + 2 * h) / deckH)) + 1);
		double best = 1e4;
		for(int d = dlo; d <= dhi; ++d)
		{
			if(!hasHull[d])
				continue;
			const double gd = G(d, p.x, p.y);
			const double below = sloping(d, d - 1, p.x, p.y) ? 0 : skin, above = sloping(d, d + 1, p.x, p.y) ? 0 : skin;
			best = qMin(best, qMax(gd - skin, qMax(d * deckH - below - p.z, p.z - (d + 1) * deckH - above)));
			if(d + 1 >= decks || !hasHull[d + 1])
				continue;
			const double gu = G(d + 1, p.x, p.y);
			const double near = qMax(gd, gu) - reach;
			// The underside of the deck above, falling over the deck below's
			// height, and the ledge of the deck below, rising over the deck
			// above's.
			const double t0 = qBound(0.0, (p.z - d * deckH) / deckH, 1.0);
			best = qMin(best, qMax(qMax((1 - t0) * gd + t0 * sample(pairDown[d], p.x, p.y) - skin, band(d, p.z)), near));
			const double t1 = qBound(0.0, (p.z - (d + 1) * deckH) / deckH, 1.0);
			best = qMin(best, qMax(qMax((1 - t1) * sample(pairUp[d], p.x, p.y) + t1 * gu - skin, band(d + 1, p.z)), near));
		}
		return best;
	};

	// The cubes.
	const double zTop = decks * deckH;
	const AuPoint origin(gx0 + 2 * h, gy0 + 2 * h, -skin - 3 * h);
	const int nx = int(ceil((gnx - 1) * s / h)) - 4, ny = int(ceil((gny - 1) * s / h)) - 4,
			nz = int(ceil((zTop + 2 * skin + 6 * h) / h));
	if(nx <= 0 || ny <= 0 || nz <= 0)
		return IndexedMesh();
	auto index = [nx, ny](int i, int j, int k) { return (qint64(k) * ny + j) * nx + i; };
	auto centreOf = [&](int i, int j, int k) { return origin + AuPoint(i + .5, j + .5, k + .5) * h; };
	QVector<float> value(qint64(nx) * ny * nz);
	for(int k = 0; k < nz; ++k)
		for(int j = 0; j < ny; ++j)
			for(int i = 0; i < nx; ++i)
			{
				const bool border = i == 0 || j == 0 || k == 0 || i == nx - 1 || j == ny - 1 || k == nz - 1;
				value[index(i, j, k)] = border ? 1.f : float(F(centreOf(i, j, k)));
			}
	auto in = [&](int i, int j, int k) {
		return i >= 0 && j >= 0 && k >= 0 && i < nx && j < ny && k < nz && value[index(i, j, k)] < 0;
	};

	IndexedMesh mesh;
	QHash<qint64, int> cornerId;
	QVector<std::array<int, 3> > cornerAt;
	auto vertex = [&](int i, int j, int k) {
		const qint64 key = (qint64(k) * (ny + 1) + j) * (nx + 1) + i;
		QHash<qint64, int>::const_iterator it = cornerId.constFind(key);
		if(it != cornerId.constEnd())
			return it.value();
		const int id = mesh.v.size();
		mesh.v << origin + AuPoint(i, j, k) * h;
		cornerAt << std::array<int, 3>{ { i, j, k } };
		cornerId.insert(key, id);
		return id;
	};
	auto paintAt = [&](const AuPoint& p) {
		const int cx = qBound(0, int(floor((p.x - c0.x()) / cell)), W - 1), cy = qBound(0, int(floor((p.y - c0.y()) / cell)), D - 1);
		const int d = qBound(0, int(floor(p.z / deckH)), decks - 1);
		const QPointF c = plan.CellCentre(cx, cy);
		const double dx = p.x - c.x(), dy = p.y - c.y();
		const TriRef t(d, cx, cy, fabs(dx) >= fabs(dy) ? (dx > 0 ? TRI_E : TRI_W) : (dy > 0 ? TRI_N : TRI_S));
		return plan.Paint(t);
	};
	auto quad = [&](int a, int b, int c, int dd, int paint) {
		mesh.f << a << b << c << a << c << dd;
		mesh.paint << paint << paint;
	};
	for(int k = 0; k < nz; ++k)
		for(int j = 0; j < ny; ++j)
			for(int i = 0; i < nx; ++i)
			{
				if(!in(i, j, k))
					continue;
				const int paint = paintAt(centreOf(i, j, k));
				if(!in(i + 1, j, k)) quad(vertex(i+1, j, k), vertex(i+1, j+1, k), vertex(i+1, j+1, k+1), vertex(i+1, j, k+1), paint);
				if(!in(i - 1, j, k)) quad(vertex(i, j, k), vertex(i, j, k+1), vertex(i, j+1, k+1), vertex(i, j+1, k), paint);
				if(!in(i, j + 1, k)) quad(vertex(i, j+1, k), vertex(i, j+1, k+1), vertex(i+1, j+1, k+1), vertex(i+1, j+1, k), paint);
				if(!in(i, j - 1, k)) quad(vertex(i, j, k), vertex(i+1, j, k), vertex(i+1, j, k+1), vertex(i, j, k+1), paint);
				if(!in(i, j, k + 1)) quad(vertex(i, j, k+1), vertex(i+1, j, k+1), vertex(i+1, j+1, k+1), vertex(i, j+1, k+1), paint);
				if(!in(i, j, k - 1)) quad(vertex(i, j, k), vertex(i, j+1, k), vertex(i+1, j+1, k), vertex(i+1, j, k), paint);
			}

	// Every corner where the planes round it meet: the points where the
	// surface crosses the edges between the eight cube centres round the
	// corner, each with the surface's normal there; the point nearest to all
	// their planes (a little drawn to their middle, for the directions the
	// planes leave free), kept within the eight centres.
	const double eps = h * 0.05;
	auto normalAt = [&](const AuPoint& p) {
		AuPoint n(F(p + AuPoint(eps, 0, 0)) - F(p - AuPoint(eps, 0, 0)),
				  F(p + AuPoint(0, eps, 0)) - F(p - AuPoint(0, eps, 0)),
				  F(p + AuPoint(0, 0, eps)) - F(p - AuPoint(0, 0, eps)));
		const double len = n.Length();
		return len > 1e-12 ? n / len : AuPoint(0, 0, 0);
	};
	for(int vi = 0; vi < mesh.v.size(); ++vi)
	{
		const int ci = cornerAt[vi][0], cj = cornerAt[vi][1], ck = cornerAt[vi][2];
		AuPoint cp[8];
		float f[8];
		for(int c = 0; c < 8; ++c)
		{
			const int i = ci - 1 + (c & 1), j = cj - 1 + ((c >> 1) & 1), k = ck - 1 + ((c >> 2) & 1);
			cp[c] = centreOf(i, j, k);
			f[c] = (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) ? 1.f : value[index(i, j, k)];
		}
		double A[3][3] = { { 0 } }, b[3] = { 0, 0, 0 };
		AuPoint mass(0, 0, 0);
		int crossings = 0;
		for(int c = 0; c < 8; ++c)
			for(int axis = 0; axis < 3; ++axis)
			{
				const int o = c | (1 << axis);
				if(o == c || (f[c] < 0) == (f[o] < 0))
					continue;
				// The crossing, found on the field itself (a few steps of the
				// false position method).
				AuPoint a = cp[c], e = cp[o];
				double fa = f[c], fe = f[o];
				AuPoint p = a + (e - a) * (fa / (fa - fe));
				for(int it = 0; it < 4; ++it)
				{
					const double fp = F(p);
					if(fabs(fp) < 1e-5)
						break;
					if((fp < 0) == (fa < 0)) { a = p; fa = fp; } else { e = p; fe = fp; }
					p = a + (e - a) * (fa / (fa - fe));
				}
				const AuPoint n = normalAt(p);
				const double nv[3] = { n.x, n.y, n.z };
				const double np = n.Dot(p);
				for(int r = 0; r < 3; ++r)
				{
					for(int q = 0; q < 3; ++q)
						A[r][q] += nv[r] * nv[q];
					b[r] += nv[r] * np;
				}
				mass = mass + p;
				++crossings;
			}
		if(crossings == 0)
			continue;
		mass = mass / crossings;
		const double lambda = 0.05;
		const double mv[3] = { mass.x, mass.y, mass.z };
		for(int r = 0; r < 3; ++r)
		{
			A[r][r] += lambda;
			b[r] += lambda * mv[r];
		}
		auto det3 = [](const double m[3][3]) {
			return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
					+ m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
		};
		const double det = det3(A);
		AuPoint x = mass;
		if(fabs(det) > 1e-12)
		{
			double sol[3];
			for(int col = 0; col < 3; ++col)
			{
				double m[3][3];
				for(int r = 0; r < 3; ++r)
					for(int q = 0; q < 3; ++q)
						m[r][q] = q == col ? b[r] : A[r][q];
				sol[col] = det3(m) / det;
			}
			x = AuPoint(sol[0], sol[1], sol[2]);
		}
		const AuPoint corner = origin + AuPoint(ci, cj, ck) * h;
		const double lim = h * 0.5;
		x = AuPoint(qBound(corner.x - lim, x.x, corner.x + lim), qBound(corner.y - lim, x.y, corner.y + lim),
					qBound(corner.z - lim, x.z, corner.z + lim));
		mesh.v[vi] = x;
	}
	return mesh;
}

PlanHull BuildPlanHull(const TriPlan& plan)
{
	PlanHull out;
	const int rows = 2;
	auto zOf = [&plan, rows](int level) { return level * plan.deckFeet / rows; };
	// The model itself as the hull, when the plan was made from one and says
	// so (ModelHullMesh); else the drawn triangles.
	std::shared_ptr<SliceSolid> modelSolid;
	if(plan.model.IsSet() && plan.model.asHull)
	{
		QString error;
		modelSolid = ModelSolidFor(plan, error);
		if(!modelSolid)
			out.notes << QString("The model %1 could not be read (%2), so the hull is made from the drawn triangles.")
					.arg(plan.model.file, error);
	}
	IndexedMesh mesh;
	if(modelSolid)
	{
		// Sharp or smoothed, as the user chose (PlanModel::sharp).
		const std::shared_ptr<AuScTriList> tris = plan.model.sharp
				? ModelTrisFor(plan) : std::shared_ptr<AuScTriList>();
		mesh = ModelHullMesh(plan, *modelSolid, tris.get());
	}
	else
	{
		int added = 0;
		const QVector<bool> hullTris = ManifoldHull(plan, added);
		const int W = plan.Width(), D = plan.Depth();
		if(plan.hullStyle == "angular")
			mesh = AngularHullMesh(plan, hullTris);
		else
			mesh = BuildPrisms(plan, [&](const TriRef& t) { return hullTris[((t.deck * D + t.cy) * W + t.cx) * 4 + t.k]; },
					zOf, rows, true, false, [&plan](const TriRef& t) { return plan.Paint(t); });
		if(added > 0)
			out.notes << QString("%1 triangle(s) were added to the hull where parts of it met only along an edge "
								 "(the hull has to be one closed surface for the game).").arg(added);
	}
	if(mesh.Faces() == 0)
	{
		out.notes << "There is no hull yet: draw some triangles first.";
		return out;
	}
	if(modelSolid)
		out.notes << QString("The hull is the model %1 (with a wall round the rooms): %2 faces before shaping.")
				.arg(QFileInfo(plan.model.file).fileName()).arg(mesh.Faces());
	else
	{
		const bool angular = plan.hullStyle == "angular";
		const bool classic = plan.hullStyle == "classic";
		if(plan.slopedDecks && !classic && !angular && plan.Decks() > 1 && !SlopeBetweenDecks(plan, mesh, zOf, rows))
			out.notes << "The hull could not be sloped between the decks (it would not be a closed surface), so it "
						 "steps from deck to deck.";
		if(plan.smoothing > 0 && !angular && !classic)
			Subdivide(mesh);
	}

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
	// The model's own surface is the hull already (ModelHullMesh grew the
	// drawn part by the skin): not inflated or smoothed again, only its
	// slivers taken away (marching tetrahedra leave many; nothing may move
	// more than a twentieth of a cube for it).
	const bool angularField = !modelSolid && plan.hullStyle == "angular";
	if(modelSolid || angularField)
	{
		DumpHull(mesh, "raw");
		DecimateHull(mesh, 0, QVector<double>(mesh.v.size(), 1e9), QVector<double>() << plan.cellFeet / 80);
	}
	QVector<double> allowance(modelSolid || angularField ? 0 : mesh.v.size(), base + maxExtra);
	for(int i = 0; i < allowance.size(); ++i)
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

	if(!modelSolid && !angularField)
	{
		DumpHull(mesh, "raw");
		if(plan.hullStyle == "classic")
			OffsetMitred(mesh, plan.skinFeet);
		else
			InflateAndSmooth(mesh, plan.skinFeet, plan.smoothing, allowance);
	}
	DumpHull(mesh, "smoothed");
	out.rawTriangles = mesh.Faces();
	CollapseFlat(mesh);

	// Over the budget: simplified, the cheapest change to the shape first
	// (DecimateHull), keeping the surface closed, the paint edges where they
	// are and a wall round every room: the surface may come in towards a room
	// only as far as leaves half the skin (at least half a foot), and nowhere
	// further than 40 percent of a cell.
	const double maxMove = plan.cellFeet * 0.4;
	const double wall = qMax(0.5, plan.skinFeet * 0.5);
	// How far the surface may come in at each vertex: its distance to the
	// nearest room less the wall (no room near: only the move limits).
	auto inwardFor = [&](const IndexedMesh& m) {
		QVector<double> inward(m.v.size(), 1e9);
		for(int i = 0; i < m.v.size() && !roomPoints.isEmpty(); ++i)
		{
			const AuPoint& p = m.v[i];
			double best = 1e30;
			for(const RoomPoint& r : roomPoints)
			{
				const double dx = r.x - p.x, dy = r.y - p.y;
				const double dz = p.z < r.z0 ? r.z0 - p.z : (p.z > r.z1 ? p.z - r.z1 : 0.0);
				const double horizontal = qMax(0.0, sqrt(dx*dx + dy*dy) - plan.cellFeet * 0.5);
				best = qMin(best, sqrt(horizontal * horizontal + dz * dz));
			}
			inward[i] = qMax(0.0, best - wall);
		}
		return inward;
	};

	// Flat panels: the faces are grouped into panels facing one way each
	// (PanelPartition), the panels simplified within themselves with their
	// borders kept like paint edges, then pressed flat (FlattenPanels) and
	// merged. The panel goes into the paint index until the end.
	int regions = 0;
	if(plan.panels > 0 && mesh.Faces() > 0)
	{
		QVector<int> region;
		QVector<AuPoint> pn;
		QVector<double> pd;
		regions = PanelPartition(mesh, plan.panels, region, pn, pd);
		QVector<AuPoint> faceN(mesh.Faces());
		QVector<double> faceD(mesh.Faces());
		for(int fi = 0; fi < mesh.Faces(); ++fi)
		{
			faceN[fi] = pn[region[fi]];
			faceD[fi] = pd[region[fi]];
			mesh.paint[fi] = mesh.paint[fi] * regions + region[fi];
		}
		const int before = mesh.Faces();
		// Simplified within the panels, pressed flat, and again: pressing the
		// borders onto the lines where the panels meet puts their vertices in
		// a row, which the second round then takes away at no cost.
		// A flat panel on a curved hull stands off the curve in its middle (a
		// 50 ft panel on the test ellipsoid by about 7 ft), so the panels may
		// move the surface a whole cell; the rooms keep their wall all the
		// same. Three rounds: the ellipsoid in 100 panels, 1,054 faces after
		// one, 648 after three.
		const double panelMove = plan.cellFeet;
		const int rounds = 3;
		int simplified = 0;
		for(int round = 0; round < rounds; ++round)
		{
			QVector<AuPoint> fn(mesh.Faces());
			QVector<double> fd(mesh.Faces());
			for(int fi = 0; fi < mesh.Faces(); ++fi)
			{
				fn[fi] = pn[mesh.paint[fi] % regions];
				fd[fi] = pd[mesh.paint[fi] % regions];
			}
			DecimateHull(mesh, 0, inwardFor(mesh), QVector<double>() << panelMove, 0, &fn, &fd, regions);
			if(round == 0)
				simplified = mesh.Faces();
			FlattenPanels(mesh, regions, pn, pd, inwardFor(mesh), panelMove);
		}
		CollapseFlat(mesh, 1.0, 0.1);
		out.notes << QString("Flat panels: %1 panels; the hull went from %2 to %3 faces (%4 after pressing them "
							 "flat and merging).").arg(regions).arg(before).arg(simplified).arg(mesh.Faces());
	}

	DumpHull(mesh, "merged");
	const int merged = mesh.Faces();
	if(mesh.Faces() > plan.maxFaces)
	{
		QVector<double> inward = inwardFor(mesh);
		// Bigger ships may move further: the limit grows in steps up to one
		// percent of the hull's longest side, as far as the budget needs.
		AuPoint lo = mesh.v.first(), hi = lo;
		for(const AuPoint& p : mesh.v)
		{
			lo = lo.Min(p);
			hi = hi.Max(p);
		}
		const double longest = qMax(hi.x - lo.x, qMax(hi.y - lo.y, hi.z - lo.z));
		const double cap = qMax(maxMove, longest * 0.01);
		QVector<double> moves;
		for(double m = maxMove; m < cap * 1.0001; m *= 1.5)
			moves << qMin(m, cap);
		if(moves.last() < cap)
			moves << cap;
		for(int i = 0; i < inward.size(); ++i)
			inward[i] = qBound(0.0, inward[i], cap);
		double used = maxMove;
		auto checksum = [](const IndexedMesh& m) {
			double sum = 0;
			for(int i = 0; i < m.v.size(); ++i)
				sum += (i % 7 + 1) * (m.v[i].x + 3 * m.v[i].y + 7 * m.v[i].z);
			return QString("%1 v %2 f %3").arg(sum, 0, 'g', 15).arg(m.v.size()).arg(m.Faces());
		};
		if(!qgetenv("SHIPGEN_DEBUG").isEmpty())
			fprintf(stderr, "before decimate: %s inward %g\n", qPrintable(checksum(mesh)),
					std::accumulate(inward.begin(), inward.end(), 0.0));
		DecimateHull(mesh, plan.maxFaces, inward, moves, &used, 0, 0, regions);
		if(!qgetenv("SHIPGEN_DEBUG").isEmpty())
			fprintf(stderr, "after decimate: %s\n", qPrintable(checksum(mesh)));
		out.notes << QString("The hull had %1 faces after the flat areas were merged, more than the budget of %2; "
							 "simplified to %3 (moving the surface at most %4 ft, and keeping %5 ft of wall round "
							 "the rooms).").arg(merged).arg(plan.maxFaces).arg(mesh.Faces()).arg(used, 0, 'f', 1)
							 .arg(wall, 0, 'f', 1);
		out.thinned = true;
	}

	// Still over: merge areas that are nearly flat, a little more each time,
	// until it fits.
	const int simplified = mesh.Faces();
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
		out.notes << QString("The hull still had %1 faces, more than the budget of %2. "
							 "Nearly flat areas were merged too (up to %3 degrees), which leaves %4.")
						 .arg(simplified).arg(plan.maxFaces).arg(usedAngle).arg(mesh.Faces());
	}
	if(mesh.Faces() > plan.maxFaces)
		out.notes << QString("The hull still has %1 faces, more than the budget of %2. Less smoothing, a bigger cell "
							 "or a smaller ship would bring it down.").arg(mesh.Faces()).arg(plan.maxFaces);
	if(regions > 0)
		for(int& p : mesh.paint)
			p /= regions;		// The panel out of the paint index again.
	DumpHull(mesh, "final");
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

// Where a ray from 'from' along 'dir' meets triangle abc (Moller and
// Trumbore): the distance along it, or a negative number when it misses.
static double RayHitsTriangle(const AuPoint& from, const AuPoint& dir, const AuPoint& a, const AuPoint& b, const AuPoint& c)
{
	const AuPoint e1 = b - a, e2 = c - a;
	const AuPoint p = dir.Cross(e2);
	const double det = e1.Dot(p);
	if(fabs(det) < 1e-12)
		return -1;
	const AuPoint t = from - a;
	const double u = t.Dot(p) / det;
	if(u < 0 || u > 1)
		return -1;
	const AuPoint q = t.Cross(e1);
	const double v = dir.Dot(q) / det;
	if(v < 0 || u + v > 1)
		return -1;
	return e2.Dot(q) / det;
}

// The point of triangle abc closest to p (Ericson, Real-Time Collision
// Detection, 5.1.5).
static AuPoint ClosestOnTriangle(const AuPoint& p, const AuPoint& a, const AuPoint& b, const AuPoint& c)
{
	const AuPoint ab = b - a, ac = c - a, ap = p - a;
	const double d1 = ab.Dot(ap), d2 = ac.Dot(ap);
	if(d1 <= 0 && d2 <= 0)
		return a;
	const AuPoint bp = p - b;
	const double d3 = ab.Dot(bp), d4 = ac.Dot(bp);
	if(d3 >= 0 && d4 <= d3)
		return b;
	const double vc = d1 * d4 - d3 * d2;
	if(vc <= 0 && d1 >= 0 && d3 <= 0)
		return a + ab * (d1 / (d1 - d3));
	const AuPoint cp = p - c;
	const double d5 = ab.Dot(cp), d6 = ac.Dot(cp);
	if(d6 >= 0 && d5 <= d6)
		return c;
	const double vb = d5 * d2 - d1 * d6;
	if(vb <= 0 && d2 >= 0 && d6 <= 0)
		return a + ac * (d2 / (d2 - d6));
	const double va = d3 * d6 - d5 * d4;
	if(va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
		return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
	const double denom = 1.0 / (va + vb + vc);
	return a + ab * (vb * denom) + ac * (vc * denom);
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
		// Where the hull surface really is: looked for from the room wall
		// outwards at the door's foot, middle and top, the furthest of them
		// (a smoothed or model hull is not where the drawn cells and the
		// skin would put it; the guess is only kept if nothing is hit).
		AuScFaceList hullFaces;
		for(AuScParts::const_iterator it = design->Parts().constBegin(); it != design->Parts().constEnd(); ++it)
			if(it.value() && it.value()->IsHullMesh() && it.value()->GroupName() != "Frame")
			{
				AuScFaceList partFaces;
				it.value()->Faces(partFaces);
				hullFaces.AddFaces(partFaces);
			}
		double surface = boundary + sgn * plan.skinFeet;
		{
			double furthest = -1;
			for(double h : { z0 + 1.0, (z0 + doorTop) / 2, doorTop - .5 })
			{
				const AuPoint eye(xWall ? roomWall + sgn * .3 : mid.x(), xWall ? mid.y() : roomWall + sgn * .3, h);
				AuPoint hit = eye + outward * 80.0;
				if(hullFaces.Sight(eye, hit))
					furthest = qMax(furthest, xWall ? fabs(hit.x - roomWall) : fabs(hit.y - roomWall));
			}
			if(furthest > 0)
				surface = roomWall + sgn * furthest;
		}
		// The door itself sits half a skin inside the surface, as before.
		const double slab = surface - sgn * plan.skinFeet * 0.5;
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
		// The way down: a ramp where one fits (and is wanted), else a ladder,
		// or none at all for a docking door. Only where the hull is in the way
		// of the ladder does the clearing in front of the door reach down:
		// looked for straight below the ladder, from under the hull upwards.
		const bool wayDown = plan.hullDoorExit != "none";
		const double rampRise = z0 - (hullBottom - SHIPGEN_RAMP_BELOW_HULL);
		const bool rampExpected = wayDown && plan.hullDoorExit == "auto" && options.ramps
				&& rampRise >= .5 && rampRise <= SHIPGEN_RAMP_MAX_RISE;
		double clearBelow = 1e30;
		if(wayDown && !rampExpected)
		{
			const double lineX = xWall ? surface + sgn * SHIPGEN_LADDER_OUTSIDE : mid.x();
			const double lineY = xWall ? mid.y() : surface + sgn * SHIPGEN_LADDER_OUTSIDE;
			const AuPoint low(lineX, lineY, hullBottom - SHIPGEN_LADDER_BELOW_HULL - 1.0);
			AuPoint hit(lineX, lineY, z0);
			if(hullFaces.Sight(low, hit))
				clearBelow = qMin(hit.z - 1.0, z0);
		}
		quint16 doorId = 0;
		if(!AddHullDoor(*design, mn, mx, outward, roomWall, surface, room->group, doorId, room->meshId, clearBelow))
		{
			report.warnings << QString("The engine did not accept the hull door of '%1'.").arg(room->group);
			continue;
		}
		++report.doors;
		// A ramp down to the ground where the door is low enough, as at the
		// user's aircraft factory; otherwise a ladder (below).
		AuPoint rampTop, rampFoot;
		const bool ramp = rampExpected && AddDoorRamp(*design, mn, mx, outward, surface,
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
			if(wayDown && !ramp && pz - foot > .5)
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

	// Emitters, from a hull triangle at mid-deck height (PlanEmitter).
	// A contrail emitter: on the hull's aftmost surface straight aft of it,
	// facing aft. A weapon emitter: the nearest point of the hull; set on the
	// fire control stations after the furnishing.
	QVector<AuPoint> weaponEmitters;
	for(const PlanEmitter& e : plan.emitters)
	{
		const TriRef t(e.deck, e.cx, e.cy, e.k);
		const QPointF c = plan.TriCentre(t);
		const AuPoint from(c.x(), c.y(), plan.DeckFloor(e.deck) + plan.deckFeet / 2);
		if(e.weapon)
		{
			AuPoint best = from;
			double bestD = 1e30;
			for(const AuScTri& tri : hull.tris)
			{
				const AuPoint q = ClosestOnTriangle(from, AuPoint(tri.V0()), AuPoint(tri.V1()), AuPoint(tri.V2()));
				const double d = (q - from).Dot(q - from);
				if(d < bestD)
				{
					bestD = d;
					best = q;
				}
			}
			weaponEmitters << best;
			continue;
		}
		const AuPoint aft(-1, 0, 0);
		double farthest = -1;
		for(const AuScTri& tri : hull.tris)
			farthest = qMax(farthest, RayHitsTriangle(from, aft, AuPoint(tri.V0()), AuPoint(tri.V1()), AuPoint(tri.V2())));
		if(farthest < 0)
		{
			report.warnings << QString("A contrail emitter on deck %1 has no hull aft of it.").arg(e.deck + 1);
			continue;
		}
		// The icon is drawn in its own x-y plane, facing its +z: turned so
		// that +z points aft (which way the turn goes is checked, not
		// assumed), a hand's breadth behind the hull.
		AuMatrix4x4 facing;
		facing.RotateY(-PI / 2);
		if(facing.Project(AuPoint(0, 0, 1)).x - facing.Project(AuPoint(0, 0, 0)).x > 0)
		{
			facing = AuMatrix4x4();
			facing.RotateY(PI / 2);
		}
		AuMatrix4x4 at;
		at.Translate(from + aft * (farthest + 0.2));
		AuScPart* part = AuScPart::Create(AuScPart::Part_ContrailEmitter);
		AuScIcon* icon = dynamic_cast<AuScIcon*>(part);
		if(icon)
		{
			const double half = qMax(1.0, e.sizeFeet) / 2;
			icon->MakeScreen(AuPoint(-half, -half), AuPoint(half, half), AuMatrix4x4());
			icon->Transform(facing);
			icon->Transform(at);
			icon->SetFaceOpacity(.95f);
			if(icon->IsValid() && design->Parts().AddPart(*icon))
				++report.contrails;
		}
		delete part;
	}

	// Everything so far was laid out in plan space. Turn it to the game's
	// axes before the furnishing, because the engine's hull volume, which
	// the system volumes are worked out from, depends on the orientation.
	TurnToGameAxes(*design);
	// Moved, where needed, so that the origin is inside the hull. This comes
	// before the hull is measured: the engine measures its volume on a grid
	// from the origin (the Omega: 17,817 cubic meters, 18,320 moved), and the
	// FTL drive and the capacitor are sized by it. What is still in plan
	// space (the room boxes and the weapon emitters) gets the same move.
	const AuPoint originShift = OriginShift(*design);
	const bool shifted = originShift.x != 0 || originShift.y != 0 || originShift.z != 0;
	if(shifted)
	{
		AuMatrix4x4 move;
		move.LoadIdentity();
		move.Translate(originShift);
		design->Parts().Transform(move);
		report.notes << QString("The design was moved %1 ft so that its origin is inside the hull, as the game "
								"needs (the hull did not reach it%2).")
								.arg(QString::number(originShift.Length(), 'f', 1))
								.arg(originShift.z < 0 ? ": it begins above the floor of the lowest deck, which may be empty"
									: originShift.z > 0 ? ": it is all below the origin" : " from the side");
	}
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
			pr.min = pr.min + originShift;
			pr.max = pr.max + originShift;
			pr.voidId = dr.voidId;
			pr.meshId = dr.meshId;
			pr.volumeM3 = (dr.boxMax.x - dr.boxMin.x) * (dr.boxMax.y - dr.boxMin.y) * (dr.boxMax.z - dr.boxMin.z) * 0.0283168466;
			placed << pr;
		}
		const LayoutResult furnished = FurnishRooms(*design, brief, analysis, layout, placed);
		report.stations = furnished.stationsPlaced;
		// The weapon emitters, turned to the game's axes like the rest, one
		// to each fire control station and console in turn.
		if(!weaponEmitters.isEmpty())
		{
			const AuMatrix4x4 turn = QuarterTurnToFore();
			int next = 0;
			for(AuScParts::iterator it = design->Parts().begin(); it != design->Parts().end() && next < weaponEmitters.size(); ++it)
			{
				AuScPart* part = it.value();
				if(!part || (part->Type() != AuScPart::Part_StationFireControl && part->Type() != AuScPart::Part_ConsoleFireControl))
					continue;
				part->SetEmitLoc(turn.Project(weaponEmitters.at(next++)) + originShift);
				++report.weaponEmitters;
			}
			if(next < weaponEmitters.size())
				report.warnings << QString("%1 weapon emitter(s) left over: the game keeps one on each fire control "
										   "station, and there %2 %3.").arg(weaponEmitters.size() - next)
										   .arg(next == 1 ? "is" : "are").arg(next);
		}
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
	if(!qgetenv("SHIPGEN_DEBUG").isEmpty())
	{
		QMap<AuPoint, float> contrails;
		design->ContrailEmitters(contrails);
		for(QMap<AuPoint, float>::const_iterator it = contrails.constBegin(); it != contrails.constEnd(); ++it)
			fprintf(stderr, "contrail at %.1f %.1f %.1f, %.1f ft across\n", it.key().x, it.key().y, it.key().z, it.value());
		for(AuScParts::const_iterator it = design->Parts().constBegin(); it != design->Parts().constEnd(); ++it)
			if(it.value() && it.value()->Type() == AuScPart::Part_ConsoleFireControl)
			{
				const AuPoint e = it.value()->WeaponEmitLoc();
				fprintf(stderr, "weapon emitter at %.1f %.1f %.1f\n", e.x, e.y, e.z);
			}
	}
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

int RecommendedSlicesPerDeck(double deckFeet)
{
	// About one every 3 ft: three in a 10 ft deck. More only smooths the
	// judgement of a triangle; the deck itself stays one step of the hull.
	// Always an odd number: with an even one a deck the model fills half way
	// up has exactly half its heights inside, a tie with the default
	// 'threshold' of one half that tips either way.
	const int n = qMax(1, int(lround(deckFeet / 3.0)));
	return qBound(3, n % 2 ? n : n + 1, 9);
}

bool PlanFromModel(const QString& modelFile, const PlanFromModelOptions& options, TriPlan& plan,
		QStringList& notes, QString& error, PlanFromModelReport* report)
{
	const bool slices = options.method == ModelInside::Slices;
	auto step = [&](const QString& what, int done = 0, int total = 0) {
		if(options.progress && !options.progress(what, done, total))
		{
			error = "Stopped.";
			return false;
		}
		return true;
	};
	if(!step("Reading the model"))
		return false;
	AuScTriList tris;
	if(!LoadMeshFile(modelFile, tris, error))
		return false;
	if(tris.isEmpty())
	{
		error = QString("%1 has no triangles.").arg(modelFile);
		return false;
	}
	// The closed-solid method needs a closed surface, so the model is repaired
	// and thinned first. Slicing needs neither (SliceSolid).
	if(!slices)
	{
		// Tidied up first, the way the mesh tool does it (and shipgen reduce):
		// models from the internet have triangles with no area, doubles, faces
		// the wrong way round and holes, and an inside cannot be told without
		// a closed surface.
		if(!step("Repairing the model (as the mesh tool does)"))
			return false;
		// A repair is kept only when it leaves fewer broken edges: on some models
		// (the Moogle, after thinning) closing the holes makes things worse.
		auto brokenEdges = [](const AuScTriList& t) {
			const MeshSolidity m = AuditSolidity(t);
			return m.openEdges + m.foldedEdges + m.thickEdges - m.pinchedEdges;
		};
		auto repair = [&](const QString& when) {
			QStringList done;
			const AuScTriList repaired = RepairMesh(tris, &done);
			if(done.isEmpty())
				return;
			if(brokenEdges(repaired) <= brokenEdges(tris))
			{
				tris = repaired;
				notes << when + done.join("; ") + ".";
			}
			else
				notes << when + "the mesh tool's repair (" + done.join("; ") + ") left more broken edges than "
						"before, so the model was kept as it was.";
		};
		repair("Repaired the model: ");
		// A big model is thinned out next, the way shipgen reduce does it, and
		// tidied up again after (thinning opens holes).
		if(options.maxFaces > 0 && tris.size() > options.maxFaces)
		{
			const int before = tris.size();
			if(!step(QString("Thinning %1 faces to %2 (the longest step)").arg(before).arg(options.maxFaces)))
				return false;
			// The careful method takes a minute and a half on half a million
			// faces; a quick clustering pass down to five times the target first
			// leaves it little to do, and the clustering's blockiness is
			// smoothed over by the careful pass.
			if(tris.size() > options.maxFaces * 5)
			{
				const AuScTriList quick = SimplifyMesh(tris, options.maxFaces * 5, SimplifyMethod::Cluster);
				if(quick.size() > options.maxFaces)
					tris = quick;
			}
			tris = SimplifyMesh(tris, options.maxFaces, SimplifyMethod::Quadric);
			notes << QString("The model was thinned from %1 to %2 faces.").arg(before).arg(tris.size());
			repair("After thinning: ");
		}

		// Repair it only when it needs it, and keep the repair only when it helps:
		// on some closed models the repair does more harm than good.
		auto problems = [](const MeshSolidity& m) { return m.openEdges + m.foldedEdges + m.thickEdges - m.pinchedEdges; };
		if(!step("Checking the surface is closed"))
			return false;
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
			notes << QString("The model is not closed even after repair (%1 open, %2 thick, %3 folded edges), so its "
					"inside cannot be told everywhere: only its closed pieces count as hull. The mesh tool "
					"(shipgen-mesh) can show where.")
					.arg(solidity.openEdges).arg(solidity.thickEdges).arg(solidity.foldedEdges);
		else if(!solidity.Closed())
			notes << QString("The model is not quite closed (%1 open edges after repair); the inside is found by a "
					"vote of rays, so look the grid over.").arg(solidity.openEdges);
	}
	else if(!step(QString("Read %1 faces; slicing needs no repair or thinning").arg(tris.size())))
		return false;

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
	const bool turned = mx.y - mn.y > mx.x - mn.x;
	if(turned)
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
	// The length, or the number of decks asked for: the length that makes
	// the model that many decks high (each deck is one slice of it).
	double lengthFeet = options.lengthFeet;
	if(options.decks > 0 && mx.z - mn.z > 1e-9)
		lengthFeet = qMin(5000.0, options.decks * options.deckFeet * length / (mx.z - mn.z));
	const double scale = lengthFeet / length;
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
	if(!slices)
	{
		if(!step("Finding its pieces"))
			return false;
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
	// Slicing: the grid of the slices is an eighth of a cell (so that the
	// recommendation below can judge cells down to a quarter of this size),
	// coarser only for a model too big for 150 million squares.
	std::shared_ptr<SliceSolid> solidShared = std::make_shared<SliceSolid>();
	SliceSolid& solid = *solidShared;
	if(slices)
	{
		const AuPoint size = mx - mn;
		const double fine = SliceStep(mn, mx, options.cellFeet);
		const int seal = qMax(0, int(ceil(options.sealFeet / fine / 2 - 1e-9)));
		const int faces = tris.size();
		if(!solid.Build(tris, mn, mx, fine, seal, [&](int phase, int done, int total) {
				return step(phase == 0 ? QString("Slicing: %1 of %2 faces").arg(done).arg(total)
						: QString("Finding the inside of the slices: %1 of %2").arg(done).arg(total), done, total);
			}))
			return false;
		tris.clear();		// Not needed any more, and a big model holds a lot of memory.
		notes << QString("Sliced %1 faces into %2 slices of %3 ft, on squares of %3 ft, closing gaps narrower "
				"than about %4 ft (%5 squares of surface, %6 inside).")
				.arg(faces).arg(int(ceil(size.z / fine))).arg(fine, 0, 'f', 2).arg(2 * seal * fine, 0, 'f', 1)
				.arg(solid.SurfaceCells()).arg(solid.InsideCells());
	}
	const std::function<bool(const AuPoint&)> inModel = [&](const AuPoint& p) {
		if(slices)
			return solid.Inside(p);
		for(const SolidMesh& piece : pieces)
			if(piece.Contains(p))
				return true;
		return false;
	};
	// The heights in a deck at which the model is looked at: the slices of
	// it each plan triangle is judged by.
	const int perDeck = options.slicesPerDeck > 0 ? options.slicesPerDeck
			: RecommendedSlicesPerDeck(options.deckFeet);
	QVector<double> heights;
	for(int s = 0; s < perDeck; ++s)
		heights << (s + .5) / perDeck;

	// The grid: a cell of room round the model, and enough decks.
	plan = TriPlan();
	plan.name = QFileInfo(modelFile).completeBaseName();
	plan.cellFeet = options.cellFeet;
	plan.deckFeet = options.deckFeet;
	// Where the model came from and how it was placed, so that the hull can
	// be the model itself (ModelHullMesh); the sliced solid is kept for it.
	plan.model.file = QFileInfo(modelFile).absoluteFilePath();
	plan.model.yUp = options.yUp;
	plan.model.turn = turned;
	plan.model.flip = options.flip;
	plan.model.centre[0] = centre.x;
	plan.model.centre[1] = centre.y;
	plan.model.centre[2] = centre.z;
	plan.model.scale = scale;
	plan.model.sealFeet = options.sealFeet;
	plan.model.asHull = slices && options.modelAsHull;
	plan.model.sharp = true;		// New plans: the hull looks like the model (a choice on the hull page).
	if(slices)
		ModelSolids().insert(ModelKey(plan.model, plan.cellFeet), solidShared);
	const int width = int(ceil((mx.x - mn.x) / options.cellFeet)) + 2;
	int depth = int(ceil(qMax(fabs(mn.y), fabs(mx.y)) * 2 / options.cellFeet)) + 2;
	depth += depth % 2;		// Even, so that the mirror line is a cell edge.
	plan.Resize(width, depth);
	const int decks = qMax(1, int(ceil(mx.z / options.deckFeet - .2)));
	while(plan.Decks() < decks)
		plan.AddDeck();

	int hull = 0;
	for(int d = 0; d < plan.Decks(); ++d)
	{
		if(!step(QString("Filling in deck %1 of %2").arg(d + 1).arg(plan.Decks()), d, plan.Decks()))
			return false;
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
						for(double h : heights)
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
	}
	// Tidying: pieces of hull that touch nothing else and are smaller than
	// 'minPiece' triangles (fragments of fins, antennas and greebles, each of
	// which costs the hull faces and holds no room) are taken away, and a
	// single missing triangle with hull on all three sides is filled in.
	if(options.minPiece > 0 || options.fillPinholes)
	{
		if(!step("Tidying the grid"))
			return false;
		int filled = 0;
		if(options.fillPinholes)
			for(int d = 0; d < plan.Decks(); ++d)
				for(int cy = 0; cy < plan.Depth(); ++cy)
					for(int cx = 0; cx < plan.Width(); ++cx)
						for(int k = 0; k < 4; ++k)
						{
							const TriRef t(d, cx, cy, k);
							if(plan.Hull(t))
								continue;
							bool closed = true;
							for(int e = 0; e < 3 && closed; ++e)
							{
								const TriRef n = plan.Across(t, e);
								closed = plan.Valid(n) && plan.Hull(n);
							}
							if(closed)
							{
								plan.SetHull(t, true);
								++filled;
								++hull;
							}
						}
		int removedPieces = 0, removedTris = 0;
		if(options.minPiece > 0)
		{
			auto key = [&plan](const TriRef& t) {
				return ((t.deck * plan.Depth() + t.cy) * plan.Width() + t.cx) * 4 + t.k;
			};
			QVector<bool> seen(plan.Decks() * plan.Depth() * plan.Width() * 4, false);
			for(int d = 0; d < plan.Decks(); ++d)
				for(int cy = 0; cy < plan.Depth(); ++cy)
					for(int cx = 0; cx < plan.Width(); ++cx)
						for(int k = 0; k < 4; ++k)
						{
							const TriRef start(d, cx, cy, k);
							if(!plan.Hull(start) || seen[key(start)])
								continue;
							QVector<TriRef> piece;
							piece << start;
							seen[key(start)] = true;
							for(int q = 0; q < piece.size(); ++q)
							{
								const TriRef t = piece[q];
								QVector<TriRef> next;
								for(int e = 0; e < 3; ++e)
									next << plan.Across(t, e);
								next << TriRef(t.deck - 1, t.cx, t.cy, t.k) << TriRef(t.deck + 1, t.cx, t.cy, t.k);
								for(const TriRef& n : next)
									if(plan.Valid(n) && plan.Hull(n) && !seen[key(n)])
									{
										seen[key(n)] = true;
										piece << n;
									}
							}
							if(piece.size() < options.minPiece)
							{
								for(const TriRef& t : piece)
									plan.SetHull(t, false);
								++removedPieces;
								removedTris += piece.size();
								hull -= piece.size();
							}
						}
		}
		if(filled || removedPieces)
			notes << QString("Tidied the grid: %1 loose piece(s) of fewer than %2 triangles taken away (%3 "
					"triangles), %4 pinhole(s) filled.").arg(removedPieces).arg(options.minPiece).arg(removedTris)
					.arg(filled);
	}

	if(!hull)
	{
		error = QString("No part of %1 is thick enough for a %2 ft cell. Make it longer, or the cells smaller.")
				.arg(QFileInfo(modelFile).fileName()).arg(options.cellFeet);
		return false;
	}
	// How much of the model came through: the share of points spread through
	// the inside of the model that land on hull triangles. A slender model,
	// or one with fine parts, keeps little of itself on a grid of cells and
	// decks.
	QVector<AuPoint> samples;
	{
		const AuPoint size = mx - mn;
		const double step3 = qMax(.25, pow(size.x * size.y * qMax(size.z, 1.0) / 30000.0, 1.0 / 3));
		for(double x = mn.x + step3 / 2; x < mx.x; x += step3)
			for(double y = mn.y + step3 / 2; y < mx.y; y += step3)
				for(double z = mn.z + step3 / 2; z < mx.z; z += step3)
					if(inModel(AuPoint(x, y, z)))
						samples << AuPoint(x, y, z);
	}
	// Which triangle of a grid of 'cell' and 'deck' a point is in.
	auto triangleOf = [](const TriPlan& grid, double deck, const AuPoint& p) {
		const QPointF lo = grid.Corner(0, 0);
		const int cx = int(floor((p.x - lo.x()) / grid.cellFeet)), cy = int(floor((p.y - lo.y()) / grid.cellFeet));
		const QPointF c = grid.CellCentre(cx, cy);
		const double dx = p.x - c.x(), dy = p.y - c.y();
		const int k = fabs(dx) >= fabs(dy) ? (dx > 0 ? TRI_E : TRI_W) : (dy > 0 ? TRI_N : TRI_S);
		return TriRef(int(floor(p.z / deck)), cx, cy, k);
	};
	auto keptOnPlan = [&]() {
		int in = 0;
		for(const AuPoint& p : samples)
		{
			const TriRef t = triangleOf(plan, plan.deckFeet, p);
			if(plan.Valid(t) && plan.Hull(t))
				++in;
		}
		return samples.isEmpty() ? 1.0 : double(in) / samples.size();
	};
	const double kept = keptOnPlan();
	notes << QString("%1 by %2 cells of %3 ft, %4 decks; %5 hull triangles, holding about %6% of the model.")
			.arg(plan.Width()).arg(plan.Depth()).arg(options.cellFeet).arg(plan.Decks()).arg(hull)
			.arg(qRound(kept * 100));
	if(options.decks > 0)
		notes << QString("%1 decks asked for: the model is %2 ft long.").arg(options.decks).arg(qRound(lengthFeet));
	notes << QString("Each deck was judged at %1 heights (slices) through it.").arg(perDeck);
	if(report)
	{
		report->keptShare = kept;
		report->lengthFeet = lengthFeet;
		report->decks = plan.Decks();
	}

	// When much is lost, find the length that keeps more: a longer ship with
	// the same cells and decks is the same as smaller cells and decks on
	// this one. The triangles the sample points fall in are judged the way
	// the grid above was, from a little longer to four times, up to 2000 ft.
	if(!samples.isEmpty() && kept < .85)
	{
		auto keptAt = [&](double k) {
			TriPlan grid;
			grid.cellFeet = options.cellFeet / k;
			const double deck = options.deckFeet / k;
			grid.Resize(int(ceil((mx.x - mn.x) / grid.cellFeet)) + 2,
					int(ceil(qMax(fabs(mn.y), fabs(mx.y)) * 2 / grid.cellFeet)) + 2);
			QHash<qint64, bool> judged;
			int in = 0;
			for(const AuPoint& p : samples)
			{
				const TriRef t = triangleOf(grid, deck, p);
				const qint64 key = ((qint64(t.deck) * 100000 + t.cy) * 100000 + t.cx) * 4 + t.k;
				QHash<qint64, bool>::const_iterator it = judged.constFind(key);
				bool hullHere;
				if(it != judged.constEnd())
					hullHere = it.value();
				else
				{
					QPointF c[3];
					grid.TriCorners(TriRef(0, t.cx, t.cy, t.k), c);
					const QPointF middle = (c[0] + c[1] + c[2]) / 3.0;
					QPointF at[4] = { middle, middle, middle, middle };
					for(int i = 0; i < 3; ++i)
						at[i + 1] = middle + (c[i] - middle) * .6;
					int inside = 0, tests = 0;
					for(const QPointF& q : at)
						for(double h : heights)
						{
							++tests;
							if(inModel(AuPoint(q.x(), q.y(), deck * (t.deck + h))))
								++inside;
						}
					hullHere = inside >= options.threshold * tests && inside > 0;
					judged.insert(key, hullHere);
				}
				if(hullHere)
					++in;
			}
			return double(in) / samples.size();
		};
		double bestLength = 0, bestKept = kept;
		for(double k : { 1.5, 2.0, 3.0, 4.0 })
		{
			const double length = lengthFeet * k;
			if(length > 2000.0)
				break;
			if(!step(QString("Looking for a length that keeps more of the model: %1 ft").arg(qRound(length))))
				return false;
			// Only worth it for at least ten points more: the shortest length
			// that keeps enough, or else the best one tried.
			const double here = keptAt(k);
			if(here >= .85 && here >= kept + .1)
			{
				bestKept = here;
				bestLength = length;
				break;
			}
			if(here >= bestKept + .1 || (bestLength > 0 && here > bestKept + .05))
			{
				bestKept = here;
				bestLength = length;
			}
		}
		if(bestLength > 0)
		{
			const double k = bestLength / lengthFeet;
			const int w = int(ceil((mx.x - mn.x) * k / options.cellFeet)) + 2;
			int dpt = int(ceil(qMax(fabs(mn.y), fabs(mx.y)) * 2 * k / options.cellFeet)) + 2;
			dpt += dpt % 2;
			const int dk = qMax(1, int(ceil(mx.z * k / options.deckFeet - .2)));
			notes << QString("Recommended: about %1 ft long holds about %2% of the model (a grid of %3 by %4 cells, "
					"%5 decks), against %6% at %7 ft. The cells and decks stay %8 and %9 ft, which doors and "
					"headroom need; a longer ship makes the thin parts of the model thick enough for them.")
					.arg(qRound(bestLength)).arg(qRound(bestKept * 100)).arg(w).arg(dpt).arg(dk)
					.arg(qRound(kept * 100)).arg(qRound(lengthFeet)).arg(options.cellFeet).arg(options.deckFeet);
			if(report)
			{
				report->suggestedLength = bestLength;
				report->suggestedShare = bestKept;
				report->suggestedDecks = dk;
			}
		}
	}
	if(kept < .5)
		notes << QString("Only a little of the model came through (about %1%): most of it is thinner than a %2 ft "
				"cell or a %3 ft deck at %4 ft long. Make it longer, or lower 'Inside at least' (now %5).")
				.arg(qRound(kept * 100)).arg(options.cellFeet).arg(options.deckFeet).arg(lengthFeet)
				.arg(options.threshold);
	return true;
}
