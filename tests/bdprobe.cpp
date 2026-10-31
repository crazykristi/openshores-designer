//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - probe: what is inside a building blueprint.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include <QCoreApplication>
#include <QTextStream>

#include "aufilever.h"
#include "auglobal.h"
#include "building.h"
#include "dgdata.h"
#include "aubddesign.h"
#include "aubdreport.h"
#include "auscparts.h"

int main(int argc, char* argv[])
{
	QCoreApplication app(argc, argv);
	QTextStream out(stdout);
	static DgData g_data;
	if(!gp_AuGlobal)
		gp_AuGlobal = new AuGlobal("Shipgen", "Hazeron Freehold", AuGlobal::WebCGI, &g_data);
	InstallIndustryTable(&g_data);
	if(argc < 2)
	{
		out << "usage: bdprobe <design.SoH>\n";
		return 2;
	}

	AuDesign* design = AuDesign::LoadFromFile(argv[1], SOH_FILE_TYPE);
	if(!design)
	{
		out << "could not load\n";
		return 1;
	}
	AuBdDesign* bd = dynamic_cast<AuBdDesign*>(design);
	if(!bd)
	{
		out << "not a building\n";
		return 1;
	}

	out << "Name       : " << bd->Name() << "\n";
	out << "Structure  : " << int(bd->Structure()) << "\n";
	out << "Industry   : " << int(bd->IndustryId()) << "\n";
	out << "Description: " << bd->Description() << "\n\n";

	const AuScParts& parts = bd->Parts();
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p)
			continue;
		AuPoint min, max;
		p->ModelExtents(min, max);
		out << QString("part %1  %2  group='%3'")
				.arg(it.key()).arg(p->TypeName()).arg(p->GroupName());
		if(p->IsHullMesh() || p->IsSite() || p->IsRoomMesh() || p->IsRoomVoid())
			out << QString("  extents %1,%2,%3 .. %4,%5,%6")
					.arg(qRound(min.x)).arg(qRound(min.y)).arg(qRound(min.z))
					.arg(qRound(max.x)).arg(qRound(max.y)).arg(qRound(max.z));
		if(p->Type() == AuScPart::Part_GroundVehicleParkingSpot || p->Type() == AuScPart::Part_SpaceVehicleParkingSpot)
		{
			AuPoint loc = p->Center();
			out << QString("  at %1,%2,%3").arg(qRound(loc.x)).arg(qRound(loc.y)).arg(qRound(loc.z));
		}
		out << "\n";
	}

	// How many faces does the blueprint have?
	{
		quint32 faces = 0, tris = 0;
		for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
		{
			AuScFaceList f;
			it.value()->Faces(f);
			faces += f.size();
			AuScTriList t;
			f.Triangles(t);
			tris += t.size();
		}
		out << "\nfaces          : " << faces << "\n";
		out << "triangles      : " << tris << "\n";
	}

	bd->RefreshObstructionsCache();
	AuBdReport report;
	bd->ReportDesign(report);
	out << "\n-- The report --\n";
	out << "hull volume   : " << report.HullVolume() << " m3\n";
	out << "site extents  : " << report.SiteMin().x << "," << report.SiteMin().y << " .. "
			<< report.SiteMax().x << "," << report.SiteMax().y << "\n";
	out << "office volume : " << report.OfficeVolume() << "  capacity " << report.OfficeCapacity() << "\n";
	out << "shop volume   : " << report.ShopVolume() << "  capacity " << report.ShopCapacity() << "\n";
	out << "store volume  : " << report.StoreVolume() << "  capacity " << report.StoreCapacity() << "\n";
	out << "home large    : " << report.HomeLargeVolume() << "  capacity " << report.HomeLargeCapacity() << "\n";
	out << "home medium   : " << report.HomeMediumVolume() << "  capacity " << report.HomeMediumCapacity() << "\n";
	out << "home small    : " << report.HomeSmallVolume() << "  capacity " << report.HomeSmallCapacity() << "\n";
	out << "lounge volume : " << report.LoungeVolume() << "  capacity " << report.LoungeCapacity() << "\n";
	out << "power plant   : " << report.PowerPlantVolume() << "  jobs " << report.GeneratorJobs() << "\n";
	out << "life support  : " << report.LifeSupportVolume() << "  units " << report.LifeSupportUnits() << "\n";
	out << "capacitor     : " << report.CapacitorVolume() << "\n";
	out << "sensors       : " << report.SensorVolume() << "\n";
	out << "shields       : " << report.ShieldVolume() << "\n";
	out << "hold          : " << report.HoldVolume() << "  capacity " << report.HoldCapacity() << "\n";
	out << "skeleton      : " << report.SkeletonVolume() << "\n";
	out << "armor         : " << report.ArmorVolume() << "\n";
	out << "auditorium    : " << report.AuditoriumVolume() << "\n";
	out << "arena         : " << report.ArenaVolume() << "\n";
	out << "citizen posts : " << report.CitizenPosts().size() << "\n";
	out << "guard posts   : " << report.GuardPosts().size() << "\n";
	out << "berths        : " << parts.CrewBerths() << " crew, " << parts.OfficerBerths() << " officer\n";

	QSet<INDUSTRYID> industries;
	report.Industries(industries);
	out << "\nindustries that could use it:";
	for(QSet<INDUSTRYID>::const_iterator it = industries.constBegin(); it != industries.constEnd(); ++it)
		out << " " << int(*it);
	out << "\n";
	out.flush();
	return 0;
}
