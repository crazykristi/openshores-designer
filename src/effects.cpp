//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the game's animated surfaces. See effects.h.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "effects.h"

#include <QSet>

#include "aubddesign.h"
#include "aubdreport.h"
#include "audesign.h"
#include "augleffects.h"
#include "auscdesign.h"
#include "auscparts.h"
#include "auscreport.h"

DesignEffects::DesignEffects()
{
	for(int e = 0; e < EFF_Count; ++e)
	{
		faces[e] = 0;
		possible[e] = false;
	}
}

QStringList DesignEffects::Unused() const
{
	QStringList lines;
	for(int e = 1; e < EFF_Count; ++e)
		if(faces[e] && !possible[e])
			lines << QString("%1: %2").arg(SurfaceEffectName(SURFACE_EFFECT(e)), need[e]);
	return lines;
}

namespace
{
// The numbers (0 to 9) that the stations of one kind pick for their effect.
QSet<int> StationEffects(const AuScParts& parts, AuScPart::PART_TYPE a, AuScPart::PART_TYPE b)
{
	QSet<int> which;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(p && !p->IsOmitted() && (p->Type() == a || p->Type() == b))
			which << p->WhichEffect();
	}
	return which;
}

bool IsPlatform(BUILDING_STRUCTURE s)
{
	return s == BD_SeaPlatform || s == BD_AerialPlatform || s == BD_GroundPlatform;
}
}

DesignEffects AnalyseEffects(const AuDesign& design, bool evenUnused)
{
	DesignEffects fx;
	const AuScParts& parts = design.Parts();
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* p = it.value();
		if(!p || p->IsOmitted())
			continue;
		AuScFaceList faces;
		p->Faces(faces);
		for(const AuScFace* f : faces)
			if(f->Effect() > EFF_None && f->Effect() < EFF_Count)
				++fx.faces[f->Effect()];
	}

	if(!evenUnused)
	{
		bool any = false;
		for(int e = 1; e < EFF_Count && !any; ++e)
			any = fx.faces[e] > 0;
		if(!any)
			return fx;		// Nothing to say, and the design's report is not free.
	}

	const QSet<int> engineers = StationEffects(parts, AuScPart::Part_StationEngineer, AuScPart::Part_ConsoleEngineer);
	const QSet<int> fireControl = StationEffects(parts, AuScPart::Part_StationFireControl, AuScPart::Part_ConsoleFireControl);
	const QSet<int> transporters = StationEffects(parts, AuScPart::Part_StationTransporter, AuScPart::Part_ConsoleTransporter);
	auto stations = [&](int first, int count, const QSet<int>& which, const QString& station, bool also, const QString& alsoNeed,
			const QString& runs) {
		for(int i = 0; i < count; ++i)
		{
			const int e = first + i;
			if(!which.contains(i))
				fx.need[e] = QString("no %1 picks effect %2").arg(station).arg(i + 1);
			else if(!also)
				fx.need[e] = alsoNeed;
			else
			{
				fx.possible[e] = true;
				fx.need[e] = runs;
			}
		}
	};

	if(const AuBdDesign* building = dynamic_cast<const AuBdDesign*>(&design))
	{
		fx.building = true;
		AuBdReport report;
		building->ReportDesign(report);
		const BUILDING_STRUCTURE structure = building->Structure();
		fx.aerialPlatform = structure == BD_AerialPlatform;
		const bool capacitor = report.CapacitorVolume() > 0;
		auto set = [&](int e, bool can, const QString& yes, const QString& no) {
			fx.possible[e] = can;
			fx.need[e] = can ? yes : no;
		};
		set(EFF_PowerPlant, IsPlatform(structure) || report.PowerPlantVolume() > 0,
				IsPlatform(structure) ? "always runs on a platform" : "runs while the power plant is not wrecked",
				"needs a power plant");
		set(EFF_Capacitor, capacitor, "runs while the capacitor holds a charge", "needs a capacitor");
		set(EFF_LifeSupport, report.LifeSupportVolume() > 0, "runs while life support draws power",
				"needs life support");
		set(EFF_Sensor, report.SensorVolume() > 0 && capacitor, "runs while the capacitor holds a charge",
				report.SensorVolume() > 0 ? "needs a capacitor" : "needs sensors");
		set(EFF_Shield, report.ShieldVolume() > 0 && capacitor, "runs while the shields are up",
				report.ShieldVolume() > 0 ? "needs a capacitor" : "needs a shield");
		set(EFF_ManeuverDrive, fx.aerialPlatform, "runs low on an aerial platform",
				"only an aerial platform runs it");
		for(int i = 0; i < 4; ++i)
			set(EFF_FTLDrive1 + i, false, QString(), "never runs on a building");
		set(EFF_Hull, false, QString(), "on a building it shows damage instead");
		stations(EFF_TransporterPad1, 10, transporters, "transporter station", true, QString(),
				"runs while that transporter is on");
		stations(EFF_Weapon1, 10, fireControl, "fire control station", true, QString(),
				"runs for two seconds after that weapon fires");
		return fx;
	}

	const AuScDesign* ship = dynamic_cast<const AuScDesign*>(&design);
	if(!ship)
		return fx;
	// No station of any kind: a hull draft, before the build puts the
	// systems and the crew in. Nothing can be told yet, so all is taken to run.
	bool anyStation = false;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd() && !anyStation; ++it)
		anyStation = it.value() && !it.value()->IsOmitted() && (it.value()->IsCommandStation() || it.value()->IsControlStation()
				|| it.value()->IsConsole());
	if(!anyStation)
	{
		fx.draft = true;
		for(int e = 1; e < EFF_Count; ++e)
		{
			fx.possible[e] = true;
			fx.need[e] = "a hull draft: taken to run until the ship is built";
		}
		return fx;
	}
	AuScReport report;
	ship->ReportDesign(report);
	const bool power = report.PowerPlantVolume() > 0;
	const bool capacitor = report.CapacitorVolume() > 0;
	const bool ftl = !report.FTLDrives().isEmpty();
	fx.warp = report.FTLWarpDrive() != 0;
	auto set = [&](int e, bool can, const QString& yes, const QString& no) {
		fx.possible[e] = can;
		fx.need[e] = can ? yes : no;
	};
	set(EFF_PowerPlant, power, "runs while the power plant works", "needs a power plant");
	set(EFF_Capacitor, capacitor, "runs while the capacitor holds a charge", "needs a capacitor");
	set(EFF_LifeSupport, report.LifeSupportVolume() > 0, "runs while life support works", "needs life support");
	set(EFF_Sensor, report.SensorVolume() > 0, "runs while the sensors work", "needs sensors");
	set(EFF_Shield, report.ShieldVolume() > 0, "runs while the shields are up", "needs a shield");
	set(EFF_ManeuverDrive, report.ManeuverDriveVolume() > 0, "runs low holding still, high under way",
			"needs a maneuver drive");
	stations(EFF_FTLDrive1, 4, engineers, "engineer station", ftl && capacitor,
			!ftl ? QString("needs an FTL drive") : QString("needs a capacitor"),
			"runs low while that FTL drive is on, high while it is charged");
	set(EFF_Hull, ftl && capacitor, fx.warp ? "runs while the warp drive is charged" : "runs while the FTL drive is charged",
			!ftl ? "needs an FTL drive" : "needs a capacitor");
	stations(EFF_TransporterPad1, 10, transporters, "transporter station", true, QString(),
			"runs low while that transporter is on, high while it sends");
	stations(EFF_Weapon1, 10, fireControl, "fire control station", true, QString(),
			"runs for two seconds after that weapon fires");
	return fx;
}

EffectLevel EffectLevelFor(const DesignEffects& fx, const EffectSituation& s, int e)
{
	if(e <= EFF_None || e >= EFF_Count || !fx.possible[e] || !s.power)
		return EffectOff;
	if(e >= EFF_FTLDrive1 && e <= EFF_FTLDrive4)
		return s.ftlCharged ? EffectHigh : EffectLow;
	if(e >= EFF_TransporterPad1 && e <= EFF_TransporterPad10)
		return s.transporting ? EffectHigh : EffectLow;
	if(e >= EFF_Weapon1 && e <= EFF_Weapon10)
		return s.weaponsFiring ? EffectHigh : EffectOff;
	switch(e)
	{
	case EFF_ManeuverDrive:
		if(fx.building)
			return EffectLow;		// An aerial platform: always low.
		return s.underWay ? EffectHigh : EffectLow;
	case EFF_Shield:
		return s.shieldsUp ? EffectHigh : EffectOff;
	case EFF_Hull:
		return s.ftlCharged ? EffectHigh : EffectOff;
	default:
		return EffectHigh;
	}
}

int EffectFrameIndex(const DesignEffects& fx, int e, EffectLevel level, int frame)
{
	if(level == EffectOff || e <= EFF_None || e >= EFF_Count)
		return 0;
	typedef AuTextureLibItem T;
	int first = T::EI_None;
	if(e >= EFF_FTLDrive1 && e <= EFF_FTLDrive4)
		first = level == EffectHigh ? T::EI_FTLDrive1 : T::EI_FTLDriveLow1;
	else if(e >= EFF_TransporterPad1 && e <= EFF_TransporterPad10)
		first = level == EffectHigh ? T::EI_Transporter1 : T::EI_TransporterLow1;
	else if(e >= EFF_Weapon1 && e <= EFF_Weapon10)
		first = T::EI_Weapon1;
	else
		switch(e)
		{
		case EFF_LifeSupport:	first = T::EI_LifeSupport1; break;
		case EFF_ManeuverDrive:	first = level == EffectHigh ? T::EI_ManeuverDrive1 : T::EI_ManeuverDriveLow1; break;
		case EFF_PowerPlant:	first = T::EI_PowerPlant1; break;
		case EFF_Sensor:		first = T::EI_Sensor1; break;
		case EFF_Shield:		first = T::EI_Shield1; break;
		case EFF_Capacitor:		first = T::EI_Capacitor1; break;
		// The wormhole drive's look is for its positive setting, the one it starts in.
		case EFF_Hull:			first = fx.warp ? T::EI_HullWarp1 : T::EI_HullWormPos1; break;
		default:				return 0;
		}
	return first + qBound(0, frame, 5);
}

int EffectFrameCount()
{
	return AuTextureLibItem::EI_Count;
}

QString EffectIndexFile(int index)
{
	if(index <= 0 || index >= AuTextureLibItem::EI_Count)
		return QString();
	QString file = AuTextureLibItem::EffectTextureFile(AuTextureLibItem::EFFECT_INDEX(index));
	if(file.startsWith(":") && !file.startsWith(":/"))
		file.insert(1, '/');
	return file;
}

QString EffectFrameFile(const DesignEffects& fx, int e, EffectLevel level, int frame)
{
	return EffectIndexFile(EffectFrameIndex(fx, e, level, frame));
}

bool EffectFramesAtRandom(int e)
{
	// As the game: the drives, sensors, shields and weapons flicker; the rest
	// run through their frames in order.
	return (e >= EFF_FTLDrive1 && e <= EFF_FTLDrive4) || e == EFF_ManeuverDrive || e == EFF_Sensor || e == EFF_Shield
			|| (e >= EFF_Weapon1 && e <= EFF_Weapon10);
}

void ProjectEffect(AuScFace& face, double repeatFeet)
{
	if(face.Effect() == EFF_None || repeatFeet <= 0)
		return;
	AuMatrix4x4 identity;
	identity.LoadIdentity();
	// The game's own unwrap: flat onto the face, lined up with the vertical.
	face.EffectUnwrap(face.Effect(), repeatFeet, true, 2, identity);
}
