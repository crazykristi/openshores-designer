//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - what the helm and the captain can see from their seats.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
//
#include "view.h"

#include <cmath>

#include "auquad.h"
#include "auscparts.h"
#include "austation.h"

namespace {

// What a ray can meet. Voids, jigs, paths and the like are not drawn.
bool Drawn(const AuScPart* part)
{
	if(!part || part->IsOmitted())
		return false;
	if(part->IsRoomVoid() || part->IsHullVoid() || part->IsEnvironmentVoid() || part->IsPath())
		return false;
	const int t = part->Type();
	return t != AuScPart::Part_DoorJig && t != AuScPart::Part_WindowJig && t != AuScPart::Part_ContrailEmitter &&
			t != AuScPart::Part_Sketch && t != AuScPart::Part_LaunchTubeJig && t != AuScPart::Part_CuttingJig &&
			t != AuScPart::Part_DecalJig;
}

bool Watched(const AuScPart* part)
{
	const int t = part ? part->Type() : 0;
	return t == AuScPart::Part_StationHelm || t == AuScPart::Part_StationCommand ||
			t == AuScPart::Part_ConsoleHelm || t == AuScPart::Part_ConsoleCommand;
}

struct Target
{
	quint16			id = 0;
	QString			label;
	int				kind = 0;		// 0 blocks, 1 glass, 2 hull.
	AuScFaceList	faces;
	AuPoint			mn;
	AuPoint			mx;
};

// Where a ray from 'a' along unit 'd' meets the box, or a negative value.
double BoxEntry(const AuPoint& a, const AuPoint& d, const AuPoint& mn, const AuPoint& mx, double maxDist)
{
	double t0 = 0, t1 = maxDist;
	const double o[3] = { a.x, a.y, a.z }, v[3] = { d.x, d.y, d.z };
	const double lo[3] = { mn.x, mn.y, mn.z }, hi[3] = { mx.x, mx.y, mx.z };
	for(int i = 0; i < 3; ++i)
	{
		if(fabs(v[i]) < 1e-12)
		{
			if(o[i] < lo[i] || o[i] > hi[i])
				return -1;
			continue;
		}
		double ta = (lo[i] - o[i]) / v[i], tb = (hi[i] - o[i]) / v[i];
		if(ta > tb)
			std::swap(ta, tb);
		t0 = qMax(t0, ta);
		t1 = qMin(t1, tb);
		if(t0 > t1)
			return -1;
	}
	return t0;
}

}

QList<StationView> CheckStationViews(const AuScParts& parts, double eyeAboveSeat)
{
	QList<StationView> views;

	// Everything a ray can meet, once.
	QList<Target> targets;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* part = it.value();
		if(!Drawn(part))
			continue;
		Target t;
		t.id = it.key();
		part->Faces(t.faces);
		if(t.faces.isEmpty())
			continue;
		t.kind = part->IsWindow() ? 1 : (part->IsHullMesh() && part->GroupName() != "Frame") ? 2 : 0;
		t.label = part->TypeName() + (part->GroupName().isEmpty() ? QString() : QString(" [%1]").arg(part->GroupName()));
		t.mn = AuPoint(1e30, 1e30, 1e30);
		t.mx = AuPoint(-1e30, -1e30, -1e30);
		for(const AuScFace* f : t.faces)
			for(int v = 0; v < f->VertexCount(); ++v)
			{
				t.mn = t.mn.Min(AuPoint(f->Vertex(v)));
				t.mx = t.mx.Max(AuPoint(f->Vertex(v)));
			}
		const AuPoint pad(.05, .05, .05);
		t.mn = t.mn - pad;
		t.mx = t.mx + pad;
		targets << t;
	}

	const double maxDist = 2000;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		// Consoles and integrated desks are not AuScStations; the part itself
		// knows where its operator is and where its screen is.
		const AuScPart* station = it.value();
		if(!station || station->IsOmitted() || !Watched(station))
			continue;
		const AuStation op = station->OperatorStation(AuPoint(0, 0, 1));
		const AuMatrix4x4& m = op.OpM();
		const AuPoint seat = m.Project(0, 0, 0);
		// Standing, the operator's place is on the floor; sitting, on the seat.
		const bool standing = op.OccPos() == AuStation::Occupant_Stand || op.OccPos() == AuStation::Occupant_StandStationary;
		StationView view;
		view.partId = it.key();
		view.type = station->TypeName();
		view.group = station->GroupName();
		view.eye = seat + AuPoint(0, 0, standing ? 5.2 : eyeAboveSeat);
		// Under a low ceiling (a fighter's cockpit) the head stays under it:
		// an eye above the ceiling would see over every wall.
		{
			const AuPoint from = seat + AuPoint(0, 0, .1);
			double ceiling = 1e30;
			for(const Target& t : targets)
			{
				if(t.kind != 0 || t.id == it.key())
					continue;
				AuPoint at = from + AuPoint(0, 0, 50);
				if(BoxEntry(from, AuPoint(0, 0, 1), t.mn, t.mx, 50) >= 0 && t.faces.Sight(from, at))
					ceiling = qMin(ceiling, at.z);
			}
			if(view.eye.z > ceiling - .3)
				view.eye.z = qMax(seat.z + .5, ceiling - .3);
		}
		AuQuad screen;
		AuPoint ahead = station->ScreenQuad(screen) ? (screen.vll + screen.vul + screen.vur + screen.vlr) * .25 - seat
				: m.Project(0, 1, 0) - seat;
		ahead.z = 0;
		if(ahead.Length() < 1e-6)
			ahead = AuPoint(0, 1, 0);
		ahead.Unit();
		view.facing = ahead;
		const AuPoint side(-ahead.y, ahead.x, 0);

		// The bands: yaw from ahead (degrees, either way) and pitch.
		struct Band { const char* name; double yaw0, yaw1, pitch0, pitch1; };
		const Band bands[] = {
			{ "ahead", 0, 30, -5, 30 },
			{ "sides", 30, 100, -5, 30 },
			{ "up", 0, 180, 40, 85 },
			{ "down ahead", 0, 30, -25, -10 },
		};
		for(const Band& b : bands)
		{
			ViewSector sector;
			sector.name = b.name;
			for(double pitch = b.pitch0; pitch <= b.pitch1 + 1e-6; pitch += 5)
				for(double yaw = b.yaw0; yaw <= b.yaw1 + 1e-6; yaw += 5)
					for(int lr = (yaw == 0 ? 1 : -1); lr <= 1; lr += 2)
					{
						const double ry = yaw * M_PI / 180 * lr, rp = pitch * M_PI / 180;
						AuPoint d = (ahead * cos(ry) + side * sin(ry)) * cos(rp) + AuPoint(0, 0, sin(rp));
						d.Unit();
						const AuPoint end = view.eye + d * maxDist;
						double best = maxDist;
						const Target* hit = 0;
						for(const Target& t : targets)
						{
							const double entry = BoxEntry(view.eye, d, t.mn, t.mx, maxDist);
							if(entry < 0 || entry >= best)
								continue;
							AuPoint at = end;
							if(t.faces.Sight(view.eye, at))
							{
								const double dist = (at - view.eye).Length();
								// The station's own seat, where the eye is, does not count.
								if(dist < best && !(t.id == it.key() && dist < .3))
								{
									best = dist;
									hit = &t;
								}
							}
						}
						++sector.rays;
						if(!hit)
							++sector.open;
						else if(hit->kind == 1)
							++sector.glass;
						else if(hit->kind == 2)
							++sector.hull;
						else
							sector.blockers[hit->id == it.key() ? QString("its own console") : QString("%1 (part %2)")
									.arg(hit->label).arg(hit->id)]++;
					}
			view.sectors << sector;
		}
		views << view;
	}
	return views;
}
