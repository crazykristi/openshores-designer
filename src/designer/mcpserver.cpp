//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - an MCP server, so that an AI assistant can help design;
// see mcpserver.h.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "mcpserver.h"

#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QStandardPaths>
#include <cstdio>
#include <iostream>
#include <string>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "designerwindow.h"
#include "plantools.h"
#include "version.h"

namespace
{

// The protocol's own output: a copy of the standard output taken before it
// was pointed at standard error.
int g_protocolFd = -1;

const QStringList kProtocolVersions = { "2025-06-18", "2025-03-26", "2024-11-05" };

QJsonObject TextContent(const QString& text)
{
	QJsonObject o;
	o["type"] = "text";
	o["text"] = text;
	return o;
}

QJsonObject ToolResult(const QString& text, bool error = false)
{
	QJsonObject r;
	r["content"] = QJsonArray() << TextContent(text);
	if(error)
		r["isError"] = true;
	return r;
}

QJsonObject Prop(const char* type, const QString& description)
{
	QJsonObject o;
	o["type"] = QString::fromLatin1(type);
	o["description"] = description;
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

}

void McpStdinReader::run()
{
	std::string line;
	while(std::getline(std::cin, line))
	{
		QByteArray bytes(line.data(), int(line.size()));
		if(bytes.endsWith('\r'))
			bytes.chop(1);
		if(!bytes.trimmed().isEmpty())
			emit LineRead(bytes);
	}
	emit Closed();
}

void McpServer::ClaimStdout()
{
	std::fflush(stdout);
#ifdef Q_OS_WIN
	g_protocolFd = _dup(_fileno(stdout));
	_setmode(g_protocolFd, _O_BINARY);
	_setmode(_fileno(stdin), _O_BINARY);
	_dup2(_fileno(stderr), _fileno(stdout));
#else
	g_protocolFd = dup(fileno(stdout));
	dup2(fileno(stderr), fileno(stdout));
#endif
}

QString McpServer::DefaultFolder()
{
	return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("Shipgen designs");
}

McpServer::McpServer(DesignerWindow* window, const QString& folder, QObject* parent) :
	QObject(parent), m_window(window), m_folder(QDir::cleanPath(QDir(folder).absolutePath()))
{
}

void McpServer::Start()
{
	QDir().mkpath(m_folder);
	m_reader = new McpStdinReader(this);
	connect(m_reader, &McpStdinReader::LineRead, this, &McpServer::OnLine, Qt::QueuedConnection);
	connect(m_reader, &McpStdinReader::Closed, this, &McpServer::OnClosed, Qt::QueuedConnection);
	m_reader->start();
	m_window->SetAiStatus("AI assistant: waiting");
}

void McpServer::Send(const QJsonObject& message)
{
	QByteArray bytes = QJsonDocument(message).toJson(QJsonDocument::Compact);
	bytes += '\n';
	const int fd = g_protocolFd >= 0 ? g_protocolFd : 1;
	const char* p = bytes.constData();
	qint64 left = bytes.size();
	while(left > 0)
	{
#ifdef Q_OS_WIN
		const int n = _write(fd, p, unsigned(qMin<qint64>(left, 1 << 20)));
#else
		const ssize_t n = write(fd, p, size_t(left));
#endif
		if(n <= 0)
			return;
		p += n;
		left -= n;
	}
}

void McpServer::Reply(const QJsonValue& id, const QJsonObject& result)
{
	QJsonObject m;
	m["jsonrpc"] = "2.0";
	m["id"] = id;
	m["result"] = result;
	Send(m);
}

void McpServer::ReplyError(const QJsonValue& id, int code, const QString& message)
{
	QJsonObject e;
	e["code"] = code;
	e["message"] = message;
	QJsonObject m;
	m["jsonrpc"] = "2.0";
	m["id"] = id;
	m["error"] = e;
	Send(m);
}

void McpServer::OnLine(const QByteArray& line)
{
	m_queue << line;
	HandleNext();
}

void McpServer::HandleNext()
{
	if(m_busy)
		return;
	m_busy = true;
	while(!m_queue.isEmpty())
		Handle(m_queue.takeFirst());
	m_busy = false;
	if(m_closed)
		OnClosed();
}

void McpServer::Handle(const QByteArray& line)
{
	QJsonParseError perr;
	const QJsonDocument doc = QJsonDocument::fromJson(line, &perr);
	if(!doc.isObject())
	{
		ReplyError(QJsonValue::Null, -32700, "Parse error: " + perr.errorString());
		return;
	}
	const QJsonObject msg = doc.object();
	const QString method = msg["method"].toString();
	const bool isRequest = msg.contains("id") && !method.isEmpty();
	const QJsonValue id = msg["id"];
	const QJsonObject params = msg["params"].toObject();
	if(method.isEmpty())
		return;			// A reply to something we never ask.

	if(method == "initialize")
	{
		const QString asked = params["protocolVersion"].toString();
		QJsonObject result;
		result["protocolVersion"] = kProtocolVersions.contains(asked) ? asked : kProtocolVersions.first();
		QJsonObject caps;
		caps["tools"] = QJsonObject{ { "listChanged", false } };
		result["capabilities"] = caps;
		result["serverInfo"] = QJsonObject{ { "name", "shipgen-designer" }, { "version", SHIPGEN_VERSION } };
		result["instructions"] = PlanToolsGuide() + QString("\nFiles are read and written in %1.").arg(m_folder);
		Reply(id, result);
		const QString client = params["clientInfo"].toObject()["name"].toString();
		m_window->SetAiStatus(client.isEmpty() ? QString("AI assistant connected") : QString("AI assistant connected (%1)").arg(client));
		m_initialised = true;
		return;
	}
	if(!isRequest)
		return;			// notifications/initialized, notifications/cancelled and the like.
	if(method == "ping")
		Reply(id, QJsonObject());
	else if(method == "tools/list")
		Reply(id, QJsonObject{ { "tools", ToolDefinitions() } });
	else if(method == "tools/call")
	{
		const QString name = params["name"].toString();
		Reply(id, CallTool(name, params["arguments"].toObject()));
	}
	else if(method == "resources/list")
		Reply(id, QJsonObject{ { "resources", QJsonArray() } });
	else if(method == "prompts/list")
		Reply(id, QJsonObject{ { "prompts", QJsonArray() } });
	else
		ReplyError(id, -32601, "Method not found: " + method);
}

void McpServer::OnClosed()
{
	m_closed = true;
	if(m_busy)
		return;			// After the messages still waiting.
	m_window->SetAiStatus(m_initialised ? "AI assistant disconnected" : QString());
	// With no window to work in (run headless), nothing is left to do.
	if(!m_window->isVisible() || QGuiApplication::platformName() == "offscreen")
		QApplication::quit();
}

QJsonArray McpServer::ToolDefinitions() const
{
	QJsonArray tools = PlanToolDefinitions();
	{
		tools << Tool("build", "Builds the blueprint from the plan, as the Build page does, and gives its report: the hull, "
					  "rooms, doors, stations and berths put in, warnings, and the game engine's own errors. Build after "
					  "changing the plan, and fix what it reports.");
	}
	{
		QJsonObject p;
		p["quality"] = Prop("integer", "The quality (Q, 1 to 255) of the modules and materials; the plan's statsQuality if left out.");
		tools << Tool("expected_stats", "Builds, then works out what the design is expected to do (acceleration, capacitor, "
					  "fuel, hit points, sensors), whether the berths are enough for the stations, and the raw materials it "
					  "takes to build, with the game's own report code. Only a guide: the game can change between versions, "
					  "so tell the player to check the design in the game before building it.", p);
	}
	{
		QJsonObject p;
		p["file"] = Prop("string", "A file name in the designs folder, ending in .json.");
		tools << Tool("save_plan", "Saves the plan (the designer's own file, to go on working on it).", p, { "file" });
	}
	{
		QJsonObject p;
		p["file"] = Prop("string", "A plan file in the designs folder (.json).");
		tools << Tool("open_plan", "Opens a plan in place of the one open (the designer can undo it).", p, { "file" });
	}
	{
		QJsonObject p;
		p["file"] = Prop("string", "A file name in the designs folder, ending in .SoH.");
		tools << Tool("save_blueprint", "Builds the blueprint and saves it as a .SoH file the game can load, with the build's "
					  "report. The player imports it in the game's design window.", p, { "file" });
	}
	{
		tools << Tool("list_files", "The plans and blueprints in the designs folder.");
	}
	{
		QJsonObject p;
		p["view"] = QJsonObject{ { "type", "string" }, { "enum", QJsonArray{ "plan", "3d" } },
								 { "description", "plan: one deck seen from above as drawn (the default); 3d: the shaped hull." } };
		p["deck"] = Prop("integer", "plan: the deck, from 1.");
		p["yaw"] = Prop("number", "3d: degrees round the ship (0 from the side, 90 from the fore). 30 if left out.");
		p["pitch"] = Prop("number", "3d: degrees from above (0 level, 90 straight down). 25 if left out.");
		p["blueprint"] = Prop("boolean", "3d: build first and show the blueprint with its rooms inside a see-through hull.");
		tools << Tool("picture", "A picture of the plan or the 3D view, to see what the design looks like.", p);
	}
	{
		tools << Tool("undo", "Takes back the last change to the plan (the AI's or the player's).");
	}
	return tools;
}

QString McpServer::FilePath(const QString& given, const QString& suffix, QString& error) const
{
	QString name = given.trimmed();
	if(name.isEmpty())
	{
		error = "A file name is needed.";
		return QString();
	}
	if(!name.endsWith(suffix, Qt::CaseInsensitive))
		name += suffix;
	const QString path = QDir::cleanPath(QDir::isAbsolutePath(name) ? name : QDir(m_folder).filePath(name));
	const Qt::CaseSensitivity cs =
#ifdef Q_OS_WIN
			Qt::CaseInsensitive;
#else
			Qt::CaseSensitive;
#endif
	if(!path.startsWith(m_folder + "/", cs))
	{
		error = QString("Files can only be read and written in the designs folder, %1.").arg(m_folder);
		return QString();
	}
	QDir().mkpath(QFileInfo(path).absolutePath());
	return path;
}

QJsonObject McpServer::CallTool(const QString& name, const QJsonObject& args)
{
	m_window->SetAiStatus("AI assistant: " + name);
	struct Restore
	{
		McpServer* s;
		~Restore() { s->m_window->SetAiStatus("AI assistant connected"); }
	} restore{ this };

	if(IsPlanTool(name))
	{
		TriPlan plan = m_window->Plan();
		const PlanToolResult res = RunPlanTool(plan, name, args);
		if(res.ok && res.changed)
			m_window->ApplyAiChange(plan, res.hullChanged, res.newPlan, res.deck, res.text.section('\n', 0, 0));
		else if(res.ok && res.deck >= 0 && name == "show_deck")
			m_window->SetDeck(res.deck);
		return ToolResult(res.text, !res.ok);
	}
	QString error;
	if(name == "build")
	{
		QString report;
		const bool ok = m_window->AiBuild(report);
		return ToolResult((ok ? QString("Built. The engine accepted the blueprint.\n\n")
							  : QString("Built, but the engine's report has errors: the game may not take it.\n\n")) + report);
	}
	if(name == "expected_stats")
	{
		const int q = args.contains("quality") ? args["quality"].toInt(m_window->Plan().statsQuality) : m_window->Plan().statsQuality;
		return ToolResult(m_window->AiStats(q));
	}
	if(name == "save_plan")
	{
		const QString path = FilePath(args["file"].toString(), ".json", error);
		if(path.isEmpty() || !m_window->AiSavePlan(path, error))
			return ToolResult(error, true);
		return ToolResult("Saved the plan as " + path);
	}
	if(name == "open_plan")
	{
		const QString path = FilePath(args["file"].toString(), ".json", error);
		if(path.isEmpty())
			return ToolResult(error, true);
		if(!QFileInfo::exists(path))
			return ToolResult(QString("There is no %1; list_files has the plans there are.").arg(path), true);
		if(!m_window->AiOpenPlan(path, error))
			return ToolResult(error, true);
		TriPlan plan = m_window->Plan();
		return ToolResult("Opened " + path + "\n" + RunPlanTool(plan, "get_plan", QJsonObject()).text);
	}
	if(name == "save_blueprint")
	{
		const QString path = FilePath(args["file"].toString(), ".SoH", error);
		QString report;
		if(path.isEmpty() || !m_window->AiSaveBlueprint(path, report, error))
			return ToolResult(error + (report.isEmpty() ? QString() : "\n\n" + report), true);
		return ToolResult(QString("Saved the blueprint as %1. The player loads it in the game's design window, and should check "
								  "it there before building.\n\n%2").arg(path, report));
	}
	if(name == "list_files")
	{
		QStringList lines;
		QDir dir(m_folder);
		for(const QFileInfo& f : dir.entryInfoList(QStringList() << "*.json" << "*.SoH" << "*.soh", QDir::Files, QDir::Time))
			lines << QString("%1  (%2 KB, %3)").arg(f.fileName()).arg((f.size() + 1023) / 1024)
					.arg(f.lastModified().toString("yyyy-MM-dd hh:mm"));
		return ToolResult(QString("The designs folder is %1.\n").arg(m_folder)
				+ (lines.isEmpty() ? QString("It has no plans or blueprints yet.") : lines.join("\n")));
	}
	if(name == "picture")
	{
		const bool threeD = args["view"].toString() == "3d";
		const int deck = args.contains("deck") ? args["deck"].toInt(1) - 1 : 0;
		QImage image = m_window->AiPicture(threeD, deck, float(args["yaw"].toDouble(30)), float(args["pitch"].toDouble(25)),
										   args["blueprint"].toBool(false));
		if(image.isNull())
			return ToolResult("No picture could be taken (the 3D view needs OpenGL and a window on the screen).", true);
		if(image.width() > 1280)
			image = image.scaledToWidth(1280, Qt::SmoothTransformation);
		QByteArray png;
		QBuffer buffer(&png);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, "PNG");
		QJsonObject img;
		img["type"] = "image";
		img["data"] = QString::fromLatin1(png.toBase64());
		img["mimeType"] = "image/png";
		QJsonObject r;
		r["content"] = QJsonArray() << img << TextContent(threeD ? QString("The 3D view.")
				: QString("Deck %1 seen from above: fore is to the right, port at the top.").arg(deck + 1));
		return r;
	}
	if(name == "undo")
		return m_window->AiUndo() ? ToolResult("Undid the last change.") : ToolResult("There is nothing to undo.", true);
	return ToolResult(QString("There is no tool \"%1\".").arg(name), true);
}
