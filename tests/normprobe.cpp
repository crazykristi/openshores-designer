// shipgen - probe: do a part's vertex normals agree with its faces?
#include <QCoreApplication>
#include <QTextStream>
#include "auglobal.h"
#include "dgdata.h"
#include "building.h"
#include "sohfile.h"
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
	if(!design)
		return 1;
	for(int a = 2; a < argc; ++a)
	{
		const AuScMeshBase* mesh = dynamic_cast<const AuScMeshBase*>(design->Parts().value(QString(argv[a]).toUShort()));
		if(!mesh)
			continue;
		AuScFaceList faces;
		mesh->Faces(faces);
		AuScTriList tris;
		faces.Triangles(tris);
		int same = 0, opposite = 0, across = 0;
		for(const AuScTri& t : tris)
		{
			AuPoint n = (AuPoint(t.V1()) - AuPoint(t.V0())).Cross(AuPoint(t.V2()) - AuPoint(t.V0()));
			if(n.Length() < 1e-9)
				continue;
			n.Unit();
			for(int i = 0; i < 3; ++i)
			{
				const double d = n.Dot(t.Vertex(i).m_N);
				if(d > .5) ++same; else if(d < -.5) ++opposite; else ++across;
			}
		}
		out << "part " << argv[a] << ": vertex normals along the winding " << same << ", against " << opposite
			<< ", across " << across << "\n";
	}
	return 0;
}
