// Geometry probe: verifies how the Hazeron design primitives behave so the
// generator can rely on them. Not part of the shipping tool.
#include <QCoreApplication>
#include <QTextStream>
#include <cstdio>

#include "auscparts.h"
#include "auscdesign.h"
#include "audesign.h"
#include "aufilever.h"

static void dumpBox(const char* what, const AuScPart* part)
{
	AuPoint min(1e30, 1e30, 1e30), max(-1e30, -1e30, -1e30);
	QSet<quint32> ids;
	part->VertexIds(ids);
	for(QSet<quint32>::const_iterator it = ids.constBegin(); it != ids.constEnd(); ++it)
	{
		AuScPoint p = part->VertexLoc(*it);
		min = min.Min(p);
		max = max.Max(p);
	}
	QTextStream(stdout) << what << ": min(" << min.x << "," << min.y << "," << min.z << ") max("
						<< max.x << "," << max.y << "," << max.z << ")  verts=" << ids.size() << "\n";
}

int main(int argc, char* argv[])
{
	QCoreApplication app(argc, argv);
	QTextStream out(stdout);

	// --- Room void: box from (10,20,30) to (50,60,42) -------------------------
	{
		AuPointArray bnd;
		bnd.append(AuPoint(10, 20, 30));
		bnd.append(AuPoint(50, 20, 30));
		bnd.append(AuPoint(50, 60, 30));
		bnd.append(AuPoint(10, 60, 30));

		AuMatrix4x4 plane;
		plane.LoadIdentity();

		AuScRoomVoid rv;
		rv.MakeExtrusion(bnd, AuPoint(0, 0, 12), plane, 1, true);
		rv.SetGroupName("Test Room");
		out << "room void valid=" << rv.IsValid() << " group=" << rv.GroupName() << "\n";
		dumpBox("  roomvoid", &rv);
	}

	// --- Room mesh: same box, reversed normals -------------------------------
	{
		AuPointArray bnd;
		bnd.append(AuPoint(10, 20, 30));
		bnd.append(AuPoint(50, 20, 30));
		bnd.append(AuPoint(50, 60, 30));
		bnd.append(AuPoint(10, 60, 30));

		AuMatrix4x4 plane;
		plane.LoadIdentity();

		AuScRoom rm;
		rm.MakeExtrusion(bnd, AuPoint(0, 0, 12), plane, 1);
		rm.SetGroupName("Test Room");
		out << "room mesh valid=" << rm.IsValid() << "\n";
		dumpBox("  room", &rm);
	}

	// --- Station on the -Y wall (XZ plane) of that box -----------------------
	{
		// Wall plane: origin at the +X end, x-axis pointing -X, y-axis pointing +Z,
		// so the plane normal (+Z local) points +Y, i.e. into the room.
		AuMatrix4x4 wall;
		wall.Translate(AuPoint(50, 20, 30));
		wall.RotateToPlane(AuPoint(-40, 0, 0), AuPoint(0, 0, 12));

		// Corners are given in design space.
		AuPoint c1(50, 20, 30);
		AuPoint c2(10, 20, 42);

		AuScStationHelm st;
		AuPoint opLoc(30, 23, 36);
		AuPoint opFacing(30, 22, 36);
		st.MakeStation(AuScStation::Operator_Sit, opLoc, opFacing, c1, c2, wall);
		out << "station valid=" << st.IsValid() << " type=" << st.TypeName()
			<< " stationType=" << int(st.StationType()) << "\n";
		dumpBox("  station", &st);
		out << "  isCommand=" << st.IsCommandStation() << " isControl=" << st.IsControlStation() << "\n";
	}

	// --- Console icon --------------------------------------------------------
	{
		AuMatrix4x4 wall;
		wall.Translate(AuPoint(10, 20, 30));
		wall.RotateToPlane(AuPoint(40, 0, 0), AuPoint(0, 0, 12));
		AuScConsoleHelm con;
		con.MakeIcon(AuPoint(30, 21, 36), 0.f, wall);
		out << "console valid=" << con.IsValid() << " isControl=" << con.IsControlStation() << "\n";
		dumpBox("  console", &con);
	}

	// --- Berth icon ----------------------------------------------------------
	{
		AuMatrix4x4 plane;
		plane.LoadIdentity();
		AuScBerthCrew b;
		b.MakeIcon(AuPoint(5, 5, 31), 0.f, plane);
		out << "berth valid=" << b.IsValid() << " isBerth=" << b.IsBerth() << "\n";
		dumpBox("  berth", &b);
	}

	out << flush;
	return 0;
}
