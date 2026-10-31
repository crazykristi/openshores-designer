//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - draw a ship with triangles and turn it into a blueprint.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include <QApplication>
#include <QCommandLineParser>
#include <QSurfaceFormat>
#include <QTimer>
#include <cstdio>

#include "auglobal.h"
#include "building.h"
#include "designerwindow.h"
#include "dgdata.h"
#include "logging.h"

int main(int argc, char* argv[])
{
	// The engine's obstruction-cache warning does not apply to an offline
	// tool; see logging.h. Everything else is reported as usual.
	InstallShipgenMessageHandler();

#if defined(Q_OS_WIN)
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
	QCommandLineOption openOption(QStringList() << "o" << "open", "The plan to open.", "plan.json");
	parser.addOption(openOption);
	QCommandLineOption stepOption(QStringList() << "step", "The step to show: 1 ship, 2 hull, 3 rooms, 4 paint, 5 build.", "n");
	parser.addOption(stepOption);
	QCommandLineOption deckOption(QStringList() << "deck", "The deck to show, from 1.", "n");
	parser.addOption(deckOption);
	QCommandLineOption buildOption(QStringList() << "build", "Build the blueprint straight away.");
	parser.addOption(buildOption);
	QCommandLineOption exportOption(QStringList() << "export", "Build the blueprint, save it here and quit.", "file.SoH");
	parser.addOption(exportOption);
	QCommandLineOption shotOption(QStringList() << "screenshot", "Save a picture of the window and quit.", "file.png");
	parser.addOption(shotOption);
	parser.process(app);

	DesignerWindow window;
	if(parser.isSet(openOption) && !window.OpenPlan(parser.value(openOption)))
		return 1;
	window.show();
	QTimer::singleShot(0, &window, &DesignerWindow::FrameView);

	if(parser.isSet(exportOption))
	{
		window.ShowStep(4);
		const bool accepted = window.BuildNow();
		const bool saved = window.SaveBlueprint(parser.value(exportOption), false);
		return saved ? (accepted ? 0 : 3) : 1;
	}
	if(parser.isSet(stepOption))
		window.ShowStep(parser.value(stepOption).toInt() - 1);
	if(parser.isSet(buildOption))
		QTimer::singleShot(100, &window, [&window]() { window.BuildNow(); window.FrameView(); });

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
	return app.exec();
}
