//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - what a design is expected to do, and what it is made of.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "stats.h"

#include <QHash>
#include <QMap>
#include <QRegularExpression>

#include "auglobal.h"
#include "aubddesign.h"
#include "aubdreport.h"
#include "auscdesign.h"
#include "auscreport.h"
#include "dbcommodity.h"
#include "deprocesscomponent.h"

const char* kStatsCaveat =
	"These numbers are worked out with the game's own design report code, for the quality chosen. They are only a "
	"guide: the game can change how it works them out from one version to the next, and what you actually build "
	"with decides the real quality. Always check the design in the game's design window before building it.";

namespace
{

const QHash<int, QString>& OurNames()
{
	static QHash<int, QString> names;
	if(names.isEmpty())
	{
		struct Entry { int id; const char* name; };
		static const Entry table[] = {
#include "commoditynames.inc"
		};
		for(const Entry& e : table)
			names.insert(e.id, QString::fromLatin1(e.name));
	}
	return names;
}

// The game's report code looks commodities up in the game's list (names,
// costs) and asserts when the id is past its end. Offline the list is empty,
// so while a report is worked out it holds a stand-in for every id: its name
// and no cost. It is emptied again afterwards, so that nothing else sees it.
class StandInCommodities
{
public:
	StandInCommodities()
	{
		if(!gp_AuGlobal || !g_Au.m_CommodityArray.isEmpty())
			return;
		m_filled = true;
		for(int id = 0; id < CID_Count; ++id)
		{
			DbCommodity c;
			c.m_id = COMMODITYID(id);
			c.m_name = OurNames().value(id, QString("commodity %1").arg(id));
			c.SetBaseCost(0);
			g_Au.m_CommodityArray.append(c);
		}
	}
	~StandInCommodities()
	{
		if(m_filled)
			g_Au.m_CommodityArray.clear();
	}
private:
	bool m_filled = false;
};

}

QString CommodityLabel(int cid)
{
	// The game's own name, when its list is loaded.
	if(gp_AuGlobal)
	{
		const QString own = g_Au.CommodityName(COMMODITYID(cid));
		if(!own.isEmpty() && !own.startsWith("Unknown:") && !own.startsWith("commodity "))
			return own;
	}
	return OurNames().value(cid, QString("commodity %1").arg(cid));
}

namespace
{

// The game's html with its unknown commodity names given ours, and without
// the pictures (they come from the game client's resources).
QString TidyHtml(QString html)
{
	html.remove(QRegularExpression("<img[^>]*>"));
	// The game's prices are not known offline (the user wants the materials,
	// not a price).
	html.remove(QRegularExpression("Estimated Cost[^<]*"));
	// The list of every part is long and says nothing about what it can do.
	const int parts = html.indexOf("<hr><b>DESIGN PARTS");
	if(parts >= 0)
		html.truncate(parts);
	QRegularExpression unknown("Unknown:(-?\\d+)");
	QString out;
	int at = 0;
	QRegularExpressionMatchIterator it = unknown.globalMatch(html);
	while(it.hasNext())
	{
		const QRegularExpressionMatch m = it.next();
		out += html.mid(at, m.capturedStart() - at);
		out += CommodityLabel(m.captured(1).toInt()).toHtmlEscaped();
		at = m.capturedEnd();
	}
	out += html.mid(at);
	return out;
}

void ListMaterials(const DeProcessComponentArray& components, DesignStats& stats)
{
	QMap<QString, int> used;
	QMap<QString, bool> tools;
	for(const DeProcessComponent& pc : components)
	{
		const QString name = CommodityLabel(pc.CommodityId());
		if(pc.QtyConsumed() && pc.Effect() == CE_Required)
			used[name] += int(pc.QtyConsumed());
		else
			tools[name] = true;
	}
	for(QMap<QString, int>::const_iterator i = used.cbegin(); i != used.cend(); ++i)
		stats.materials << qMakePair(i.key(), i.value());
	stats.tools = tools.keys();
}

}

DesignStats WorkOutStats(const AuDesign& design, int q)
{
	DesignStats stats;
	stats.quality = qBound(1, q, 255);
	StandInCommodities standIn;

	if(const AuScDesign* sc = dynamic_cast<const AuScDesign*>(&design))
	{
		// The report works out the drive, capacitor, fuel and the rest at the
		// design's minimum quality, so a copy is given the quality asked for.
		// (The minimum quality is the design's own: the game will not build it
		// any lower. The copy leaves the design as it was.)
		AuScDesign copy(*sc);
		copy.SetMinQ(quint8(stats.quality));
		copy.RefreshObstructionsCache();
		AuScReport report;
		copy.ReportDesign(report);
		report.SetQuality(quint8(stats.quality));
		stats.html = TidyHtml(report.ReportAnalysis(AuDesign::Edit));

		ListMaterials(report.ManufacturingProcess().Components(), stats);

		stats.crewStations = report.CrewStationCount();
		stats.crewBerths = report.CrewBerths();
		stats.troopStations = report.Turrets().size() + report.GuardPosts().size();
		stats.troopBerths = report.TroopBerths();
		stats.captBerths = report.CaptBerths();
		stats.officerBerths = report.OfficerBerths();
		stats.passengerBerths = report.PassengerBerths();
		stats.needsCaptainAndOfficer = report.Structure() != SC_Spaceship;
	}
	else if(const AuBdDesign* bd = dynamic_cast<const AuBdDesign*>(&design))
	{
		AuBdReport report;
		bd->ReportDesign(report);
		report.SetQuality(quint8(stats.quality));
		stats.html = TidyHtml(report.ReportAnalysis(AuDesign::Edit));

		// As the game's design window shows it: the building's own material
		// in place of the lumber its process is written with.
		DeConstructionProcess cproc(report.ConstructionProcess());
		if(report.Structure() == BD_Fence && !cproc.Components().IsComponent(CID_Lumber))
			cproc.Components().AddRequirement(CID_Lumber, CE_Required, 1);
		cproc.ChangeMaterial(CID_Lumber, report.HullMaterial());
		ListMaterials(cproc.Components(), stats);
		stats.needsCaptainAndOfficer = false;
	}
	return stats;
}

QStringList BerthWarnings(const DesignStats& s)
{
	QStringList w;
	const int berths = s.captBerths + s.officerBerths + s.crewBerths + s.passengerBerths + s.troopBerths;
	// The game says nothing when there are no berths at all.
	if(berths == 0)
	{
		if(s.crewStations > 0 || s.troopStations > 0)
			w << QString("No berths at all, for %1 station(s): the game will not warn about this, but no crew "
						 "can live aboard to run them.").arg(s.crewStations + s.troopStations);
		return w;
	}
	if(s.crewStations > s.crewBerths)
		w << QString("%1 crew station(s) but only %2 crew berth(s): the stations without a crew member to sleep "
					 "aboard may stay empty.").arg(s.crewStations).arg(s.crewBerths);
	if(s.troopStations > s.troopBerths)
		w << QString("%1 turret(s) and guard post(s) but only %2 troop berth(s): some may have no one to man them.")
				.arg(s.troopStations).arg(s.troopBerths);
	if(s.needsCaptainAndOfficer)
	{
		if(!s.captBerths)
			w << "No captain berth: your avatar has nowhere to sleep aboard.";
		if(!s.officerBerths)
			w << "No officer berth: there is no room for an NPC officer.";
	}
	return w;
}

QStringList MaterialLines(const DesignStats& stats)
{
	QStringList lines;
	for(const QPair<QString, int>& m : stats.materials)
		lines << QString("%L1 x %2").arg(m.second).arg(m.first);
	if(!stats.tools.isEmpty())
		lines << QString("Tools (not used up): %1").arg(stats.tools.join(", "));
	return lines;
}

QString StatsText(const DesignStats& stats)
{
	QString text = stats.html;
	text.replace(QRegularExpression("<br\\s*/?>|</div>|<hr>"), "\n");
	text.remove(QRegularExpression("<[^>]*>"));
	text.replace("&nbsp;", " ").replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">");
	text.replace(QRegularExpression("\n\\s*\n+"), "\n");
	QString out = QString("Expected at Q%1:\n%2\n\n").arg(stats.quality).arg(text.trimmed());
	out += "Berths:\n";
	const QStringList berth = BerthWarnings(stats);
	if(berth.isEmpty())
		out += "  enough for the stations\n";
	for(const QString& w : berth)
		out += "  warning: " + w + "\n";
	out += "\nMaterials:\n";
	for(const QString& m : MaterialLines(stats))
		out += "  " + m + "\n";
	out += QString("\n") + kStatsCaveat + "\n";
	return out;
}
