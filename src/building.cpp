//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - building blueprint generator.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "building.h"
#include "interior.h"

#include "generate.h"
#include "shape.h"

#include "names.h"

#include <QDataStream>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QSet>

#include "aufilever.h"
#include "auglobal.h"
#include "aubdreport.h"
#include "dbindustry.h"
#include "dgdata.h"

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The industry table.
//
// The real table lives in the game's database. This is a small stand-in with
// the industries that matter for a building blueprint, so that the engine's
// report can still work out whether a building is useful for anything.

namespace {

struct IndustryEntry
{
	INDUSTRYID	id;
	const char*	name;
	qint8		homeLarge;
	qint8		homeMedium;
	qint8		homeSmall;
	qint8		office;
	qint8		shop;
	qint8		store;
	qint8		lounge;
	qint8		generator;
	qint8		battery;
	qint8		cargoStorage;
	qint8		guardPost;
	qint8		sensor;
	qint8		shield;
	qint8		turret;
	qint8		weaponBay;
	qint8		surgery;
	qint8		transporter;
	qint8		parkGround;
	qint8		arena;
	qint8		auditorium;
	qint8		field;
	qint8		livestockPost;
	qint8		vehicleLauncher;
};

const IndustryEntry kIndustries[] = {
	{ IID_DesignStudio, "Design Studio", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Warehouse, "Warehouse", 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_House, "House", 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Apartment, "Apartment", 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Hotel, "Hotel", 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_BaseBarracks, "Base Barracks", 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_BaseMarineBarracks, "Marine Barracks", 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_BaseHeadquarters, "Base Headquarters", 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_BaseWeapon, "Weapon System", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_BaseShield, "Shield Generator", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_BaseRadar, "Radar", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_BaseGuardPost, "Guard Post", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_CoalPowerPlant, "Coal Power Plant", 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_GasolinePowerPlant, "Gasoline Power Plant", 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_HydroelectricPowerPlant, "Hydroelectric Plant", 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_HydrogenPowerPlant, "Hydrogen Power Plant", 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_SolarPowerPlant, "Solar Power Plant", 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_NuclearPowerPlant, "Nuclear Power Plant", 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Bank, "Bank", 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Broker, "Broker", 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Grocery, "Grocery", 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_RetailStore, "Retail Store", 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Cantina, "Cantina", 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_PoliceDepartment, "Police Department", 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_PoliceAcademy, "Police Academy", 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Hospital, "Hospital", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Capitol, "Capitol", 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_School, "School", 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Observatory, "Observatory", 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Park, "Park", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 },
	{ IID_Arena, "Arena", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 },
	{ IID_Farm, "Farm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
	{ IID_Mine, "Mine", 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_MachineShop, "Machine Shop", 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_Smelter, "Smelter", 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_ShipYard, "Ship Yard", 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ IID_Biodome, "Biodome", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_SpacecraftFactory, "Spacecraft Factory", 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ IID_BaseAirTerminal, "Base Air Terminal", 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
};

} // namespace

void InstallIndustryTable(DgData* data)
{
	if(!data)
		return;

	DbIndustryArray& industries = data->m_bIndustryArray;
	industries.clear();
	industries.resize(IID_Count);

	for(const IndustryEntry& e : kIndustries)
	{
		// The industry record's fields are private, so the record is written
		// out and read back in the same order the engine uses.
		QByteArray bytes;
		QDataStream out(&bytes, QIODevice::WriteOnly);
		out.setVersion(QDataStream::Qt_5_0);
		out << (quint8)0;						// capacitor
		out << (quint8)e.id;					// id
		out << QString(e.name);					// name
		out << (quint8)0;						// population
		out << (qint8)0;						// employees
		out << (quint8)0;						// storage
		out << (quint8)BD_Building;				// structure
		out << (qint8)e.field;
		out << (qint8)e.arena;
		out << (qint8)e.auditorium;
		out << (qint8)e.battery;
		out << (qint8)e.cargoStorage;
		out << (qint8)e.generator;
		out << (qint8)e.guardPost;
		out << (qint8)e.homeLarge;
		out << (qint8)e.homeMedium;
		out << (qint8)e.homeSmall;
		out << (qint8)e.livestockPost;
		out << (qint8)e.lounge;
		out << QString();						// note
		out << (qint8)e.office;
		out << (qint8)e.parkGround;
		out << (qint8)0;						// parkSpace
		out << (qint8)0;						// parkWater
		out << (qint8)e.store;
		out << (qint8)e.sensor;
		out << (qint8)e.shield;
		out << (qint8)e.shop;
		out << (qint8)e.surgery;
		out << (qint8)e.transporter;
		out << (qint8)e.turret;
		out << (qint8)e.vehicleLauncher;
		out << (qint8)0;						// vehicleRecovery
		out << (qint8)e.weaponBay;
		out << QString();						// doesWhat
		out << (quint8)0;						// buildRule

		QDataStream in(&bytes, QIODevice::ReadOnly);
		in.setVersion(QDataStream::Qt_5_0);
		DbIndustry db;
		db.operator<<(in);
		industries.Add(db);
	}
}

QStringList KnownIndustries()
{
	QStringList names;
	for(const IndustryEntry& e : kIndustries)
		names << QString("%1 (%2)").arg(e.name).arg(int(e.id));
	return names;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Templates.

QList<BuildingTemplate> AllBuildingTemplates()
{
	// A building template says what the building is for and what it provides.
	// The rooms are optional: a monument made from a model has none.
	QList<BuildingTemplate> list;

	{
		BuildingTemplate t;
		t.key = "capitol";
		t.label = "Capitol";
		t.description = "The seat of government. Offices, an auditorium for ceremonies and a place to park.";
		t.footprintX = 350;
		t.footprintY = 350;
		t.rooms.clear();
		t.volumes.offices = 500;
		t.volumes.store = 500;
		t.volumes.hold = 500;
		t.volumes.auditorium = 2000;
		t.industry = "capitol";
		t.groundParking = 5;
		t.spaceParking = 2;
		list << t;
	}

	{
		BuildingTemplate t;
		t.key = "arena";
		t.label = "Arena";
		t.description = "A venue for games and spectacles, with stands and parking for the crowds.";
		t.footprintX = 400;
		t.footprintY = 400;
		t.rooms.clear();
		t.volumes.arena = 6000;
		t.volumes.auditorium = 1200;
		t.volumes.store = 500;
		t.volumes.hold = 500;
		t.industry = "arena";
		t.groundParking = 6;
		t.spaceParking = 1;
		list << t;
	}

	{
		BuildingTemplate t;
		t.key = "office_block";
		t.label = "Office block";
		t.description = "Offices for a company or a department.";
		t.footprintX = 200;
		t.footprintY = 200;
		t.rooms.clear();
		t.volumes.offices = 2000;
		t.volumes.store = 500;
		t.volumes.hold = 500;
		t.industry = "office";
		t.groundParking = 4;
		t.spaceParking = 1;
		list << t;
	}

	{
		BuildingTemplate t;
		t.key = "residential";
		t.label = "Residential block";
		t.description = "Homes for a lot of people.";
		t.footprintX = 200;
		t.footprintY = 200;
		t.rooms.clear();
		t.volumes.homesMedium = 2000;
		t.volumes.store = 500;
		t.volumes.hold = 500;
		t.industry = "apartment";
		t.groundParking = 4;
		list << t;
	}

	{
		BuildingTemplate t;
		t.key = "barracks";
		t.label = "Barracks";
		t.description = "Accommodation for troops, with an armoury.";
		t.footprintX = 180;
		t.footprintY = 180;
		t.rooms.clear();
		t.volumes.homesMedium = 1500;
		t.volumes.store = 500;
		t.volumes.armor = 200;
		t.industry = "barracks";
		t.groundParking = 3;
		t.guardPosts = 2;
		list << t;
	}

	{
		BuildingTemplate t;
		t.key = "command_post";
		t.label = "Command post";
		t.description = "The headquarters of a military base, with sensors, shields and a guard post.";
		t.footprintX = 200;
		t.footprintY = 200;
		t.rooms.clear();
		t.volumes.offices = 1000;
		t.volumes.store = 500;
		t.volumes.sensors = 200;
		t.volumes.shields = 500;
		t.volumes.armor = 300;
		t.industry = "military";
		t.groundParking = 4;
		t.spaceParking = 1;
		t.guardPosts = 4;
		list << t;
	}

	{
		BuildingTemplate t;
		t.key = "warehouse";
		t.label = "Warehouse";
		t.description = "Storage and distribution.";
		t.footprintX = 250;
		t.footprintY = 250;
		t.rooms.clear();
		t.volumes.hold = 3000;
		t.volumes.store = 1000;
		t.industry = "warehouse";
		t.groundParking = 4;
		t.spaceParking = 2;
		list << t;
	}

	{
		BuildingTemplate t;
		t.key = "factory";
		t.label = "Factory";
		t.description = "A production building with stores and parking for the lorries.";
		t.footprintX = 300;
		t.footprintY = 300;
		t.rooms.clear();
		t.volumes.hold = 1500;
		t.volumes.store = 1000;
		t.volumes.offices = 500;
		t.industry = "factory";
		t.groundParking = 5;
		list << t;
	}

	{
		BuildingTemplate t;
		t.key = "biodome";
		t.label = "Biodome";
		t.description = "A sealed dome with a garden inside it.";
		t.footprintX = 250;
		t.footprintY = 250;
		t.rooms.clear();
		t.volumes.offices = 500;
		t.volumes.hold = 500;
		t.volumes.lifeSupport = 200;
		t.industry = "biodome";
		t.groundParking = 2;
		list << t;
	}

	return list;
}

const BuildingTemplate* FindBuildingTemplate(const QString& key)
{
	static const QList<BuildingTemplate> all = AllBuildingTemplates();
	for(const BuildingTemplate& t : all)
	{
		if(t.key == key)
			return &t;
	}
	return 0;
}

void ApplyBuildingTemplate(BuildingBrief& brief, const QString& templateKey)
{
	const BuildingTemplate* t = FindBuildingTemplate(templateKey);
	if(!t)
		return;
	brief.footprintX = t->footprintX;
	brief.footprintY = t->footprintY;
	brief.floors = t->floors;
	brief.floorHeight = t->floorHeight;
	brief.rooms = t->rooms;
	brief.volumes = t->volumes;
	brief.citizenPosts = t->citizenPosts;
	brief.guardPosts = t->guardPosts;
	brief.groundParking = t->groundParking;
	brief.spaceParking = t->spaceParking;
	if(brief.industry.isEmpty() || brief.industry == "auto")
		brief.industry = t->industry;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Building brief file I/O.

// Reads a brief out of a JSON object.
static void briefFromJson(const QJsonObject& o, BuildingBrief& brief)
{
	// The building settings can be at the top level or under "building".
	QJsonObject b = o.contains("building") ? o.value("building").toObject() : o;

	if(b.contains("name"))			brief.name = b.value("name").toString();
	if(b.contains("description"))	brief.description = b.value("description").toString();
	if(b.contains("structure"))		brief.structure = b.value("structure").toString();
	if(b.contains("industry"))		brief.industry = b.value("industry").toString();
	if(b.contains("layout"))			brief.layout = b.value("layout").toString();
	if(b.contains("floors"))			brief.floors = b.value("floors").toInt(1);
	if(b.contains("floorHeight"))	brief.floorHeight = b.value("floorHeight").toDouble(14);
	if(b.contains("wallThickness"))	brief.wallThickness = b.value("wallThickness").toDouble(4);
	if(b.contains("citizenPosts"))	brief.citizenPosts = b.value("citizenPosts").toInt();
	if(b.contains("guardPosts"))	brief.guardPosts = b.value("guardPosts").toInt();
	if(b.contains("livestockPosts"))	brief.livestockPosts = b.value("livestockPosts").toInt();
	if(b.contains("groundParking"))	brief.groundParking = b.value("groundParking").toInt();
	if(b.contains("spaceParking"))	brief.spaceParking = b.value("spaceParking").toInt();

	if(b.contains("footprint"))
	{
		QJsonArray fp = b.value("footprint").toArray();
		if(fp.size() >= 2)
		{
			brief.footprintX = fp.at(0).toDouble();
			brief.footprintY = fp.at(1).toDouble();
		}
	}
	if(b.contains("footprintX"))	brief.footprintX = b.value("footprintX").toDouble();
	if(b.contains("footprintY"))	brief.footprintY = b.value("footprintY").toDouble();

	if(b.contains("rooms"))
	{
		QJsonArray arr = b.value("rooms").toArray();
		for(const QJsonValue& v : arr)
		{
			QJsonObject ro = v.toObject();
			RoomSpec r;
			r.type = ro.value("type").toString();
			r.label = ro.value("label").toString();
			if(r.label.isEmpty())
				r.label = r.type;
			r.count = ro.value("count").toInt(1);
			if(ro.contains("size"))
			{
				QJsonArray sz = ro.value("size").toArray();
				if(sz.size() >= 3)
				{
					r.sizeX = sz.at(0).toDouble();
					r.sizeY = sz.at(1).toDouble();
					r.sizeZ = sz.at(2).toDouble();
				}
			}
			brief.rooms << r;
		}
	}

	if(b.contains("volumes"))
	{
		QJsonObject vo = b.value("volumes").toObject();
		BuildingVolumes& v = brief.volumes;
		v.offices = vo.value("offices").toInt();
		v.shops = vo.value("shops").toInt();
		v.store = vo.value("store").toInt();
		v.lounge = vo.value("lounge").toInt();
		v.homesLarge = vo.value("homesLarge").toInt();
		v.homesMedium = vo.value("homesMedium").toInt();
		v.homesSmall = vo.value("homesSmall").toInt();
		v.powerPlant = vo.value("powerPlant").toInt();
		v.lifeSupport = vo.value("lifeSupport").toInt();
		v.capacitor = vo.value("capacitor").toInt();
		v.sensors = vo.value("sensors").toInt();
		v.shields = vo.value("shields").toInt();
		v.hold = vo.value("hold").toInt();
		v.skeleton = vo.value("skeleton").toInt();
		v.armor = vo.value("armor").toInt();
		v.generators = vo.value("generators").toInt();
		v.guardPosts = vo.value("guardPosts").toInt();
		v.citizenPosts = vo.value("citizenPosts").toInt();
		v.livestockPosts = vo.value("livestockPosts").toInt();
		v.turrets = vo.value("turrets").toInt();
		v.weaponBays = vo.value("weaponBays").toInt();
		v.surgery = vo.value("surgery").toInt();
		v.transporters = vo.value("transporters").toInt();
		v.parkGround = vo.value("parkGround").toInt();
		v.parkSpace = vo.value("parkSpace").toInt();
		v.parkWater = vo.value("parkWater").toInt();
		v.arena = vo.value("arena").toInt();
		v.auditorium = vo.value("auditorium").toInt();
		v.field = vo.value("field").toInt();
	}
}

bool LoadBuildingBrief(const QString& fileName, BuildingBrief& brief, QString& error)
{
	QFile file(fileName);
	if(!file.open(QIODevice::ReadOnly))
	{
		error = QString("cannot open %1: %2").arg(fileName).arg(file.errorString());
		return false;
	}
	QJsonParseError err;
	QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
	if(err.error != QJsonParseError::NoError)
	{
		error = QString("JSON error at offset %1: %2").arg(err.offset).arg(err.errorString());
		return false;
	}
	if(!doc.isObject())
	{
		error = "brief must be a JSON object";
		return false;
	}
	briefFromJson(doc.object(), brief);
	return true;
}

bool SaveBuildingBrief(const QString& fileName, const BuildingBrief& brief, QString& error)
{
	QFile file(fileName);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		error = QString("cannot write %1: %2").arg(fileName).arg(file.errorString());
		return false;
	}

	QJsonObject o;
	o["name"] = brief.name;
	o["description"] = brief.description;
	o["structure"] = brief.structure;
	o["industry"] = brief.industry;
	o["layout"] = brief.layout;
	o["floors"] = brief.floors;
	o["floorHeight"] = brief.floorHeight;
	o["wallThickness"] = brief.wallThickness;
	o["citizenPosts"] = brief.citizenPosts;
	o["guardPosts"] = brief.guardPosts;
	o["livestockPosts"] = brief.livestockPosts;
	o["groundParking"] = brief.groundParking;
	o["spaceParking"] = brief.spaceParking;
	QJsonArray fp;
	fp.append(brief.footprintX);
	fp.append(brief.footprintY);
	o["footprint"] = fp;

	QJsonArray rooms;
	for(const RoomSpec& r : brief.rooms)
	{
		QJsonObject ro;
		ro["type"] = r.type;
		ro["label"] = r.label;
		ro["count"] = r.count;
		QJsonArray sz;
		sz.append(r.sizeX);
		sz.append(r.sizeY);
		sz.append(r.sizeZ);
		ro["size"] = sz;
		rooms.append(ro);
	}
	o["rooms"] = rooms;

	const BuildingVolumes& v = brief.volumes;
	QJsonObject vo;
	vo["offices"] = int(v.offices);
	vo["shops"] = int(v.shops);
	vo["store"] = int(v.store);
	vo["lounge"] = int(v.lounge);
	vo["homesLarge"] = int(v.homesLarge);
	vo["homesMedium"] = int(v.homesMedium);
	vo["homesSmall"] = int(v.homesSmall);
	vo["powerPlant"] = int(v.powerPlant);
	vo["lifeSupport"] = int(v.lifeSupport);
	vo["capacitor"] = int(v.capacitor);
	vo["sensors"] = int(v.sensors);
	vo["shields"] = int(v.shields);
	vo["hold"] = int(v.hold);
	vo["skeleton"] = int(v.skeleton);
	vo["armor"] = int(v.armor);
	vo["generators"] = v.generators;
	vo["guardPosts"] = v.guardPosts;
	vo["citizenPosts"] = v.citizenPosts;
	vo["livestockPosts"] = v.livestockPosts;
	vo["turrets"] = v.turrets;
	vo["weaponBays"] = v.weaponBays;
	vo["surgery"] = v.surgery;
	vo["transporters"] = v.transporters;
	vo["parkGround"] = v.parkGround;
	vo["parkSpace"] = v.parkSpace;
	vo["parkWater"] = v.parkWater;
	vo["arena"] = int(v.arena);
	vo["auditorium"] = int(v.auditorium);
	vo["field"] = v.field;
	o["volumes"] = vo;

	file.write(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Indented)).toUtf8());
	return true;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The generator.

namespace {

struct BuildingRoom
{
	QString	type;
	QString	label;
	AuPoint	mn;
	AuPoint	mx;
	int		floor = 0;
	quint16	voidId = 0;
};

// Builds the silhouette of the hull parts of a design. The rooms have to fit
// inside it.
static void HullSilhouette(const AuScParts& parts, AuScSilhouette3d& silhouette)
{
	AuScFaceList faces;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* part = it.value();
		if(!part || !part->IsHullMesh())
			continue;
		AuScFaceList partFaces;
		part->Faces(partFaces);
		faces.AddFaces(partFaces);
	}
	silhouette.Build(faces, 0.f);
}

// The extents of the hull parts of a design.
static bool HullExtents(const AuScParts& parts, AuPoint& min, AuPoint& max)
{
	bool have = false;
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* part = it.value();
		if(!part || !part->IsHullMesh())
			continue;
		AuScPoint pmin, pmax;
		part->ModelExtents(pmin, pmax);
		if(!have)
		{
			min = pmin;
			max = pmax;
			have = true;
		}
		else
		{
			min = min.Min(pmin);
			max = max.Max(pmax);
		}
	}
	return have;
}

// Finds the largest rectangle that fits inside the shape of the building.
// A model that comes from a 3D print site is a solid object, so the rooms have
// to be fitted into whatever space the shape leaves.
static bool LargestFreeRect(const AuScSilhouette3d& silhouette, const AuPoint& min, const AuPoint& max, double z,
		double cell, AuPoint& rectMin, AuPoint& rectMax)
{
	const int cols = int((max.x - min.x) / cell);
	const int rows = int((max.y - min.y) / cell);
	if(cols < 1 || rows < 1)
		return false;

	// For every row, the height of the run of cells that are inside the shape.
	QVector<int> heights(cols, 0);
	int bestArea = 0;
	int bestRow = 0;
	int bestLeft = 0;
	int bestRight = 0;
	int bestHeight = 0;

	for(int row = 0; row < rows; ++row)
	{
		const double y = min.y + (row + .5) * cell;
		for(int col = 0; col < cols; ++col)
		{
			const double x = min.x + (col + .5) * cell;
			if(silhouette.IsInside(AuPoint(x, y, z)))
				heights[col]++;
			else
				heights[col] = 0;
		}

		// The largest rectangle in the histogram of this row.
		QVector<int> stack;
		for(int col = 0; col <= cols; ++col)
		{
			const int height = (col < cols) ? heights[col] : 0;
			while(!stack.isEmpty() && heights[stack.last()] >= height)
			{
				const int bar = stack.takeLast();
				const int barHeight = heights[bar];
				const int left = stack.isEmpty() ? 0 : stack.last() + 1;
				const int area = barHeight * (col - left);
				if(area > bestArea)
				{
					bestArea = area;
					bestRow = row;
					bestLeft = left;
					bestRight = col - 1;
					bestHeight = barHeight;
				}
			}
			if(col < cols)
				stack.append(col);
		}
	}
	if(!bestArea)
		return false;

	rectMin = AuPoint(min.x + bestLeft * cell, min.y + (bestRow - bestHeight + 1) * cell, z);
	rectMax = AuPoint(min.x + (bestRight + 1) * cell, min.y + (bestRow + 1) * cell, z);
	return true;
}

// Packs the rooms of one floor into the floor plate. The rooms are scaled
// down if they do not all fit.
static QList<BuildingRoom> PackFloor(const QList<RoomSpec>& rooms, const AuPoint& plateMin, const AuPoint& plateMax,
		double floorBase, double maxHeight, int floor, const QString& strategy)
{
	QList<BuildingRoom> out;
	if(rooms.isEmpty())
		return out;

	const double plateWidth = plateMax.x - plateMin.x;
	const double plateDepth = plateMax.y - plateMin.y;
	if(plateWidth <= 0 || plateDepth <= 0)
		return out;

	// A corridor down the middle for the corridor and atrium strategies.
	const bool corridor = (strategy != "grid");
	const double corridorWidth = corridor ? qMin(16.0, plateDepth * .2) : 0.0;

	// Scale the rooms down until their area fits on the floor.
	double area = 0;
	for(const RoomSpec& r : rooms)
		area += qMax(1.0, r.sizeX) * qMax(1.0, r.sizeY);
	const double plateArea = plateWidth * (plateDepth - corridorWidth);
	double scale = 1.0;
	if(area > plateArea * .85 && area > 0)
		scale = plateArea * .85 / area;

	double x = plateMin.x;
	double y = plateMin.y;
	double rowDepth = 0;
	bool side = false;

	for(const RoomSpec& r : rooms)
	{
		double w = r.sizeX > 0 ? r.sizeX * scale : plateWidth * .25;
		double d = r.sizeY > 0 ? r.sizeY * scale : plateDepth * .4;
		const double h = qMin(r.sizeZ > 0 ? r.sizeZ : maxHeight, maxHeight);

		if(corridor)
			d = qMin(d, (plateDepth - corridorWidth) * .5);
		else
			d = qMin(d, plateDepth);
		w = qMin(w, plateWidth);

		if(x + w > plateMax.x)
		{
			// Next row.
			x = plateMin.x;
			if(corridor)
			{
				y = side ? plateMin.y : plateMin.y + corridorWidth;
				side = !side;
			}
			else
			{
				y += rowDepth;
				rowDepth = 0;
			}
		}
		if(y + d > plateMax.y)
			continue;	// No room left on this floor.

		BuildingRoom br;
		br.type = r.type;
		br.label = r.label;
		br.floor = floor;
		br.mn = AuPoint(x, y, floorBase);
		br.mx = AuPoint(x + w, y + d, floorBase + h);
		out << br;

		x += w;
		rowDepth = qMax(rowDepth, d);
	}
	return out;
}

} // namespace

BuildingResult GenerateBuilding(AuBdDesign& design, const BuildingBrief& brief, const QString& strategy)
{
	BuildingResult result;
	result.strategy = strategy;

	design.SetStructure(BD_Building);
	if(!brief.description.isEmpty())
		design.SetDescription(brief.description);

	const double wall = brief.wallThickness;
	const double height = brief.floors * brief.floorHeight;

	// The hull of the building. If the design already has one - because a
	// model was imported - that one is used and the brief only decides how
	// the rooms are arranged inside it.
	AuPoint hullMin, hullMax;
	const bool haveHull = HullExtents(design.Parts(), hullMin, hullMax);
	if(!haveHull)
	{
		AuScHull hull;
		const AuPoint mn(-brief.footprintX/2, -brief.footprintY/2, 0);
		const AuPoint mx(brief.footprintX/2, brief.footprintY/2, height);
		AuScFaceList hullFaces = BoxFaces(mn, mx);
		OrientAsHull(hullFaces);
		hull.AddFaces(hullFaces);
		if(hull.IsValid())
			design.Parts().AddPart(hull);
		HullExtents(design.Parts(), hullMin, hullMax);
	}
	const double buildingHeight = qMax(1.0, hullMax.z - hullMin.z);

	// The parking. It goes in a row in front of the building, on the ground.
	// The ground vehicles park along the front and the space vehicles land
	// just behind them.
	const double parkingY = hullMin.y - 30.;
	const double groundSpacing = 33.;
	const double spaceSpacing = 55.;
	{
		AuMatrix4x4 plane = AuMatrix4x4::IdentityMatrix();
		for(int i = 0; i < brief.groundParking; ++i)
		{
			const double x = (i - (brief.groundParking - 1) / 2.) * groundSpacing;
			AuScGroundVehicleParkingSpot spot;
			spot.MakeIcon(AuPoint(x, parkingY, 0), 0.f, plane);
			if(spot.IsValid())
				design.Parts().AddPart(spot);
		}
		for(int i = 0; i < brief.spaceParking; ++i)
		{
			const double x = (i - (brief.spaceParking - 1) / 2.) * spaceSpacing;
			AuScSpaceVehicleParkingSpot spot;
			spot.MakeIcon(AuPoint(x, parkingY + 10., 0), 0.f, plane);
			if(spot.IsValid())
				design.Parts().AddPart(spot);
		}
	}

	// The site. It is a flat polygon on the ground that has to cover the
	// whole building and the parking.
	{
		const double margin = 20.;
		AuPoint siteMin(hullMin.x - margin, hullMin.y - margin - 40.);
		AuPoint siteMax(hullMax.x + margin, hullMax.y + margin);
		AuPointArray bnd;
		bnd.append(AuPoint(siteMin.x, siteMin.y, 0));
		bnd.append(AuPoint(siteMax.x, siteMin.y, 0));
		bnd.append(AuPoint(siteMax.x, siteMax.y, 0));
		bnd.append(AuPoint(siteMin.x, siteMax.y, 0));
		AuScSite site;
		site.MakeSite(bnd, AuMatrix4x4::IdentityMatrix());
		site.SetGrade(AuScSite::Level);
		site.SetGrass(1);
		if(site.IsValid())
			design.Parts().AddPart(site);
	}

	// The interior rooms, floor by floor. The rooms of the brief are the
	// total for the whole building, so they are spread over the floors.
	QList<BuildingRoom> allRooms;
	{
		QList<QList<RoomSpec> > perFloor;
		for(int f = 0; f < brief.floors; ++f)
			perFloor << QList<RoomSpec>();
		for(const RoomSpec& r : brief.rooms)
		{
			int left = r.count;
			int f = 0;
			while(left > 0)
			{
				RoomSpec one(r);
				one.count = 1;
				perFloor[f % brief.floors] << one;
				--left;
				++f;
			}
		}
		// The floors are stacked inside the hull. Each floor is laid out in
		// the largest rectangle that fits inside the shape of the building.
		const double storey = buildingHeight / qMax(1, brief.floors);
		AuScSilhouette3d silhouette;
		HullSilhouette(design.Parts(), silhouette);
		for(int floor = 0; floor < brief.floors; ++floor)
		{
			const double floorBase = hullMin.z + floor * storey + wall;
			AuPoint plateMin, plateMax;
			bool havePlate = false;
			if(silhouette.IsValid())
			{
				havePlate = LargestFreeRect(silhouette, hullMin, hullMax, floorBase + 1, 4.0, plateMin, plateMax);
				if(havePlate)
				{
					// Keep a little space to the walls of the shape.
					plateMin = AuPoint(plateMin.x + wall, plateMin.y + wall, plateMin.z);
					plateMax = AuPoint(plateMax.x - wall, plateMax.y - wall, plateMax.z);
					if(plateMax.x <= plateMin.x || plateMax.y <= plateMin.y)
						havePlate = false;
				}
			}
			if(!havePlate)
			{
				plateMin = AuPoint(hullMin.x + wall, hullMin.y + wall, floorBase);
				plateMax = AuPoint(hullMax.x - wall, hullMax.y - wall, floorBase);
			}
			const double maxHeight = qMax(6.0, storey - wall * 2);
			allRooms += PackFloor(perFloor[floor], plateMin, plateMax, floorBase, maxHeight, floor, strategy);
		}
	}

	// The rooms have to fit inside the shape of the building. A building
	// made from a model is not a box, so the rooms are shrunk and moved until
	// the silhouette of the hull takes them.
	{
		AuScSilhouette3d silhouette;
		HullSilhouette(design.Parts(), silhouette);
		if(silhouette.IsValid())
		{
			QList<BuildingRoom> fitted;
			for(const BuildingRoom& br : allRooms)
			{
				BuildingRoom best = br;
				bool ok = false;
				const double depthScales[] = { 1.0, .9, .8, .7, .6, .5, .4 };
				const double lengthScales[] = { 1.0, .95, .9, .85, .8 };
				for(int di = 0; di < 7 && !ok; ++di)
				{
					for(int li = 0; li < 5 && !ok; ++li)
					{
						BuildingRoom t = br;
						const double cx = (t.mn.x + t.mx.x) / 2;
						const double cy = (t.mn.y + t.mx.y) / 2;
						const double hx = (t.mx.x - t.mn.x) / 2 * lengthScales[li];
						const double hy = (t.mx.y - t.mn.y) / 2 * depthScales[di];
						t.mn = AuPoint(cx - hx, cy - hy, t.mn.z);
						t.mx = AuPoint(cx + hx, cy + hy, t.mx.z);
						// Then try it at a few heights and places.
						for(int zi = 0; zi < 4 && !ok; ++zi)
						{
							for(int yi = 0; yi < 3 && !ok; ++yi)
							{
								BuildingRoom u = t;
								u.mn.z += (t.mx.z - t.mn.z) * zi * .2;
								u.mx.z += (t.mx.z - t.mn.z) * zi * .2;
								u.mn.y += (t.mx.y - t.mn.y) * yi * .25;
								u.mx.y += (t.mx.y - t.mn.y) * yi * .25;
								// All four corners have to be inside the shape.
								const AuPoint corners[8] = {
									AuPoint(u.mn.x, u.mn.y, u.mn.z), AuPoint(u.mx.x, u.mn.y, u.mn.z),
									AuPoint(u.mx.x, u.mx.y, u.mn.z), AuPoint(u.mn.x, u.mx.y, u.mn.z),
									AuPoint(u.mn.x, u.mn.y, u.mx.z), AuPoint(u.mx.x, u.mn.y, u.mx.z),
									AuPoint(u.mx.x, u.mx.y, u.mx.z), AuPoint(u.mn.x, u.mx.y, u.mx.z) };
								bool allInside = true;
								for(int c = 0; c < 8; ++c)
								{
									if(!silhouette.IsInside(corners[c]))
									{
										allInside = false;
										break;
									}
								}
								if(allInside)
								{
									best = u;
									ok = true;
								}
							}
						}
					}
				}
				if(ok)
					fitted << best;
				else
					result.warnings << QString("room '%1' does not fit inside the building and was left out.").arg(br.label);
			}
			allRooms = fitted;
		}
	}

	for(BuildingRoom& br : allRooms)
	{
		// The room's constant void and its mesh.
		quint16 meshId = 0;
		if(!AddBoxRoom(design, br.label, br.mn, br.mx, br.voidId, meshId, true))
			continue;
		++result.roomsPlaced;
	}

	// An entrance door in the front wall of the ground floor, with a doorway
	// from the first room out through the wall.
	if(!allRooms.isEmpty() && allRooms.first().voidId)
	{
		const BuildingRoom& first = allRooms.first();
		const double doorWidth = 10;
		const double doorHeight = 8;
		const double y = -brief.footprintY/2;
		const double cx = (first.mn.x + first.mx.x) / 2;

		quint16 doorId = 0;
		if(AddHullDoor(design, AuPoint(cx - doorWidth/2, y - 1, first.mn.z), AuPoint(cx + doorWidth/2, y + 1, first.mn.z + doorHeight),
				AuPoint(0, -1, 0), first.mn.y, y, first.label, doorId))
			++result.doorsPlaced;
	}

	// Control stations for the sensors and the shields. The engine insists
	// that there is a station for every system that has volume allocated.
	{
		AuMatrix4x4 plane = AuMatrix4x4::IdentityMatrix();
		struct StationEntry { bool wanted; AuScPart::PART_TYPE type; COMMODITYID module; };
		StationEntry entries[] = {
			{ brief.volumes.sensors > 0, AuScPart::Part_ConsoleSensor, CID_SensorModule },
			{ brief.volumes.shields > 0, AuScPart::Part_ConsoleShield, CID_EnergyShieldModule },
			{ brief.volumes.capacitor > 0, AuScPart::Part_ConsoleEngineer, CID_GravityDriveModule },
		};
		int slot = 0;
		for(const StationEntry& e : entries)
		{
			if(!e.wanted)
				continue;
			AuScPart* part = AuScPart::Create(e.type);
			if(!part)
				continue;
			AuScConsole* console = dynamic_cast<AuScConsole*>(part);
			if(!console)
			{
				delete part;
				continue;
			}
			const double px = -brief.footprintX/4 + slot * 8;
			console->MakeIcon(AuPoint(px, brief.footprintY/4, wall + 4), 0.f, plane);
			console->SetModule(e.module);
			if(console->IsValid())
			{
				design.Parts().AddPart(*console);
				++result.postsPlaced;
			}
			delete part;
			++slot;
		}
	}

	// Citizen and guard posts.
	{
		AuMatrix4x4 plane = AuMatrix4x4::IdentityMatrix();
		for(int i = 0; i < brief.citizenPosts; ++i)
		{
			AuScCitizenPost post;
			const double px = -brief.footprintX/4 + (i % 4) * 8;
			const double py = -brief.footprintY/4 + (i / 4) * 8;
			post.MakeIcon(AuPoint(px, py, wall + 2), 0.f, plane);
			if(post.IsValid())
			{
				design.Parts().AddPart(post);
				++result.postsPlaced;
			}
		}
		for(int i = 0; i < brief.guardPosts; ++i)
		{
			AuScTroopPost post;
			const double px = brief.footprintX/4 + (i % 4) * 8;
			const double py = -brief.footprintY/4 + (i / 4) * 8;
			post.MakeIcon(AuPoint(px, py, wall + 2), 0.f, plane);
			if(post.IsValid())
			{
				design.Parts().AddPart(post);
				++result.postsPlaced;
			}
		}
	}

	// The hull volume is shared out between the systems. What the brief asks
	// for is used as it is, and whatever is left over goes to the structure.
	// The whole has to add up to the hull volume, so if the brief asks for
	// more than the building has, it is scaled down.
	{
		design.RefreshObstructionsCache();
		const quint32 hullVolume = qMax(1u, design.HullVolume());

		BuildingVolumes v = brief.volumes;
		const quint32 requested = v.offices + v.shops + v.store + v.lounge + v.homesLarge + v.homesMedium +
			v.homesSmall + v.powerPlant + v.lifeSupport + v.capacitor + v.sensors + v.shields + v.hold + v.armor +
			v.arena + v.auditorium;
		if(requested > hullVolume)
		{
			// More than the building can hold. Scale it down.
			const double factor = double(hullVolume) / double(requested);
			v.offices = quint32(v.offices * factor);
			v.shops = quint32(v.shops * factor);
			v.store = quint32(v.store * factor);
			v.lounge = quint32(v.lounge * factor);
			v.homesLarge = quint32(v.homesLarge * factor);
			v.homesMedium = quint32(v.homesMedium * factor);
			v.homesSmall = quint32(v.homesSmall * factor);
			v.powerPlant = quint32(v.powerPlant * factor);
			v.lifeSupport = quint32(v.lifeSupport * factor);
			v.capacitor = quint32(v.capacitor * factor);
			v.sensors = quint32(v.sensors * factor);
			v.shields = quint32(v.shields * factor);
			v.hold = quint32(v.hold * factor);
			v.armor = quint32(v.armor * factor);
			v.arena = quint32(v.arena * factor);
			v.auditorium = quint32(v.auditorium * factor);
			result.warnings << QString("the building asked for more room than it has, so the systems were scaled down.");
		}

		const quint32 used = v.offices + v.shops + v.store + v.lounge + v.homesLarge + v.homesMedium +
			v.homesSmall + v.powerPlant + v.lifeSupport + v.capacitor + v.sensors + v.shields + v.hold + v.armor +
			v.arena + v.auditorium;
		v.skeleton = (hullVolume > used) ? (hullVolume - used) : 0;

		design.SetOfficeVolume(v.offices);
		design.SetShopVolume(v.shops);
		design.SetStoreVolume(v.store);
		design.SetLoungeVolume(v.lounge);
		design.SetHomeLargeVolume(v.homesLarge);
		design.SetHomeMediumVolume(v.homesMedium);
		design.SetHomeSmallVolume(v.homesSmall);
		design.SetPowerPlantVolume(v.powerPlant);
		design.SetLifeSupportVolume(v.lifeSupport);
		design.SetCapacitorVolume(v.capacitor);
		design.SetSensorVolume(v.sensors);
		design.SetShieldVolume(v.shields);
		design.SetHoldVolume(v.hold);
		design.SetArmorVolume(v.armor);
		design.SetArenaVolume(v.arena);
		design.SetAuditoriumVolume(v.auditorium);
		design.SetSkeletonVolume(v.skeleton);

		result.warnings << QString("hull volume %1 cubic meters: offices %2, shops %3, store %4, lounge %5, homes %6, "
				"auditorium %7, arena %8, hold %9, power plant %10, life support %11, capacitor %12, sensors %13, "
				"shields %14, armor %15, structure %16.")
				.arg(hullVolume).arg(v.offices).arg(v.shops).arg(v.store).arg(v.lounge)
				.arg(v.homesLarge + v.homesMedium + v.homesSmall).arg(v.auditorium).arg(v.arena).arg(v.hold)
				.arg(v.powerPlant).arg(v.lifeSupport).arg(v.capacitor).arg(v.sensors).arg(v.shields)
				.arg(v.armor).arg(v.skeleton);
	}

	// The industry says what the building is for. When the brief does not
	// name one, the first industry that could use the building is taken.
	{
		const int wanted = IndustryIdFor(brief.industry);
		INDUSTRYID chosen = IID_None;
		if(wanted != int(IID_None))
			chosen = INDUSTRYID(wanted);
		else if(!brief.industry.isEmpty() && brief.industry != "auto")
			result.warnings << QString("'%1' is not an industry a building can have.").arg(brief.industry);
		else
		{
			AuBdReport report;
			design.ReportDesign(report);
			QSet<INDUSTRYID> usable;
			report.Industries(usable);
			if(usable.contains(IID_DesignStudio))			chosen = IID_DesignStudio;
			else if(usable.contains(IID_BaseHeadquarters))	chosen = IID_BaseHeadquarters;
			else if(usable.contains(IID_Warehouse))			chosen = IID_Warehouse;
			else if(usable.contains(IID_House))				chosen = IID_House;
			else if(!usable.isEmpty())						chosen = *usable.constBegin();
		}
		if(chosen != IID_None)
		{
			design.SetIndustryId(chosen);
			result.industryName = QString::number(int(chosen));

			// Check that the building can actually serve that industry.
			AuBdReport report;
			design.ReportDesign(report);
			QSet<INDUSTRYID> usable;
			report.Industries(usable);
			if(!usable.contains(chosen))
				result.warnings << QString("the building does not have enough of something for a %1.")
						.arg(brief.industry);
		}
		else
			result.warnings << "no industry could use this building as it stands.";
	}

	// The engine needs the obstruction cache to work out the hull volume.
	design.RefreshObstructionsCache();

	// How big is the blueprint now?
	{
		quint32 faces = 0, triangles = 0, hullFaces = 0;
		const AuScParts& parts = design.Parts();
		for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
		{
			AuScFaceList f;
			it.value()->Faces(f);
			faces += f.size();
			if(it.value()->IsHullMesh())
				hullFaces += f.size();
			AuScTriList t;
			f.Triangles(t);
			triangles += t.size();
		}
		result.faces = faces;
		result.hullFaces = hullFaces;
		result.triangles = triangles;
	}

	// The rooms that could not be placed.
	{
		QMap<QString, int> placed;
		for(const BuildingRoom& br : allRooms)
			placed[br.label]++;
		for(const RoomSpec& r : brief.rooms)
		{
			int have = 0;
			for(const BuildingRoom& br : allRooms)
			{
				if(br.type == r.type)
					++have;
			}
			if(have < r.count)
			{
				result.roomsMissing += r.count - have;
				result.missingRooms << QString("%1 (wanted %2, placed %3)").arg(r.label).arg(r.count).arg(have);
			}
		}
	}

	return result;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

bool ValidateBuilding(AuBdDesign& design, QStringList& errors, QStringList& warnings)
{
	// The engine needs the obstruction cache to work out the hull volume.
	design.RefreshObstructionsCache();

	AuBdReport report;
	design.ReportDesign(report);

	QStringList msgs;
	report.OkForAssembly(msgs, design);
	report.OkToConstruct(msgs, &design);

	for(const QString& m : msgs)
	{
		QString text = m;
		text.remove(QString("<span style=\"color: rgb(255, 32, 32);\">"));
		text.remove(QString("<span style=\"color: rgb(32, 255, 32);\">"));
		text.remove(QString("<span style=\"color: rgb(255, 128, 32);\">"));
		text.remove(QString("</span>"));
		text = text.trimmed();
		if(text.isEmpty())
			continue;
		if(m.contains("rgb(255, 32, 32)"))
			errors << text;
		else if(m.contains("rgb(255, 128, 32)"))
			warnings << text;
	}
	return errors.isEmpty();
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// A set of buildings.

bool LoadBuildingSet(const QString& fileName, BuildingSet& set, QString& error)
{
	QFile file(fileName);
	if(!file.open(QIODevice::ReadOnly))
	{
		error = QString("cannot open %1: %2").arg(fileName).arg(file.errorString());
		return false;
	}
	QJsonParseError err;
	QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
	if(err.error != QJsonParseError::NoError)
	{
		error = QString("JSON error at offset %1: %2").arg(err.offset).arg(err.errorString());
		return false;
	}
	if(!doc.isObject())
	{
		error = "the set must be a JSON object";
		return false;
	}
	QJsonObject o = doc.object();

	if(o.contains("name"))			set.name = o.value("name").toString();
	if(o.contains("fit"))			set.fit = o.value("fit").toDouble();
	if(o.contains("faces"))			set.maxFaces = o.value("faces").toInt(5000);
	if(o.contains("model"))
	{
		if(o.value("model").isArray())
		{
			const QJsonArray arr = o.value("model").toArray();
			for(const QJsonValue& v : arr)
				set.modelFiles << v.toString();
		}
		else
			set.modelFiles << o.value("model").toString();
	}

	const QJsonArray arr = o.value("buildings").toArray();
	if(arr.isEmpty())
	{
		error = "the set has no buildings in it";
		return false;
	}
	for(const QJsonValue& v : arr)
	{
		QJsonObject bo = v.toObject();
		BuildingSetEntry entry;
		entry.templateKey = bo.value("template").toString();
		entry.shapeKey = bo.value("shape").toString();
		entry.name = bo.value("name").toString();
		entry.fit = bo.value("fit").toDouble();
		entry.variations = qMax(1, bo.value("variations").toInt(1));
		if(bo.contains("model"))
		{
			if(bo.value("model").isArray())
			{
				const QJsonArray marr = bo.value("model").toArray();
				for(const QJsonValue& mv : marr)
					entry.modelFiles << mv.toString();
			}
			else
				entry.modelFiles << bo.value("model").toString();
		}
		// A building without a template has to bring its own brief.
		if(entry.templateKey.isEmpty() && bo.contains("brief"))
		{
			QJsonObject b = bo.value("brief").toObject();
			briefFromJson(b, entry.brief);
		}
		if(entry.templateKey.isEmpty() && entry.brief.name.isEmpty())
		{
			error = "a building in the set has neither a template nor a brief";
			return false;
		}
		set.buildings << entry;
	}

	// The looks have to be ones that exist.
	for(const BuildingSetEntry& entry : set.buildings)
	{
		if(entry.shapeKey.isEmpty())
			continue;
		ShapePreset preset;
		if(!FindShapePreset(entry.shapeKey, preset))
		{
			error = QString("unknown shape: %1").arg(entry.shapeKey);
			return false;
		}
	}

	return true;
}

bool SaveBuildingSet(const QString& fileName, const BuildingSet& set, QString& error)
{
	QJsonObject o;
	if(!set.name.isEmpty())
		o.insert("name", set.name);
	if(set.fit > 0)
		o.insert("fit", set.fit);
	o.insert("faces", int(set.maxFaces));
	if(!set.modelFiles.isEmpty())
		o.insert("model", QJsonArray::fromStringList(set.modelFiles));

	QJsonArray arr;
	for(const BuildingSetEntry& entry : set.buildings)
	{
		QJsonObject bo;
		if(!entry.templateKey.isEmpty())
			bo.insert("template", entry.templateKey);
		if(!entry.shapeKey.isEmpty())
			bo.insert("shape", entry.shapeKey);
		if(!entry.name.isEmpty())
			bo.insert("name", entry.name);
		if(entry.fit > 0)
			bo.insert("fit", entry.fit);
		if(entry.variations > 1)
			bo.insert("variations", entry.variations);
		if(!entry.modelFiles.isEmpty())
			bo.insert("model", QJsonArray::fromStringList(entry.modelFiles));
		arr.append(bo);
	}
	o.insert("buildings", arr);

	QFile file(fileName);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		error = QString("cannot write %1: %2").arg(fileName).arg(file.errorString());
		return false;
	}
	file.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
	file.close();
	return true;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Making the set.

BuildingSetResult GenerateBuildingSet(const BuildingSet& set, const QString& outDir)
{
	BuildingSetResult result;

	for(const BuildingSetEntry& entry : set.buildings)
	{
		// The brief for this building.
		BuildingBrief brief = entry.brief;
		if(!entry.templateKey.isEmpty())
			ApplyBuildingTemplate(brief, entry.templateKey);
		if(!entry.name.isEmpty())
			brief.name = entry.name;
		else if(brief.name.isEmpty())
			brief.name = entry.templateKey;

		// The model and the shape.
		const QStringList models = entry.modelFiles.isEmpty() ? set.modelFiles : entry.modelFiles;
		const double fit = entry.fit > 0 ? entry.fit : set.fit;

		ModelShape shape;
		if(!entry.shapeKey.isEmpty())
		{
			ShapePreset preset;
			if(FindShapePreset(entry.shapeKey, preset))
				shape = preset.shape;
		}

		const int variations = qMax(1, entry.variations);
		for(int variation = 0; variation < variations; ++variation)
		{
			AuBdDesign design;
			design.SetId(1);
			design.SetArchitectId(AuId(0));
			design.SetArchitectName("shipgen");
			design.SetName(variations > 1 ? QString("%1 %2").arg(brief.name).arg(variation + 1) : brief.name);

			if(!models.isEmpty())
			{
				// The model becomes the shell of the building.
				ModelImportOptions modelOpts;
				modelOpts.fit = fit;
				modelOpts.maxFaces = set.maxFaces;
				modelOpts.shape = VaryShape(shape, variation);
				AuPoint modelMin, modelMax;
				QString importError;
				if(!ImportHullModel(design, models, modelOpts, modelMin, modelMax, importError))
				{
					result.errors << importError;
					continue;
				}
			}

			const BuildingResult building = GenerateBuilding(design, brief, brief.layout);

			QStringList errors, warnings;
			ValidateBuilding(design, errors, warnings);
			for(const QString& e : errors)
				result.errors << QString("%1: %2").arg(brief.name).arg(e);
			for(const QString& w : warnings)
				result.warnings << QString("%1: %2").arg(brief.name).arg(w);

			const QString fileName = QDir(outDir).filePath(SanitiseName(design.Name()) + ".SoH");
			if(!design.SaveToFile(fileName, SOH_FILE_TYPE))
			{
				result.errors << QString("could not write %1").arg(fileName);
				continue;
			}
			result.fileNames << fileName;
			++result.built;
		}
	}

	return result;
}
