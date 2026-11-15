//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - holes round the windows (see windowleak.h).
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "windowleak.h"

#include <QSet>
#include <QVector>
#include <algorithm>
#include <cmath>

#include "auscface.h"
#include "auscparts.h"
#include "interior.h"

namespace {

enum Kind { Glass, HullFace, Other };

struct Tri
{
	AuPoint		v[3];
	AuPoint		out;			// Out of the solid, for hull faces.
	Kind		kind;
	quint16		part;
	int			face;
};

QVector<Tri> CollectTris(const AuDesign& design)
{
	QVector<Tri> tris;
	const AuScParts& parts = design.Parts();
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* part = it.value();
		if(!part || part->IsOmitted() || part->IsRoomVoid() || part->IsHullVoid() || part->IsPath() || part->IsDecal())
			continue;
		// A room is a kind of hull part to the engine; its walls are walls.
		const Kind kind = part->IsWindow() ? Glass : part->IsRoomMesh() ? Other : part->IsHullMesh() ? HullFace : Other;
		AuScFaceList faces;
		part->Faces(faces);
		const double outSign = kind == HullFace ? (SignedVolume(faces) > 0 ? 1.0 : -1.0) : 1.0;
		for(int fi = 0; fi < faces.size(); ++fi)
		{
			AuScTriList list;
			faces.at(fi)->Triangles(list);
			for(const AuScTri& t : list)
			{
				Tri tri;
				tri.v[0] = t.V0();
				tri.v[1] = t.V1();
				tri.v[2] = t.V2();
				tri.out = (tri.v[1] - tri.v[0]).Cross(tri.v[2] - tri.v[0]) * outSign;
				if(tri.out.Length() < 1e-12)
					continue;
				tri.kind = kind;
				tri.part = it.key();
				tri.face = fi;
				tris << tri;
			}
		}
	}
	return tris;
}

// The first thing a ray meets: -1 for nothing.
int FirstHit(const QVector<Tri>& tris, const AuPoint& o, const AuPoint& d, double& dist)
{
	int best = -1;
	dist = 1e30;
	for(int i = 0; i < tris.size(); ++i)
	{
		const Tri& t = tris.at(i);
		const AuPoint e1 = t.v[1] - t.v[0], e2 = t.v[2] - t.v[0], p = d.Cross(e2);
		const double det = e1.Dot(p);
		if(fabs(det) < 1e-12)
			continue;
		const double inv = 1.0 / det;
		const AuPoint s = o - t.v[0];
		const double u = s.Dot(p) * inv;
		if(u < 0 || u > 1)
			continue;
		const AuPoint q = s.Cross(e1);
		const double v = d.Dot(q) * inv;
		if(v < 0 || u + v > 1)
			continue;
		const double at = e2.Dot(q) * inv;
		if(at > 1e-4 && at < dist)
		{
			dist = at;
			best = i;
		}
	}
	return best;
}

QString Text(const AuPoint& p)
{
	return QString("(%1 %2 %3)").arg(p.x, 0, 'f', 1).arg(p.y, 0, 'f', 1).arg(p.z, 0, 'f', 1);
}

} // namespace

WindowLeaks FindWindowLeaks(const AuDesign& design, int grid, const QStringList& skipRooms)
{
	WindowLeaks result;
	const QVector<Tri> tris = CollectTris(design);
	const AuScParts& parts = design.Parts();
	const int eyesWanted = grid > 10 ? 6 : 4;
	QSet<QPair<quint16, int> > inside;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* window = it.value();
		if(!window || !window->IsWindow() || skipRooms.contains(window->GroupName()))
			continue;
		const AuScPart* room = 0;
		for(AuScParts::const_iterator r = parts.constBegin(); r != parts.constEnd() && !room; ++r)
			if(r.value() && r.value()->IsRoomVoid() && !r.value()->AssociatedPartId() && r.value()->GroupName() == window->GroupName())
				room = r.value();
		if(!room)
			continue;
		++result.windows;
		AuPoint wmn, wmx, rmn, rmx;
		window->ModelExtents(wmn, wmx);
		room->ModelExtents(rmn, rmx);
		const double eyeZ = qMin(rmn.z + 5.5, rmx.z - .3);
		// Only what lies near the window and its room can be met first; a
		// ray that would go further counts as meeting nothing, a leak either way.
		const double reach = 30;
		const AuPoint bmn = wmn.Min(rmn) - AuPoint(reach, reach, reach), bmx = wmx.Max(rmx) + AuPoint(reach, reach, reach);
		QVector<Tri> near;
		for(const Tri& t : tris)
		{
			const AuPoint tmn = t.v[0].Min(t.v[1]).Min(t.v[2]), tmx = t.v[0].Max(t.v[1]).Max(t.v[2]);
			if(tmx.x >= bmn.x && tmn.x <= bmx.x && tmx.y >= bmn.y && tmn.y <= bmx.y && tmx.z >= bmn.z && tmn.z <= bmx.z)
				near << t;
		}
		// Places to look from: in the room, at eye height, nearest the window.
		const AuPoint wc = (wmn + wmx) * .5;
		QList<QPair<double, AuPoint> > eyes;
		for(int i = 0; i <= 8; ++i)
			for(int j = 0; j <= 8; ++j)
			{
				const AuPoint e(rmn.x + (rmx.x - rmn.x) * (i + .5) / 9.5, rmn.y + (rmx.y - rmn.y) * (j + .5) / 9.5, eyeZ);
				if(room->IsInsideVoid(e))
					eyes << qMakePair(fabs(e.x - wc.x) + fabs(e.y - wc.y), e);
			}
		std::sort(eyes.begin(), eyes.end(), [](const QPair<double, AuPoint>& a, const QPair<double, AuPoint>& b) {
			return a.first < b.first;
		});
		while(eyes.size() > eyesWanted)
			eyes.removeLast();
		int here = 0, hereRays = 0;
		AuPoint where;
		const double m = 2.0;
		for(const QPair<double, AuPoint>& e : eyes)
			for(int i = 0; i <= grid; ++i)
				for(int j = 0; j <= grid; ++j)
					for(int k = 0; k <= 1; ++k)
					{
						const AuPoint target(wmn.x - m + (wmx.x - wmn.x + 2 * m) * (k ? i : j) / grid,
								wmn.y - m + (wmx.y - wmn.y + 2 * m) * (k ? j : i) / grid,
								wmn.z - m + (wmx.z - wmn.z + 2 * m) * (k ? (i + j) % (grid + 1) : j) / grid);
						AuPoint d = target - e.second;
						if(d.Length() < .5)
							continue;
						d.Unit();
						double dist;
						// Near first; past it (seldom), the whole design.
						const QVector<Tri>* list = &near;
						int hit = FirstHit(near, e.second, d, dist);
						if(hit < 0)
						{
							list = &tris;
							hit = FirstHit(tris, e.second, d, dist);
						}
						++hereRays;
						const bool fromInside = hit >= 0 && list->at(hit).kind == HullFace && list->at(hit).out.Dot(d) > 0;
						if(hit < 0)
							++result.escapes;
						if(fromInside)
							inside.insert(qMakePair(list->at(hit).part, list->at(hit).face));
						if(hit < 0 || fromInside)
						{
							if(!here)
								where = hit < 0 ? target : e.second + d * dist;
							++here;
						}
					}
		result.rays += hereRays;
		result.leaks += here;
		if(here)
		{
			++result.leaky;
			result.lines << QString("window %1 [%2]: %3 of %4 rays see out past the glass, e.g. at %5")
					.arg(it.key()).arg(window->GroupName()).arg(here).arg(hereRays).arg(Text(where));
		}
	}
	result.insideFaces = inside.values();
	std::sort(result.insideFaces.begin(), result.insideFaces.end());
	return result;
}

int SealWindowLeaks(AuDesign& design, QStringList& notes, const QStringList& skipRooms)
{
	// Each face turned round and moved in a little along its own normal, so
	// that from inside it is seen instead of the sky. Looked for again with
	// other rays after each round: what one grid of rays misses, the next
	// may see.
	const double in = .03;
	int total = 0, windows = 0;
	for(int grid : { 16, 13, 11 })
	{
		const WindowLeaks leaks = FindWindowLeaks(design, grid, skipRooms);
		windows = qMax(windows, leaks.leaky);
		if(leaks.insideFaces.isEmpty())
			break;
		AuScFaceList lining;
		QHash<quint16, double> outSigns;
		for(const QPair<quint16, int>& pf : leaks.insideFaces)
		{
			const AuScPart* part = design.Parts().value(pf.first);
			if(!part)
				continue;
			AuScFaceList faces;
			part->Faces(faces);
			if(pf.second >= faces.size())
				continue;
			if(!outSigns.contains(pf.first))
				outSigns.insert(pf.first, SignedVolume(faces) > 0 ? 1.0 : -1.0);
			const AuScFace* face = faces.at(pf.second);
			// Out of the hull, as the check works it out: the right-handed
			// normal of the face's triangles with the part's sign.
			AuScTriList tl;
			face->Triangles(tl);
			AuPoint n(0, 0, 0);
			for(const AuScTri& t : tl)
				n += (AuPoint(t.V1()) - AuPoint(t.V0())).Cross(AuPoint(t.V2()) - AuPoint(t.V0()));
			if(n.Length() < 1e-9)
				continue;
			n.Unit();
			const AuPoint out = n * outSigns.value(pf.first);
			AuScFaceList one;
			one.AddFace(*face);
			one.ReverseTriangleFacing();
			one.ReverseVertexNormals();
			AuMatrix4x4 move;
			move.Translate(out * -in);
			one.Transform(move);
			lining.AddFaces(one);
		}
		if(lining.isEmpty())
			break;
		AuScMesh mesh;
		mesh.AddFaces(lining);
		mesh.SetGroupName("Window Lining");
		if(!mesh.IsValid() || !design.Parts().AddPart(mesh))
		{
			notes << "The lining behind the windows could not be added.";
			break;
		}
		total += lining.size();
	}
	if(total)
		notes << QString("Window lining: %1 faces of the hull that were seen from inside through %2 windows are covered.")
				.arg(total).arg(windows);
	return total;
}
