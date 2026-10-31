// shipgen - probe: which vertices of a part are outside the hull part.
#include <QCoreApplication>
#include <QTextStream>
#include <QSet>
#include "auglobal.h"
#include "dgdata.h"
#include "building.h"
#include "sohfile.h"
#include "audit.h"
#include "auscparts.h"

int main(int argc, char* argv[])
{
	QCoreApplication app(argc, argv);
	QTextStream out(stdout);
	static DgData g_data;
	if(!gp_AuGlobal)
		gp_AuGlobal = new AuGlobal("Shipgen", "Hazeron Freehold", AuGlobal::WebCGI, &g_data);
	InstallIndustryTable(&g_data);
	AuDesign* design = LoadDesign(argv[1]);
	const AuScPart* hull = design->Parts().value(QString(argv[2]).toUShort());
	const AuScPart* part = design->Parts().value(QString(argv[3]).toUShort());
	AuScFaceList faces;
	hull->Faces(faces);
	AuScTriList tris;
	faces.Triangles(tris);
	SolidMesh solid(tris);
	solid.SetOpenings(40);
	QSet<quint32> ids;
	part->VertexIds(ids);
	for(quint32 id : ids)
	{
		const AuPoint v = part->VertexLoc(id);
		bool on = false;
		if(!solid.Contains(v, &on) && !on)
			out << QString("outside %1 %2 %3\n").arg(v.x, 0, 'f', 2).arg(v.y, 0, 'f', 2).arg(v.z, 0, 'f', 2);
	}
	// Points inside the room that the hull test calls outside.
	AuScFaceList rf;
	part->Faces(rf);
	AuScTriList rt;
	rf.Triangles(rt);
	SolidMesh room(rt);
	room.SetOpenings(64);
	AuPoint mn(1e30,1e30,1e30), mx(-1e30,-1e30,-1e30);
	for(quint32 id : ids) { mn = mn.Min(part->VertexLoc(id)); mx = mx.Max(part->VertexLoc(id)); }
	int n = 0, bad = 0;
	for(double x = mn.x + 1; x < mx.x; x += 2.5)
		for(double y = mn.y + 1; y < mx.y; y += 2.5)
			for(double z = mn.z + 1; z < mx.z; z += 2.5)
			{
				const AuPoint p(x, y, z);
				bool on = false;
				if(!room.Contains(p, &on) || on)
					continue;
				++n;
				if(!solid.Contains(p, &on) && !on)
				{
					if(bad++ < 15)
						out << QString("sample outside %1 %2 %3\n").arg(x).arg(y).arg(z);
				}
			}
	out << "samples " << n << " outside " << bad << " room has inside " << room.HasInside() << "\n";
	return 0;
}
