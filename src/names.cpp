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
	// The building prototypes, by the game's names.
	{ "coal power plant",		IID_CoalPowerPlant },
	{ "gasoline power plant",	IID_GasolinePowerPlant },
	{ "hydroelectric plant",	IID_HydroelectricPowerPlant },
	{ "hydrogen power plant",	IID_HydrogenPowerPlant },
	{ "natural gas power plant", IID_NaturalGasPowerPlant },
	{ "solar power plant",		IID_SolarPowerPlant },
	{ "wind power plant",		IID_WindPowerPlant },
	{ "nuclear power plant",	IID_NuclearPowerPlant },
	{ "orchard",				IID_Orchard },
	{ "well",					IID_Well },
	{ "smelter",				IID_Smelter },
	{ "police department",		IID_PoliceDepartment },
	{ "fire department",		IID_FireDepartment },
	{ "mass media",			IID_MassMedia },
	{ "church",				IID_Church },
	{ "grocery",				IID_Grocery },
	{ "retail store",			IID_RetailStore },
	{ "broker",				IID_Broker },
	{ "cantina",				IID_Cantina },
	{ "design studio",			IID_DesignStudio },
	{ "textile mill",			IID_TextileMill },
	{ "clothing factory",		IID_ClothingFactory },
	{ "explosives factory",	IID_ExplosivesFactory },
	{ "plastics factory",		IID_PlasticsFactory },
	{ "electronics factory",	IID_ElectronicsFactory },
	{ "computer factory",		IID_ComputerFactory },
	{ "pharmaceutical lab",	IID_PharmaceuticalLab },
	{ "munitions factory",		IID_MunitionsFactory },
	{ "gunsmith",				IID_GunSmith },
	{ "armorer",				IID_Armorer },
	{ "machine shop",			IID_MachineShop },
	{ "vehicle factory",		IID_VehicleFactory },
	{ "aircraft factory",		IID_AircraftFactory },
	{ "spacecraft factory",	IID_SpacecraftFactory },
	{ "airport terminal",		IID_AirportTerminal },
	{ "airport radar",			IID_AirportRadar },
	{ "airport repair shop",	IID_AirportRepairShop },
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
		if(type == ROOM_GravityWell)
			names << ZeroGRoomType();
		else if(!name.isEmpty())
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
	if(templateKey == "fighter")
		return QStringList() << "Bridge" << "Engine Room" << "Hold";
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

QList<QPair<int, QString> > InteriorTextures()
{
	static const struct { int id; const char* name; } textures[] = {
		{ 33, "Wall Blue White" },
		{ 67, "Wall Red White" },
		{ 96, "Wall Metal" },
		{ 94, "Wall Cargo" },
		{ 97, "Wall Pipes" },
		{ 39, "Wall Concrete" },
		{ 43, "Wall Gypsum" },
		{ 105, "Wall Wood Studs" },
		{ 57, "Plate Metal Box" },
		{ 58, "Plate Metal Squares" },
		{ 59, "Plate Metal X" },
		{ 108, "Plate Magmium Box" },
		{ 109, "Plate Magmium Squares" },
		{ 110, "Plate Magmium X" },
		{ 111, "Plate Vulcium Box" },
		{ 112, "Plate Vulcium Squares" },
		{ 113, "Plate Vulcium X" },
		{ 117, "Plate Adamantine Box" },
		{ 118, "Plate Adamantine Squares" },
		{ 119, "Plate Adamantine X" },
		{ 106, "Metal Brushed" },
		{ 60, "Metal Scratch Bumps" },
		{ 61, "Metal Scratch Rust" },
		{ 62, "Metal Tread" },
		{ 37, "Ceiling Metal" },
		{ 53, "Ceiling Gold" },
		{ 54, "Ceiling Silver" },
		{ 44, "Tile Metal" },
		{ 28, "Tile White Diagonal" },
		{ 29, "Tile White Square" },
		{ 51, "Tile White Black" },
		{ 32, "Black Techno Gothic Tile" },
		{ 79, "Tile Stone" },
		{ 80, "Tile Green Tan Triangles" },
		{ 90, "Tile Jade" },
		{ 99, "Tile Wood" },
		{ 104, "Tile Wood Rustic" },
		{ 56, "Grill Metal Holes" },
		{ 91, "Grill Black" },
		{ 92, "Grill Copper" },
		{ 93, "Grill White" },
		{ 107, "Grill Black Oblong Holes" },
		{ 40, "Copper Tubes" },
		{ 30, "Black Round Dots" },
		{ 31, "Black Round Dots Big" },
		{ 38, "Concrete" },
		{ 34, "Brick Light" },
		{ 35, "Brick Wall" },
		{ 50, "Marble" },
		{ 49, "Marble Green Gold" },
		{ 63, "Marble Obsidian" },
		{ 83, "Marble Tan" },
		{ 116, "Stone" },
		{ 36, "Gold" },
		{ 52, "Magmium" },
		{ 55, "Vulcium" },
		{ 98, "Wood Bare" },
		{ 101, "Wood Planks Beams" },
		{ 103, "Wood Siding" },
		{ 102, "Wood Siding Painted" },
		{ 115, "Fabric" },
		{ 81, "Stucco Light" },
		{ 82, "Stucco Heavy" },
	};
	QList<QPair<int, QString> > out;
	for(const auto& t : textures)
		out << qMakePair(t.id, QString(t.name));
	return out;
}
