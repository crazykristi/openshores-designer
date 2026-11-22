//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - simple tools that change a triangle plan; see plantools.h.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "plantools.h"

#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QMap>
#include <QPolygonF>
#include <QRectF>
#include <QSet>
#include <QTemporaryDir>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>

#include "building.h"
#include "generate.h"
#include "names.h"
#include "shipgen.h"
#include "triplan.h"

QColor RoomColour(int index)
{
	static const QColor colours[] = {
		QColor(90, 140, 220), QColor(220, 130, 70), QColor(120, 190, 110), QColor(200, 90, 160),
		QColor(80, 190, 190), QColor(210, 190, 80), QColor(150, 110, 210), QColor(200, 100, 100),
	};
	return colours[index % 8];
}

QStringList HandBerthWarnings(const TriPlan& plan)
{
	QStringList w;
	if(plan.IsBuilding() || plan.autoBerths)
		return w;
	int stations = 0, crew = 0, any = 0;
	for(const PlanStation& st : plan.stations)
		if(!st.skip && st.type != "transporter")
			++stations;
	for(const PlanBerth& b : plan.berths)
	{
		if(b.skip)
			continue;
		++any;
		if(b.kind == "crew")
			++crew;
	}
	if(plan.autoStations)
	{
		if(any == 0)
			w << "The automatic berths are off and no berths are placed by hand: the ship will have no berths "
				 "at all, and the game will not warn about it.";
		else if(stations > crew)
			w << QString("The automatic berths are off: %1 station(s) are placed by hand but only %2 crew "
						 "berth(s), and the template's own stations need crew too.").arg(stations).arg(crew);
	}
	else if(stations > crew)
		w << QString("The automatic berths are off: %1 station(s) placed by hand but only %2 crew berth(s).")
				.arg(stations).arg(crew);
	return w;
}

namespace
{

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The schemas.

QJsonObject Prop(const char* type, const QString& description)
{
	QJsonObject o;
	o["type"] = QString::fromLatin1(type);
	o["description"] = description;
	return o;
}

QJsonObject Choice(const QStringList& values, const QString& description)
{
	QJsonObject o = Prop("string", description);
	o["enum"] = QJsonArray::fromStringList(values);
	return o;
}

QJsonObject Tool(const char* name, const QString& description, const QJsonObject& props = QJsonObject(),
				 const QStringList& required = QStringList())
{
	QJsonObject schema;
	schema["type"] = "object";
	schema["properties"] = props;
	if(!required.isEmpty())
		schema["required"] = QJsonArray::fromStringList(required);
	QJsonObject tool;
	tool["name"] = QString::fromLatin1(name);
	tool["description"] = description;
	tool["inputSchema"] = schema;
	return tool;
}

// The rectangle of cells most tools take.
void AddRect(QJsonObject& props)
{
	props["x"] = Prop("integer", "The aft-most cell column (0 is the grid's aft end; x grows towards the fore).");
	props["y"] = Prop("integer", "The starboard-most cell row (0 is the grid's starboard side; y grows towards port). "
						 "Leave it out with centred.");
	props["sizeX"] = Prop("integer", "Cells along x (fore and aft).");
	props["sizeY"] = Prop("integer", "Cells along y (across, port to starboard).");
	props["centred"] = Prop("boolean", "Centre it across the grid (y is worked out). Ships are usually symmetric.");
}

QJsonObject DeckProp(const QString& what = "The deck, from 1 at the bottom.")
{
	return Prop("integer", what);
}

const QStringList kStationTypes = { "helm", "navigator", "command", "engineer", "firecontrol", "sensor", "shield",
									"medic", "transporter", "powerrelay" };
const QStringList kBerthKinds = { "crew", "officer", "capt", "passenger", "troop" };

}

QJsonArray PlanToolDefinitions()
{
	QJsonArray tools;
	{
		tools << Tool("get_plan", "What the plan is: its name and kind, the grid and its scale, the decks, every room "
					  "(type, decks, cells, where), and how many doors, ladders, stations, berths and so on. Start here.");
	}
	{
		QJsonObject p;
		p["deck"] = DeckProp();
		p["full"] = Prop("boolean", "The whole grid; otherwise only round what is drawn on the deck.");
		tools << Tool("show_deck", "A map of one deck as text, one character a cell, fore to the right and port at the top, "
					  "with the rooms' letters, and a list of the doors, windows, ladders, stairs, stations and berths on it. "
					  "Use it to see the layout before and after changing it.", p, { "deck" });
	}
	{
		QJsonObject p;
		p["what"] = Choice({ "room_types", "station_types", "berth_kinds", "ship_templates", "building_templates",
							 "ftl_drives", "hull_styles", "system_volumes", "all" }, "Which list; all of them without it.");
		tools << Tool("list_choices", "The names the other tools take: room types, station types, berth kinds, ship and "
					  "building templates, FTL drives, hull styles and the ship systems whose volumes can be set.", p);
	}
	{
		QJsonObject p;
		p["kind"] = Choice({ "ship", "building", "fighter" }, "A ship (the default), a building, or a fighter (a small craft; experimental).");
		p["template"] = Prop("string", "A ship template (list_choices ship_templates) or, for a building, a building template. "
							 "It decides the stations, berths and systems the build puts in.");
		p["name"] = Prop("string", "The design's name.");
		p["sizeX"] = Prop("integer", "Cells along x (fore and aft), up to 400.");
		p["sizeY"] = Prop("integer", "Cells along y (across), up to 400.");
		p["decks"] = Prop("integer", "How many decks (storeys).");
		p["cellFeet"] = Prop("number", "Feet per cell side (5 for a ship, 2.5 for a fighter).");
		p["deckFeet"] = Prop("number", "Feet from one deck floor to the next (10 for a ship).");
		tools << Tool("new_plan", "Starts a new, empty plan in place of the one open (the designer can undo it). Then draw the "
					  "hull with draw_hull and the rooms with add_room.", p);
	}
	{
		QJsonObject p;
		p["name"] = Prop("string", "The design's name.");
		p["template"] = Prop("string", "Ships: the ship template.");
		p["ftlDrive"] = Prop("string", "Ships: the FTL drive (list_choices ftl_drives).");
		p["station"] = Prop("boolean", "Ships: a space station (built in space, no FTL drive).");
		p["cellFeet"] = Prop("number", "Feet per cell side.");
		p["deckFeet"] = Prop("number", "Feet from one deck floor to the next.");
		p["hullStyle"] = Choice({ "smooth", "angular", "classic" }, "How the hull is shaped round the drawn cells.");
		p["smoothing"] = Prop("integer", "How much the hull edges are rounded, 0 to 10.");
		p["slopedDecks"] = Prop("boolean", "The hull slopes between decks of different sizes instead of stepping.");
		p["autoStations"] = Prop("boolean", "The build puts in the template's stations that are not placed by hand.");
		p["autoBerths"] = Prop("boolean", "The build puts in the template's berths. Off: only the berths placed by hand.");
		p["autoPads"] = Prop("boolean", "Transporter pads for transporters whose room has none placed by hand.");
		p["autoPaths"] = Prop("boolean", "Walk paths through every door, ladder and stairs.");
		p["bridgeStyle"] = Choice({ "hazeron", "desks" }, "How stations look: swivel chairs, or integrated desks.");
		p["hullDoorExit"] = Choice({ "auto", "ladder", "none" }, "The way down from the hull doors.");
		p["leaveOut"] = Prop("array", "Ships: stations left out, with what goes with them: shield, firecontrol, transporter.");
		p["statsQuality"] = Prop("integer", "The quality (1 to 255) expected_stats works the numbers out for.");
		p["systemVolumes"] = Prop("object", "Ships: system volumes in cubic meters set by hand, by key (list_choices "
								  "system_volumes); 0 for the template's share. The structure makes up the difference.");
		tools << Tool("set_settings", "Changes settings of the whole plan; only the ones given.", p);
	}
	{
		QJsonObject p;
		p["sizeX"] = Prop("integer", "Cells along x.");
		p["sizeY"] = Prop("integer", "Cells along y.");
		tools << Tool("resize_grid", "Makes the grid bigger or smaller, keeping what is drawn centred (so cell numbers move: "
					  "look again with show_deck).", p, { "sizeX", "sizeY" });
	}
	{
		QJsonObject p;
		p["below"] = Prop("boolean", "Under the bottom deck (deck numbers move up); on top otherwise.");
		tools << Tool("add_deck", "Adds an empty deck on top, or below.", p);
	}
	{
		QJsonObject p;
		p["deck"] = DeckProp();
		tools << Tool("remove_deck", "Takes a deck away, with everything on it.", p, { "deck" });
	}
	{
		QJsonObject p;
		p["deck"] = DeckProp();
		p["times"] = Prop("integer", "How many copies go on top of it (the decks above move up). 1 if left out.");
		p["withRooms"] = Prop("boolean", "Copy its rooms, doors and the rest too (as new rooms); only the hull otherwise.");
		tools << Tool("copy_deck", "Puts copies of a deck straight on top of it.", p, { "deck" });
	}
	{
		QJsonObject p;
		p["deck"] = DeckProp();
		p["toDeck"] = DeckProp("The same on every deck from 'deck' up to this one.");
		AddRect(p);
		p["shape"] = Choice({ "rect", "ellipse", "wedge_fore", "wedge_aft" }, "rect (the default); ellipse; wedge_fore "
							"narrows to a point at the fore end, wedge_aft at the aft end. Slanted edges use half cells.");
		p["erase"] = Prop("boolean", "Take the hull (and any rooms) away there instead.");
		tools << Tool("draw_hull", "Draws the hull outline of a deck: where the inside of the ship is. Rooms can only go on "
					  "hull. The build shapes the real hull round it (skinned and smoothed), so draw the decks' outlines "
					  "as the ship should look from above. add_room draws hull under a room by itself.", p,
					  { "deck", "x", "sizeX", "sizeY" });
	}
	{
		QJsonObject p;
		p["type"] = Prop("string", "The room type (list_choices room_types), such as Bridge, Engine Room, Crew Cabin, Officer Cabin, Hall, Hold.");
		p["label"] = Prop("string", "Its name, unique; 'Type N' if left out.");
		p["deck"] = DeckProp();
		p["toDeck"] = DeckProp("Turbolifts only: the highest deck the lift goes to (the same cells on every deck between).");
		AddRect(p);
		p["replace"] = Prop("boolean", "Take the cells from rooms already there; otherwise overlapping is an error.");
		tools << Tool("add_room", "Adds a room: a rectangle of whole cells on a deck, with hull drawn under it if there was "
					  "none. Rooms that touch share a wall; join them with add_door. For other shapes add a rectangle and "
					  "grow it with extend_room.", p, { "type", "deck", "x", "sizeX", "sizeY" });
	}
	{
		QJsonObject p;
		p["room"] = Prop("string", "The room's name.");
		p["deck"] = DeckProp();
		AddRect(p);
		p["replace"] = Prop("boolean", "Take the cells from rooms already there.");
		tools << Tool("extend_room", "Adds a rectangle of cells to a room (an L shape, a wider end, or the same room on "
					  "another deck for a turbolift).", p, { "room", "deck", "x", "sizeX", "sizeY" });
	}
	{
		QJsonObject p;
		p["room"] = Prop("string", "The room's name.");
		p["label"] = Prop("string", "A new name.");
		p["type"] = Prop("string", "A new room type.");
		p["access"] = Prop("array", "Who may be in it: any of officers, crew, passengers, troops (everyone if left out).");
		tools << Tool("edit_room", "Renames a room, or changes its type or who may be in it.", p, { "room" });
	}
	{
		QJsonObject p;
		p["room"] = Prop("string", "The room's name.");
		tools << Tool("remove_room", "Takes a room away (its cells stay hull) with its doors and what was in it.", p, { "room" });
	}
	{
		QJsonObject p;
		p["from"] = Prop("string", "One room's name.");
		p["to"] = Prop("string", "The other room's name, or \"outside\" for a door through the hull (a way in).");
		p["deck"] = DeckProp("The deck, where the rooms meet on more than one.");
		p["width"] = Prop("integer", "Cells wide (1 if left out; 2 or more for a wide door or a vehicle bay door).");
		p["side"] = Choice({ "fore", "aft", "port", "starboard" }, "Which wall of 'from' it goes in, if it matters.");
		p["kind"] = Choice({ "door", "window", "balcony" }, "Between two rooms only: a window (nobody walks through) or an "
						   "opening above a low parapet. A door if left out.");
		p["open"] = Prop("boolean", "The door is open when the blueprint is built.");
		p["exit"] = Choice({ "auto", "ladder", "ramp", "none" }, "Hull doors: the way down to the ground (none: a docking door).");
		tools << Tool("add_door", "Puts a door in the wall between two rooms that touch on the same deck, or between a room "
					  "and the outside. It picks the middle of the longest stretch of wall they share.", p, { "from", "to" });
	}
	{
		QJsonObject p;
		p["room"] = Prop("string", "The room's name.");
		p["side"] = Choice({ "fore", "aft", "port", "starboard", "any" }, "Which outer wall; the longest one with any.");
		p["length"] = Prop("integer", "Cells long (2 if left out).");
		p["deck"] = DeckProp("The deck, for a room on more than one.");
		tools << Tool("add_window", "Puts a window in an outer wall of a room (a wall with no room beyond it).", p, { "room" });
	}
	{
		QJsonObject p;
		p["lower"] = Prop("string", "The room on the lower deck.");
		p["upper"] = Prop("string", "The room on the deck above it.");
		p["kind"] = Choice({ "ladder", "hatch_ladder", "hatch", "stairs", "ramp" }, "A ladder (the default), a ladder "
						   "through a hatch that opens and closes, a hatch alone, stairs or a ramp (they need a run of "
						   "cells in both rooms).");
		p["x"] = Prop("integer", "Ladders and hatches: the cell, if it matters.");
		p["y"] = Prop("integer", "Ladders and hatches: the cell, if it matters.");
		p["width"] = Prop("integer", "Stairs and ramps: cells across, 1 to 3.");
		tools << Tool("connect_decks", "Joins a room to a room on the deck above, where they overlap.", p, { "lower", "upper" });
	}
	{
		QJsonObject p;
		p["type"] = Choice(kStationTypes, "The station.");
		p["room"] = Prop("string", "The room it goes in (the helm, navigator and command usually on the Bridge, the "
						 "engineer in the Engine Room).");
		p["count"] = Prop("integer", "How many (1 if left out).");
		p["seat"] = Choice({ "chair", "desk" }, "A swivel chair or an integrated desk; the ship's bridge style if left out.");
		p["module"] = Prop("integer", "Fire control: the weapon bay it runs (a commodity id; the plan's own if left out).");
		tools << Tool("add_station", "Places crew stations in a room, against a wall, clear of doors and of each other. Each one "
					  "placed counts towards the template's: the build puts in only the rest (unless autoStations is off).",
					  p, { "type", "room" });
	}
	{
		QJsonObject p;
		p["kind"] = Choice(kBerthKinds, "Whose berth: crew, officer, capt (the captain), passenger or troop.");
		p["room"] = Prop("string", "The room they go in (usually a Crew Cabin, Officer Cabin, Stateroom or Barracks).");
		p["count"] = Prop("integer", "How many (1 if left out).");
		tools << Tool("add_berths", "Places berths (beds) in a room, along the walls and clear of doors. With autoBerths "
					  "on, the build adds the template's other berths too.", p, { "kind", "room" });
	}
	{
		QJsonObject p;
		p["what"] = Choice({ "doors", "windows", "ladders", "hatches", "stairs", "stations", "berths", "pads" }, "What to take away.");
		p["room"] = Prop("string", "Only those in or on the walls of this room.");
		p["deck"] = DeckProp("Only those on this deck.");
		tools << Tool("remove_items", "Takes doors, windows, ladders, stairs, stations or berths away.", p, { "what" });
	}
	{
		tools << Tool("check_plan", "Looks for problems before building: rooms nobody can reach from a door to the outside, "
					  "no way in, turbolift shafts that do not line up, too few berths placed by hand, things outside a "
					  "building's site. Build to have the engine's own checks.");
	}
	{
		tools << Tool("get_plan_json", "The whole plan as JSON, the designer's own file format, for detailed changes the "
					  "simple tools cannot make. Prefer the simple tools.");
	}
	{
		QJsonObject p;
		p["plan"] = Prop("object", "The plan, as get_plan_json gives it.");
		tools << Tool("set_plan_json", "Replaces the plan with JSON in the designer's file format.", p, { "plan" });
	}
	return tools;
}

bool IsPlanTool(const QString& name)
{
	static QSet<QString> names;
	if(names.isEmpty())
		for(const QJsonValue& v : PlanToolDefinitions())
			names.insert(v.toObject()["name"].toString());
	return names.contains(name);
}

QString PlanToolsGuide()
{
	return
		"Shipgen designer: design Shores of Hazeron ships and buildings.\n"
		"The plan is a grid of square cells seen from above, one grid per deck. x runs aft (0) to fore, y starboard (0) "
		"to port, decks are numbered from 1 at the bottom. Rooms are rectangles of whole cells; rooms that touch share a "
		"wall.\n"
		"Usual order: get_plan (or new_plan) -> draw_hull for each deck's outline -> add_room -> add_door between rooms and "
		"at least one to \"outside\" -> connect_decks (ladders, stairs) -> optionally add_station and add_berths (the "
		"build puts in the template's stations and berths by itself) -> check_plan -> build -> expected_stats -> "
		"save_plan and save_blueprint. Look with show_deck often, and with picture.\n"
		"A ship needs a Bridge for the helm, an Engine Room for the engineer, and cabins for the berths; list_choices "
		"gives the names. Every change can be undone in the designer (and with undo).\n"
		"The expected stats are only a guide worked out with the game's own code; the game can change, so the player must "
		"always check the design in the game before building it.";
}

namespace
{

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Helpers.

struct Fail
{
	QString	text;
};

int IntArg(const QJsonObject& args, const char* key, int fallback)
{
	const QJsonValue v = args.value(QLatin1String(key));
	if(v.isDouble())
		return int(std::lround(v.toDouble()));
	if(v.isString())
	{
		bool ok = false;
		const int n = v.toString().trimmed().toInt(&ok);
		if(ok)
			return n;
		throw Fail{ QString("%1 has to be a whole number.").arg(key) };
	}
	return fallback;
}

double NumArg(const QJsonObject& args, const char* key, double fallback)
{
	const QJsonValue v = args.value(QLatin1String(key));
	if(v.isDouble())
		return v.toDouble();
	if(v.isString())
	{
		bool ok = false;
		const double n = v.toString().trimmed().toDouble(&ok);
		if(ok)
			return n;
		throw Fail{ QString("%1 has to be a number.").arg(key) };
	}
	return fallback;
}

bool BoolArg(const QJsonObject& args, const char* key, bool fallback)
{
	const QJsonValue v = args.value(QLatin1String(key));
	if(v.isBool())
		return v.toBool();
	if(v.isString())
		return v.toString() == "true" || v.toString() == "1" || v.toString() == "yes";
	if(v.isDouble())
		return v.toDouble() != 0;
	return fallback;
}

QString StrArg(const QJsonObject& args, const char* key, const QString& fallback = QString())
{
	const QJsonValue v = args.value(QLatin1String(key));
	return v.isString() ? v.toString().trimmed() : fallback;
}

bool Has(const QJsonObject& args, const char* key)
{
	return args.contains(QLatin1String(key)) && !args.value(QLatin1String(key)).isNull();
}

// A deck argument from 1, as the 0-based deck.
int DeckArg(const TriPlan& plan, const QJsonObject& args, const char* key = "deck")
{
	if(!Has(args, key))
		throw Fail{ QString("%1 is needed.").arg(key) };
	const int deck = IntArg(args, key, 0);
	if(deck < 1 || deck > plan.Decks())
		throw Fail{ QString("There is no deck %1: the plan has decks 1 to %2.").arg(deck).arg(plan.Decks()) };
	return deck - 1;
}

const char* SideName(int k)
{
	switch(k)
	{
	case TRI_S: return "starboard";
	case TRI_E: return "fore";
	case TRI_N: return "port";
	default: return "aft";
	}
}

int SideFromName(const QString& name)
{
	if(name == "fore") return TRI_E;
	if(name == "aft") return TRI_W;
	if(name == "port") return TRI_N;
	if(name == "starboard") return TRI_S;
	return -1;
}

struct CellRect
{
	int	x0 = 0, y0 = 0, x1 = 0, y1 = 0;		// x1, y1 not included.
};

CellRect RectArg(const TriPlan& plan, const QJsonObject& args)
{
	CellRect r;
	const int sx = IntArg(args, "sizeX", 0), sy = IntArg(args, "sizeY", 0);
	if(sx < 1 || sy < 1)
		throw Fail{ "sizeX and sizeY have to be 1 or more." };
	if(!Has(args, "x"))
		throw Fail{ "x is needed." };
	r.x0 = IntArg(args, "x", 0);
	const bool centred = BoolArg(args, "centred", false) || BoolArg(args, "centered", false);
	r.y0 = centred || !Has(args, "y") ? (plan.Depth() - sy) / 2 : IntArg(args, "y", 0);
	r.x1 = r.x0 + sx;
	r.y1 = r.y0 + sy;
	if(r.x0 < 0 || r.y0 < 0 || r.x1 > plan.Width() || r.y1 > plan.Depth())
		throw Fail{ QString("Cells x %1 to %2, y %3 to %4 do not fit on the grid, which is x 0 to %5 and y 0 to %6. "
							"Make it bigger with resize_grid, or pick other cells.")
				.arg(r.x0).arg(r.x1 - 1).arg(r.y0).arg(r.y1 - 1).arg(plan.Width() - 1).arg(plan.Depth() - 1) };
	return r;
}

QString RectText(const CellRect& r)
{
	return QString("x %1-%2, y %3-%4").arg(r.x0).arg(r.x1 - 1).arg(r.y0).arg(r.y1 - 1);
}

QStringList RoomTypes(const TriPlan& plan)
{
	QStringList types = SpacecraftRoomTypes();
	types << "Turbo Lift";
	if(plan.IsBuilding())
	{
		types << "Public Lift";
		for(const QString& t : BuildingRoomTypes())
			if(!types.contains(t, Qt::CaseInsensitive))
				types << t;
	}
	return types;
}

QString RoomTypeArg(const TriPlan& plan, const QString& given)
{
	const QStringList types = RoomTypes(plan);
	for(const QString& t : types)
		if(t.compare(given, Qt::CaseInsensitive) == 0)
			return t;
	// What people (and AIs) tend to call them.
	static const QHash<QString, QString> aliases = {
		{ "engineering", "Engine Room" }, { "engine", "Engine Room" }, { "crew quarters", "Crew Cabin" },
		{ "quarters", "Crew Cabin" }, { "officer quarters", "Officer Cabin" }, { "cargo hold", "Hold" },
		{ "cargo", "Hold" }, { "corridor", "Hall" }, { "hallway", "Hall" }, { "medical", "Sick Bay" },
		{ "medbay", "Sick Bay" }, { "infirmary", "Sick Bay" }, { "mess", "Galley" }, { "kitchen", "Galley" },
		{ "hangar", "Vehicle Bay" }, { "lift", "Turbo Lift" }, { "elevator", "Turbo Lift" },
		{ "turbolift", "Turbo Lift" }, { "captain's quarters", "Stateroom" }, { "cockpit", "Bridge" },
		{ "armory", "Weapon Bay" }, { "sensors", "Sensor Room" }, { "shields", "Shield Room" },
		{ "transporter", "Transporter Room" }, { "troop quarters", "Barracks" } };
	const QString alias = aliases.value(given.toLower());
	if(!alias.isEmpty() && types.contains(alias))
		return alias;
	QStringList close;
	for(const QString& t : types)
		if(t.contains(given, Qt::CaseInsensitive) || given.contains(t, Qt::CaseInsensitive))
			close << t;
	throw Fail{ QString("There is no room type \"%1\".%2 list_choices room_types has them all.").arg(given)
			.arg(close.isEmpty() ? QString() : QString(" Did you mean %1?").arg(close.join(" or "))) };
}

// A room by its name (any case), or by its type when only one room has it.
int RoomArg(const TriPlan& plan, const QJsonObject& args, const char* key)
{
	const QString name = StrArg(args, key);
	if(name.isEmpty())
		throw Fail{ QString("%1 (a room's name) is needed.").arg(key) };
	for(int i = 0; i < plan.rooms.size(); ++i)
		if(plan.rooms.at(i).label.compare(name, Qt::CaseInsensitive) == 0)
			return i;
	int byType = -1, count = 0;
	for(int i = 0; i < plan.rooms.size(); ++i)
		if(plan.rooms.at(i).type.compare(name, Qt::CaseInsensitive) == 0)
		{
			byType = i;
			++count;
		}
	if(count == 1)
		return byType;
	QStringList labels;
	for(const PlanRoom& r : plan.rooms)
		labels << r.label;
	throw Fail{ QString("There is no room \"%1\". The rooms are: %2.").arg(name)
			.arg(labels.isEmpty() ? QString("none yet") : labels.join(", ")) };
}

QString NewLabel(const TriPlan& plan, const QString& type)
{
	int n = 1;
	auto taken = [&plan](const QString& label) {
		for(const PlanRoom& r : plan.rooms)
			if(r.label.compare(label, Qt::CaseInsensitive) == 0)
				return true;
		return false;
	};
	while(taken(QString("%1 %2").arg(type).arg(n)))
		++n;
	return QString("%1 %2").arg(type).arg(n);
}

// The cells of a room on a deck, whole or in part.
struct RoomExtent
{
	int		cells = 0;			// Whole cells.
	int		tris = 0;
	int		x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1;
	bool	Any() const { return tris > 0; }
};

RoomExtent Extent(const TriPlan& plan, int room, int deck)
{
	RoomExtent e;
	for(int cy = 0; cy < plan.Depth(); ++cy)
		for(int cx = 0; cx < plan.Width(); ++cx)
		{
			int n = 0;
			for(int k = 0; k < 4; ++k)
				if(plan.Room(TriRef(deck, cx, cy, k)) == room)
					++n;
			if(!n)
				continue;
			e.tris += n;
			if(n == 4)
				++e.cells;
			e.x0 = qMin(e.x0, cx);
			e.y0 = qMin(e.y0, cy);
			e.x1 = qMax(e.x1, cx);
			e.y1 = qMax(e.y1, cy);
		}
	return e;
}

QList<int> RoomDecks(const TriPlan& plan, int room)
{
	QList<int> decks;
	for(int d = 0; d < plan.Decks(); ++d)
		if(Extent(plan, room, d).Any())
			decks << d;
	return decks;
}

QString RoomName(const TriPlan& plan, int room)
{
	return room >= 0 && room < plan.rooms.size() ? plan.rooms.at(room).label : QString("outside");
}

QPointF CellsOf(const TriPlan& plan, const QPointF& world)
{
	return (world - plan.Corner(0, 0)) / plan.cellFeet;
}

// Takes a room off its triangles and moves the higher room numbers down.
void DeleteRoom(TriPlan& plan, int room)
{
	for(int d = 0; d < plan.Decks(); ++d)
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const TriRef t(d, cx, cy, k);
					const int r = plan.Room(t);
					if(r == room)
						plan.SetRoom(t, -1);
					else if(r > room)
						plan.SetRoom(t, r - 1);
				}
	plan.rooms.removeAt(room);
}

// Puts a room on a rectangle of a deck, with hull under it.
void PaintRoom(TriPlan& plan, int room, int deck, const CellRect& r, bool replace)
{
	if(!replace)
	{
		QMap<int, int> others;
		for(int cy = r.y0; cy < r.y1; ++cy)
			for(int cx = r.x0; cx < r.x1; ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const int o = plan.Room(TriRef(deck, cx, cy, k));
					if(o >= 0 && o != room)
						others[o]++;
				}
		if(!others.isEmpty())
		{
			QStringList names;
			for(QMap<int, int>::const_iterator i = others.cbegin(); i != others.cend(); ++i)
			{
				const RoomExtent e = Extent(plan, i.key(), deck);
				names << QString("%1 (x %2-%3, y %4-%5)").arg(RoomName(plan, i.key())).arg(e.x0).arg(e.x1).arg(e.y0).arg(e.y1);
			}
			throw Fail{ QString("Cells %1 on deck %2 overlap %3. Pick other cells, or pass replace to take them over.")
					.arg(RectText(r)).arg(deck + 1).arg(names.join(", ")) };
		}
	}
	for(int cy = r.y0; cy < r.y1; ++cy)
		for(int cx = r.x0; cx < r.x1; ++cx)
			for(int k = 0; k < 4; ++k)
			{
				const TriRef t(deck, cx, cy, k);
				plan.SetHull(t, true);
				plan.SetRoom(t, room);
			}
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Looking.

QString PlanSummary(const TriPlan& plan)
{
	QStringList out;
	const QString kind = plan.IsBuilding() ? "building" : plan.IsFighter() ? "fighter (experimental)"
			: plan.station ? "space station" : "ship";
	out << QString("\"%1\": a %2").arg(plan.name, kind);
	if(plan.IsBuilding())
	{
		const BuildingTemplate* t = FindBuildingTemplate(plan.building.structure);
		out << QString("Building: structure %1, industry %2").arg(plan.building.structure, plan.building.industry)
				+ (t ? QString(" (%1)").arg(t->label) : QString());
	}
	else
	{
		const ShipTemplate* t = FindTemplate(plan.templateKey);
		out << QString("Template: %1%2   FTL drive: %3").arg(plan.templateKey)
				.arg(t ? QString(" (%1)").arg(t->label) : QString()).arg(plan.ftlDrive);
	}
	out << QString("Grid: %1 x %2 cells of %3 ft (%4 x %5 ft); x 0-%6 aft to fore, y 0-%7 starboard to port")
			.arg(plan.Width()).arg(plan.Depth()).arg(plan.cellFeet).arg(plan.Width() * plan.cellFeet)
			.arg(plan.Depth() * plan.cellFeet).arg(plan.Width() - 1).arg(plan.Depth() - 1);
	out << QString("Decks: %1, %2 ft apart. Hull style %3.").arg(plan.Decks()).arg(plan.deckFeet).arg(plan.hullStyle);
	for(int d = 0; d < plan.Decks(); ++d)
	{
		int hull = 0, x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1;
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
			{
				bool any = false;
				for(int k = 0; k < 4; ++k)
					any = any || plan.Hull(TriRef(d, cx, cy, k));
				if(!any)
					continue;
				++hull;
				x0 = qMin(x0, cx); y0 = qMin(y0, cy); x1 = qMax(x1, cx); y1 = qMax(y1, cy);
			}
		out << (hull ? QString("  deck %1: hull on %2 cells, x %3-%4, y %5-%6").arg(d + 1).arg(hull).arg(x0).arg(x1).arg(y0).arg(y1)
				: QString("  deck %1: no hull drawn").arg(d + 1));
	}
	out << QString("Rooms: %1").arg(plan.rooms.size());
	for(int i = 0; i < plan.rooms.size(); ++i)
	{
		const PlanRoom& r = plan.rooms.at(i);
		QStringList where;
		for(int d = 0; d < plan.Decks(); ++d)
		{
			const RoomExtent e = Extent(plan, i, d);
			if(e.Any())
				where << QString("deck %1: %2 cells, x %3-%4, y %5-%6").arg(d + 1).arg(e.cells).arg(e.x0).arg(e.x1).arg(e.y0).arg(e.y1);
		}
		out << QString("  %1 [%2]: %3").arg(r.label, r.type, where.isEmpty() ? QString("no cells drawn") : where.join("; "));
	}
	int hullDoors = 0;
	for(const PlanDoor& d : plan.doors)
		if(plan.Room(TriRef(d.deck, d.cx, d.cy, d.side)) < 0 || plan.Room(plan.AcrossCellEdge(d.deck, d.cx, d.cy, d.side)) < 0)
			++hullDoors;
	out << QString("Doors: %1 (to the outside: %2)   Windows: %3   Ladders: %4   Hatches: %5   Stairs: %6   Parking: %7")
			.arg(plan.doors.size()).arg(hullDoors).arg(plan.windows.size()).arg(plan.ladders.size()).arg(plan.hatches.size())
			.arg(plan.stairs.size()).arg(plan.parking.size());
	QMap<QString, int> st, be;
	for(const PlanStation& s : plan.stations)
		st[s.type]++;
	for(const PlanBerth& b : plan.berths)
		be[b.kind]++;
	QStringList sl, bl;
	for(QMap<QString, int>::const_iterator i = st.cbegin(); i != st.cend(); ++i)
		sl << QString("%1 %2").arg(i.value()).arg(i.key());
	for(QMap<QString, int>::const_iterator i = be.cbegin(); i != be.cend(); ++i)
		bl << QString("%1 %2").arg(i.value()).arg(i.key());
	if(!plan.IsBuilding())
	{
		out << QString("Stations placed by hand: %1; the template's others put in by the build: %2")
				.arg(sl.isEmpty() ? QString("none") : sl.join(", "), plan.autoStations ? "yes" : "no");
		out << QString("Berths placed by hand: %1; the template's others put in by the build: %2")
				.arg(bl.isEmpty() ? QString("none") : bl.join(", "), plan.autoBerths ? "yes" : "no");
		QStringList vols;
		for(const HandModuleKey& k : HandModuleKeys())
			if(plan.handModules.*k.field)
				vols << QString("%1 %2 m3").arg(k.key).arg(plan.handModules.*k.field);
		if(!vols.isEmpty())
			out << "System volumes set by hand: " + vols.join(", ");
	}
	out << QString("Expected stats quality: Q%1").arg(plan.statsQuality);
	return out.join("\n");
}

QString DeckMap(const TriPlan& plan, int deck, bool full)
{
	static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	auto letter = [](int room) { return room < 62 ? QChar(letters[room]) : QChar('*'); };
	int x0 = 0, y0 = 0, x1 = plan.Width() - 1, y1 = plan.Depth() - 1;
	if(!full)
	{
		int bx0 = 1 << 20, by0 = 1 << 20, bx1 = -1, by1 = -1;
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
				for(int k = 0; k < 4; ++k)
					if(plan.Hull(TriRef(deck, cx, cy, k)))
					{
						bx0 = qMin(bx0, cx); by0 = qMin(by0, cy); bx1 = qMax(bx1, cx); by1 = qMax(by1, cy);
					}
		if(bx1 >= 0)
		{
			x0 = qMax(0, bx0 - 1); y0 = qMax(0, by0 - 1);
			x1 = qMin(plan.Width() - 1, bx1 + 1); y1 = qMin(plan.Depth() - 1, by1 + 1);
		}
	}
	QStringList out;
	out << QString("Deck %1 of %2, cells x %3-%4 and y %5-%6 (grid %7 x %8). Fore is to the right ->, port at the top.")
			.arg(deck + 1).arg(plan.Decks()).arg(x0).arg(x1).arg(y0).arg(y1).arg(plan.Width()).arg(plan.Depth());
	out << "Key: . outside the hull   # hull with no room   + a cell split between rooms or half hull   letters: rooms";
	QString tens = "    ", units = "    ";
	for(int x = x0; x <= x1; ++x)
	{
		tens += x % 10 == 0 ? QString::number((x / 10) % 10) : QString(" ");
		units += QString::number(x % 10);
	}
	out << tens << units;
	QSet<int> seen;
	for(int y = y1; y >= y0; --y)
	{
		QString row = QString("%1 ").arg(y, 3);
		for(int x = x0; x <= x1; ++x)
		{
			int hull = 0;
			int room = -2;
			bool mixed = false;
			for(int k = 0; k < 4; ++k)
			{
				const TriRef t(deck, x, y, k);
				if(plan.Hull(t))
					++hull;
				const int r = plan.Room(t);
				if(room == -2)
					room = r;
				else if(r != room)
					mixed = true;
			}
			if(!hull)
				row += '.';
			else if(mixed || (room >= 0 && hull < 4))
				row += '+';
			else if(room >= 0)
			{
				row += letter(room);
				seen.insert(room);
			}
			else
				row += hull == 4 ? '#' : '+';
		}
		out << row;
	}
	out << "";
	QList<int> rooms = seen.values();
	std::sort(rooms.begin(), rooms.end());
	for(int r = 0; r < plan.rooms.size(); ++r)
		if(!seen.contains(r) && Extent(plan, r, deck).Any())
			rooms << r;
	for(int r : rooms)
	{
		const RoomExtent e = Extent(plan, r, deck);
		out << QString("%1 = %2 [%3]: %4 cells, x %5-%6, y %7-%8").arg(letter(r)).arg(plan.rooms.at(r).label)
				.arg(plan.rooms.at(r).type).arg(e.cells).arg(e.x0).arg(e.x1).arg(e.y0).arg(e.y1);
	}
	QStringList items;
	for(const PlanDoor& d : plan.doors)
		if(d.deck == deck)
		{
			const int a = plan.Room(TriRef(d.deck, d.cx, d.cy, d.side));
			const int b = plan.Room(plan.AcrossCellEdge(d.deck, d.cx, d.cy, d.side));
			items << QString("door%1: %2 - %3, the %4 edge of cell %5,%6%7").arg(d.kind.isEmpty() ? QString() : " (" + d.kind + ")")
					.arg(RoomName(plan, a), RoomName(plan, b)).arg(SideName(d.side)).arg(d.cx).arg(d.cy).arg(d.skip ? " (left out)" : "");
		}
	for(const PlanDoor& w : plan.windows)
		if(w.deck == deck)
			items << QString("window: %1, the %2 of cell %3,%4").arg(RoomName(plan, plan.Room(TriRef(w.deck, w.cx, w.cy, w.side))))
					.arg(w.edge == 0 ? QString("%1 edge").arg(SideName(w.side)) : QString("diagonal")).arg(w.cx).arg(w.cy);
	for(const PlanLadder& l : plan.ladders)
	{
		if(l.deck == deck)
			items << QString("ladder up to deck %1: cell %2,%3").arg(deck + 2).arg(l.cx).arg(l.cy);
		else if(l.deck + 1 == deck)
			items << QString("ladder down to deck %1: cell %2,%3").arg(deck).arg(l.cx).arg(l.cy);
	}
	for(const PlanHatch& h : plan.hatches)
	{
		if(h.deck == deck)
			items << QString("hatch in the ceiling: cell %1,%2").arg(h.cx).arg(h.cy);
		else if(h.deck + 1 == deck)
			items << QString("hatch in the floor: cell %1,%2").arg(h.cx).arg(h.cy);
	}
	for(const PlanStair& s : plan.stairs)
	{
		if(s.deck == deck)
			items << QString("%1 up to deck %2: from cell %3,%4 climbing %5, %6 cells long, %7 wide").arg(s.IsRamp() ? "ramp" : "stairs")
					.arg(deck + 2).arg(s.cx).arg(s.cy).arg(SideName(s.side)).arg(plan.StairCells(s)).arg(s.width);
		else if(s.deck + 1 == deck)
		{
			int x, y;
			plan.StairCell(s, plan.StairCells(s), x, y);
			items << QString("%1 down to deck %2: landing at cell %3,%4").arg(s.IsRamp() ? "ramp" : "stairs").arg(deck).arg(x).arg(y);
		}
	}
	for(const PlanStation& s : plan.stations)
		if(s.deck == deck)
			items << QString("station %1: at %2,%3 facing %4 deg, in %5").arg(s.type).arg(s.x, 0, 'f', 1).arg(s.y, 0, 'f', 1)
					.arg(s.yaw).arg(RoomName(plan, plan.RoomAtPoint(deck, s.x, s.y)));
	QMap<QString, int> berths;
	for(const PlanBerth& b : plan.berths)
		if(b.deck == deck)
			berths[QString("%1 berths in %2").arg(b.kind, RoomName(plan, plan.RoomAtPoint(deck, b.x, b.y)))]++;
	for(QMap<QString, int>::const_iterator i = berths.cbegin(); i != berths.cend(); ++i)
		items << QString("%1 %2").arg(i.value()).arg(i.key());
	for(const PlanParking& p : plan.parking)
		if(p.deck == deck)
			items << QString("parking (%1): corner of cell %2,%3").arg(p.spacecraft ? "spacecraft" : p.space ? "space vehicle"
					: p.vehicle.isEmpty() ? "ground vehicle" : p.vehicle).arg(p.cx).arg(p.cy);
	if(!items.isEmpty())
	{
		out << "";
		out << items;
	}
	return out.join("\n");
}

QString Choices(const TriPlan& plan, const QString& what)
{
	QStringList out;
	const bool all = what.isEmpty() || what == "all";
	if(all || what == "room_types")
		out << "Room types (one per line):\n  " + RoomTypes(plan).join("\n  ");
	if(all || what == "station_types")
		out << "Station types: " + kStationTypes.join(", ");
	if(all || what == "berth_kinds")
		out << "Berth kinds: " + kBerthKinds.join(", ");
	if(all || what == "ship_templates")
	{
		out << "Ship templates:";
		for(const ShipTemplate& t : AllTemplates())
			out << QString("  %1: %2. %3").arg(t.key, t.label, t.description);
	}
	if(all || what == "building_templates")
	{
		out << "Building templates:";
		for(const BuildingTemplate& t : AllBuildingTemplates())
			out << QString("  %1: %2. %3").arg(t.key, t.label, t.description);
	}
	if(all || what == "ftl_drives")
	{
		QStringList d;
		for(const QPair<QString, QString>& c : FtlDriveChoices())
			d << QString("%1 (%2)").arg(c.first, c.second);
		out << "FTL drives: " + d.join(", ");
	}
	if(all || what == "hull_styles")
		out << "Hull styles: smooth (rounded), angular (flat panels), classic (straight walls, as the game's old designer)";
	if(all || what == "system_volumes")
	{
		QStringList v;
		for(const HandModuleKey& k : HandModuleKeys())
			v << QString("%1 (%2)").arg(k.key, k.label);
		out << "System volumes (cubic meters): " + v.join(", ");
	}
	if(out.isEmpty())
		throw Fail{ QString("There is no list \"%1\".").arg(what) };
	return out.join("\n");
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Doors and windows.

// One edge of a cell (a door's or window's place).
struct Edge
{
	int	deck, cx, cy, side;
};

// Edges in straight runs along one wall line.
QList<QList<Edge> > Runs(QList<Edge> edges)
{
	std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
		const bool alongY = a.side == TRI_E || a.side == TRI_W;
		const int la = alongY ? a.cx : a.cy, lb = alongY ? b.cx : b.cy;
		const int pa = alongY ? a.cy : a.cx, pb = alongY ? b.cy : b.cx;
		if(a.deck != b.deck) return a.deck < b.deck;
		if(a.side != b.side) return a.side < b.side;
		if(la != lb) return la < lb;
		return pa < pb;
	});
	QList<QList<Edge> > runs;
	for(const Edge& e : edges)
	{
		if(!runs.isEmpty())
		{
			const Edge& last = runs.last().last();
			const bool alongY = e.side == TRI_E || e.side == TRI_W;
			if(last.deck == e.deck && last.side == e.side && (alongY ? last.cx == e.cx && last.cy + 1 == e.cy
					: last.cy == e.cy && last.cx + 1 == e.cx))
			{
				runs.last() << e;
				continue;
			}
		}
		runs << (QList<Edge>() << e);
	}
	return runs;
}

// The run to use: the longest, and of those the one nearest the middle of the room's wall.
const QList<Edge>* BestRun(const QList<QList<Edge> >& runs)
{
	const QList<Edge>* best = 0;
	for(const QList<Edge>& r : runs)
		if(!best || r.size() > best->size())
			best = &r;
	return best;
}

PlanToolResult AddDoor(TriPlan& plan, const QJsonObject& args)
{
	const int a = RoomArg(plan, args, "from");
	const QString toName = StrArg(args, "to");
	const bool outside = toName.compare("outside", Qt::CaseInsensitive) == 0;
	const int b = outside ? -1 : RoomArg(plan, args, "to");
	if(a == b)
		throw Fail{ "A door joins two different rooms." };
	const int onlyDeck = Has(args, "deck") ? DeckArg(plan, args) : -1;
	const int side = Has(args, "side") ? SideFromName(StrArg(args, "side")) : -1;
	if(Has(args, "side") && side < 0)
		throw Fail{ "side is fore, aft, port or starboard." };
	QString kind = StrArg(args, "kind");
	if(kind == "door")
		kind.clear();
	if(!kind.isEmpty() && kind != "window" && kind != "balcony")
		throw Fail{ "kind is door, window or balcony." };
	if(!kind.isEmpty() && outside)
		throw Fail{ "A window or balcony door goes between two rooms; for a window in the hull use add_window." };
	const int width = qMax(1, IntArg(args, "width", 1));
	const QString exit = StrArg(args, "exit");
	if(!exit.isEmpty() && exit != "auto" && exit != "ladder" && exit != "ramp" && exit != "none")
		throw Fail{ "exit is auto, ladder, ramp or none." };

	// Every cell edge with 'a' on one side and 'b' (or no room) on the other.
	// Through the hull, edges that are truly outside are better than ones
	// with hull (but no room) beyond them.
	QList<Edge> open, inHull;
	int already = 0;
	for(int d = 0; d < plan.Decks(); ++d)
	{
		if(onlyDeck >= 0 && d != onlyDeck)
			continue;
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
				for(int k = 0; k < 4; ++k)
				{
					if(side >= 0 && k != side)
						continue;
					const TriRef ta(d, cx, cy, k);
					if(plan.Room(ta) != a)
						continue;
					const TriRef tb = plan.AcrossCellEdge(d, cx, cy, k);
					const int rb = plan.Room(tb);
					if(outside ? rb >= 0 : rb != b)
						continue;
					if(plan.DoorAt(d, cx, cy, k) >= 0)
					{
						++already;
						continue;
					}
					if(outside && plan.Hull(tb))
						inHull << Edge{ d, cx, cy, k };
					else
						open << Edge{ d, cx, cy, k };
				}
	}
	if(outside && open.isEmpty() && !inHull.isEmpty())
		throw Fail{ QString("No wall of %1%2 is on the edge of the hull: there is hull with no room beyond it, so a door "
							"there would open into the inside of the hull. Give the room a wall on the hull's edge "
							"(extend it, or draw_hull with erase), or put the door in a room that has one.")
				.arg(RoomName(plan, a)).arg(side >= 0 ? QString(" on its %1 side").arg(SideName(side)) : QString()) };
	const QList<Edge>& edges = open;
	if(edges.isEmpty())
	{
		if(already)
			throw Fail{ QString("%1 and %2 already have doors on every edge they share.").arg(RoomName(plan, a), RoomName(plan, b)) };
		throw Fail{ outside
				? QString("%1 has no%2 outer wall%3 for a door to the outside: every edge has a room beyond it.")
						.arg(RoomName(plan, a)).arg(side >= 0 ? QString(" ") + SideName(side) : QString())
						.arg(onlyDeck >= 0 ? QString(" on deck %1").arg(onlyDeck + 1) : QString())
				: QString("%1 and %2 do not share a wall%3%4. Rooms need to touch along a cell edge on the same deck; "
						  "use show_deck to see where they are.").arg(RoomName(plan, a), RoomName(plan, b))
						.arg(side >= 0 ? QString(" on the %1 side of %2").arg(SideName(side), RoomName(plan, a)) : QString())
						.arg(onlyDeck >= 0 ? QString(" on deck %1").arg(onlyDeck + 1) : QString()) };
	}
	const QList<QList<Edge> > runs = Runs(edges);
	const QList<Edge>& run = *BestRun(runs);
	const int n = qMin(width, run.size());
	const int start = (run.size() - n) / 2;
	const int row = plan.NewDoorRow();
	for(int i = start; i < start + n; ++i)
	{
		const Edge& e = run.at(i);
		PlanDoor door;
		door.deck = e.deck;
		door.cx = e.cx;
		door.cy = e.cy;
		door.side = e.side;
		door.row = row;
		door.kind = kind;
		door.open = BoolArg(args, "open", false) && kind.isEmpty();
		if(outside && !exit.isEmpty())
			door.exit = exit;
		plan.doors << door;
	}
	const Edge& first = run.at(start);
	PlanToolResult res;
	res.changed = true;
	res.deck = first.deck;
	res.text = QString("Put %1%2 %3 between %4 and %5 on deck %6, on the %7 edge of cell%8 %9.")
			.arg(n == 1 ? "a" : QString("a %1 cells wide").arg(n)).arg(kind.isEmpty() ? QString() : " " + kind)
			.arg(kind.isEmpty() ? "door" : "opening").arg(RoomName(plan, a), RoomName(plan, b)).arg(first.deck + 1)
			.arg(SideName(first.side)).arg(n == 1 ? "" : "s")
			.arg(n == 1 ? QString("%1,%2").arg(first.cx).arg(first.cy)
					: QString("%1,%2 to %3,%4").arg(first.cx).arg(first.cy).arg(run.at(start + n - 1).cx).arg(run.at(start + n - 1).cy));
	if(width > n)
		res.text += QString(" The wall they share is only %1 cells long there.").arg(run.size());
	return res;
}

PlanToolResult AddWindow(TriPlan& plan, const QJsonObject& args)
{
	const int room = RoomArg(plan, args, "room");
	const QString sideName = StrArg(args, "side", "any");
	const int side = SideFromName(sideName);
	if(side < 0 && sideName != "any")
		throw Fail{ "side is fore, aft, port, starboard or any." };
	const int onlyDeck = Has(args, "deck") ? DeckArg(plan, args) : -1;
	const int length = qMax(1, IntArg(args, "length", 2));
	QList<Edge> outer, inHull;
	for(int d = 0; d < plan.Decks(); ++d)
	{
		if(onlyDeck >= 0 && d != onlyDeck)
			continue;
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
				for(int k = 0; k < 4; ++k)
				{
					if(side >= 0 && k != side)
						continue;
					const TriRef t(d, cx, cy, k);
					if(plan.Room(t) != room)
						continue;
					const TriRef across = plan.AcrossCellEdge(d, cx, cy, k);
					if(plan.Room(across) >= 0 || plan.DoorAt(d, cx, cy, k) >= 0 || plan.WindowAt(d, cx, cy, k, 0) >= 0)
						continue;
					(plan.Hull(across) ? inHull : outer) << Edge{ d, cx, cy, k };
				}
	}
	if(outer.isEmpty() && !inHull.isEmpty())
		throw Fail{ QString("No free wall of %1%2 is on the edge of the hull (there is hull with no room beyond it), so a "
							"window there would look into the inside of the hull.").arg(RoomName(plan, room))
				.arg(side >= 0 ? QString(" on its %1 side").arg(SideName(side)) : QString()) };
	const QList<Edge>& edges = outer;
	if(edges.isEmpty())
		throw Fail{ QString("%1 has no free outer wall%2 (a wall with no room beyond it).").arg(RoomName(plan, room))
				.arg(side >= 0 ? QString(" on its %1 side").arg(SideName(side)) : QString()) };
	const QList<QList<Edge> > runs = Runs(edges);
	const QList<Edge>& run = *BestRun(runs);
	const int n = qMin(length, run.size());
	const int start = (run.size() - n) / 2;
	const int row = plan.NewWindowRow();
	for(int i = start; i < start + n; ++i)
	{
		const Edge& e = run.at(i);
		PlanDoor w;
		w.deck = e.deck;
		w.cx = e.cx;
		w.cy = e.cy;
		w.side = e.side;
		w.edge = 0;
		w.row = row;
		plan.windows << w;
	}
	const Edge& first = run.at(start);
	PlanToolResult res;
	res.changed = true;
	res.deck = first.deck;
	res.text = QString("Put a window %1 cell%2 long in the %3 wall of %4 on deck %5, from cell %6,%7.").arg(n).arg(n == 1 ? "" : "s")
			.arg(SideName(first.side)).arg(RoomName(plan, room)).arg(first.deck + 1).arg(first.cx).arg(first.cy);
	return res;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Ladders and stairs.

// Whether a cell of a room on a deck has a wall (a neighbour that is not the room) and a door next to it.
bool CellAtWall(const TriPlan& plan, int deck, int cx, int cy, int room)
{
	const int dx[4] = { 0, 1, 0, -1 }, dy[4] = { -1, 0, 1, 0 };
	for(int k = 0; k < 4; ++k)
	{
		const int nx = cx + dx[k], ny = cy + dy[k];
		if(nx < 0 || ny < 0 || nx >= plan.Width() || ny >= plan.Depth() || plan.CellRoom(deck, nx, ny) != room)
			return true;
	}
	return false;
}

bool CellByDoor(const TriPlan& plan, int deck, int cx, int cy)
{
	for(int k = 0; k < 4; ++k)
		if(plan.DoorAt(deck, cx, cy, k) >= 0)
			return true;
	return false;
}

PlanToolResult ConnectDecks(TriPlan& plan, const QJsonObject& args)
{
	const int lower = RoomArg(plan, args, "lower");
	const int upper = RoomArg(plan, args, "upper");
	const QString kind = StrArg(args, "kind", "ladder");
	if(kind != "ladder" && kind != "hatch_ladder" && kind != "hatch" && kind != "stairs" && kind != "ramp")
		throw Fail{ "kind is ladder, hatch_ladder, hatch, stairs or ramp." };
	PlanToolResult res;
	res.changed = true;

	if(kind == "stairs" || kind == "ramp")
	{
		const int width = IntArg(args, "width", 1);
		QString why;
		bool tried = false;
		for(int d = 0; d + 1 < plan.Decks(); ++d)
		{
			// Starts against a wall of the lower room first, so the flight runs along it.
			struct Try { PlanStair st; int score; };
			QList<Try> tries;
			for(int cy = 0; cy < plan.Depth(); ++cy)
				for(int cx = 0; cx < plan.Width(); ++cx)
				{
					if(plan.CellRoom(d, cx, cy) != lower)
						continue;
					for(int k = 0; k < 4; ++k)
					{
						PlanStair st;
						st.deck = d;
						st.cx = cx;
						st.cy = cy;
						st.side = k;
						st.width = width;
						st.kind = kind == "ramp" ? "ramp" : QString();
						int x, y;
						plan.StairCell(st, plan.StairCells(st), x, y);
						if(x < 0 || y < 0 || x >= plan.Width() || y >= plan.Depth() || plan.CellRoom(d + 1, x, y) != upper)
							continue;
						tried = true;
						if(!plan.StairFits(st, &why))
							continue;
						int score = 0;
						for(int i = 0; i < plan.StairCells(st); ++i)
						{
							plan.StairCell(st, i, x, y);
							if(CellAtWall(plan, d, x, y, lower))
								++score;
							if(CellByDoor(plan, d, x, y))
								score -= 3;
						}
						tries << Try{ st, score };
					}
				}
			if(tries.isEmpty())
				continue;
			const Try* best = &tries.first();
			for(const Try& t : tries)
				if(t.score > best->score)
					best = &t;
			plan.stairs << best->st;
			res.deck = d;
			res.text = QString("Put %1 from %2 on deck %3 up to %4: starting at cell %5,%6 and climbing %7 over %8 cells.")
					.arg(kind == "ramp" ? "a ramp" : "stairs").arg(RoomName(plan, lower)).arg(d + 1).arg(RoomName(plan, upper))
					.arg(best->st.cx).arg(best->st.cy).arg(SideName(best->st.side)).arg(plan.StairCells(best->st));
			return res;
		}
		PlanStair probe;
		probe.kind = kind == "ramp" ? "ramp" : QString();
		throw Fail{ QString("No room for %1 from %2 up to %3. A flight needs %4 cells in a row, %5 wide, in %2 with %3 over "
							"them, and one more cell of %3 to land on.%6")
				.arg(kind).arg(RoomName(plan, lower), RoomName(plan, upper)).arg(plan.StairCells(probe)).arg(width)
				.arg(tried && !why.isEmpty() ? " Last reason: " + why : QString()) };
	}

	struct Spot { int deck, cx, cy, score; };
	QList<Spot> spots;
	const bool given = Has(args, "x") && Has(args, "y");
	const int gx = IntArg(args, "x", -1), gy = IntArg(args, "y", -1);
	for(int d = 0; d + 1 < plan.Decks(); ++d)
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
			{
				if(given && (cx != gx || cy != gy))
					continue;
				if(plan.CellRoom(d, cx, cy) != lower || plan.CellRoom(d + 1, cx, cy) != upper)
					continue;
				if(plan.LadderAt(d, cx, cy) >= 0 || plan.HatchAt(d, cx, cy) >= 0 || plan.StairAt(d, cx, cy) >= 0
						|| plan.StairAt(d + 1, cx, cy) >= 0)
					continue;
				int score = 0;
				if(CellAtWall(plan, d, cx, cy, lower)) score += 2;
				if(CellAtWall(plan, d + 1, cx, cy, upper)) score += 1;
				if(CellByDoor(plan, d, cx, cy)) score -= 4;
				if(CellByDoor(plan, d + 1, cx, cy)) score -= 4;
				spots << Spot{ d, cx, cy, score };
			}
	if(spots.isEmpty())
		throw Fail{ given ? QString("Cell %1,%2 is not a whole cell of %3 with a whole cell of %4 over it, or something is "
									"already there.").arg(gx).arg(gy).arg(RoomName(plan, lower), RoomName(plan, upper))
				: QString("%1 and %2 do not overlap: a ladder or hatch needs a whole cell of %1 with a whole cell of %2 "
						  "straight over it, on the next deck up.").arg(RoomName(plan, lower), RoomName(plan, upper)) };
	const Spot* best = &spots.first();
	for(const Spot& s : spots)
		if(s.score > best->score)
			best = &s;
	if(kind != "hatch")
	{
		PlanLadder l;
		l.deck = best->deck;
		l.cx = best->cx;
		l.cy = best->cy;
		plan.ladders << l;
	}
	if(kind != "ladder")
	{
		PlanHatch h;
		h.deck = best->deck;
		h.cx = best->cx;
		h.cy = best->cy;
		h.row = plan.NewHatchRow();
		plan.hatches << h;
	}
	res.deck = best->deck;
	res.text = QString("Put %1 from %2 on deck %3 up to %4 on deck %5, in cell %6,%7.")
			.arg(kind == "ladder" ? "a ladder" : kind == "hatch" ? "a hatch" : "a ladder through a hatch")
			.arg(RoomName(plan, lower)).arg(best->deck + 1).arg(RoomName(plan, upper)).arg(best->deck + 2).arg(best->cx).arg(best->cy);
	return res;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Stations and berths.

// An item's outline in feet, as the designer's Crew page draws it
// (PlanView::ItemOutline): 0 a station and its seat, 1 a berth, 2 a pad.
QPolygonF ItemOutline(const TriPlan& plan, int kind, const QPointF& world, double yawDegrees, const QString& seat, double grow = 0)
{
	double halfAcross = 1.64, behind = 1.97, ahead = 3.61;
	const bool desk = seat == "desk" || (seat.isEmpty() && plan.bridgeStyle == "desks");
	if(kind == 0 && desk)
	{
		halfAcross = 2.0;
		behind = 1.2 + 2.0;
		ahead = 3.4;
	}
	else if(kind == 1)
	{
		halfAcross = 1.65;
		behind = ahead = 3.3;
	}
	else if(kind == 2)
		halfAcross = behind = ahead = 1.5;
	halfAcross += grow;
	behind += grow;
	ahead += grow;
	const double a = yawDegrees * M_PI / 180.0;
	const QPointF f(cos(a), sin(a)), side(-sin(a), cos(a));
	QPolygonF out;
	out << world - f * behind - side * halfAcross << world + f * ahead - side * halfAcross
		<< world + f * ahead + side * halfAcross << world - f * behind + side * halfAcross;
	return out;
}

int RoomAtWorld(const TriPlan& plan, int deck, const QPointF& world)
{
	TriRef t;
	if(!plan.TriAt(deck, world, t))
		return -1;
	return plan.Room(t);
}

// The room the whole outline lies in, or -1 (PlanView::OutlineRoom).
int OutlineRoom(const TriPlan& plan, int deck, const QPolygonF& outline)
{
	QList<QPointF> points;
	QPointF mid;
	const QPointF centre = outline.boundingRect().center();
	for(int i = 0; i < outline.size(); ++i)
	{
		const QPointF a = outline.at(i), b = outline.at((i + 1) % outline.size());
		points << a + (centre - a) * 0.02 << (a + b) / 2.0 + (centre - (a + b) / 2.0) * 0.02;
		mid += a;
	}
	points << mid / outline.size();
	int room = -2;
	for(const QPointF& w : points)
	{
		const int r = RoomAtWorld(plan, deck, w);
		if(r < 0 || (room != -2 && r != room))
			return -1;
		room = r;
	}
	return room < 0 ? -1 : room;
}

// What a new item must not touch on a deck: the other items, the ground in
// front of doors, and ladders, hatches and stairs.
QList<QPolygonF> Occupied(const TriPlan& plan, int deck)
{
	QList<QPolygonF> out;
	for(const PlanStation& s : plan.stations)
		if(s.deck == deck && !s.skip)
			out << ItemOutline(plan, 0, plan.CellPointToWorld(s.x, s.y), s.yaw, s.seat);
	for(const PlanBerth& b : plan.berths)
		if(b.deck == deck && !b.skip)
			out << ItemOutline(plan, 1, plan.CellPointToWorld(b.x, b.y), b.yaw, QString());
	for(const PlanPad& p : plan.pads)
		if(p.deck == deck && !p.skip)
			out << ItemOutline(plan, 2, plan.CellPointToWorld(p.x, p.y), 0, QString());
	auto cell = [&plan](int cx, int cy) {
		return QPolygonF(QRectF(plan.Corner(cx, cy), plan.Corner(cx + 1, cy + 1)).normalized());
	};
	for(const PlanDoor& d : plan.doors)
		if(d.deck == deck)
		{
			QPointF a, b;
			plan.EdgeEnds(TriRef(d.deck, d.cx, d.cy, d.side), 0, a, b);
			const QPointF m = (a + b) / 2.0;
			const double r = qMax(3.5, plan.cellFeet * 0.75);
			out << QPolygonF(QRectF(m - QPointF(r, r), m + QPointF(r, r)));
		}
	for(const PlanLadder& l : plan.ladders)
		if(l.deck == deck || l.deck + 1 == deck)
			out << cell(l.cx, l.cy);
	for(const PlanHatch& h : plan.hatches)
		if(h.deck == deck || h.deck + 1 == deck)
			out << cell(h.cx, h.cy);
	for(const PlanStair& s : plan.stairs)
		if(s.deck == deck || s.deck + 1 == deck)
			for(int lane = 0; lane < qMax(1, s.width); ++lane)
				for(int i = 0; i <= plan.StairCells(s); ++i)
				{
					int x, y;
					plan.StairCell(s, i, x, y, lane);
					out << cell(x, y);
				}
	return out;
}

struct Placed
{
	int		deck;
	QPointF	cells;
	double	yaw;
};

// Finds a free spot for an item in a room: against a wall, clear of the rest.
bool FindSpot(const TriPlan& plan, int room, int kind, const QString& stationType, const QString& seat, Placed& out)
{
	const QList<int> decks = RoomDecks(plan, room);
	const double step = qBound(0.5, plan.cellFeet / 4.0, 1.0);
	bool found = false;
	double bestScore = -1e9;
	for(int deck : decks)
	{
		const RoomExtent e = Extent(plan, room, deck);
		const QList<QPolygonF> occupied = Occupied(plan, deck);
		const QPointF lo = plan.Corner(e.x0, e.y0), hi = plan.Corner(e.x1 + 1, e.y1 + 1);
		const QPointF centre = (lo + hi) / 2.0;
		double s = step;
		while(((hi.x() - lo.x()) / s) * ((hi.y() - lo.y()) / s) > 40000)
			s *= 2;
		for(double wy = lo.y() + s / 2; wy < hi.y(); wy += s)
			for(double wx = lo.x() + s / 2; wx < hi.x(); wx += s)
				for(int yi = 0; yi < 4; ++yi)
				{
					const double yaw = yi * 90.0;
					const QPointF at(wx, wy);
					const QPolygonF outline = ItemOutline(plan, kind, at, yaw, seat);
					if(OutlineRoom(plan, deck, outline) != room)
						continue;
					const QPolygonF clear = ItemOutline(plan, kind, at, yaw, seat, 0.75);
					bool hit = false;
					for(const QPolygonF& o : occupied)
						if(clear.intersects(o))
						{
							hit = true;
							break;
						}
					if(hit)
						continue;
					const double a = yaw * M_PI / 180.0;
					const QPointF f(cos(a), sin(a)), side(-sin(a), cos(a));
					auto wall = [&](const QPointF& p) { return RoomAtWorld(plan, deck, p) != room; };
					double score = 0;
					if(kind == 0)
					{
						const bool desk = seat == "desk" || (seat.isEmpty() && plan.bridgeStyle == "desks");
						if(stationType != "command" && wall(at + f * ((desk ? 3.4 : 3.61) + 1.0)))
							score += 2;
						if((stationType == "helm" || stationType == "navigator" || stationType == "command") && yi == 0)
							score += 1.5;
					}
					else
					{
						if(wall(at + side * (1.65 + 1.0)) || wall(at - side * (1.65 + 1.0)))
							score += 2;
						if(wall(at - f * (3.3 + 1.0)) || wall(at + f * (3.3 + 1.0)))
							score += 1;
					}
					// Stations nearer the middle of the room, berths tidily in its corners.
					const QPointF off = at - centre;
					const double dist = sqrt(QPointF::dotProduct(off, off));
					score += kind == 0 ? -dist * 0.01 : dist * 0.005;
					if(!found || score > bestScore + 1e-9)
					{
						found = true;
						bestScore = score;
						out.deck = deck;
						out.cells = CellsOf(plan, at);
						out.yaw = yaw;
					}
				}
	}
	return found;
}

PlanToolResult AddStations(TriPlan& plan, const QJsonObject& args)
{
	if(plan.IsBuilding())
		throw Fail{ "Buildings have no crew stations." };
	const QString type = StrArg(args, "type");
	if(!kStationTypes.contains(type))
		throw Fail{ QString("type is one of %1.").arg(kStationTypes.join(", ")) };
	const int room = RoomArg(plan, args, "room");
	const int count = qBound(1, IntArg(args, "count", 1), 50);
	const QString seat = StrArg(args, "seat");
	if(!seat.isEmpty() && seat != "chair" && seat != "desk")
		throw Fail{ "seat is chair or desk." };
	const int module = IntArg(args, "module", 0);
	int placed = 0;
	QStringList where;
	PlanToolResult res;
	for(int i = 0; i < count; ++i)
	{
		Placed p;
		if(!FindSpot(plan, room, 0, type, seat, p))
			break;
		PlanStation st;
		st.deck = p.deck;
		st.x = qRound(p.cells.x() * 16) / 16.0;
		st.y = qRound(p.cells.y() * 16) / 16.0;
		st.yaw = p.yaw;
		st.type = type;
		st.seat = seat;
		st.module = type == "firecontrol" || type == "engineer" ? module : 0;
		plan.stations << st;
		++placed;
		res.deck = p.deck;
		where << QString("%1,%2 facing %3").arg(st.x).arg(st.y).arg(p.yaw == 0 ? "fore" : p.yaw == 90 ? "port" : p.yaw == 180 ? "aft" : "starboard");
	}
	if(!placed)
		throw Fail{ QString("There is no free spot for a %1 station in %2: a station and its seat need about 4 by 6 ft of "
							"floor clear of doors, ladders and other items. Make the room bigger.").arg(type, RoomName(plan, room)) };
	res.changed = true;
	res.text = QString("Placed %1 %2 station%3 in %4 (deck %5): %6.").arg(placed).arg(type).arg(placed == 1 ? "" : "s")
			.arg(RoomName(plan, room)).arg(res.deck + 1).arg(where.join("; "));
	if(placed < count)
		res.text += QString(" Only %1 of the %2 fit.").arg(placed).arg(count);
	return res;
}

PlanToolResult AddBerths(TriPlan& plan, const QJsonObject& args)
{
	if(plan.IsBuilding())
		throw Fail{ "Buildings have no berths." };
	const QString kind = StrArg(args, "kind");
	if(!kBerthKinds.contains(kind))
		throw Fail{ QString("kind is one of %1.").arg(kBerthKinds.join(", ")) };
	const int room = RoomArg(plan, args, "room");
	const int count = qBound(1, IntArg(args, "count", 1), 200);
	int placed = 0;
	PlanToolResult res;
	for(int i = 0; i < count; ++i)
	{
		Placed p;
		if(!FindSpot(plan, room, 1, QString(), QString(), p))
			break;
		PlanBerth b;
		b.deck = p.deck;
		b.x = qRound(p.cells.x() * 16) / 16.0;
		b.y = qRound(p.cells.y() * 16) / 16.0;
		b.yaw = p.yaw;
		b.kind = kind;
		plan.berths << b;
		++placed;
		res.deck = p.deck;
	}
	if(!placed)
		throw Fail{ QString("There is no free spot for a berth in %1: a berth needs about 3.5 by 7 ft of floor clear of "
							"doors, ladders and other items. Make the room bigger.").arg(RoomName(plan, room)) };
	res.changed = true;
	res.text = QString("Placed %1 %2 berth%3 in %4.").arg(placed).arg(kind).arg(placed == 1 ? "" : "s").arg(RoomName(plan, room));
	if(placed < count)
		res.text += QString(" Only %1 of the %2 fit; make the room bigger or use another room for the rest.").arg(placed).arg(count);
	if(plan.autoBerths)
		res.text += " The build still adds the template's other berths (autoBerths is on).";
	return res;
}

// Takes the items 'match' picks out of a list; returns how many.
template<typename T>
int Sweep(QList<T>& list, const std::function<bool(const T&)>& match)
{
	int removed = 0;
	for(int i = list.size() - 1; i >= 0; --i)
		if(match(list.at(i)))
		{
			list.removeAt(i);
			++removed;
		}
	return removed;
}

PlanToolResult RemoveItems(TriPlan& plan, const QJsonObject& args)
{
	const QString what = StrArg(args, "what");
	const int room = Has(args, "room") ? RoomArg(plan, args, "room") : -2;
	const int deck = Has(args, "deck") ? DeckArg(plan, args) : -1;
	auto inRoom = [&](int r) { return room == -2 || r == room; };
	int removed = 0;
	if(what == "doors")
		removed = Sweep<PlanDoor>(plan.doors, [&](const PlanDoor& d) {
			return (deck < 0 || d.deck == deck) && (inRoom(plan.Room(TriRef(d.deck, d.cx, d.cy, d.side)))
					|| inRoom(plan.Room(plan.AcrossCellEdge(d.deck, d.cx, d.cy, d.side))));
		});
	else if(what == "windows")
		removed = Sweep<PlanDoor>(plan.windows, [&](const PlanDoor& w) {
			const TriRef t(w.deck, w.cx, w.cy, w.side);
			return (deck < 0 || w.deck == deck) && (inRoom(plan.Room(t)) || inRoom(plan.Room(plan.Across(t, w.edge))));
		});
	else if(what == "ladders")
		removed = Sweep<PlanLadder>(plan.ladders, [&](const PlanLadder& l) {
			return (deck < 0 || l.deck == deck || l.deck + 1 == deck) && (inRoom(plan.CellRoom(l.deck, l.cx, l.cy))
					|| inRoom(plan.CellRoom(l.deck + 1, l.cx, l.cy)));
		});
	else if(what == "hatches")
		removed = Sweep<PlanHatch>(plan.hatches, [&](const PlanHatch& h) {
			return (deck < 0 || h.deck == deck || h.deck + 1 == deck) && (inRoom(plan.CellRoom(h.deck, h.cx, h.cy))
					|| inRoom(plan.CellRoom(h.deck + 1, h.cx, h.cy)));
		});
	else if(what == "stairs")
		removed = Sweep<PlanStair>(plan.stairs, [&](const PlanStair& s) {
			int x, y;
			plan.StairCell(s, plan.StairCells(s), x, y);
			return (deck < 0 || s.deck == deck || s.deck + 1 == deck) && (inRoom(plan.CellRoom(s.deck, s.cx, s.cy))
					|| inRoom(plan.CellRoom(s.deck + 1, x, y)));
		});
	else if(what == "stations")
		removed = Sweep<PlanStation>(plan.stations, [&](const PlanStation& s) {
			return (deck < 0 || s.deck == deck) && inRoom(plan.RoomAtPoint(s.deck, s.x, s.y));
		});
	else if(what == "berths")
		removed = Sweep<PlanBerth>(plan.berths, [&](const PlanBerth& b) {
			return (deck < 0 || b.deck == deck) && inRoom(plan.RoomAtPoint(b.deck, b.x, b.y));
		});
	else if(what == "pads")
		removed = Sweep<PlanPad>(plan.pads, [&](const PlanPad& p) {
			return (deck < 0 || p.deck == deck) && inRoom(plan.RoomAtPoint(p.deck, p.x, p.y));
		});
	else
		throw Fail{ "what is doors, windows, ladders, hatches, stairs, stations, berths or pads." };
	PlanToolResult res;
	res.changed = removed > 0;
	res.deck = deck;
	res.text = removed ? QString("Took away %1 %2.").arg(removed).arg(what) : QString("There were no %1 to take away there.").arg(what);
	return res;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Checking.

QString CheckPlan(const TriPlan& plan)
{
	QStringList problems, notes;
	const int n = plan.rooms.size();
	for(int r = 0; r < n; ++r)
		if(RoomDecks(plan, r).isEmpty())
			problems << QString("%1 has no cells drawn.").arg(RoomName(plan, r));

	// Who can walk where: rooms joined by doors, ladders and stairs, from the
	// doors to the outside.
	QVector<QSet<int> > links(n);
	QSet<int> entries;
	auto link = [&](int a, int b) {
		if(a < 0 || b < 0 || a == b)
			return;
		links[a].insert(b);
		links[b].insert(a);
	};
	for(const PlanDoor& d : plan.doors)
	{
		if(d.skip || !d.kind.isEmpty())
			continue;
		const int a = plan.Room(TriRef(d.deck, d.cx, d.cy, d.side));
		const int b = plan.Room(plan.AcrossCellEdge(d.deck, d.cx, d.cy, d.side));
		if(a >= 0 && b < 0)
			entries.insert(a);
		else if(b >= 0 && a < 0)
			entries.insert(b);
		else
			link(a, b);
	}
	for(const PlanLadder& l : plan.ladders)
		if(!l.skip)
			link(plan.CellRoom(l.deck, l.cx, l.cy), plan.CellRoom(l.deck + 1, l.cx, l.cy));
	for(const PlanStair& s : plan.stairs)
		if(!s.skip)
		{
			int x, y;
			plan.StairCell(s, plan.StairCells(s), x, y);
			link(plan.CellRoom(s.deck, s.cx, s.cy), plan.CellRoom(s.deck + 1, x, y));
		}
	// A room open to the deck below is part of the room under it.
	for(int r = 0; r < n; ++r)
		if(IsOpenToDeckBelowType(plan.rooms.at(r).type))
			for(int d : RoomDecks(plan, r))
				if(d > 0)
					for(int cy = 0; cy < plan.Depth(); ++cy)
						for(int cx = 0; cx < plan.Width(); ++cx)
							if(plan.CellRoom(d, cx, cy) == r)
								link(r, plan.CellRoom(d - 1, cx, cy));
	if(n && entries.isEmpty())
		problems << "No door to the outside: nobody can get in. Add one with add_door to \"outside\".";
	else if(n)
	{
		QSet<int> reached = entries;
		QList<int> queue = entries.values();
		while(!queue.isEmpty())
		{
			const int r = queue.takeFirst();
			for(int o : links.at(r))
				if(!reached.contains(o))
				{
					reached.insert(o);
					queue << o;
				}
		}
		QStringList cut;
		for(int r = 0; r < n; ++r)
			if(!reached.contains(r) && !RoomDecks(plan, r).isEmpty() && !IsOpenToDeckBelowType(plan.rooms.at(r).type))
				cut << RoomName(plan, r);
		if(!cut.isEmpty())
			problems << QString("Nobody can walk to %1 from a door to the outside: add doors, ladders or stairs (a turbolift "
								"needs a door on each deck it stops at).").arg(cut.join(", "));
	}
	for(const QString& p : plan.LiftProblems())
		problems << p;
	for(const QString& w : HandBerthWarnings(plan))
		problems << w;
	if(plan.IsBuilding())
		for(const QString& s : plan.OutsideSiteText())
			problems << s;

	if(!plan.IsBuilding())
	{
		bool bridge = false;
		for(const PlanRoom& r : plan.rooms)
			if(r.type.compare("Bridge", Qt::CaseInsensitive) == 0 || r.type.compare("Cockpit", Qt::CaseInsensitive) == 0)
				bridge = true;
		if(!bridge && n)
			notes << "There is no Bridge (or Cockpit): the build puts the helm wherever it finds room.";
	}
	int hull = 0;
	for(int d = 0; d < plan.Decks(); ++d)
		for(int cy = 0; cy < plan.Depth(); ++cy)
			for(int cx = 0; cx < plan.Width(); ++cx)
				if(plan.Hull(TriRef(d, cx, cy, 0)))
					++hull;
	if(!hull)
		problems << "No hull is drawn yet.";

	QStringList out;
	if(problems.isEmpty())
		out << "No problems found in the plan.";
	else
	{
		out << QString("%1 problem(s):").arg(problems.size());
		for(const QString& p : problems)
			out << "- " + p;
	}
	for(const QString& note : notes)
		out << "Note: " + note;
	out << "Build to have the engine's own checks and the berth check (build, then expected_stats).";
	return out.join("\n");
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The plan as a whole.

PlanToolResult NewPlan(TriPlan& plan, const QJsonObject& args)
{
	const QString kind = StrArg(args, "kind", "ship");
	TriPlan fresh;
	const QString tmpl = StrArg(args, "template");
	if(kind == "building")
	{
		const BuildingTemplate* t = tmpl.isEmpty() ? FindBuildingTemplate("office_block") : FindBuildingTemplate(tmpl);
		if(!t)
			throw Fail{ QString("There is no building template \"%1\"; list_choices building_templates has them.").arg(tmpl) };
		fresh.MakeBuilding(t->key, true);
		fresh.name = t->label;
		fresh.Resize(qBound(8, int(ceil(t->footprintX / fresh.cellFeet)) + 2, int(TriPlan::kMaxCells)),
					 qBound(8, int(ceil(t->footprintY / fresh.cellFeet)) + 2, int(TriPlan::kMaxCells)));
		while(fresh.Decks() < qBound(1, t->floors, 20))
			fresh.AddDeck();
	}
	else if(kind == "fighter")
	{
		fresh.name = "New fighter";
		fresh.MakeFighter(true);
		fresh.Resize(24, 12);
		while(fresh.Decks() < 3)
			fresh.AddDeck();
	}
	else if(kind == "ship")
	{
		fresh.name = "New ship";
		if(!tmpl.isEmpty())
		{
			if(!FindTemplate(tmpl))
				throw Fail{ QString("There is no ship template \"%1\"; list_choices ship_templates has them.").arg(tmpl) };
			fresh.templateKey = tmpl;
		}
	}
	else
		throw Fail{ "kind is ship, building or fighter." };
	if(Has(args, "name"))
		fresh.name = StrArg(args, "name");
	if(Has(args, "cellFeet"))
		fresh.cellFeet = qBound(0.5, NumArg(args, "cellFeet", fresh.cellFeet), 50.0);
	if(Has(args, "deckFeet"))
		fresh.deckFeet = qBound(3.0, NumArg(args, "deckFeet", fresh.deckFeet), 100.0);
	if(Has(args, "sizeX") || Has(args, "sizeY"))
		fresh.Resize(IntArg(args, "sizeX", fresh.Width()), IntArg(args, "sizeY", fresh.Depth()));
	if(Has(args, "decks"))
	{
		const int decks = qBound(1, IntArg(args, "decks", fresh.Decks()), 40);
		while(fresh.Decks() < decks)
			fresh.AddDeck();
		while(fresh.Decks() > decks)
			fresh.RemoveDeck(fresh.Decks() - 1);
	}
	plan = fresh;
	PlanToolResult res;
	res.changed = res.hullChanged = res.newPlan = true;
	res.deck = 0;
	res.text = "Started a new plan.\n" + PlanSummary(plan);
	return res;
}

PlanToolResult SetSettings(TriPlan& plan, const QJsonObject& args)
{
	QStringList done;
	bool hull = false;
	for(QJsonObject::const_iterator i = args.constBegin(); i != args.constEnd(); ++i)
	{
		const QString key = i.key();
		if(key == "name")
			plan.name = StrArg(args, "name", plan.name);
		else if(key == "template")
		{
			const QString t = StrArg(args, "template");
			if(plan.IsBuilding())
				throw Fail{ "A building's template is chosen with new_plan." };
			if(!FindTemplate(t))
				throw Fail{ QString("There is no ship template \"%1\"; list_choices ship_templates has them.").arg(t) };
			plan.templateKey = t;
		}
		else if(key == "ftlDrive")
		{
			const QString d = StrArg(args, "ftlDrive");
			bool known = false;
			for(const QPair<QString, QString>& c : FtlDriveChoices())
				known = known || c.first == d;
			if(!known)
				throw Fail{ QString("There is no FTL drive \"%1\"; list_choices ftl_drives has them.").arg(d) };
			plan.ftlDrive = d;
		}
		else if(key == "station")
			plan.station = BoolArg(args, "station", plan.station);
		else if(key == "cellFeet")
		{
			plan.cellFeet = qBound(0.5, NumArg(args, "cellFeet", plan.cellFeet), 50.0);
			hull = true;
		}
		else if(key == "deckFeet")
		{
			plan.deckFeet = qBound(3.0, NumArg(args, "deckFeet", plan.deckFeet), 100.0);
			hull = true;
		}
		else if(key == "hullStyle")
		{
			const QString s = StrArg(args, "hullStyle");
			if(s != "smooth" && s != "angular" && s != "classic")
				throw Fail{ "hullStyle is smooth, angular or classic." };
			plan.hullStyle = s;
			hull = true;
		}
		else if(key == "smoothing")
		{
			plan.smoothing = qBound(0, IntArg(args, "smoothing", plan.smoothing), 10);
			hull = true;
		}
		else if(key == "slopedDecks")
		{
			plan.slopedDecks = BoolArg(args, "slopedDecks", plan.slopedDecks);
			hull = true;
		}
		else if(key == "autoStations")
			plan.autoStations = BoolArg(args, "autoStations", plan.autoStations);
		else if(key == "autoBerths")
			plan.autoBerths = BoolArg(args, "autoBerths", plan.autoBerths);
		else if(key == "autoPads")
			plan.autoPads = BoolArg(args, "autoPads", plan.autoPads);
		else if(key == "autoPaths")
			plan.autoPaths = BoolArg(args, "autoPaths", plan.autoPaths);
		else if(key == "bridgeStyle")
		{
			const QString s = StrArg(args, "bridgeStyle");
			if(s != "hazeron" && s != "desks")
				throw Fail{ "bridgeStyle is hazeron or desks." };
			plan.bridgeStyle = s;
		}
		else if(key == "hullDoorExit")
		{
			const QString s = StrArg(args, "hullDoorExit");
			if(s != "auto" && s != "ladder" && s != "none")
				throw Fail{ "hullDoorExit is auto, ladder or none." };
			plan.hullDoorExit = s;
		}
		else if(key == "leaveOut")
		{
			QStringList out;
			for(const QJsonValue& v : i.value().toArray())
			{
				const QString s = v.toString();
				if(s != "shield" && s != "firecontrol" && s != "transporter")
					throw Fail{ "leaveOut takes shield, firecontrol and transporter." };
				out << s;
			}
			plan.leaveOut = out;
		}
		else if(key == "statsQuality")
			plan.statsQuality = qBound(1, IntArg(args, "statsQuality", plan.statsQuality), 255);
		else if(key == "systemVolumes")
		{
			if(plan.IsBuilding())
				throw Fail{ "System volumes are for ships; a building's volumes come from its template." };
			const QJsonObject v = i.value().toObject();
			for(QJsonObject::const_iterator j = v.constBegin(); j != v.constEnd(); ++j)
			{
				const HandModuleKey* found = 0;
				for(const HandModuleKey& k : HandModuleKeys())
					if(j.key() == k.key)
						found = &k;
				if(!found)
					throw Fail{ QString("There is no system \"%1\"; list_choices system_volumes has them.").arg(j.key()) };
				plan.handModules.*found->field = quint32(qBound(0.0, j.value().toDouble(), 1e7));
			}
		}
		else
			throw Fail{ QString("There is no setting \"%1\".").arg(key) };
		done << key;
	}
	if(done.isEmpty())
		throw Fail{ "No settings were given." };
	PlanToolResult res;
	res.changed = true;
	res.hullChanged = hull;
	res.text = "Changed " + done.join(", ") + ".";
	return res;
}

PlanToolResult DrawHull(TriPlan& plan, const QJsonObject& args)
{
	const int deck = DeckArg(plan, args);
	const int toDeck = Has(args, "toDeck") ? DeckArg(plan, args, "toDeck") : deck;
	if(toDeck < deck)
		throw Fail{ "toDeck has to be at or above deck." };
	const CellRect r = RectArg(plan, args);
	const QString shape = StrArg(args, "shape", "rect");
	if(shape != "rect" && shape != "ellipse" && shape != "wedge_fore" && shape != "wedge_aft")
		throw Fail{ "shape is rect, ellipse, wedge_fore or wedge_aft." };
	const bool erase = BoolArg(args, "erase", false);
	const double w = r.x1 - r.x0, h = r.y1 - r.y0;
	const double mx = (r.x0 + r.x1) / 2.0, my = (r.y0 + r.y1) / 2.0;
	auto inside = [&](const QPointF& c) {
		if(shape == "ellipse")
		{
			const double dx = (c.x() - mx) / (w / 2.0), dy = (c.y() - my) / (h / 2.0);
			return dx * dx + dy * dy <= 1.0;
		}
		if(shape == "wedge_fore" || shape == "wedge_aft")
		{
			// Full width at one end, a point at the other.
			const double along = shape == "wedge_fore" ? (r.x1 - c.x()) / w : (c.x() - r.x0) / w;
			return fabs(c.y() - my) <= along * h / 2.0;
		}
		return true;
	};
	int count = 0;
	for(int d = deck; d <= toDeck; ++d)
		for(int cy = r.y0; cy < r.y1; ++cy)
			for(int cx = r.x0; cx < r.x1; ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const TriRef t(d, cx, cy, k);
					if(!inside(CellsOf(plan, plan.TriCentre(t))))
						continue;
					if(erase)
					{
						if(!plan.Hull(t))
							continue;
						plan.SetRoom(t, -1);
						plan.SetHull(t, false);
					}
					else
					{
						if(plan.Hull(t))
							continue;
						plan.SetHull(t, true);
					}
					++count;
				}
	if(erase)
		plan.Tidy();
	PlanToolResult res;
	res.changed = res.hullChanged = count > 0;
	res.deck = deck;
	res.text = QString("%1 %2 triangles (%3 cells) of hull on deck%4 %5, %6 %7.").arg(erase ? "Took away" : "Drew")
			.arg(count).arg(count / 4.0).arg(toDeck > deck ? "s" : "")
			.arg(toDeck > deck ? QString("%1-%2").arg(deck + 1).arg(toDeck + 1) : QString::number(deck + 1))
			.arg(shape).arg(RectText(r));
	if(!count)
		res.text += erase ? " There was no hull there." : " It was all hull already.";
	return res;
}

PlanToolResult AddRoom(TriPlan& plan, const QJsonObject& args, bool extend)
{
	int room;
	const int deck = DeckArg(plan, args);
	const CellRect r = RectArg(plan, args);
	const bool replace = BoolArg(args, "replace", false);
	PlanToolResult res;
	if(extend)
	{
		room = RoomArg(plan, args, "room");
		PaintRoom(plan, room, deck, r, replace);
		res.text = QString("Added cells %1 on deck %2 to %3.").arg(RectText(r)).arg(deck + 1).arg(RoomName(plan, room));
	}
	else
	{
		const QString type = RoomTypeArg(plan, StrArg(args, "type"));
		QString label = StrArg(args, "label");
		if(label.isEmpty())
			label = NewLabel(plan, type);
		for(const PlanRoom& o : plan.rooms)
			if(o.label.compare(label, Qt::CaseInsensitive) == 0)
				throw Fail{ QString("There is a room called \"%1\" already; every room needs its own name.").arg(label) };
		const int toDeck = Has(args, "toDeck") ? DeckArg(plan, args, "toDeck") : deck;
		if(toDeck < deck)
			throw Fail{ "toDeck has to be at or above deck." };
		if(toDeck > deck && !IsLiftRoomType(type))
			throw Fail{ "Only a turbolift spans decks. For a tall room, put an \"Open to Deck Below\" room over it on the "
						"deck above." };
		PlanRoom pr;
		pr.type = type;
		pr.label = label;
		pr.colour = RoomColour(plan.rooms.size());
		room = plan.rooms.size();
		plan.rooms << pr;
		for(int d = deck; d <= toDeck; ++d)
			PaintRoom(plan, room, d, r, replace);
		res.text = QString("Added %1 [%2] on deck%3 %4, cells %5 (%6 x %7 ft).").arg(label, type).arg(toDeck > deck ? "s" : "")
				.arg(toDeck > deck ? QString("%1-%2").arg(deck + 1).arg(toDeck + 1) : QString::number(deck + 1))
				.arg(RectText(r)).arg((r.x1 - r.x0) * plan.cellFeet).arg((r.y1 - r.y0) * plan.cellFeet);
		if(IsLiftRoomType(type))
			res.text += " A turbolift needs a door on each deck it stops at.";
	}
	// Rooms left with no cells by 'replace'.
	QStringList emptied;
	for(int i = 0; i < plan.rooms.size(); ++i)
		if(i != room && RoomDecks(plan, i).isEmpty())
			emptied << RoomName(plan, i);
	if(!emptied.isEmpty())
		res.text += QString(" %1 now ha%2 no cells; remove_room takes it away.").arg(emptied.join(", ")).arg(emptied.size() == 1 ? "s" : "ve");
	plan.Tidy();
	res.changed = true;
	res.hullChanged = true;
	res.deck = deck;
	return res;
}

PlanToolResult EditRoom(TriPlan& plan, const QJsonObject& args)
{
	const int room = RoomArg(plan, args, "room");
	QStringList done;
	if(Has(args, "label"))
	{
		const QString label = StrArg(args, "label");
		if(label.isEmpty())
			throw Fail{ "A room needs a name." };
		for(int i = 0; i < plan.rooms.size(); ++i)
			if(i != room && plan.rooms.at(i).label.compare(label, Qt::CaseInsensitive) == 0)
				throw Fail{ QString("There is a room called \"%1\" already.").arg(label) };
		plan.rooms[room].label = label;
		done << "renamed it " + label;
	}
	if(Has(args, "type"))
	{
		plan.rooms[room].type = RoomTypeArg(plan, StrArg(args, "type"));
		done << "made it a " + plan.rooms.at(room).type;
	}
	if(Has(args, "access"))
	{
		int bits = 0;
		for(const QJsonValue& v : args.value("access").toArray())
		{
			const QString s = v.toString();
			if(s == "officers") bits |= 1;
			else if(s == "crew") bits |= 2;
			else if(s == "passengers") bits |= 4;
			else if(s == "troops") bits |= 8;
			else
				throw Fail{ "access takes officers, crew, passengers and troops." };
		}
		plan.rooms[room].access = bits ? bits : 15;
		done << "set who may be in it";
	}
	if(done.isEmpty())
		throw Fail{ "Nothing to change: give label, type or access." };
	PlanToolResult res;
	res.changed = true;
	res.text = QString("%1: %2.").arg(StrArg(args, "room"), done.join(", "));
	return res;
}

PlanToolResult RemoveRoom(TriPlan& plan, const QJsonObject& args)
{
	const int room = RoomArg(plan, args, "room");
	const QString name = RoomName(plan, room);
	const QList<int> decks = RoomDecks(plan, room);
	// What stood in it goes with it.
	for(int i = plan.stations.size() - 1; i >= 0; --i)
		if(plan.RoomAtPoint(plan.stations.at(i).deck, plan.stations.at(i).x, plan.stations.at(i).y) == room)
			plan.stations.removeAt(i);
	for(int i = plan.berths.size() - 1; i >= 0; --i)
		if(plan.RoomAtPoint(plan.berths.at(i).deck, plan.berths.at(i).x, plan.berths.at(i).y) == room)
			plan.berths.removeAt(i);
	for(int i = plan.pads.size() - 1; i >= 0; --i)
		if(plan.RoomAtPoint(plan.pads.at(i).deck, plan.pads.at(i).x, plan.pads.at(i).y) == room)
			plan.pads.removeAt(i);
	DeleteRoom(plan, room);
	plan.Tidy();
	PlanToolResult res;
	res.changed = true;
	res.hullChanged = true;
	res.deck = decks.isEmpty() ? -1 : decks.first();
	res.text = QString("Took %1 away; its cells are hull with no room now.").arg(name);
	return res;
}

PlanToolResult GetJson(const TriPlan& plan)
{
	QTemporaryDir dir;
	if(!dir.isValid())
		throw Fail{ "Could not make a temporary folder." };
	const QString file = dir.filePath("plan.json");
	QString error;
	if(!plan.Save(file, error))
		throw Fail{ error };
	QFile f(file);
	if(!f.open(QIODevice::ReadOnly))
		throw Fail{ "Could not read the plan back." };
	PlanToolResult res;
	res.text = QString::fromUtf8(f.readAll());
	return res;
}

PlanToolResult SetJson(TriPlan& plan, const QJsonObject& args)
{
	QByteArray data;
	const QJsonValue v = args.value("plan");
	if(v.isObject())
		data = QJsonDocument(v.toObject()).toJson();
	else if(v.isString())
		data = v.toString().toUtf8();
	else
		throw Fail{ "plan (the plan as JSON) is needed." };
	QTemporaryDir dir;
	if(!dir.isValid())
		throw Fail{ "Could not make a temporary folder." };
	const QString file = dir.filePath("plan.json");
	QFile f(file);
	if(!f.open(QIODevice::WriteOnly) || f.write(data) != data.size())
		throw Fail{ "Could not write the plan to a temporary file." };
	f.close();
	TriPlan loaded;
	QString error;
	if(!loaded.Load(file, error))
		throw Fail{ "The plan could not be read: " + error };
	plan = loaded;
	PlanToolResult res;
	res.changed = res.hullChanged = res.newPlan = true;
	res.deck = 0;
	res.text = "Replaced the plan.\n" + PlanSummary(plan);
	return res;
}

}

PlanToolResult RunPlanTool(TriPlan& plan, const QString& name, const QJsonObject& args)
{
	TriPlan before = plan;
	PlanToolResult res;
	try
	{
		if(name == "get_plan")
			res.text = PlanSummary(plan);
		else if(name == "show_deck")
		{
			const int deck = DeckArg(plan, args);
			res.text = DeckMap(plan, deck, BoolArg(args, "full", false));
			res.deck = deck;
		}
		else if(name == "list_choices")
			res.text = Choices(plan, StrArg(args, "what"));
		else if(name == "new_plan")
			res = NewPlan(plan, args);
		else if(name == "set_settings")
			res = SetSettings(plan, args);
		else if(name == "resize_grid")
		{
			const int w = IntArg(args, "sizeX", plan.Width()), d = IntArg(args, "sizeY", plan.Depth());
			if(w < 2 || d < 2 || w > TriPlan::kMaxCells || d > TriPlan::kMaxCells)
				throw Fail{ QString("The grid can be 2 to %1 cells each way.").arg(int(TriPlan::kMaxCells)) };
			const int ox = (w - plan.Width()) / 2, oy = (d - plan.Depth()) / 2;
			plan.Resize(w, d);
			plan.Tidy();
			res.changed = res.hullChanged = res.newPlan = true;
			res.text = QString("The grid is %1 x %2 cells now. What was drawn moved by %3 cells along x and %4 along y.")
					.arg(plan.Width()).arg(plan.Depth()).arg(ox).arg(oy);
		}
		else if(name == "add_deck")
		{
			const bool below = BoolArg(args, "below", false);
			plan.AddDeck(below);
			res.changed = true;
			res.deck = below ? 0 : plan.Decks() - 1;
			res.text = QString("Added deck %1%2; the plan has %3 decks.").arg(res.deck + 1)
					.arg(below ? " at the bottom (the others moved up one)" : " on top").arg(plan.Decks());
		}
		else if(name == "remove_deck")
		{
			const int deck = DeckArg(plan, args);
			if(plan.Decks() < 2)
				throw Fail{ "The plan needs at least one deck." };
			plan.RemoveDeck(deck);
			plan.Tidy();
			res.changed = res.hullChanged = true;
			res.deck = qMin(deck, plan.Decks() - 1);
			res.text = QString("Took deck %1 away; the plan has %2 decks.").arg(deck + 1).arg(plan.Decks());
		}
		else if(name == "copy_deck")
		{
			const int deck = DeckArg(plan, args);
			const int times = qBound(1, IntArg(args, "times", 1), 20);
			const int rooms = plan.CopyDeck(deck, times, BoolArg(args, "withRooms", false));
			res.changed = res.hullChanged = true;
			res.deck = deck + 1;
			res.text = QString("Put %1 cop%2 of deck %3 on top of it (%4 rooms added); the plan has %5 decks.").arg(times)
					.arg(times == 1 ? "y" : "ies").arg(deck + 1).arg(rooms).arg(plan.Decks());
		}
		else if(name == "draw_hull")
			res = DrawHull(plan, args);
		else if(name == "add_room")
			res = AddRoom(plan, args, false);
		else if(name == "extend_room")
			res = AddRoom(plan, args, true);
		else if(name == "edit_room")
			res = EditRoom(plan, args);
		else if(name == "remove_room")
			res = RemoveRoom(plan, args);
		else if(name == "add_door")
			res = AddDoor(plan, args);
		else if(name == "add_window")
			res = AddWindow(plan, args);
		else if(name == "connect_decks")
			res = ConnectDecks(plan, args);
		else if(name == "add_station")
			res = AddStations(plan, args);
		else if(name == "add_berths")
			res = AddBerths(plan, args);
		else if(name == "remove_items")
			res = RemoveItems(plan, args);
		else if(name == "check_plan")
			res.text = CheckPlan(plan);
		else if(name == "get_plan_json")
			res = GetJson(plan);
		else if(name == "set_plan_json")
			res = SetJson(plan, args);
		else
			throw Fail{ QString("There is no tool \"%1\".").arg(name) };
	}
	catch(const Fail& f)
	{
		plan = before;
		res = PlanToolResult();
		res.ok = false;
		res.text = f.text;
	}
	return res;
}
