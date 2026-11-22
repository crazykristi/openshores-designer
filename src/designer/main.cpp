//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - draw a ship with triangles and turn it into a blueprint.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include <QApplication>
#include <QCommandLineParser>
#include <QHash>
#include <QSurfaceFormat>
#include <QTimer>
#include <cstdio>

#include "auglobal.h"
#include "building.h"
#include "designerwindow.h"
#include "dgdata.h"
#include "logging.h"
#include "mcpserver.h"
#include "preview.h"
#include "version.h"

int main(int argc, char* argv[])
{
	// The engine keeps a ship's parts in a QHash and cuts them in its order, and
	// Qt picks a new order for every run. A fixed order makes the same plan build
	// to the same blueprint.
	qSetGlobalQHashSeed(0);

	// As an MCP server, standard output carries the protocol alone; it is
	// claimed before anything can be printed on it.
	bool mcp = false;
	for(int i = 1; i < argc; ++i)
		if(QByteArray(argv[i]) == "--mcp")
			mcp = true;
	if(mcp)
		McpServer::ClaimStdout();

	// The engine's obstruction-cache warning does not apply to an offline
	// tool; see logging.h. Everything else is reported as usual.
	InstallShipgenMessageHandler();

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
	// Ask for the desktop OpenGL 3.3 context that the preview shaders need.
	QSurfaceFormat format;
	format.setRenderableType(QSurfaceFormat::OpenGL);
	format.setVersion(3, 3);
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setDepthBufferSize(24);
	QSurfaceFormat::setDefaultFormat(format);
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

	QApplication app(argc, argv);
	QApplication::setApplicationName("Shipgen designer");
	// What the 3D view needs, checked before any window opens.
	const QString glProblem = ChooseOpenGLFormat();
	QApplication::setApplicationVersion(SHIPGEN_VERSION);
	QApplication::setApplicationDisplayName("Shipgen - draw a Shores of Hazeron ship with triangles");

	// The engine's report and design code reaches for the global data object.
	// It is normally created by the game client, so make a bare one here.
	static DgData g_data;
	if(!gp_AuGlobal)
		gp_AuGlobal = new AuGlobal("Shipgen", "Hazeron Freehold", AuGlobal::WebCGI, &g_data);
	InstallIndustryTable(&g_data);

	QCommandLineParser parser;
	parser.setApplicationDescription("Draw a ship with triangles and turn it into a Shores of Hazeron blueprint.");
	parser.addHelpOption();
	parser.addVersionOption();
	QCommandLineOption openOption(QStringList() << "o" << "open", "The plan to open, or a blueprint (.SoH) to make a plan from.", "plan.json");
	parser.addOption(openOption);
	QCommandLineOption stepOption(QStringList() << "step", "The step to show: 1 ship, 2 hull, 3 rooms, 4 crew, 5 paint, 6 build.", "n");
	parser.addOption(stepOption);
	QCommandLineOption deckOption(QStringList() << "deck", "The deck to show, from 1.", "n");
	parser.addOption(deckOption);
	QCommandLineOption buildOption(QStringList() << "build", "Build the blueprint straight away.");
	parser.addOption(buildOption);
	QCommandLineOption statsOption(QStringList() << "stats", "With --build: show the expected stats of the blueprint.");
	parser.addOption(statsOption);
	QCommandLineOption exportOption(QStringList() << "export", "Build the blueprint, save it here and quit.", "file.SoH");
	parser.addOption(exportOption);
	QCommandLineOption shotOption(QStringList() << "screenshot", "Save a picture of the window and quit.", "file.png");
	parser.addOption(shotOption);
	QCommandLineOption modelOption(QStringList() << "from-model", "Start a new plan from a 3D model (STL, 3DS, OBJ, and FBX, glTF and more when built with Assimp).", "model");
	parser.addOption(modelOption);
	QCommandLineOption lengthOption(QStringList() << "length", "With --from-model: the length in feet.", "feet");
	parser.addOption(lengthOption);
	QCommandLineOption mcpOption(QStringList() << "mcp", "Also be an MCP server on standard input and output, so that an "
			"AI assistant (Claude Desktop, Claude Code and others) can help design in this window.");
	parser.addOption(mcpOption);
	QCommandLineOption mcpFolderOption(QStringList() << "mcp-folder", "With --mcp: the only folder the AI may read and write "
			"plans and blueprints in (default: \"Shipgen designs\" in your documents).", "folder");
	parser.addOption(mcpFolderOption);
	parser.process(app);

	// Run from a script nobody is there to answer; otherwise say what is
	// missing and let the user decide.
	if(!glProblem.isEmpty() && !parser.isSet(shotOption) && !parser.isSet(exportOption))
	{
		std::fprintf(stderr, "warning: %s\n", qPrintable(glProblem));
		if(!WarnAboutPrerequisites(glProblem))
			return 1;
	}

	DesignerWindow window;
	if(parser.isSet(openOption))
	{
		// A blueprint (.SoH) becomes a new plan; a plan is opened.
		const QString file = parser.value(openOption);
		const bool scripted = parser.isSet(shotOption) || parser.isSet(exportOption);
		if(file.endsWith(".soh", Qt::CaseInsensitive) ? !window.ImportModel(file, PlanFromModelOptions(), !scripted)
				: !window.OpenPlan(file))
			return 1;
	}
	window.show();
	QTimer::singleShot(0, &window, &DesignerWindow::FrameView);
	if(parser.isSet(modelOption))
	{
		PlanFromModelOptions options;
		if(parser.isSet(lengthOption))
			options.lengthFeet = parser.value(lengthOption).toDouble();
		// Without --screenshot or --export the user is there to read what went wrong.
		const bool scripted = parser.isSet(shotOption) || parser.isSet(exportOption);
		const bool made = window.ImportModel(parser.value(modelOption), options, !scripted);
		std::printf("%s\n", made ? "plan made from the model" : "no plan from the model");
		if(!made && scripted)
			return 1;
	}

	if(mcp)
	{
		McpServer* server = new McpServer(&window, parser.isSet(mcpFolderOption) ? parser.value(mcpFolderOption)
				: McpServer::DefaultFolder(), &app);
		server->Start();
	}

	if(parser.isSet(exportOption))
	{
		window.ShowStep(5);		// Build.
		const bool accepted = window.BuildNow();
		const bool saved = window.SaveBlueprint(parser.value(exportOption), false);
		return saved ? (accepted ? 0 : 3) : 1;
	}
	if(parser.isSet(stepOption))
		window.ShowStep(parser.value(stepOption).toInt() - 1);
	if(parser.isSet(buildOption))
	{
		const bool stats = parser.isSet(statsOption);
		QTimer::singleShot(100, &window, [&window, stats]() {
			window.BuildNow();
			window.FrameView();
			if(stats)
				window.ShowStatsTab();
		});
	}

	if(parser.isSet(shotOption))
	{
		const QString file = parser.value(shotOption);
		const int deck = parser.isSet(deckOption) ? parser.value(deckOption).toInt() : 0;
		QTimer::singleShot(1500, [&window, file, deck]() {
			if(deck > 0)
				window.SetDeck(deck - 1);
			QTimer::singleShot(500, [&window, file]() {
				window.grab().save(file);
				std::printf("saved %s\n", qPrintable(file));
				QApplication::quit();
			});
		});
	}
	else if(parser.isSet(deckOption))
		window.SetDeck(parser.value(deckOption).toInt() - 1);
	// Not when run from a script (a picture or an export), nor in the tests.
	if(!parser.isSet(shotOption) && !parser.isSet(exportOption) && qEnvironmentVariable("QT_QPA_PLATFORM") != "offscreen")
		QTimer::singleShot(3000, &window, &DesignerWindow::CheckForUpdatesAtStart);
	return app.exec();
}
