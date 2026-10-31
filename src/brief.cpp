//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - brief model, ship templates and brief file I/O.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "shipgen.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

QList<RoomSpec> Brief::ExpandedRooms() const
{
	QList<RoomSpec> out;
	for(const RoomSpec& r : rooms)
	{
		for(int i = 0; i < r.count; ++i)
		{
			RoomSpec e(r);
			e.count = 1;
			if(r.count > 1)
				e.label = QString("%1 %2").arg(r.label).arg(i + 1);
			out << e;
		}
	}
	return out;
}

QList<StationSpec> Brief::ExpandedStations() const
{
	QList<StationSpec> out;
	for(const StationSpec& s : stations)
	{
		for(int i = 0; i < s.count; ++i)
		{
			StationSpec e(s);
			e.count = 1;
			out << e;
		}
	}
	return out;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Room priorities. When the hull is too small for everything, the rooms that
// matter least are the ones that get dropped.

static int RoomPriority(const QString& type)
{
	if(type == "Bridge")			return 1;
	if(type == "Engine Room")		return 1;
	if(type == "Weapon Bay")		return 2;
	if(type == "Sensor Room")		return 3;
	if(type == "Shield Room")		return 3;
	if(type == "Sick Bay")			return 4;
	if(type == "Hall")				return 4;
	if(type == "Crew Quarters")	return 5;
	if(type == "Barracks")			return 5;
	if(type == "Hold")				return 5;
	if(type == "Galley")			return 6;
	if(type == "Workshop")			return 6;
	if(type == "Store")			return 7;
	if(type == "Transporter Room")	return 8;
	return 5;
}

static void ApplyPriorities(Brief& brief)
{
	for(RoomSpec& r : brief.rooms)
		r.priority = RoomPriority(r.type);
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Templates.
//
// Room sizes are in feet. Module volumes are given per 1000 cubic meters of
// hull volume and are scaled to the actual hull when a brief does not
// override them.

// clang-format off
QList<ShipTemplate> AllTemplates()
{
	QList<ShipTemplate> list;

	{
		ShipTemplate t;
		t.key = "military_frigate";
		t.label = "Military frigate";
		t.description = "Armed patrol vessel. Bridge, engineering, weapons, sensors and a small crew complement.";
		t.crew = 60;
		t.rooms
			<< RoomSpec("Bridge", "Bridge", 1, 44, 34, 13)
			<< RoomSpec("Engine Room", "Engine Room", 2, 54, 34, 15)
			<< RoomSpec("Hold", "Reactor", 1, 30, 30, 18)
			<< RoomSpec("Weapon Bay", "Weapons", 1, 40, 30, 12)
			<< RoomSpec("Weapon Bay", "Fire Control", 1, 26, 26, 11)
			<< RoomSpec("Sensor Room", "Sensor Room", 1, 26, 26, 11)
			<< RoomSpec("Shield Room", "Shield Room", 1, 22, 22, 10)
			<< RoomSpec("Sick Bay", "Medical", 1, 30, 26, 11)
			<< RoomSpec("Crew Quarters", "Crew Quarters", 2, 44, 32, 11)
			<< RoomSpec("Store", "Stores", 1, 30, 26, 10)
			<< RoomSpec("Transporter Room", "Transporter", 1, 34, 30, 12);
		t.stations
			<< StationSpec("helm", 1, "Bridge")
			<< StationSpec("navigator", 1, "Bridge")
			<< StationSpec("firecontrol", 1, "Weapon Bay")
			<< StationSpec("engineer", 2, "Engine Room")
			<< StationSpec("sensor", 1, "Sensor Room")
			<< StationSpec("shield", 1, "Shield Room")
			<< StationSpec("medic", 1, "Sick Bay")
			<< StationSpec("transporter", 1, "Transporter Room")
			<< StationSpec("command", 1, "Bridge");
		t.berths.officer = 6;
		t.berths.crew = 54;
		t.modules.powerPlant = 120;
		t.modules.maneuverDrive = 90;
		t.modules.capacitor = 60;
		t.modules.fuel = 200;
		t.modules.lifeSupport = 60;
		t.modules.armor = 200;
		t.modules.skeleton = 700;
		t.modules.sensors = 25;
		t.modules.shields = 40;
		list << t;
	}

	{
		ShipTemplate t;
		t.key = "cargo_hauler";
		t.label = "Cargo hauler";
		t.description = "Freighter. Large holds, minimal crew, long endurance.";
		t.crew = 14;
		t.rooms
			<< RoomSpec("Bridge", "Bridge", 1, 40, 30, 12)
			<< RoomSpec("Engine Room", "Engineering", 1, 56, 34, 15)
			<< RoomSpec("Hold", "Reactor", 1, 28, 28, 18)
			<< RoomSpec("Hold", "Cargo Hold", 3, 90, 50, 24)
			<< RoomSpec("Crew Quarters", "Crew Quarters", 1, 38, 30, 11)
			<< RoomSpec("Galley", "Galley", 1, 24, 20, 10)
			<< RoomSpec("Store", "Stores", 1, 26, 22, 10)
			<< RoomSpec("Transporter Room", "Transporter", 1, 32, 28, 12);
		t.stations
			<< StationSpec("helm", 1, "Bridge")
			<< StationSpec("navigator", 1, "Bridge")
			<< StationSpec("engineer", 1, "Engine Room")
			<< StationSpec("transporter", 1, "Transporter Room");
		t.berths.officer = 3;
		t.berths.crew = 11;
		t.modules.powerPlant = 90;
		t.modules.maneuverDrive = 70;
		t.modules.capacitor = 30;
		t.modules.fuel = 320;
		t.modules.lifeSupport = 25;
		t.modules.armor = 60;
		t.modules.skeleton = 600;
		t.modules.hold = 900;
		list << t;
	}

	{
		ShipTemplate t;
		t.key = "science_lab";
		t.label = "Science vessel";
		t.description = "Survey ship. Laboratories, sensor array, modest drive and long life support.";
		t.crew = 32;
		t.rooms
			<< RoomSpec("Bridge", "Bridge", 1, 38, 30, 12)
			<< RoomSpec("Engine Room", "Engineering", 1, 48, 32, 14)
			<< RoomSpec("Hold", "Reactor", 1, 26, 26, 17)
			<< RoomSpec("Hall", "Laboratory", 2, 44, 32, 12)
			<< RoomSpec("Sensor Room", "Sensor Room", 1, 26, 26, 11)
			<< RoomSpec("Sick Bay", "Medical", 1, 28, 24, 11)
			<< RoomSpec("Crew Quarters", "Crew Quarters", 1, 40, 30, 11)
			<< RoomSpec("Galley", "Galley", 1, 24, 20, 10)
			<< RoomSpec("Workshop", "Workshop", 1, 32, 26, 11);
		t.stations
			<< StationSpec("helm", 1, "Bridge")
			<< StationSpec("navigator", 1, "Bridge")
			<< StationSpec("engineer", 1, "Engine Room")
			<< StationSpec("sensor", 1, "Sensor Room")
			<< StationSpec("medic", 1, "Sick Bay");
		t.berths.officer = 8;
		t.berths.crew = 24;
		t.modules.powerPlant = 80;
		t.modules.maneuverDrive = 70;
		t.modules.capacitor = 40;
		t.modules.fuel = 180;
		t.modules.lifeSupport = 70;
		t.modules.armor = 60;
		t.modules.skeleton = 500;
		t.modules.sensors = 40;
		list << t;
	}

	{
		ShipTemplate t;
		t.key = "colony_transport";
		t.label = "Colony transport";
		t.description = "Passenger liner. Berths, life support and holds for a settlement.";
		t.crew = 40;
		t.rooms
			<< RoomSpec("Bridge", "Bridge", 1, 40, 30, 12)
			<< RoomSpec("Engine Room", "Engineering", 2, 50, 32, 14)
			<< RoomSpec("Hold", "Reactor", 1, 30, 30, 18)
			<< RoomSpec("Crew Quarters", "Crew Quarters", 2, 40, 30, 11)
			<< RoomSpec("Barracks", "Passenger Bay", 3, 70, 44, 16)
			<< RoomSpec("Galley", "Galley", 1, 30, 24, 11)
			<< RoomSpec("Sick Bay", "Medical", 1, 30, 26, 11)
			<< RoomSpec("Store", "Stores", 1, 34, 28, 12)
			<< RoomSpec("Transporter Room", "Transporter", 1, 34, 30, 12);
		t.stations
			<< StationSpec("helm", 1, "Bridge")
			<< StationSpec("navigator", 1, "Bridge")
			<< StationSpec("engineer", 2, "Engine Room")
			<< StationSpec("medic", 1, "Sick Bay")
			<< StationSpec("transporter", 1, "Transporter Room")
			<< StationSpec("command", 1, "Bridge");
		t.berths.officer = 6;
		t.berths.crew = 34;
		t.berths.passenger = 120;
		t.modules.powerPlant = 110;
		t.modules.maneuverDrive = 80;
		t.modules.capacitor = 40;
		t.modules.fuel = 260;
		t.modules.lifeSupport = 200;
		t.modules.armor = 80;
		t.modules.skeleton = 800;
		t.modules.hold = 200;
		list << t;
	}

	{
		ShipTemplate t;
		t.key = "patrol_cutter";
		t.label = "Patrol cutter";
		t.description = "Small fast boat. One room per function, minimal crew.";
		t.crew = 10;
		t.rooms
			<< RoomSpec("Bridge", "Bridge", 1, 32, 26, 11)
			<< RoomSpec("Engine Room", "Engineering", 1, 38, 26, 13)
			<< RoomSpec("Weapon Bay", "Weapons", 1, 28, 24, 10)
			<< RoomSpec("Sensor Room", "Sensor Room", 1, 22, 22, 10)
			<< RoomSpec("Crew Quarters", "Crew Quarters", 1, 26, 22, 10)
			<< RoomSpec("Store", "Stores", 1, 20, 18, 9);
		t.stations
			<< StationSpec("helm", 1, "Bridge")
			<< StationSpec("navigator", 1, "Bridge")
			<< StationSpec("engineer", 1, "Engine Room")
			<< StationSpec("sensor", 1, "Sensor Room");
		t.berths.officer = 2;
		t.berths.crew = 8;
		t.modules.powerPlant = 35;
		t.modules.maneuverDrive = 40;
		t.modules.capacitor = 15;
		t.modules.fuel = 90;
		t.modules.lifeSupport = 18;
		t.modules.armor = 30;
		t.modules.skeleton = 220;
		t.modules.sensors = 12;
		list << t;
	}

	{
		ShipTemplate t;
		t.key = "shuttle";
		t.label = "Shuttle";
		t.description = "Small craft. Pilot, a couple of seats and an engine bay.";
		t.crew = 3;
		t.rooms
			<< RoomSpec("Bridge", "Cockpit", 1, 22, 18, 9)
			<< RoomSpec("Engine Room", "Engineering", 1, 24, 18, 10)
			<< RoomSpec("Hold", "Cargo", 1, 22, 18, 8);
		t.stations
			<< StationSpec("helm", 1, "Bridge")
			<< StationSpec("engineer", 1, "Engine Room");
		t.berths.crew = 2;
		t.modules.powerPlant = 10;
		t.modules.maneuverDrive = 12;
		t.modules.capacitor = 4;
		t.modules.fuel = 22;
		t.modules.lifeSupport = 6;
		t.modules.armor = 6;
		t.modules.skeleton = 90;
		list << t;
	}

	return list;
}
// clang-format on
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

const ShipTemplate* FindTemplate(const QString& key)
{
	// Kept in a static so the returned pointer stays valid.
	static const QList<ShipTemplate> all = AllTemplates();
	for(const ShipTemplate& t : all)
	{
		if(t.key == key)
			return &t;
	}
	return 0;
}

void ApplyTemplate(Brief& brief, const QString& templateKey)
{
	const ShipTemplate* t = FindTemplate(templateKey);
	if(!t)
		return;

	brief.templateName = t->key;
	ApplyPriorities(brief);
	if(brief.name.isEmpty())
		brief.name = t->label;
	if(brief.crew <= 0)
		brief.crew = t->crew;

	// Replace rooms of the same type, keep any the template does not know about.
	QList<RoomSpec> merged;
	for(const RoomSpec& r : brief.rooms)
	{
		bool replaced = false;
		for(const RoomSpec& tr : t->rooms)
		{
			if(tr.type == r.type)
				replaced = true;
		}
		if(!replaced)
			merged << r;
	}
	merged << t->rooms;
	brief.rooms = merged;

	// Same for stations.
	QList<StationSpec> mergedS;
	for(const StationSpec& s : brief.stations)
	{
		bool replaced = false;
		for(const StationSpec& ts : t->stations)
		{
			if(ts.type == s.type)
				replaced = true;
		}
		if(!replaced)
			mergedS << s;
	}
	mergedS << t->stations;
	brief.stations = mergedS;

	// Berth counts: use whichever is larger.
	if(t->berths.officer > brief.berths.officer) brief.berths.officer = t->berths.officer;
	if(t->berths.crew > brief.berths.crew) brief.berths.crew = t->berths.crew;
	if(t->berths.passenger > brief.berths.passenger) brief.berths.passenger = t->berths.passenger;
	if(t->berths.troop > brief.berths.troop) brief.berths.troop = t->berths.troop;
	if(t->berths.capt > brief.berths.capt) brief.berths.capt = t->berths.capt;

	// Module volumes come from the template unless the brief sets its own.
	if(brief.modules.IsEmpty())
		brief.modules = t->modules;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Brief file I/O.

static double JsonSize(const QJsonValue& v, double dflt)
{
	if(v.isArray() && v.toArray().size() >= 3)
		return v.toArray().at(0).toDouble(dflt);
	return dflt;
}

bool LoadBrief(const QString& fileName, Brief& brief, QString& error)
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
	QJsonObject o = doc.object();

	if(o.contains("template"))
	{
		brief.templateName = o.value("template").toString();
		if(!FindTemplate(brief.templateName))
		{
			error = QString("unknown template '%1'").arg(brief.templateName);
			return false;
		}
	}
	if(o.contains("name")) brief.name = o.value("name").toString();
	if(o.contains("description")) brief.description = o.value("description").toString();
	if(o.contains("layout")) brief.layout = o.value("layout").toString();
	if(o.contains("crew")) brief.crew = o.value("crew").toInt();
	if(o.contains("ftl")) brief.ftlDrive = o.value("ftl").toString();

	if(o.contains("rooms"))
	{
		QJsonArray arr = o.value("rooms").toArray();
		for(const QJsonValue& v : arr)
		{
			QJsonObject ro = v.toObject();
			RoomSpec r;
			r.type = ro.value("type").toString();
			r.label = ro.value("label").toString();
			if(r.label.isEmpty())
				r.label = r.type;
			r.count = ro.value("count").toInt(1);
			r.priority = ro.value("priority").toInt(RoomPriority(r.type));
			r.sizeX = JsonSize(ro.value("size"), 0);
			r.sizeY = JsonSize(ro.value("size"), 0);
			r.sizeZ = JsonSize(ro.value("size"), 0);
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
			if(ro.contains("width")) r.sizeX = ro.value("width").toDouble();
			if(ro.contains("depth")) r.sizeY = ro.value("depth").toDouble();
			if(ro.contains("height")) r.sizeZ = ro.value("height").toDouble();
			brief.rooms << r;
		}
	}

	if(o.contains("stations"))
	{
		QJsonArray arr = o.value("stations").toArray();
		for(const QJsonValue& v : arr)
		{
			QJsonObject so = v.toObject();
			StationSpec s;
			s.type = so.value("type").toString();
			s.count = so.value("count").toInt(1);
			s.roomType = so.value("room").toString();
			brief.stations << s;
		}
	}

	if(o.contains("berths"))
	{
		QJsonObject bo = o.value("berths").toObject();
		brief.berths.capt = bo.value("capt").toInt();
		brief.berths.officer = bo.value("officer").toInt();
		brief.berths.crew = bo.value("crew").toInt();
		brief.berths.passenger = bo.value("passenger").toInt();
		brief.berths.troop = bo.value("troop").toInt();
	}

	if(o.contains("modules"))
	{
		QJsonObject mo = o.value("modules").toObject();
		brief.modules.powerPlant = mo.value("powerPlant").toInt();
		brief.modules.maneuverDrive = mo.value("maneuverDrive").toInt();
		brief.modules.capacitor = mo.value("capacitor").toInt();
		brief.modules.fuel = mo.value("fuel").toInt();
		brief.modules.lifeSupport = mo.value("lifeSupport").toInt();
		brief.modules.armor = mo.value("armor").toInt();
		brief.modules.skeleton = mo.value("skeleton").toInt();
		brief.modules.hold = mo.value("hold").toInt();
		brief.modules.sensors = mo.value("sensors").toInt();
		brief.modules.shields = mo.value("shields").toInt();
		brief.modules.cryoBerths = mo.value("cryoBerths").toInt();
		// Only treat the modules as an explicit override when something was set.
		brief.modulesFromTemplate = brief.modules.IsEmpty();
	}

	return true;
}

QString BriefToJson(const Brief& brief)
{
	QJsonObject o;
	o["name"] = brief.name;
	o["description"] = brief.description;
	if(!brief.templateName.isEmpty())
		o["template"] = brief.templateName;
	if(!brief.layout.isEmpty())
		o["layout"] = brief.layout;
	o["crew"] = brief.crew;
	o["ftl"] = brief.ftlDrive;

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

	QJsonArray stations;
	for(const StationSpec& s : brief.stations)
	{
		QJsonObject so;
		so["type"] = s.type;
		so["count"] = s.count;
		if(!s.roomType.isEmpty())
			so["room"] = s.roomType;
		stations.append(so);
	}
	o["stations"] = stations;

	QJsonObject bo;
	bo["capt"] = brief.berths.capt;
	bo["officer"] = brief.berths.officer;
	bo["crew"] = brief.berths.crew;
	bo["passenger"] = brief.berths.passenger;
	bo["troop"] = brief.berths.troop;
	o["berths"] = bo;

	QJsonObject mo;
	mo["powerPlant"] = int(brief.modules.powerPlant);
	mo["maneuverDrive"] = int(brief.modules.maneuverDrive);
	mo["capacitor"] = int(brief.modules.capacitor);
	mo["fuel"] = int(brief.modules.fuel);
	mo["lifeSupport"] = int(brief.modules.lifeSupport);
	mo["armor"] = int(brief.modules.armor);
	mo["skeleton"] = int(brief.modules.skeleton);
	mo["hold"] = int(brief.modules.hold);
	mo["sensors"] = int(brief.modules.sensors);
	mo["shields"] = int(brief.modules.shields);
	mo["cryoBerths"] = int(brief.modules.cryoBerths);
	o["modules"] = mo;

	return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

bool SaveBrief(const QString& fileName, const Brief& brief, QString& error)
{
	QFile file(fileName);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		error = QString("cannot write %1: %2").arg(fileName).arg(file.errorString());
		return false;
	}
	file.write(BriefToJson(brief).toUtf8());
	return true;
}
