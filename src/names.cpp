//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the names the game itself uses.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "names.h"

#include "audef.h"
#include "dbindustry.h"

namespace {

struct IndustryEntry
{
	const char*	name;
	INDUSTRYID	id;
};

// The industries a blueprint can be given. The ones that are missing from the
// game's own table are not much use for a blueprint.
const IndustryEntry kIndustries[] = {
	{ "capitol",		IID_Capitol },
	{ "arena",			IID_Arena },
	{ "military",		IID_BaseHeadquarters },
	{ "barracks",		IID_BaseBarracks },
	{ "warehouse",		IID_Warehouse },
	{ "house",			IID_House },
	{ "apartment",		IID_Apartment },
	{ "hotel",			IID_Hotel },
	{ "office",		IID_DesignStudio },
	{ "observatory",	IID_Observatory },
	{ "biodome",		IID_Biodome },
	{ "park",			IID_Park },
	{ "shop",			IID_RetailStore },
	{ "bank",			IID_Bank },
	{ "hospital",		IID_Hospital },
	{ "school",			IID_School },
	{ "police",		IID_PoliceDepartment },
	{ "fire",			IID_FireDepartment },
	{ "factory",		IID_MachineShop },
	{ "shipyard",		IID_ShipYard },
	{ "refinery",		IID_Refinery },
	{ "farm",			IID_Farm },
	{ "mine",			IID_Mine },
	{ "powerplant",	IID_HydrogenPowerPlant },
	{ "streetlight",	IID_StreetLight },
	{ "fence",			IID_Fence },
	{ "bridge",		IID_Bridge },
	{ "spaceport",		IID_BaseAirTerminal },
};

} // namespace

int IndustryIdFor(const QString& name)
{
	for(const IndustryEntry& e : kIndustries)
	{
		if(name.compare(e.name, Qt::CaseInsensitive) == 0)
			return int(e.id);
	}
	return int(IID_None);
}

QStringList KnownIndustryNames()
{
	QStringList names;
	for(const IndustryEntry& e : kIndustries)
		names << e.name;
	return names;
}

QStringList SpacecraftRoomTypes()
{
	QStringList names;
	// The engine's own room type list, in the order it uses it.
	for(int type = ROOM_None + 1; type <= ROOM_RoofRound; ++type)
	{
		const QString name = RoomTypeNameInSpacecraft((ROOMTYPE)type);
		if(!name.isEmpty())
			names << name;
	}
	return names;
}

QStringList BuildingRoomTypes()
{
	// The rooms the game's own building blueprints use, plus the ones that
	// decide what a building is good for.
	return QStringList()
		<< "Hall"
		<< "Lobby"
		<< "Reception"
		<< "Office"
		<< "Conference Room"
		<< "Council Chamber"
		<< "Main Terminal"
		<< "Turbo Lift"
		<< "Public Lift"
		<< "Hangar"
		<< "Hangar Bay"
		<< "Shack"
		<< "Tower"
		<< "Home"
		<< "Apartment"
		<< "Barracks"
		<< "Crew Quarters"
		<< "Quarters"
		<< "Shop"
		<< "Store"
		<< "Market"
		<< "Factory"
		<< "Workshop"
		<< "Machine Shop"
		<< "Power Plant"
		<< "Generator Room"
		<< "Life Support"
		<< "Sensor Room"
		<< "Shield Room"
		<< "Radio Room"
		<< "Armory"
		<< "Arena"
		<< "Arena Floor"
		<< "Spectator Stand"
		<< "Living Room"
		<< "Lounge"
		<< "Galley"
		<< "Dormitory"
		<< "Sick Bay"
		<< "Transporter Room"
		<< "Vehicle Bay"
		<< "Hold"
		<< "Cargo Hold"
		<< "Utility"
		<< "Emergency Room";
}

QStringList StationTypes()
{
	// The console types of the engine. The order is the order of the
	// stations on a ship.
	return QStringList()
		<< "helm"
		<< "navigator"
		<< "command"
		<< "engineer"
		<< "firecontrol"
		<< "medic"
		<< "sensor"
		<< "shield"
		<< "transporter"
		<< "powerrelay";
}

QStringList ShipRoomTypesFor(const QString& templateKey)
{
	if(templateKey == "military_frigate")
		return QStringList() << "Bridge" << "Engine Room" << "Hold" << "Weapon Bay" << "Sensor Room"
				<< "Shield Room" << "Sick Bay" << "Crew Quarters" << "Hall" << "Transporter Room" << "Store";
	if(templateKey == "cargo_hauler")
		return QStringList() << "Bridge" << "Engine Room" << "Hold" << "Crew Quarters" << "Galley"
				<< "Store" << "Transporter Room" << "Hall";
	if(templateKey == "science_lab")
		return QStringList() << "Bridge" << "Engine Room" << "Sensor Room" << "Sick Bay" << "Crew Quarters"
				<< "Galley" << "Hall" << "Lounge";
	if(templateKey == "colony_transport")
		return QStringList() << "Bridge" << "Engine Room" << "Barracks" << "Crew Quarters" << "Galley"
				<< "Sick Bay" << "Store" << "Transporter Room" << "Hall";
	if(templateKey == "patrol_cutter")
		return QStringList() << "Bridge" << "Engine Room" << "Weapon Bay" << "Sensor Room" << "Crew Quarters"
				<< "Store" << "Hall";
	if(templateKey == "shuttle")
		return QStringList() << "Bridge" << "Engine Room" << "Hold" << "Hall";
	return SpacecraftRoomTypes();
}

QStringList BuildingRoomTypesFor(const QString& templateKey)
{
	if(templateKey == "office_block")
		return QStringList() << "Lobby" << "Office" << "Conference Room" << "Utility" << "Hall";
	if(templateKey == "residential")
		return QStringList() << "Lobby" << "Apartment" << "Galley" << "Utility" << "Hall";
	if(templateKey == "barracks")
		return QStringList() << "Barracks" << "Armory" << "Shack" << "Utility" << "Hall";
	if(templateKey == "command_post")
		return QStringList() << "Council Chamber" << "Office" << "Sensor Room" << "Shield Room" << "Radio Room"
				<< "Utility" << "Hall";
	if(templateKey == "warehouse")
		return QStringList() << "Cargo Hold" << "Store" << "Office" << "Utility" << "Hall";
	if(templateKey == "factory")
		return QStringList() << "Factory" << "Workshop" << "Machine Shop" << "Store" << "Office" << "Utility" << "Hall";
	if(templateKey == "biodome")
		return QStringList() << "Garden" << "Greenhouse" << "Power Plant" << "Life Support" << "Hall";
	if(templateKey == "capitol")
		return QStringList() << "Council Chamber" << "Hall" << "Office" << "Conference Room" << "Public Lift"
				<< "Turbo Lift" << "Reception" << "Utility";
	if(templateKey == "arena")
		return QStringList() << "Arena Floor" << "Spectator Stand" << "Emergency Room" << "Utility" << "Hall";
	return BuildingRoomTypes();
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Makes a name safe to use as a file name.

QString SanitiseName(const QString& text)
{
	QString s;
	for(QChar c : text)
	{
		if(c.isLetterOrNumber() || c == '-' || c == '_' || c == ' ')
			s += c;
		else
			s += '_';
	}
	s = s.simplified();
	return s.isEmpty() ? QString("design") : s;
}
