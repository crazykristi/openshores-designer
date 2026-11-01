//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - headless layout generator for Shores of Hazeron blueprints.
//
// Reads a 3ds or stl model (or an existing design) and a short requirements
// brief, and writes finished .SoH spacecraft or building blueprints with a
// complete internal layout.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>

#include <cstdio>
#include <algorithm>
#include <functional>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>

#include "aufilever.h"
#include "auglobal.h"
#include "auscparts.h"
#include "auscdesign.h"
#include "auscreport.h"
#include "audesign.h"
#include "aubddesign.h"
#include "aubdreport.h"
#include "aether.h"
#include "audit.h"
#include "building.h"
#include "dgdata.h"
#include "generate.h"
#include "interior.h"
#include "logging.h"
#include "mesh.h"
#include "shape.h"
#include "shipgen.h"
#include "planbuild.h"
#include "sohfile.h"
#include "version.h"
#include "triplan.h"

namespace {

QTextStream& out()
{
	static QTextStream s(stdout);
	return s;
}

QTextStream& err()
{
	static QTextStream s(stderr);
	return s;
}

// Loads a design, stepping over part types this engine build does not know,
// and says what was stepped over.
AuDesign* LoadForTool(const QString& fileName)
{
	DesignLoadReport report;
	AuDesign* design = LoadDesign(fileName, &report);
	for(const QString& note : report.notes)
		out() << "Note: " << note << "\n";
	out().flush();
	return design;
}


//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

// Bounds of all vertices of a part.
static bool PartBounds(const AuScPart* part, AuPoint& mn, AuPoint& mx)
{
	if(!part)
		return false;
	QSet<quint32> ids;
	part->VertexIds(ids);
	if(ids.isEmpty())
		return false;
	mn = AuPoint(1e30, 1e30, 1e30);
	mx = AuPoint(-1e30, -1e30, -1e30);
	for(QSet<quint32>::const_iterator it = ids.constBegin(); it != ids.constEnd(); ++it)
	{
		const AuPoint p = part->VertexLoc(*it);
		mn = mn.Min(p);
		mx = mx.Max(p);
	}
	return true;
}

static bool PointInsideAnyHull(const QList<AuScSilhouette3d>& hulls, const AuScSilhouette3d& all, const AuPoint& p)
{
	if(hulls.isEmpty())
		return all.IsValid() && all.IsInside(p);
	for(const AuScSilhouette3d& s : hulls)
		if(s.IsValid() && s.IsInside(p))
			return true;
	return false;
}

// Every part, one line each: id, type, group, door tie, module, bounds.
int cmdParts(const QStringList& args)
{
	if(args.isEmpty())
	{
		err() << "usage: shipgen parts <design.SoH> [<type name part>]\n";
		return 2;
	}
	AuDesign* design = LoadForTool(args[0]);
	if(!design)
	{
		err() << "could not load: " << args[0] << "\n";
		return 1;
	}
	const bool openEdges = args.contains("--open-edges");
	const QString filter = (args.size() > 1 && args[1] != "--open-edges") ? args[1] : QString();
	if(AuScDesign* sc = dynamic_cast<AuScDesign*>(design))
	{
		out() << "Volumes (cubic meters): hull " << sc->HullVolume()
			  << ", maneuver drive " << sc->ManeuverDriveVolume()
			  << ", capacitor " << sc->CapacitorVolume()
			  << ", power plant " << sc->PowerPlantVolume()
			  << ", fuel " << sc->FuelVolume() << "\n";
		for(AuScParts::const_iterator it = design->Parts().constBegin(); it != design->Parts().constEnd(); ++it)
			if(it.value() && it.value()->ModuleSystem() == SYS_FTLDrive)
				out() << "FTL drive: part " << it.key() << ", " << sc->FTLDriveVolume(it.key()) << " cubic meters\n";
		out() << "\n";
	}
	const AuScParts& parts = design->Parts();
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p || (!filter.isEmpty() && !p->TypeName().contains(filter, Qt::CaseInsensitive)))
			continue;
		QString line = QString("%1  %2").arg(it.key(), 5).arg(p->TypeName());
		if(!p->GroupName().isEmpty())
			line += QString("  [%1]").arg(p->GroupName());
		if(p->AssociatedPartId())
			line += QString("  tied to %1 state %2").arg(p->AssociatedPartId()).arg(int(p->AssociatedPartState()));
		if(p->Type() == AuScPart::Part_RoomVoid && p->GravityType() == G_Zero)
			line += "  zero-G";
		if(p->Module() != CID_Invalid)
			line += QString("  module %1").arg(g_Au.CommodityName(p->Module()));
		{
			// Transporter consoles and stations: their pads and which pad
			// effect (Transporter Pad 1 to 10) the pads are drawn with.
			AuPointArray pads;
			p->PadLocs(pads);
			if(!pads.isEmpty())
			{
				line += QString("  pad effect %1, pads").arg(p->WhichEffect() + 1);
				for(const AuPoint& pad : pads)
					line += QString(" (%1, %2, %3)").arg(pad.x, 0, 'f', 1).arg(pad.y, 0, 'f', 1).arg(pad.z, 0, 'f', 1);
			}
		}
		if(p->IsDoor())
		{
			// Where the door is opened and closed (the middle of the closed
			// door when the blueprint does not say).
			AuPoint a, b;
			p->DoorOpLocs(a, b);
			line += QString("  switches (%1, %2, %3) (%4, %5, %6)").arg(a.x, 0, 'f', 1).arg(a.y, 0, 'f', 1).arg(a.z, 0, 'f', 1)
					.arg(b.x, 0, 'f', 1).arg(b.y, 0, 'f', 1).arg(b.z, 0, 'f', 1);
		}
		if(p->IsRoomMesh())
		{
			static const char* lights[] = { "dark", "bright", "dim", "?" };
			line += QString("  light %1%2").arg(lights[qBound(0, int(p->InteriorLight()), 3)])
					.arg(p->InteriorIsSunlit() ? ", sunlit" : "");
		}
		if(p->IsOmitted())
			line += "  omitted";
		if(p->IsHidden())
			line += "  hidden";
		{
			// Signed volume: positive when the faces wind outwards.
			AuScFaceList faces;
			p->Faces(faces);
			AuScTriList tris;
			faces.Triangles(tris);
			if(!tris.isEmpty())
			{
				double vol = 0;
				for(const AuScTri& t : tris)
					vol += AuPoint(t.V0()).Dot(AuPoint(t.V1()).Cross(AuPoint(t.V2()))) / 6.0;
				line += QString("  %1 tris, signed volume %2").arg(tris.size()).arg(vol, 0, 'f', 0);
				if(openEdges)
				{
					// Edges used once, as a box around them, grouped by where they are.
					QMap<QString, int> uses;
					auto key = [](const AuPoint& a) { return QString("%1,%2,%3").arg(a.x, 0, 'f', 2).arg(a.y, 0, 'f', 2).arg(a.z, 0, 'f', 2); };
					QMap<QString, QPair<AuPoint, AuPoint> > edges;
					for(const AuScTri& t : tris)
					{
						const AuPoint v[3] = { t.V0(), t.V1(), t.V2() };
						for(int e = 0; e < 3; ++e)
						{
							const QString ka = key(v[e]), kb = key(v[(e + 1) % 3]);
							const QString k = ka < kb ? ka + "|" + kb : kb + "|" + ka;
							uses[k]++;
							edges[k] = qMakePair(v[e], v[(e + 1) % 3]);
						}
					}
					// Folded edges: the same direction twice.
					{
						QMap<QString, int> directed;
						for(const AuScTri& t : tris)
						{
							const AuPoint v[3] = { t.V0(), t.V1(), t.V2() };
							for(int e = 0; e < 3; ++e)
								directed[key(v[e]) + ">" + key(v[(e + 1) % 3])]++;
						}
						AuPoint fmn(1e30, 1e30, 1e30), fmx(-1e30, -1e30, -1e30);
						int folded = 0;
						for(QMap<QString, int>::const_iterator di = directed.constBegin(); di != directed.constEnd(); ++di)
							if(di.value() > 1)
							{
								++folded;
								const QStringList ends = di.key().split(">");
								for(const QString& end : ends)
								{
									const QStringList c = end.split(",");
									const AuPoint p(c[0].toDouble(), c[1].toDouble(), c[2].toDouble());
									fmn = fmn.Min(p);
									fmx = fmx.Max(p);
								}
							}
						if(folded)
							line += QString("  folded %1 in x %2..%3 y %4..%5 z %6..%7").arg(folded)
									.arg(fmn.x, 0, 'f', 1).arg(fmx.x, 0, 'f', 1).arg(fmn.y, 0, 'f', 1)
									.arg(fmx.y, 0, 'f', 1).arg(fmn.z, 0, 'f', 1).arg(fmx.z, 0, 'f', 1);
					}
					AuPoint omn(1e30, 1e30, 1e30), omx(-1e30, -1e30, -1e30);
					int open = 0;
					for(QMap<QString, int>::const_iterator ui = uses.constBegin(); ui != uses.constEnd(); ++ui)
						if(ui.value() == 1)
						{
							++open;
							omn = omn.Min(edges[ui.key()].first).Min(edges[ui.key()].second);
							omx = omx.Max(edges[ui.key()].first).Max(edges[ui.key()].second);
						}
					if(open)
						line += QString("  open edges %1 in x %2..%3 y %4..%5 z %6..%7").arg(open)
								.arg(omn.x, 0, 'f', 1).arg(omx.x, 0, 'f', 1).arg(omn.y, 0, 'f', 1)
								.arg(omx.y, 0, 'f', 1).arg(omn.z, 0, 'f', 1).arg(omx.z, 0, 'f', 1);
				}
			}
		}
		AuPoint mn, mx;
		if(PartBounds(p, mn, mx))
			line += QString("  x %1..%2  y %3..%4  z %5..%6")
					.arg(mn.x, 0, 'f', 1).arg(mx.x, 0, 'f', 1)
					.arg(mn.y, 0, 'f', 1).arg(mx.y, 0, 'f', 1)
					.arg(mn.z, 0, 'f', 1).arg(mx.z, 0, 'f', 1);
		out() << line << "\n";
	}
	delete design;
	return 0;
}

// Cuts a window or door through a hull and a room with the engine's jig cut.
int cmdCut(const QStringList& args)
{
	if(args.size() < 11)
	{
		err() << "usage: shipgen cut <in.SoH> <out.SoH> window|door <hull part> <room part|0> x0 y0 z0 x1 y1 z1 [--flip]\n";
		return 2;
	}
	DesignLoadReport report;
	AuDesign* design = LoadDesign(args[0], &report);
	if(!design || report.truncated)
	{
		err() << "could not load: " << args[0] << "\n";
		return 1;
	}
	const int type = args[2] == "door" ? AuScPart::Part_DoorJig : AuScPart::Part_WindowJig;
	const AuPoint mn(args[5].toDouble(), args[6].toDouble(), args[7].toDouble());
	const AuPoint mx(args[8].toDouble(), args[9].toDouble(), args[10].toDouble());
	AuScFaceList jig = BoxFaces(mn.Min(mx), mn.Max(mx));
	OrientFaces(jig, args.contains("--flip"));
	QList<quint16> added;
	// The axis of the opening: the long horizontal side of the box.
	const int axis = fabs(mx.x - mn.x) >= fabs(mx.y - mn.y) ? 0 : 1;
	const bool ok = JigCut(*design, type, jig, quint16(args[3].toUInt()), quint16(args[4].toUInt()), &added,
			axis, type != AuScPart::Part_DoorJig);
	// A window is lit by the room it belongs to.
	if(const AuScPart* room = design->Parts().value(quint16(args[4].toUInt())))
		for(quint16 id : added)
			if(AuScPart* part = design->Parts().value(id))
				if(part->IsWindow())
					part->SetGroupName(room->GroupName());
	out() << (ok ? "cut" : "the engine did not cut") << "; new parts:";
	for(quint16 id : added)
		out() << " " << id << " (" << design->Parts().value(id)->TypeName() << ")";
	out() << "\n";
	if(!SaveDesign(*design, args[1], report.opaque))
	{
		err() << "could not write: " << args[1] << "\n";
		return 1;
	}
	delete design;
	return ok ? 0 : 1;
}

// How the faces look: texture slot, colour theme, paint, opacity, light,
// effect and glow, counted per part. This is where a blueprint's look lives.
int cmdFaces(const QStringList& args)
{
	if(args.isEmpty())
	{
		err() << "usage: shipgen faces <design.SoH> [<part id>]\n";
		return 2;
	}
	AuDesign* design = LoadForTool(args[0]);
	if(!design)
	{
		err() << "could not load: " << args[0] << "\n";
		return 1;
	}
	AuDesignTex palette[SCTEXI_Count];
	design->TexturePalette(palette);
	out() << "Texture palette:\n";
	for(int i = 0; i < SCTEXI_Count; ++i)
		if(palette[i].TexLibId() || !palette[i].Image().isNull())
			out() << "  slot " << i << ": library texture " << palette[i].TexLibId()
				  << (palette[i].Image().isNull() ? QString() : QString(", image %1x%2 %3").arg(palette[i].Image().width())
						.arg(palette[i].Image().height()).arg(palette[i].ImageName())) << "\n";
	const quint16 only = args.size() > 1 ? quint16(args[1].toUInt()) : 0;
	const AuScParts& parts = design->Parts();
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p || (only && it.key() != only))
			continue;
		AuScFaceList faces;
		p->Faces(faces);
		if(faces.isEmpty())
			if(const AuScStatePart* sp = dynamic_cast<const AuScStatePart*>(p))
			{
				// A door read from a file has no current state: show its
				// first one (closed, usually).
				AuScStatePart* copy = dynamic_cast<AuScStatePart*>(sp->Clone());
				for(int state = 0; copy && state < 256 && faces.isEmpty(); ++state)
					if(copy->HasState(quint8(state)))
					{
						copy->SetState(quint8(state));
						copy->Faces(faces);
					}
				delete copy;
			}
		if(faces.isEmpty())
			continue;
		QMap<QString, int> looks;
		for(const AuScFace* f : faces)
		{
			QString glow;
			for(int v = 0; v < f->VertexCount(); ++v)
				if(f->Vertex(v).IsGlowing())
					glow = f->Vertex(v).Glow().name();
			const QString look = QString("slot %1  theme %2  paint %3  opacity %4  self-lit %5  shine %6  reflect %7  effect %8%9")
					.arg(f->TextureIndex()).arg(int(f->ColorTheme()))
					.arg(f->IsPainted() ? f->Paint().name(QColor::HexArgb) : QString("none"))
					.arg(f->Opacity(), 0, 'f', 2).arg(f->SelfIllumination(), 0, 'f', 2)
					.arg(f->Shine(), 0, 'f', 0).arg(f->Reflectivity(), 0, 'f', 2)
					.arg(SurfaceEffectName(f->Effect()))
					.arg(glow.isEmpty() ? QString() : QString("  glow %1").arg(glow));
			looks[look]++;
		}
		out() << "part " << it.key() << " " << p->TypeName()
			  << (p->GroupName().isEmpty() ? QString() : QString(" [%1]").arg(p->GroupName()))
			  << ": " << faces.size() << " faces\n";
		QList<QPair<int, QString> > sorted;
		for(QMap<QString, int>::const_iterator li = looks.constBegin(); li != looks.constEnd(); ++li)
			sorted << qMakePair(-li.value(), li.key());
		std::sort(sorted.begin(), sorted.end());
		for(int i = 0; i < sorted.size() && i < 12; ++i)
			out() << QString("  %1 x  ").arg(-sorted[i].first, 6) << sorted[i].second << "\n";
		if(sorted.size() > 12)
			out() << "  ... and " << sorted.size() - 12 << " more looks\n";
	}
	delete design;
	return 0;
}

// The Aether kit: a set of blueprints in the high-tech and magic style.
int cmdKit(const QStringList& args)
{
	if(args.isEmpty() || args[0] == "list")
	{
		out() << "The Aether kit (shipgen kit <key|all> [--out <dir>]):\n";
		for(const KitItem& item : KitItems())
			out() << QString("  %1  %2 (%3)\n      %4\n").arg(item.key, -22).arg(item.label).arg(item.kind).arg(item.description);
		return 0;
	}
	QString outDir = ".";
	const int o = args.indexOf("--out");
	if(o >= 0 && o + 1 < args.size())
		outDir = args[o + 1];
	QDir().mkpath(outDir);
	QStringList keys;
	if(args[0] == "all")
		for(const KitItem& item : KitItems())
			keys << item.key;
	else
		keys << args[0];
	int failed = 0;
	for(const QString& key : keys)
	{
		QStringList log;
		const bool ok = BuildKitItem(key, outDir, log);
		out() << "== " << key << "\n";
		for(const QString& line : log)
			out() << "  " << line << "\n";
		if(!ok)
			++failed;
	}
	return failed ? 3 : 0;
}

int cmdResave(const QStringList& args)
{
	if(args.size() != 2)
	{
		err() << "usage: shipgen resave <in.SoH> <out.SoH>\n";
		return 2;
	}
	DesignLoadReport report;
	AuDesign* design = LoadDesign(args[0], &report);
	if(!design)
	{
		err() << "could not load: " << args[0] << "\n";
		return 1;
	}
	for(const QString& note : report.notes)
		out() << "Note: " << note << "\n";
	if(report.truncated)
	{
		err() << "refusing to write an incomplete design\n";
		return 1;
	}
	if(!SaveDesign(*design, args[1], report.opaque))
	{
		err() << "could not write: " << args[1] << "\n";
		return 1;
	}
	out() << "wrote " << args[1] << " (" << design->Parts().size() << " parts, " << report.opaque.size() << " kept unread)\n";
	return 0;
}

int cmdPlanExample(const QStringList& args)
{
	if(args.size() != 1)
	{
		err() << "usage: shipgen plan-example <plan.json>\n";
		return 2;
	}
	QString error;
	if(!TriPlan::Example().Save(args.first(), error))
	{
		err() << error << "\n";
		return 1;
	}
	out() << "wrote " << args.first() << "\n";
	return 0;
}

int cmdPlanFlip(const QStringList& args)
{
	QStringList files;
	bool foreAft = false, portStarboard = false, upsideDown = false;
	int deck = -1;
	for(int i = 0; i < args.size(); ++i)
	{
		const QString& a = args.at(i);
		if(a == "--turn")
			foreAft = portStarboard = true;
		else if(a == "--fore-aft")
			foreAft = true;
		else if(a == "--mirror")
			portStarboard = !portStarboard;
		else if(a == "--upside-down")
			upsideDown = true;
		else if(a == "--deck" && i + 1 < args.size())
			deck = args.at(++i).toInt() - 1;
		else
			files << a;
	}
	if(files.size() != 2 || (!foreAft && !portStarboard && !upsideDown))
	{
		err() << "usage: shipgen plan-flip <plan.json> <out.json> [--turn] [--mirror] [--fore-aft] [--upside-down] "
				 "[--deck <n>]\n";
		return 2;
	}
	TriPlan plan;
	QString error;
	if(!plan.Load(files.at(0), error))
	{
		err() << error << "\n";
		return 1;
	}
	if(deck >= plan.Decks())
	{
		err() << "the plan has " << plan.Decks() << " decks\n";
		return 2;
	}
	const int lost = plan.Flip(foreAft, portStarboard, upsideDown, deck);
	if(!plan.Save(files.at(1), error))
	{
		err() << error << "\n";
		return 1;
	}
	out() << "wrote " << files.at(1);
	if(lost > 0)
		out() << " (" << lost << " ladder(s), stairs or parking spot(s) no longer fitted)";
	out() << "\n";
	return 0;
}

int cmdPlanFromModel(const QStringList& args)
{
	QString modelFile, planFile;
	PlanFromModelOptions options;
	double faces = options.maxFaces, decks = 0, slices = 0, minPiece = options.minPiece;
	for(int i = 0; i < args.size(); ++i)
	{
		const QString& a = args.at(i);
		auto number = [&](double& v) {
			bool ok = false;
			if(i + 1 < args.size())
				v = args.at(++i).toDouble(&ok);
			return ok;
		};
		if(a == "--length" ? !number(options.lengthFeet) :
				a == "--threshold" ? !number(options.threshold) :
				a == "--cell" ? !number(options.cellFeet) :
				a == "--deck" ? !number(options.deckFeet) :
				a == "--faces" ? !number(faces) :
				a == "--decks" ? !number(decks) :
				a == "--slices" ? !number(slices) :
				a == "--seal" ? !number(options.sealFeet) :
				a == "--min-piece" ? !number(minPiece) : false)
		{
			err() << a << " needs a number\n";
			return 2;
		}
		else if(a == "--length" || a == "--threshold" || a == "--cell" || a == "--deck" || a == "--seal")
			continue;
		else if(a == "--decks")
			options.decks = int(decks);
		else if(a == "--min-piece")
			options.minPiece = int(minPiece);
		else if(a == "--no-pinholes")
			options.fillPinholes = false;
		else if(a == "--grid-hull")
			options.modelAsHull = false;
		else if(a == "--slices")
			options.slicesPerDeck = int(slices);
		else if(a == "--method" && i + 1 < args.size())
		{
			const QString m = args.at(++i);
			if(m == "slices")
				options.method = ModelInside::Slices;
			else if(m == "solid")
				options.method = ModelInside::Solid;
			else
			{
				err() << "--method is slices or solid\n";
				return 2;
			}
		}
		else if(a == "--faces")
			options.maxFaces = int(faces);
		else if(a == "--flip")
			options.flip = true;
		else if(a == "--y-up")
			options.yUp = true;
		else if(modelFile.isEmpty())
			modelFile = a;
		else if(planFile.isEmpty())
			planFile = a;
		else
		{
			err() << "unexpected argument " << a << "\n";
			return 2;
		}
	}
	if(modelFile.isEmpty() || planFile.isEmpty())
	{
		err() << "usage: shipgen plan-from-model <model.stl|3ds|obj> <plan.json> [--length <ft>] [--threshold <0..1>]\n"
				 "         [--decks <n>] [--slices <per deck>] [--method slices|solid] [--seal <ft>]\n"
				 "         [--min-piece <triangles>] [--no-pinholes] [--grid-hull]\n"
				 "         [--cell <ft>] [--deck <ft>] [--faces <n>] [--flip] [--y-up]\n";
		return 2;
	}
	TriPlan plan;
	QStringList notes;
	QString error;
	// Each step and how long it took, on stderr.
	QElapsedTimer clock;
	clock.start();
	options.progress = [&clock](const QString& what, int, int) {
		err() << QString("  [%1 s] %2\n").arg(clock.elapsed() / 1000.0, 0, 'f', 1).arg(what);
		return true;
	};
	const bool made = PlanFromModel(modelFile, options, plan, notes, error);
	for(const QString& n : notes)
		out() << n << "\n";
	if(!made || !plan.Save(planFile, error))
	{
		err() << error << "\n";
		return 1;
	}
	out() << "wrote " << planFile << "\n";
	return 0;
}

int cmdBuildPlan(const QStringList& args)
{
	QString planFile, outFile;
	int panels = -1, faces = -1, gridHull = -1;
	PlanBuildOptions options;
	for(int i = 0; i < args.size(); ++i)
	{
		const QString& a = args.at(i);
		if(a == "--out" && i + 1 < args.size())	outFile = args.at(++i);
		else if(a == "--no-furnish")			options.furnish = false;
		else if(a == "--no-paths")				options.walkPaths = false;
		else if(a == "--no-ramps")				options.ramps = false;
		else if(a == "--panels" && i + 1 < args.size())	panels = args.at(++i).toInt();
		else if(a == "--grid-hull")				gridHull = 1;
		else if(a == "--model-hull")			gridHull = 0;
		else if(a == "--faces" && i + 1 < args.size())	faces = args.at(++i).toInt();
		else if(planFile.isEmpty() && !a.startsWith("--"))	planFile = a;
		else
		{
			err() << "unknown or incomplete option: " << a << "\n";
			return 2;
		}
	}
	if(planFile.isEmpty())
	{
		err() << "usage: shipgen build-plan <plan.json> [--out <file.SoH>] [--no-furnish] [--no-paths] [--no-ramps]\n"
				 "         [--panels <n>] [--faces <budget>] [--grid-hull|--model-hull]\n";
		return 2;
	}
	TriPlan plan;
	QString error;
	if(!plan.Load(planFile, error))
	{
		err() << error << "\n";
		return 1;
	}
	if(panels >= 0)
		plan.panels = panels;
	if(gridHull >= 0)
		plan.model.asHull = plan.model.IsSet() && gridHull == 0;
	if(faces > 0)
		plan.maxFaces = faces;
	if(outFile.isEmpty())
		outFile = QFileInfo(planFile).dir().filePath(SanitiseName(plan.name) + ".SoH");

	PlanBuildReport report;
	AuScDesign* design = BuildPlanDesign(plan, options, report);
	out() << "Plan         : " << plan.name << " (" << plan.Width() << " x " << plan.Depth() << " cells of "
		  << plan.cellFeet << " feet, " << plan.Decks() << " decks)\n";
	out() << "Hull         : " << report.hullFaces << " faces, about " << qRound(report.hullVolumeM3) << " cubic meters\n";
	out() << "Shaped hull  : " << report.shapedFaces << " faces before the doors and windows were cut\n";
	out() << "Rooms        : " << report.rooms << "\n";
	out() << "Doors        : " << report.doors << "\n";
	out() << "Windows      : " << report.windows << "\n";
	out() << "Ladders      : " << report.ladders << "\n";
	out() << "Zero-G joins : " << report.zeroGJoins << "\n";
	out() << "Parking      : " << report.parking << "\n";
	out() << "Ramps        : " << report.ramps << "\n";
	out() << "Stairs       : " << report.stairs << "\n";
	out() << "Contrails    : " << report.contrails << "\n";
	out() << "Weapon emit  : " << report.weaponEmitters << "\n";
	out() << "Stations     : " << report.stations << "\n";
	out() << "Berths       : " << report.berths << "\n";
	for(const QString& n : report.notes)
		out() << "Note: " << n << "\n";
	for(const QString& w : report.warnings)
		out() << "Warning: " << w << "\n";
	out() << "\nThe engine's report:\n";
	out() << "Errors (" << report.errors.size() << "):\n";
	for(const QString& e : report.errors)
		out() << "  " << e << "\n";
	out() << "Warnings (" << report.engineWarnings.size() << "):\n";
	for(const QString& w : report.engineWarnings)
		out() << "  " << w << "\n";

	const bool saved = design->SaveToFile(outFile, SOH_FILE_TYPE);
	out() << (saved ? "\nsaved as " : "\ncould not write ") << outFile << "\n";
	delete design;
	return saved ? (report.ok ? 0 : 3) : 1;
}

int cmdInspect(const QStringList& args)
{
	if(args.isEmpty())
	{
		err() << "usage: shipgen inspect <design.SoH>\n";
		return 2;
	}
	AuDesign* design = LoadForTool(args.first());
	if(!design)
	{
		err() << "could not load: " << args.first() << "\n";
		return 1;
	}

	out() << "Design name  : " << design->Name() << "\n";
	out() << "Architect    : " << design->ArchitectName() << "\n";

	AuScDesign* sc = dynamic_cast<AuScDesign*>(design);
	AuBdDesign* bd = dynamic_cast<AuBdDesign*>(design);
	if(!sc && !bd)
	{
		err() << "not a spacecraft or building design\n";
		delete design;
		return 1;
	}

	const AuScParts& parts = design->Parts();
	QMap<QString, int> byType;
	QStringList roomVoids;
	QStringList stations;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p)
			continue;
		byType[p->TypeName()]++;
		if(p->IsRoomVoid())
			roomVoids << QString("%1 (part %2%3)").arg(p->GroupName()).arg(it.key())
					.arg(p->AssociatedPartId() ? QString(", doorway of door %1").arg(p->AssociatedPartId()) : QString());
		if(p->IsCommandStation() || p->IsControlStation())
			stations << QString("part %1  %2  %3").arg(it.key()).arg(p->TypeName()).arg(p->GroupName());
	}

	if(sc)
		out() << "Structure    : " << int(sc->Structure()) << " (spacecraft)\n";
	else
	{
		out() << "Structure    : " << int(bd->Structure()) << " (building)\n";
		out() << "Industry     : " << int(bd->IndustryId()) << "\n";
	}
	out() << "Parts        : " << parts.size() << "\n";
	out() << "Hull volume  : " << parts.HullVolume() << " cubic meters\n";
	out() << "Crew berths  : " << parts.CrewBerths() << "\n\n";
	out() << "Parts by type:\n";
	for(QMap<QString, int>::const_iterator it = byType.constBegin(); it != byType.constEnd(); ++it)
		out() << "  " << it.key() << ": " << it.value() << "\n";

	out() << "\nHull parts:\n";
	int hullPartsSeen = 0;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p || p->Type() != AuScPart::Part_Hull)
			continue;
		++hullPartsSeen;
		AuScFaceList faces;
		p->Faces(faces);
		AuPoint hmn, hmx;
		QString bounds;
		if(PartBounds(p, hmn, hmx))
			bounds = QString("  x %1..%2  y %3..%4  z %5..%6")
					.arg(qRound(hmn.x)).arg(qRound(hmx.x))
					.arg(qRound(hmn.y)).arg(qRound(hmx.y))
					.arg(qRound(hmn.z)).arg(qRound(hmx.z));
		out() << QString("  part %1  %2  hull mesh: %3  mesh: %4  omitted: %5  faces: %6%7\n")
				.arg(it.key()).arg(p->TypeName())
				.arg(p->IsHullMesh() ? "yes" : "no")
				.arg(p->IsMesh() ? "yes" : "no")
				.arg(p->IsOmitted() ? "yes" : "no")
				.arg(faces.size()).arg(bounds);
	}
	if(!hullPartsSeen)
		out() << "  (none)\n";

	// The exact audit. A hull part that is not closed has no inside, so the
	// rooms in it cannot be certified; the silhouette below is only a rough
	// fallback for that case.
	QList<HullSolidity> hullSolids;
	QList<RoomContainment> roomAudit;
	QStringList auditNotes;
	AuditDesign(parts, hullSolids, roomAudit, auditNotes);
	QHash<quint16, RoomContainment> auditById;
	for(const RoomContainment& r : roomAudit)
		auditById.insert(r.partId, r);

	out() << "\nHull solidity (a closed hull is what makes an inside):\n";
	for(const HullSolidity& h : hullSolids)
	{
		out() << QString("  part %1  %2 triangles  ").arg(h.id).arg(h.mesh.triangles);
		if(h.mesh.Closed())
			out() << "closed solid\n";
		else
			out() << QString("NOT CLOSED (%1 open, %2 thick, %3 folded edge(s))\n")
					.arg(h.mesh.openEdges).arg(h.mesh.thickEdges).arg(h.mesh.foldedEdges);
	}
	if(hullSolids.isEmpty())
		out() << "  (no hull mesh)\n";

	// The texture palette and whether the hull faces actually have texture
	// coordinates. A palette entry on its own does nothing; the faces have to
	// be given a slot and coordinates as well.
	{
		AuDesignTex palette[SCTEXI_Count];
		design->TexturePalette(palette);
		out() << "\nTextures:\n";
		int textureCount = 0;
		for(int i = 0; i < SCTEXI_Count; ++i)
		{
			const QImage& image = palette[i].Image();
			if(image.isNull() && palette[i].ImageName().isEmpty())
				continue;
			++textureCount;
			out() << QString("  slot %1  %2  %3x%4\n").arg(i)
					.arg(palette[i].ImageName().isEmpty() ? QString("(colour)") : palette[i].ImageName())
					.arg(image.width()).arg(image.height());
		}
		if(!textureCount)
			out() << "  (none)\n";

		out() << "\nHull texture coverage:\n";
		int covered = 0;
		for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
		{
			const AuScPart* p = it.value();
			if(!p || !p->IsHullMesh() || p->IsOmitted())
				continue;
			AuScFaceList faces;
			p->Faces(faces);
			QSet<int> slotIds;
			int vertices = 0;
			int withUv = 0;
			for(int fi = 0; fi < faces.size(); ++fi)
			{
				const AuScFace* face = faces.at(fi);
				if(!face)
					continue;
				slotIds.insert(int(face->TextureIndex()));
				for(int vi = 0; vi < face->VertexCount(); ++vi)
				{
					const AuScPoint v = face->Vertex(vi);
					++vertices;
					if(v.m_Tx != 0.f || v.m_Ty != 0.f)
						++withUv;
				}
			}
			QStringList slotNames;
			QList<int> sorted;
			for(int slot : slotIds)
				sorted << slot;
			std::sort(sorted.begin(), sorted.end());
			for(int slot : sorted)
				slotNames << QString("%1").arg(slot);
			out() << QString("  part %1  slots: %2  UVs on %3 of %4 vertices\n")
					.arg(it.key())
					.arg(slotNames.isEmpty() ? QString("(none)") : slotNames.join(", "))
					.arg(withUv).arg(vertices);
			if(withUv > 0)
				++covered;
		}
		if(!covered)
			out() << "  (no hull part has texture coordinates)\n";
	}

	out() << "\nRoom voids (these define the rooms):\n";
	for(const QString& rv : roomVoids)
		out() << "  " << rv << "\n";
	if(roomVoids.isEmpty())
		out() << "  (none)\n";

	// Does each room really sit inside the hull? The engine's obstruction
	// model takes in the faces of anything that touches it, so it can call a
	// room inside even when the room pokes out. The exact audit uses the hull
	// triangles and a closed-solid test, which is the answer to trust. When
	// no hull part is closed there is no inside to test, so the old
	// silhouette test is shown and labelled as unverified.
	AuScSilhouette3d hullOnly;
	QList<AuScSilhouette3d> hullParts;
	const bool haveSilhouette = HullOnlySilhouette(parts, hullOnly, &hullParts);
	out() << "\nRoom fit (exact, against the hull triangles):\n";
	int roomCount = 0;
	int outsideCount = 0;
	int uncheckedCount = 0;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p || !(p->IsRoomVoid() || p->IsRoomMesh()))
			continue;
		if(p->IsOmitted())
			continue;
		if(p->AssociatedPartId())
			continue;		// A doorway, which reaches through the wall (or the hull) on purpose.
		++roomCount;
		AuPoint mn, mx;
		PartBounds(p, mn, mx);
		const QString bounds = QString("x %1..%2  y %3..%4  z %5..%6")
				.arg(qRound(mn.x)).arg(qRound(mx.x))
				.arg(qRound(mn.y)).arg(qRound(mx.y))
				.arg(qRound(mn.z)).arg(qRound(mx.z));

		const RoomContainment audit = auditById.value(it.key());
		QString verdict;
		if(audit.checked)
		{
			if(audit.inside)
				verdict = QString("inside hull part %1").arg(audit.hullId);
			else
			{
				verdict = "OUTSIDE";
				++outsideCount;
			}
		}
		else
		{
			// Fallback only. It is a projection test, so it cannot see a
			// concavity; say so rather than calling it inside.
			++uncheckedCount;
			if(!haveSilhouette)
				verdict = "CANNOT BE CHECKED: no closed hull and no silhouette";
			else
			{
				QSet<quint32> ids;
				p->VertexIds(ids);
				int outside = 0;
				for(QSet<quint32>::const_iterator vi = ids.constBegin(); vi != ids.constEnd(); ++vi)
					if(!PointInsideAnyHull(hullParts, hullOnly, p->VertexLoc(*vi)))
						++outside;
				verdict = QString("UNVERIFIED (silhouette says %1): %2")
						.arg(outside ? QString("outside") : QString("inside"))
						.arg(audit.note);
			}
		}
		out() << QString("  part %1  %2  %3  %4\n")
				.arg(it.key()).arg(p->TypeName()).arg(verdict).arg(bounds);
	}
	if(!roomCount)
		out() << "  (no room parts)\n";
	out() << QString("\nRooms outside the hull: %1\n").arg(outsideCount);
	if(uncheckedCount)
		out() << QString("Rooms that could not be checked exactly: %1\n").arg(uncheckedCount);
	for(const QString& note : auditNotes)
		out() << "Note: " << note << "\n";

	// Rooms must not share space. Two rooms that only touch at a wall are
	// fine; anything more means the furniture and the air of one room are
	// inside the other.
	if(roomCount)
	{
		bool haveVoids = false;
		for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
		{
			const AuScPart* p = it.value();
			if(p && p->IsRoomVoid() && !p->IsOmitted())
				haveVoids = true;
		}
		QList<QPair<quint16, QPair<AuPoint, AuPoint> > > boxes;
		for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
		{
			const AuScPart* p = it.value();
			if(!p || p->IsOmitted())
				continue;
			if(haveVoids ? !p->IsRoomVoid() : !p->IsRoomMesh())
				continue;
			if(p->AssociatedPartId())
				continue;		// Doorways overlap the rooms they join.
			if(p->GroupName().startsWith(LadderGroupPrefix()))
				continue;		// So do ladder shafts.
			AuPoint mn, mx;
			if(!PartBounds(p, mn, mx))
				continue;
			boxes << qMakePair(it.key(), qMakePair(mn, mx));
		}
		QStringList overlaps;
		const double e = .05;
		for(int i = 0; i < boxes.size(); ++i)
			for(int j = i + 1; j < boxes.size(); ++j)
			{
				const AuPoint& amn = boxes.at(i).second.first;
				const AuPoint& amx = boxes.at(i).second.second;
				const AuPoint& bmn = boxes.at(j).second.first;
				const AuPoint& bmx = boxes.at(j).second.second;
				if(amn.x < bmx.x - e && amx.x > bmn.x + e
						&& amn.y < bmx.y - e && amx.y > bmn.y + e
						&& amn.z < bmx.z - e && amx.z > bmn.z + e)
					overlaps << QString("%1 and %2").arg(boxes.at(i).first).arg(boxes.at(j).first);
			}
		if(overlaps.isEmpty())
			out() << "\nRoom overlaps: none\n";
		else
			out() << "\nRoom overlaps: " << overlaps.join(", ") << "\n";
	}

	// Doors and the room voids associated with their open state. A door that
	// has no open state or no associated room is not functional.
	out() << "\nDoors:\n";
	int doorCount = 0;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScDoor* door = dynamic_cast<const AuScDoor*>(it.value());
		if(!door || door->IsOmitted())
			continue;
		++doorCount;
		QString kind = door->IsHullDoor() ? "hull door" : (door->IsAutomaticDoor() ? "automatic" : "standard");
		if(door->IsTurboLiftDoor())
			kind += ", lift stop " + door->StopName();
		QStringList states;
		const QMap<quint8, QString> names = door->StateNames();
		for(QMap<quint8, QString>::const_iterator si = names.constBegin(); si != names.constEnd(); ++si)
			states << si.value();
		QStringList openVoids, openHullVoids;
		for(AuScParts::const_iterator vi = parts.constBegin(); vi != parts.constEnd(); ++vi)
		{
			const AuScPart* v = vi.value();
			if(!v || v->IsOmitted())
				continue;
			if(v->AssociatedPartId() != it.key())
				continue;
			if(v->AssociatedPartState() != AuScDoor::Open)
				continue;
			if(v->IsRoomVoid())
			{
				openVoids << QString("%1").arg(vi.key());
			}
			else if(v->IsHullVoid())
				openHullVoids << QString("%1").arg(vi.key());
		}
		out() << QString("  part %1  %2  states: %3\n")
				.arg(it.key()).arg(kind)
				.arg(states.isEmpty() ? QString("(none)") : states.join(", "));
		out() << QString("    open state room voids: %1\n")
				.arg(openVoids.isEmpty() ? QString("NONE") : openVoids.join(", "));
		if(door->IsHullDoor())
			out() << QString("    open state hull voids: %1\n")
					.arg(openHullVoids.isEmpty() ? QString("NONE") : openHullVoids.join(", "));
	}
	if(!doorCount)
		out() << "  (no doors)\n";

	// A constant room void (one not tied to a door) is a room. It can be
	// reached when a void that comes with an open door touches it: the
	// doorway. A void tied to a door is not a room of its own: the engine
	// skips it when it lists the rooms (AuScParts::ListRoomNames), so a room
	// whose only void is tied to a door is there only while the door is open.
	// A ladder shaft (interior.h) joins the rooms above and below it.
	QStringList roomsWithoutDoor;
	QStringList comeAndGo;
	{
		struct VoidBox { quint16 id; QString group; AuPoint mn, mx; };
		QList<VoidBox> rooms, doorways, connectors;
		QSet<QString> constantGroups;
		for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
		{
			const AuScPart* p = it.value();
			if(!p || p->IsOmitted() || !p->IsRoomVoid())
				continue;
			VoidBox b;
			b.id = it.key();
			b.group = p->GroupName();
			if(!PartBounds(p, b.mn, b.mx))
				continue;
			if(p->AssociatedPartId())
				doorways << b;
			else if(b.group.startsWith(LadderGroupPrefix()))
				connectors << b;
			else
			{
				rooms << b;
				constantGroups.insert(b.group);
			}
		}
		for(const VoidBox& d : doorways)
			if(!constantGroups.contains(d.group))
				comeAndGo << QString("%1 (%2)").arg(d.id).arg(d.group);

		auto touches = [](const VoidBox& a, const VoidBox& b) {
			const double tol = .1;
			return a.mn.x <= b.mx.x + tol && a.mx.x >= b.mn.x - tol && a.mn.y <= b.mx.y + tol && a.mx.y >= b.mn.y - tol &&
				a.mn.z <= b.mx.z + tol && a.mx.z >= b.mn.z - tol;
		};

		// Rooms joined by ladder shafts are one space; a door into any
		// room of that space reaches all of it.
		QVector<int> parent(rooms.size());
		for(int i = 0; i < parent.size(); ++i)
			parent[i] = i;
		std::function<int(int)> root = [&parent, &root](int i) { return parent[i] == i ? i : (parent[i] = root(parent[i])); };
		for(const VoidBox& c : connectors)
		{
			int first = -1;
			for(int i = 0; i < rooms.size(); ++i)
				if(touches(c, rooms[i]))
				{
					if(first < 0)
						first = i;
					else
						parent[root(i)] = root(first);
				}
		}
		QSet<int> withDoor;
		for(const VoidBox& d : doorways)
			for(int i = 0; i < rooms.size(); ++i)
				if(touches(d, rooms[i]))
					withDoor.insert(root(i));
		for(int i = 0; i < rooms.size(); ++i)
			if(!withDoor.contains(root(i)))
				roomsWithoutDoor << QString("%1 (%2)").arg(rooms[i].id).arg(rooms[i].group);
	}
	if(!comeAndGo.isEmpty())
		out() << "\nRooms that are only there while a door is open: " << comeAndGo.size() << "  "
				<< comeAndGo.join(", ") << "\n"
				<< "  Their only room void is tied to a door's open state. Give each room a void of its own\n"
				<< "  that is not tied to anything, and tie a small doorway void to the door instead.\n";
	if(!roomsWithoutDoor.isEmpty())
		out() << "\nRooms without a door: " << roomsWithoutDoor.size()
				<< "  " << roomsWithoutDoor.join(", ") << "\n";

	out() << "\nStations:\n";
	for(const QString& s : stations)
		out() << "  " << s << "\n";
	if(stations.isEmpty())
		out() << "  (none)\n";

	out() << flush;
	delete design;
	return 0;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The exact version of the room check: it uses the hull's triangles rather
// than the raster silhouettes that placement works on. This is what answers
// "is the room really inside the model", not "does the engine's silhouette
// think it is".

int cmdAudit(const QStringList& args)
{
	if(args.isEmpty())
	{
		err() << "usage: shipgen audit <design.SoH>\n";
		return 2;
	}
	AuDesign* design = LoadForTool(args.first());
	if(!design)
	{
		err() << "could not load: " << args.first() << "\n";
		return 1;
	}
	QList<HullSolidity> hulls;
	QList<RoomContainment> rooms;
	QStringList notes;
	AuditDesign(design->Parts(), hulls, rooms, notes);

	out() << "Design: " << design->Name() << "\n\nHull solidity:\n";
	for(const HullSolidity& h : hulls)
	{
		out() << QString("  part %1  %2 triangles, %3 vertices  ")
				.arg(h.id).arg(h.mesh.triangles).arg(h.mesh.vertices);
		if(h.mesh.Closed())
			out() << "closed solid\n";
		else if(h.mesh.ClosedButForOpenings())
			out() << QString("closed but for openings (doors, windows: %1 open edge(s)); rooms are tested by a vote of rays\n")
					.arg(h.mesh.openEdges);
		else
			out() << QString("NOT CLOSED (%1 open, %2 thick, %3 folded edge(s))\n")
					.arg(h.mesh.openEdges).arg(h.mesh.thickEdges).arg(h.mesh.foldedEdges);
	}
	if(hulls.isEmpty())
		out() << "  (no hull mesh)\n";

	out() << "\nRooms, tested against the hull triangles:\n";
	int outside = 0;
	int unchecked = 0;
	for(const RoomContainment& r : rooms)
	{
		out() << QString("  part %1  %2  ").arg(r.partId).arg(r.type);
		if(!r.checked)
		{
			out() << "CANNOT BE CHECKED: " << r.note << "\n";
			++unchecked;
		}
		else if(r.inside)
			out() << QString("inside hull part %1%2\n").arg(r.hullId)
					.arg(r.throughOpenings ? QString(" (which has openings)") : QString());
		else
		{
			out() << "OUTSIDE\n";
			++outside;
		}
	}
	if(rooms.isEmpty())
		out() << "  (no room parts)\n";

	out() << "\nRooms outside the hull: ";
	if(outside)
		out() << outside << "\n";
	else
		out() << "none\n";
	if(unchecked)
		out() << "Rooms that could not be checked: " << unchecked << "\n";
	for(const QString& note : notes)
		out() << "Note: " << note << "\n";

	out() << flush;
	delete design;
	return (outside || unchecked || !notes.isEmpty()) ? 1 : 0;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

int cmdValidate(const QStringList& args)
{
	if(args.isEmpty())
	{
		err() << "usage: shipgen validate <design.SoH>\n";
		return 2;
	}
	AuDesign* design = LoadForTool(args.first());
	if(!design)
	{
		err() << "could not load: " << args.first() << "\n";
		return 1;
	}

	QStringList errors, warnings;
	AuScDesign* sc = dynamic_cast<AuScDesign*>(design);
	AuBdDesign* bd = dynamic_cast<AuBdDesign*>(design);
	if(sc)
		ValidateDesign(*sc, errors, warnings);
	else if(bd)
		ValidateBuilding(*bd, errors, warnings);
	else
	{
		err() << "not a spacecraft or building design\n";
		delete design;
		return 1;
	}

	out() << "Errors (" << errors.size() << "):\n";
	for(const QString& e : errors)
		out() << "  " << e << "\n";
	out() << "\nWarnings (" << warnings.size() << "):\n";
	for(const QString& w : warnings)
		out() << "  " << w << "\n";
	out() << flush;

	delete design;
	return errors.isEmpty() ? 0 : 1;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

int cmdTemplates()
{
	out() << "Ship templates:\n\n";
	const QList<ShipTemplate> all = AllTemplates();
	for(const ShipTemplate& t : all)
	{
		out() << "  " << t.key << "\n";
		out() << "    " << t.label << " - " << t.description << "\n";
		out() << "    crew " << t.crew << ", " << t.rooms.size() << " room types, "
			  << t.stations.size() << " station types\n\n";
	}

	out() << "Building templates:\n\n";
	const QList<BuildingTemplate> balls = AllBuildingTemplates();
	for(const BuildingTemplate& t : balls)
	{
		out() << "  " << t.key << "\n";
		out() << "    " << t.label << " - " << t.description << "\n";
		out() << "    " << qRound(t.footprintX) << " x " << qRound(t.footprintY) << " feet, "
			  << t.floors << " floor(s), " << t.rooms.size() << " room types\n\n";
	}

	out() << "Write a starting brief with:  shipgen brief <template> <file.json>\n";
	out() << "                            shipgen building-brief <template> <file.json>\n";
	out() << flush;
	return 0;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

int cmdBrief(const QStringList& args)
{
	if(args.size() < 2)
	{
		err() << "usage: shipgen brief <template> <file.json>\n";
		return 2;
	}
	Brief brief;
	if(!FindTemplate(args.first()))
	{
		err() << "unknown template: " << args.first() << "\n";
		return 2;
	}
	ApplyTemplate(brief, args.first());

	QString error;
	if(!SaveBrief(args.at(1), brief, error))
	{
		err() << error << "\n";
		return 1;
	}
	out() << "wrote " << args.at(1) << "\n";
	out() << flush;
	return 0;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

int cmdBuildingBrief(const QStringList& args)
{
	if(args.size() < 2)
	{
		err() << "usage: shipgen building-brief <template> <file.json>\n";
		return 2;
	}
	if(!FindBuildingTemplate(args.first()))
	{
		err() << "unknown building template: " << args.first() << "\n";
		return 2;
	}
	BuildingBrief brief;
	brief.name = FindBuildingTemplate(args.first())->label;
	ApplyBuildingTemplate(brief, args.first());

	QString error;
	if(!SaveBuildingBrief(args.at(1), brief, error))
	{
		err() << error << "\n";
		return 1;
	}
	out() << "wrote " << args.at(1) << "\n";
	out() << flush;
	return 0;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

int cmdGenerate(const QStringList& args)
{
	QString hullFile, briefFile, outDir, name, layout;
	QStringList modelFiles;
	QStringList textureFiles;
	PaintOptions paint;
	double scale = 1.0;
	double fit = 0.0;
	double wall = 8.0;
	double deck = 4.0;
	double decimate = 0.0;
	int candidates = 0;
	quint32 maxFaces = 5000;
	ModelShape shape;
	BaseShape base;
	bool repair = true;
	bool allowOpenHull = false;
	bool keepAxes = false;
	QString ftlDrive;
	QString presetKey;
	int variations = 1;

	for(int i = 0; i < args.size(); ++i)
	{
		const QString& a = args.at(i);
		bool hasNext = (i + 1) < args.size();
		if(a == "--model" && hasNext)			modelFiles << args.at(++i);
		else if(a == "--hull" && hasNext)		hullFile = args.at(++i);
		else if(a == "--brief" && hasNext)		briefFile = args.at(++i);
		else if(a == "--out" && hasNext)		outDir = args.at(++i);
		else if(a == "--texture" && hasNext)	textureFiles << args.at(++i);
		else if(a == "--paint" && hasNext)
		{
			if(!PaintProjectionFromName(args.at(++i), paint.projection))
			{
				err() << "unknown paint projection: " << args.at(i) << "\n";
				return 2;
			}
		}
		else if(a == "--paint-scale" && hasNext)	paint.size = args.at(++i).toDouble();
		else if(a == "--paint-per-part")		paint.perPart = true;
		else if(a == "--paint-single")			paint.perPart = false;
		else if(a == "--name" && hasNext)		name = args.at(++i);
		else if(a == "--layout" && hasNext)	layout = args.at(++i);
		else if(a == "--scale" && hasNext)		scale = args.at(++i).toDouble();
		else if(a == "--faces" && hasNext)	maxFaces = args.at(++i).toUInt();
		else if(a == "--taper" && hasNext)	shape.taperTop = args.at(++i).toDouble();
		else if(a == "--taper-bottom" && hasNext)	shape.taperBottom = args.at(++i).toDouble();
		else if(a == "--twist" && hasNext)	shape.twist = args.at(++i).toDouble();
		else if(a == "--repeat" && hasNext)	shape.repeats = args.at(++i).toInt();
		else if(a == "--repeat-radius" && hasNext)	shape.repeatRadius = args.at(++i).toDouble();
		else if(a == "--stack" && hasNext)	shape.stack = args.at(++i).toInt();
		else if(a == "--noise" && hasNext)	shape.noise = args.at(++i).toDouble();
		else if(a == "--noise-scale" && hasNext)	shape.noiseScale = args.at(++i).toDouble();
		else if(a == "--stretch" && hasNext)	shape.stretch = args.at(++i).toDouble();
		else if(a == "--seed" && hasNext)	shape.seed = args.at(++i).toInt();
		else if(a == "--preset" && hasNext)	presetKey = args.at(++i);
		else if(a == "--no-repair")			repair = false;
		else if(a == "--allow-open-hull")		allowOpenHull = true;
		else if(a == "--keep-axes")			keepAxes = true;
		else if(a == "--ftl" && hasNext)		ftlDrive = args.at(++i);
		else if(a == "--vary" && hasNext)	variations = args.at(++i).toInt();
		else if(a == "--wall" && hasNext)		wall = args.at(++i).toDouble();
		else if(a == "--deck" && hasNext)		deck = args.at(++i).toDouble();
		else if(a == "--decimate" && hasNext)	decimate = args.at(++i).toDouble();
		else if(a == "--fit" && hasNext)		fit = args.at(++i).toDouble();
		else if(a == "--base" && hasNext)
		{
			BaseShapeKind kind;
			if(!FindBaseShape(args.at(++i), kind))
			{
				err() << "unknown base shape: " << args.at(i) << "\n";
				return 2;
			}
			base.kind = kind;
		}
		else if(a == "--base-size" && hasNext)
		{
			const QStringList parts = args.at(++i).toLower().split('x');
			if(parts.size() != 2 || parts.at(0).toDouble() <= 0 || parts.at(1).toDouble() <= 0)
			{
				err() << "--base-size wants width x depth, like 140x100\n";
				return 2;
			}
			base.width = parts.at(0).toDouble();
			base.depth = parts.at(1).toDouble();
		}
		else if(a == "--base-height" && hasNext)	base.height = args.at(++i).toDouble();
		else if(a == "--base-together")			base.separateModels = false;
		else if(a == "--base-separate")			base.separateModels = true;
		else if(a == "--candidates" && hasNext)	candidates = args.at(++i).toInt();
		else
		{
			err() << "unknown or incomplete option: " << a << "\n";
			return 2;
		}
	}

	if(modelFiles.isEmpty() && hullFile.isEmpty() && !base.IsSet())
	{
		err() << "one of --model, --hull or --base is required\n";
		return 2;
	}
	paint.textures = textureFiles;
	if(!presetKey.isEmpty())
	{
		ShapePreset preset;
		if(!FindShapePreset(presetKey, preset))
		{
			err() << "unknown shape preset: " << presetKey << "\n";
			return 2;
		}
		shape = preset.shape;
	}
	if(briefFile.isEmpty())
	{
		err() << "--brief is required\n";
		return 2;
	}
	if(outDir.isEmpty())
		outDir = ".";
	QDir().mkpath(outDir);

	// The brief.
	Brief brief;
	QString error;
	if(!LoadBrief(briefFile, brief, error))
	{
		err() << error << "\n";
		return 1;
	}
	if(!brief.templateName.isEmpty())
		ApplyTemplate(brief, brief.templateName);
	if(!ftlDrive.isEmpty())
	{
		if(ftlDrive != "none" && FtlDriveModule(ftlDrive) == CID_Invalid)
		{
			QStringList keys;
			for(const auto& choice : FtlDriveChoices())
				keys << choice.first;
			err() << "unknown FTL drive '" << ftlDrive << "'; one of: " << keys.join(", ") << "\n";
			return 2;
		}
		brief.ftlDrive = ftlDrive;
	}
	if(brief.name.isEmpty())
		brief.name = name.isEmpty() ? QString("Generated Ship") : name;

	// Which strategies to run.
	QStringList strategies;
	if(!layout.isEmpty() && layout != "auto")
	{
		if(!LayoutStrategies().contains(layout))
		{
			err() << "unknown layout strategy: " << layout << "\n";
			return 2;
		}
		strategies << layout;
	}
	else
		strategies = LayoutStrategies();
	if(candidates > 0 && candidates < strategies.size())
		strategies = strategies.mid(0, candidates);

	// The layout options are the same for every variation.
	LayoutOptions opts;
	opts.wallThickness = wall;
	opts.deckThickness = deck;
	opts.foreAlongLongAxis = !keepAxes;

	// Generate one candidate per strategy, for each variation of the shape.
	int index = 0;
	for(int variation = 0; variation < qMax(1, variations); ++variation)
	{
		// The hull.
		QStringList importNotes;
		AuScDesign baseDesign;
		baseDesign.SetId(1);
		baseDesign.SetArchitectId(AuId(0));
		baseDesign.SetArchitectName("shipgen");
		baseDesign.SetName(variations > 1 ? QString("%1 %2").arg(brief.name).arg(variation + 1) : brief.name);
		baseDesign.SetStructure(SC_Spaceship);
		baseDesign.SetStreamline(SLINE_None);
		if(!brief.description.isEmpty())
			baseDesign.SetDescription(brief.description);

		if(!hullFile.isEmpty())
		{
			AuDesign* src = LoadForTool(hullFile);
			if(!src)
			{
				err() << "could not load hull design: " << hullFile << "\n";
				return 1;
			}
			AuScDesign* srcSc = dynamic_cast<AuScDesign*>(src);
			if(!srcSc || !CopyHull(*srcSc, baseDesign, error, allowOpenHull))
			{
				err() << (error.isEmpty() ? QString("not a spacecraft design") : error) << "\n";
				delete src;
				return 1;
			}
			delete src;

			// A new palette can be put on an existing hull as well.
			if(paint.IsSet() && !PaintDesign(baseDesign, paint, &importNotes, error))
			{
				err() << error << "\n";
				return 1;
			}
		}
		else
		{
			ModelImportOptions modelOpts;
			modelOpts.scale = scale;
			modelOpts.fit = fit;
			modelOpts.decimate = decimate;
			modelOpts.maxFaces = maxFaces;
			modelOpts.shape = VaryShape(shape, variation);
			modelOpts.base = base;
			modelOpts.paint = paint;
			modelOpts.repair = repair;
			modelOpts.notes = &importNotes;
			AuPoint modelMin, modelMax;
			if(!ImportHullModel(baseDesign, modelFiles, modelOpts, modelMin, modelMax, error))
			{
				err() << error << "\n";
				return 1;
			}
		}

		// Analyse the hull.
		HullAnalysis hull;
		if(!AnalyseHull(baseDesign, opts, hull, error))
		{
			err() << error << "\n";
			return 1;
		}
		out() << "Hull: " << qRound(hull.max.x - hull.min.x) << " x " << qRound(hull.max.y - hull.min.y) << " x "
			  << qRound(hull.max.z - hull.min.z) << " feet, about " << qRound(hull.volumeM3) << " cubic meters.\n";
		out() << "Faces: " << hull.faces << " (the game wants about 2000, and copes with up to 15000).\n";
		if(hull.turned)
			out() << "Note: the model was longer across than fore to aft, so it was turned a quarter turn: "
					 "the game takes +Y as fore. Use --keep-axes to leave it as it is.\n";
	for(const QString& note : importNotes)
		out() << "Note: " << note << "\n";
		out() << "Usable interior: " << qRound(hull.usableMax.x - hull.usableMin.x) << " x "
			  << qRound(hull.usableMax.y - hull.usableMin.y) << " x " << qRound(hull.usableMax.z - hull.usableMin.z)
			  << " feet.\n\n";

	for(const QString& strategy : strategies)
	{
		++index;
		AuScDesign design = baseDesign;

		LayoutOptions sOpts = opts;
		sOpts.strategy = strategy;
		const LayoutResult result = GenerateLayout(design, brief, hull, sOpts);

		QStringList errors, warnings;
		ValidateDesign(design, errors, warnings);

		const QString tag = variations > 1 ? QString("%1_%2").arg(strategy).arg(variation + 1) : strategy;
		const QString fileName = QDir(outDir).filePath(SanitiseName(brief.name) + "_" + SanitiseName(tag) + ".SoH");
		const bool saved = design.SaveToFile(fileName, SOH_FILE_TYPE);

		out() << "Candidate " << index << " (" << tag << "):\n";
		out() << "  rooms placed    : " << result.roomsPlaced << "\n";
		out() << "  stations placed : " << result.stationsPlaced << "\n";
		out() << "  berths placed   : " << result.berthsPlaced << "\n";
		out() << "  doors placed    : " << result.doorsPlaced << "\n";
		out() << "  windows placed  : " << result.windowsPlaced << "\n";
		out() << "  door ramps      : " << result.rampsPlaced << "\n";
		if(!result.missingRooms.isEmpty())
			out() << "  missing rooms   : " << result.missingRooms.join(", ") << "\n";
		for(const QString& w : result.warnings)
			out() << "  note            : " << w << "\n";
		for(const QString& e : errors)
			out() << "  ERROR           : " << e << "\n";
		out() << "  saved as        : " << fileName << (saved ? "" : "  (FAILED TO SAVE)") << "\n\n";
	}
	}

	out() << flush;
	return 0;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

int cmdGenerateBuilding(const QStringList& args)
{
	QString briefFile, outDir, name, layout;
	QStringList modelFiles;
	QStringList textureFiles;
	PaintOptions paint;
	double fit = 0.0;
	double scale = 1.0;
	quint32 maxFaces = 5000;
	ModelShape shape;
	BaseShape base;
	bool repair = true;
	QString presetKey;
	int variations = 1;
	for(int i = 0; i < args.size(); ++i)
	{
		const QString& a = args.at(i);
		bool hasNext = (i + 1) < args.size();
		if(a == "--brief" && hasNext)		briefFile = args.at(++i);
		else if(a == "--out" && hasNext)		outDir = args.at(++i);
		else if(a == "--name" && hasNext)		name = args.at(++i);
		else if(a == "--layout" && hasNext)	layout = args.at(++i);
		else if(a == "--model" && hasNext)		modelFiles << args.at(++i);
		else if(a == "--texture" && hasNext)	textureFiles << args.at(++i);
		else if(a == "--paint" && hasNext)
		{
			if(!PaintProjectionFromName(args.at(++i), paint.projection))
			{
				err() << "unknown paint projection: " << args.at(i) << "\n";
				return 2;
			}
		}
		else if(a == "--paint-scale" && hasNext)	paint.size = args.at(++i).toDouble();
		else if(a == "--paint-per-part")		paint.perPart = true;
		else if(a == "--paint-single")			paint.perPart = false;
		else if(a == "--fit" && hasNext)		fit = args.at(++i).toDouble();
		else if(a == "--base" && hasNext)
		{
			BaseShapeKind kind;
			if(!FindBaseShape(args.at(++i), kind))
			{
				err() << "unknown base shape: " << args.at(i) << "\n";
				return 2;
			}
			base.kind = kind;
		}
		else if(a == "--base-size" && hasNext)
		{
			const QStringList parts = args.at(++i).toLower().split('x');
			if(parts.size() != 2 || parts.at(0).toDouble() <= 0 || parts.at(1).toDouble() <= 0)
			{
				err() << "--base-size wants width x depth, like 140x100\n";
				return 2;
			}
			base.width = parts.at(0).toDouble();
			base.depth = parts.at(1).toDouble();
		}
		else if(a == "--base-height" && hasNext)	base.height = args.at(++i).toDouble();
		else if(a == "--base-together")			base.separateModels = false;
		else if(a == "--base-separate")			base.separateModels = true;
		else if(a == "--faces" && hasNext)	maxFaces = args.at(++i).toUInt();
		else if(a == "--taper" && hasNext)	shape.taperTop = args.at(++i).toDouble();
		else if(a == "--taper-bottom" && hasNext)	shape.taperBottom = args.at(++i).toDouble();
		else if(a == "--twist" && hasNext)	shape.twist = args.at(++i).toDouble();
		else if(a == "--repeat" && hasNext)	shape.repeats = args.at(++i).toInt();
		else if(a == "--repeat-radius" && hasNext)	shape.repeatRadius = args.at(++i).toDouble();
		else if(a == "--stack" && hasNext)	shape.stack = args.at(++i).toInt();
		else if(a == "--noise" && hasNext)	shape.noise = args.at(++i).toDouble();
		else if(a == "--noise-scale" && hasNext)	shape.noiseScale = args.at(++i).toDouble();
		else if(a == "--stretch" && hasNext)	shape.stretch = args.at(++i).toDouble();
		else if(a == "--seed" && hasNext)	shape.seed = args.at(++i).toInt();
		else if(a == "--preset" && hasNext)	presetKey = args.at(++i);
		else if(a == "--no-repair")			repair = false;
		else if(a == "--vary" && hasNext)	variations = args.at(++i).toInt();
		else if(a == "--scale" && hasNext)		scale = args.at(++i).toDouble();
		else
		{
			err() << "unknown or incomplete option: " << a << "\n";
			return 2;
		}
	}
	if(!presetKey.isEmpty())
	{
		ShapePreset preset;
		if(!FindShapePreset(presetKey, preset))
		{
			err() << "unknown shape preset: " << presetKey << "\n";
			return 2;
		}
		shape = preset.shape;
	}
	paint.textures = textureFiles;
	if(briefFile.isEmpty())
	{
		err() << "--brief is required\n";
		return 2;
	}
	if(outDir.isEmpty())
		outDir = ".";
	QDir().mkpath(outDir);

	BuildingBrief brief;
	QString error;
	if(!LoadBuildingBrief(briefFile, brief, error))
	{
		err() << error << "\n";
		return 1;
	}
	if(brief.name.isEmpty())
		brief.name = name.isEmpty() ? QString("Generated Building") : name;

	QStringList strategies;
	if(!layout.isEmpty() && layout != "auto")
		strategies << layout;
	else
		strategies << QStringList() << "grid" << "corridor";

	int index = 0;
	for(int variation = 0; variation < qMax(1, variations); ++variation)
	{
		QStringList importNotes;
	for(const QString& strategy : strategies)
	{
		++index;
		AuBdDesign design;
		design.SetId(1);
		design.SetArchitectId(AuId(0));
		design.SetArchitectName("shipgen");
		design.SetName(variations > 1 ? QString("%1 %2").arg(brief.name).arg(variation + 1) : brief.name);

		if(!modelFiles.isEmpty() || base.IsSet())
		{
			// The model or the primitive base becomes the shell of the building.
			ModelImportOptions modelOpts;
			modelOpts.scale = scale;
			modelOpts.fit = fit;
			modelOpts.maxFaces = maxFaces;
			modelOpts.shape = VaryShape(shape, variation);
			modelOpts.base = base;
			modelOpts.paint = paint;
			modelOpts.repair = repair;
			modelOpts.notes = &importNotes;
			AuPoint modelMin, modelMax;
			if(!ImportHullModel(design, modelFiles, modelOpts, modelMin, modelMax, error))
			{
				err() << error << "\n";
				return 1;
			}
		}

		const BuildingResult result = GenerateBuilding(design, brief, strategy);

		QStringList errors, warnings;
		ValidateBuilding(design, errors, warnings);

		const QString tag = variations > 1 ? QString("%1_%2").arg(strategy).arg(variation + 1) : strategy;
		const QString fileName = QDir(outDir).filePath(SanitiseName(brief.name) + "_" + SanitiseName(tag) + ".SoH");
		const bool saved = design.SaveToFile(fileName, SOH_FILE_TYPE);

		out() << "Candidate " << index << " (" << tag << "):\n";
		out() << "  rooms placed    : " << result.roomsPlaced << "\n";
		out() << "  doors placed    : " << result.doorsPlaced << "\n";
		out() << "  posts placed    : " << result.postsPlaced << "\n";
		for(const QString& note : importNotes)
			out() << "  note            : " << note << "\n";
		out() << "  faces           : " << result.faces << " ("
			  << (result.hullFaces <= 2000 ? "ideal" : (result.hullFaces <= 5000 ? "fine" : "over the ideal of 5000"))
			  << ")\n";
		out() << "  shell faces     : " << result.hullFaces << "\n";
		if(!result.missingRooms.isEmpty())
			out() << "  missing rooms   : " << result.missingRooms.join(", ") << "\n";
		for(const QString& w : result.warnings)
			out() << "  note            : " << w << "\n";
		for(const QString& e : errors)
			out() << "  ERROR           : " << e << "\n";
		out() << "  saved as        : " << fileName << (saved ? "" : "  (FAILED TO SAVE)") << "\n\n";
	}
	}

	out() << flush;
	return 0;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

void usage()
{
	out() << "shipgen " SHIPGEN_VERSION " - Shores of Hazeron layout generator\n\n";
	out() << "usage:\n";
	out() << "  shipgen inspect   <design.SoH>\n";
	out() << "  shipgen audit     <design.SoH>    rooms against the hull triangles\n";
	out() << "  shipgen parts     <design.SoH> [<type>]  every part: type, group, door tie, module, bounds\n";
	out() << "  shipgen kit       list | <key> | all  [--out <dir>]  the Aether kit: high-tech and magic blueprints\n";
	out() << "  shipgen faces     <design.SoH> [<part>]  how the faces look: texture slot, paint, opacity, glow, effect\n";
	out() << "  shipgen cut       <in.SoH> <out.SoH> window|door <hull part> <room part|0> x0 y0 z0 x1 y1 z1\n";
	out() << "                     cut a window or door through the hull (and a room) the way the game's designer does\n";
	out() << "  shipgen resave    <in.SoH> <out.SoH>  read and write back, keeping parts this build cannot read\n";
	out() << "  shipgen plan-example <plan.json>     write a small triangle plan to start from\n";
	out() << "  shipgen plan-flip <plan.json> <out.json> [--turn] [--mirror] [--fore-aft] [--upside-down] [--deck <n>]\n"
			 "                                       turn a plan around, mirror it or turn it upside down\n";
	out() << "  shipgen plan-from-model <model> <plan.json> [--length <ft>] [--threshold <0..1>] [--flip] [--y-up]\n";
	out() << "                     a triangle plan with its hull filled in from a 3D model\n";
	out() << "  shipgen build-plan   <plan.json> [--out <file.SoH>] [--no-furnish] [--no-paths] [--no-ramps]\n";
	out() << "                     turn a triangle plan (from shipgen-designer) into a blueprint\n";
	out() << "  shipgen validate  <design.SoH>\n";
	out() << "  shipgen templates\n";
	out() << "  shipgen brief         <template> <file.json>\n";
	out() << "  shipgen building-brief <template> <file.json>\n";
	out() << "  shipgen set-brief      <file.json>\n";
	out() << "  shipgen generate-set   --set <file.json> [--out <dir>]\n";
	out() << "  shipgen mesh-info --model <file.3ds|stl|obj>\n";
	out() << "  shipgen reduce     --model <file.3ds|stl|obj> --faces <n> --out <file>\n";
	out() << "                     [--method quadric|cluster] [--no-repair]\n";
	out() << "  shipgen generate  --model <file.3ds|file.stl> | --hull <design.SoH> | --base <shape>\n";
	out() << "                     --brief <file.json> [--out <dir>] [--texture <file.png|#rrggbb>]...\n";
	out() << "                     [--paint by-face|planar|cylindrical|spherical|keep]\n";
	out() << "                     [--paint-scale <feet>] [--paint-per-part|--paint-single]\n";
	out() << "                     [--name <name>] [--layout decks|spine|blocks|auto]\n";
	out() << "                     [--scale <n>] [--fit <feet>] [--wall <feet>] [--deck <feet>]\n";
	out() << "                     [--decimate <feet>] [--faces <n>]\n";
	out() << "                     [--base none|rectangle|circle|hexagon] [--base-size <w>x<d>]\n";
	out() << "                     [--base-height <feet>] [--base-together|--base-separate]\n";
	out() << "                     [--allow-open-hull]  place rooms in a hull that is not closed (unverified)\n";
	out() << "                     [--ftl none|warp1..warp9|wormhole-p|wormhole-pn|wormhole-pnn]  default wormhole-p\n";
	out() << "                     [--keep-axes]  do not turn a hull that is longer along X (the game's fore is +Y)\n";
	out() << "  shipgen generate-building --brief <file.json> [--out <dir>] [--name <name>]\n";
	out() << "                     [--layout grid|corridor|auto] [--model <file>] [--fit <feet>]\n";
	out() << "                     [--texture <file.png|#rrggbb>]... [--paint <mode>] [--paint-scale <feet>]\n";
	out() << "                     [--scale <n>] [--faces <n>]\n";
	out() << "                     [--base none|rectangle|circle|hexagon] [--base-size <w>x<d>]\n";
	out() << "                     [--base-height <feet>] [--base-together|--base-separate]\n";
	out() << "\n";
	out() << "  The shape of the model:\n";
	out() << "    --preset <name>       put the model into a named look\n";
	out() << "    --taper <n>            how wide it is at the top\n";
	out() << "    --taper-bottom <n>     how wide it is at the bottom\n";
	out() << "    --twist <degrees>      how far it turns about its axis\n";
	out() << "    --repeat <n>           copies around a circle\n";
	out() << "    --repeat-radius <n>    how far apart the copies are\n";
	out() << "    --stack <n>            copies on top of each other\n";
	out() << "    --noise <n>            how rough the surface is\n";
	out() << "    --noise-scale <n>      how big the lumps are\n";
	out() << "    --stretch <n>          how tall it is\n";
	out() << "    --seed <n>             the seed for the roughness\n";
	out() << "    --vary <n>             make this many, each one a little different\n";
	out() << "\n";
	out() << "  The named looks:\n";
	out() << "    as-is spire obelisk column flared spiral spiral-cluster\n";
	out() << "    cathedral ring tower rough wide\n";
	out() << flush;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Taking a model down to a number of faces.

int cmdReduce(const QStringList& args)
{
	QStringList modelFiles;
	QString outFile;
	int faces = 5000;
	QString method = "quadric";
	bool repair = true;

	for(int i = 0; i < args.size(); ++i)
	{
		const QString& a = args.at(i);
		const bool hasNext = (i + 1) < args.size();
		if(a == "--model" && hasNext)			modelFiles << args.at(++i);
		else if(a == "--faces" && hasNext)	faces = args.at(++i).toInt();
		else if(a == "--out" && hasNext)		outFile = args.at(++i);
		else if(a == "--method" && hasNext)	method = args.at(++i);
		else if(a == "--no-repair")		repair = false;
		else
		{
			err() << "unknown or incomplete option: " << a << "\n";
			return 2;
		}
	}
	if(modelFiles.isEmpty())
	{
		err() << "--model is required\n";
		return 2;
	}
	if(outFile.isEmpty())
	{
		err() << "--out is required\n";
		return 2;
	}

	SimplifyMethod how = SimplifyMethod::Quadric;
	if(method == "cluster")
		how = SimplifyMethod::Cluster;
	else if(method != "quadric")
	{
		err() << "unknown method: " << method << " (use quadric or cluster)\n";
		return 2;
	}

	// Read every file.
	AuScTriList all;
	for(const QString& fileName : modelFiles)
	{
		AuScTriList tris;
		QString error;
		if(!LoadMeshFile(fileName, tris, error))
		{
			err() << error << "\n";
			return 1;
		}
		out() << fileName << ": " << tris.size() << " faces\n";
		all.append(tris);
	}

	// A model that is not closed cannot be measured, so the holes are closed
	// and the windings put right before anything else happens.
	if(repair)
	{
		const MeshReport before = InspectMesh(all);
		if(!before.windingsAgree)
		{
			all = FixWindings(all);
			out() << "turned the triangles that were the wrong way round\n";
		}
		if(before.loose > 0)
		{
			all = FillHoles(all);
			const MeshReport after = InspectMesh(all);
			out() << "closed " << before.loose << " loose edge(s), " << after.loose << " left\n";
		}
	}

	// Thin it out.
	QElapsedTimer timer;
	timer.start();
	AuScTriList reduced = SimplifyMesh(all, faces, how);

	// Thinning a model out can open holes in it, so they are closed again.
	if(repair)
	{
		const MeshReport before = InspectMesh(reduced);
		if(before.loose > 0)
		{
			reduced = FillHoles(reduced);
			const MeshReport after = InspectMesh(reduced);
			out() << "closed " << before.loose << " hole(s) that the thinning opened, "
				  << after.loose << " left\n";
		}
	}
	const qint64 ms = timer.elapsed();

	// Write it out.
	QString error;
	if(!SaveMeshFile(outFile, reduced, error))
	{
		err() << error << "\n";
		return 1;
	}

	out() << "kept " << reduced.size() << " faces out of " << all.size() << " in " << ms << " ms\n";
	out() << "wrote " << outFile << "\n";
	out() << flush;
	return 0;
}

// Looking at a model.

int cmdMeshInfo(const QStringList& args)
{
	QStringList modelFiles;
	for(int i = 0; i < args.size(); ++i)
	{
		const QString& a = args.at(i);
		if(a == "--model" && i + 1 < args.size())
			modelFiles << args.at(++i);
		else
		{
			err() << "unknown or incomplete option: " << a << "\n";
			return 2;
		}
	}
	if(modelFiles.isEmpty())
	{
		err() << "--model is required\n";
		return 2;
	}

	AuScTriList all;
	for(const QString& fileName : modelFiles)
	{
		AuScTriList tris;
		QString error;
		if(!LoadMeshFile(fileName, tris, error))
		{
			err() << error << "\n";
			return 1;
		}
		all.append(tris);
	}

	const MeshReport report = InspectMesh(all);
	AuPoint min, max;
	MeshExtents(all, min, max);

	out() << "triangles      : " << report.triangles << "\n";
	out() << "vertices       : " << report.vertices << "\n";
	out() << "size           : " << (max.x - min.x) << " x " << (max.y - min.y) << " x " << (max.z - min.z) << "\n";
	out() << "edges          : " << report.edges << "\n";
	out() << "loose edges    : " << report.loose << (report.loose ? "  (the model has holes in it)" : "  (the model is closed)") << "\n";
	out() << "no area        : " << report.degenerate << "\n";
	out() << "duplicates     : " << report.duplicate << "\n";
	out() << "windings       : " << (report.windingsAgree ? "all the same way round" : "some of them are the wrong way round") << "\n";
	const MeshSolidity solid = AuditSolidity(all);
	out() << "solid          : "
			<< (solid.Closed() ? QString("yes, closed")
					: QString("no (%1 open, %2 thick, %3 folded edge(s))")
						.arg(solid.openEdges).arg(solid.thickEdges).arg(solid.foldedEdges))
			<< "\n";
	out() << flush;
	return 0;
}

// Making a set of buildings.

int cmdGenerateSet(const QStringList& args)
{
	QString setFile, outDir;
	for(int i = 0; i < args.size(); ++i)
	{
		const QString& a = args.at(i);
		const bool hasNext = (i + 1) < args.size();
		if(a == "--set" && hasNext)			setFile = args.at(++i);
		else if(a == "--out" && hasNext)	outDir = args.at(++i);
		else
		{
			err() << "unknown or incomplete option: " << a << "\n";
			return 2;
		}
	}
	if(setFile.isEmpty())
	{
		err() << "--set is required\n";
		return 2;
	}
	if(outDir.isEmpty())
		outDir = ".";
	QDir().mkpath(outDir);

	BuildingSet set;
	QString error;
	if(!LoadBuildingSet(setFile, set, error))
	{
		err() << error << "\n";
		return 1;
	}

	const BuildingSetResult result = GenerateBuildingSet(set, outDir);
	for(const QString& w : result.warnings)
		out() << "  warning         : " << w << "\n";
	for(const QString& e : result.errors)
		out() << "  ERROR           : " << e << "\n";
	out() << "Built " << result.built << " blueprint(s):\n";
	for(const QString& f : result.fileNames)
		out() << "  " << f << "\n";
	out() << flush;
	return result.errors.isEmpty() ? 0 : 1;
}

int cmdSetBrief(const QStringList& args)
{
	if(args.isEmpty())
	{
		err() << "usage: shipgen set-brief <file.json>\n";
		return 2;
	}
	BuildingSet set;
	set.name = "Aetherite Buildings";
	set.fit = 397;
	set.maxFaces = 5000;

	BuildingSetEntry entry;
	entry.templateKey = "capitol";
	entry.shapeKey = "cathedral";
	entry.name = "Crystal Capitol";
	set.buildings << entry;

	entry.templateKey = "arena";
	entry.shapeKey = "spiral";
	entry.name = "Spiral Arena";
	set.buildings << entry;

	entry.templateKey = "office_block";
	entry.shapeKey = "tower";
	entry.name = "Crystal Tower";
	entry.variations = 2;
	set.buildings << entry;

	QString error;
	if(!SaveBuildingSet(args.first(), set, error))
	{
		err() << error << "\n";
		return 1;
	}
	out() << "wrote " << args.first() << "\n";
	out() << flush;
	return 0;
}

} // namespace

int main(int argc, char* argv[])
{
	// The engine's obstruction-cache warning does not apply to an offline
	// tool; see logging.h. Everything else is reported as usual.
	InstallShipgenMessageHandler();

	QCoreApplication app(argc, argv);
	const QStringList args = app.arguments();

	// The engine's report and design code reaches for the global data object.
	// It is normally created by the game client, so make a bare one here.
	static DgData g_data;
	if(!gp_AuGlobal)
		gp_AuGlobal = new AuGlobal("Shipgen", "Hazeron Freehold", AuGlobal::WebCGI, &g_data);
	InstallIndustryTable(&g_data);

	if(args.size() < 2)
	{
		usage();
		return 2;
	}

	const QString command = args.at(1);
	const QStringList rest = args.mid(2);

	if(command == "--version" || command == "version")
	{
		out() << "shipgen " SHIPGEN_VERSION "\n";
		return 0;
	}

	if(command == "inspect")			return cmdInspect(rest);
	if(command == "audit")				return cmdAudit(rest);
	if(command == "resave")				return cmdResave(rest);
	if(command == "parts")				return cmdParts(rest);
	if(command == "cut")				return cmdCut(rest);
	if(command == "faces")				return cmdFaces(rest);
	if(command == "kit")				return cmdKit(rest);
	if(command == "plan-example")		return cmdPlanExample(rest);
	if(command == "plan-flip")			return cmdPlanFlip(rest);
	if(command == "plan-from-model")	return cmdPlanFromModel(rest);
	if(command == "build-plan")			return cmdBuildPlan(rest);
	if(command == "validate")			return cmdValidate(rest);
	if(command == "templates")			return cmdTemplates();
	if(command == "brief")				return cmdBrief(rest);
	if(command == "building-brief")	return cmdBuildingBrief(rest);
	if(command == "generate")			return cmdGenerate(rest);
	if(command == "generate-building")	return cmdGenerateBuilding(rest);
	if(command == "generate-set")		return cmdGenerateSet(rest);
	if(command == "reduce")			return cmdReduce(rest);
	if(command == "mesh-info")		return cmdMeshInfo(rest);
	if(command == "set-brief")			return cmdSetBrief(rest);
	if(command == "help" || command == "--help" || command == "-h")
	{
		usage();
		return 0;
	}

	err() << "unknown command: " << command << "\n";
	usage();
	return 2;
}
