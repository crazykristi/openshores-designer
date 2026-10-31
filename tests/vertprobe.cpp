// shipgen - probe: the vertices of some parts of a blueprint.
#include <QCoreApplication>
#include <QTextStream>
#include <QSet>
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
	if(argc < 3)
	{
		out << "usage: vertprobe <design.SoH> <part>...\n";
		return 2;
	}
	AuDesign* design = LoadDesign(argv[1]);
	if(!design)
		return 1;
	for(int a = 2; a < argc; ++a)
	{
		const AuScPart* p = design->Parts().value(QString(argv[a]).toUShort());
		if(!p)
			continue;
		QSet<quint32> ids;
		p->VertexIds(ids);
		out << "part " << argv[a] << " " << p->TypeName() << "\n";
		QList<quint32> list = ids.values();
		std::sort(list.begin(), list.end());
		for(quint32 id : list)
		{
			const AuPoint v = p->VertexLoc(id);
			out << QString("  %1 %2 %3\n").arg(v.x, 0, 'f', 2).arg(v.y, 0, 'f', 2).arg(v.z, 0, 'f', 2);
		}
	}
	return 0;
}
