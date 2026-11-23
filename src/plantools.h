//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - simple tools that change a triangle plan (triplan.h), for an AI
// assistant (the designer's MCP server, designer/mcpserver.h) or a script.
//
// A plan is drawn triangle by triangle, which suits a mouse but not an AI:
// it is poor at laying things out from raw coordinates. These tools work in
// whole cells and in rooms by name instead: "a Bridge 6 by 4 cells on deck 2",
// "a door between the Bridge and the Hall", "a ladder from the Hall up to the
// Crew Quarters", "four crew berths in the Crew Quarters". Each tool finds
// the edges, cells and spots itself, the way the designer's own tools place
// them, and says what it did, or why it could not.
//
// Cells are counted from 0 at the grid's aft (x) and starboard (y) corner:
// x grows towards the fore, y towards port. Decks are counted from 1 at the
// bottom, as the designer shows them.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_PLANTOOLS_H
#define SHIPGEN_PLANTOOLS_H

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

class TriPlan;

struct PlanToolResult
{
	bool	ok = true;			// False: nothing was changed, and 'text' says why.
	QString	text;				// What was done, or what was asked for.
	bool	changed = false;	// The plan was changed.
	bool	hullChanged = false;	// The hull was, so it has to be shaped again.
	bool	newPlan = false;	// A new or reloaded plan: the views are fitted to it again.
	int		deck = -1;			// The deck the change was on (from 0), to show it; -1 for none.
};

// The tools: each one's name, description and input schema (JSON schema), as
// an MCP server lists them.
QJsonArray		PlanToolDefinitions();
bool			IsPlanTool(const QString& name);

// Runs a tool on 'plan'. When it fails the plan is left as it was.
PlanToolResult	RunPlanTool(TriPlan& plan, const QString& name, const QJsonObject& args);

// How the tools are meant to be used, for the AI: the axes, the units and the
// usual order of work.
QString			PlanToolsGuide();
QString			PlanToolsDesignRules();		// What a design needs to work in the game (design_rules).

// The colour the designer draws room 'index' in, so that rooms next to each
// other can be told apart.
QColor			RoomColour(int index);

// Berths placed by hand that may be too few for the stations, when the
// automatic berths are off (empty when they are on, and for buildings).
QStringList		HandBerthWarnings(const TriPlan& plan);

#endif
