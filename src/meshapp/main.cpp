//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the mesh tool.
//
// A small program that takes a model down to a given number of faces and
// shows what is left.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include <QApplication>
#include <QCommandLineParser>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTimer>
#include <cstdio>

#include "logging.h"
#include "meshwindow.h"

int main(int argc, char* argv[])
{
	// The engine's obstruction-cache warning does not apply to an offline
	// tool; see logging.h. Everything else is reported as usual.
	InstallShipgenMessageHandler();

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
	// Ask for the desktop OpenGL 3.3 context that the preview shaders need.
	// Qt's default Windows context can be older, and ANGLE would only give
	// OpenGL ES. The rest of the tool still works if this cannot be made.
	QSurfaceFormat format;
	format.setRenderableType(QSurfaceFormat::OpenGL);
	format.setVersion(3, 3);
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setDepthBufferSize(24);
	QSurfaceFormat::setDefaultFormat(format);
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#endif

	QApplication app(argc, argv);
	QApplication::setApplicationName("Shipgen mesh tool");
	QApplication::setApplicationDisplayName("Shipgen - Shores of Hazeron mesh tool");

	QCommandLineParser parser;
	parser.setApplicationDescription("Takes a 3ds, stl or obj model down to a given number of faces.");
	parser.addHelpOption();
	QCommandLineOption openOption(QStringList() << "o" << "open", "The model to open.", "file");
	parser.addOption(openOption);
	QCommandLineOption facesOption(QStringList() << "f" << "faces", "The number of faces to aim for.", "n");
	parser.addOption(facesOption);
	QCommandLineOption shotOption(QStringList() << "s" << "screenshot", "Save a picture of the window.", "file");
	parser.addOption(shotOption);
	QCommandLineOption viewOption(QStringList() << "v" << "view", "What to show: reduced, original or both.", "view");
	parser.addOption(viewOption);
	QCommandLineOption methodOption(QStringList() << "m" << "method", "How to thin it out: quadric or cluster.", "method");
	parser.addOption(methodOption);
	QCommandLineOption sizeOption(QStringList() << "z" << "size", "The size of the window, like 900x600.", "size");
	parser.addOption(sizeOption);
	parser.addPositionalArgument("file", "The model to open.");
	parser.process(app);

	MeshWindow window;
	if(parser.isSet(sizeOption))
	{
		const QStringList parts = parser.value(sizeOption).split('x');
		if(parts.size() == 2)
			window.resize(parts.at(0).toInt(), parts.at(1).toInt());
	}
	window.show();

	const QStringList positional = parser.positionalArguments();
	QString fileName;
	if(parser.isSet(openOption))
		fileName = parser.value(openOption);
	else if(!positional.isEmpty())
		fileName = positional.first();
	if(!fileName.isEmpty())
		window.OpenFile(fileName);
	if(parser.isSet(methodOption))
		window.SetMethod(parser.value(methodOption) == "cluster" ? 1 : 0);
	if(parser.isSet(viewOption))
	{
		const QString what = parser.value(viewOption);
		int view = 1;
		if(what == "original")
			view = 2;
		else if(what == "both")
			view = 3;
		window.SetView(view);
	}
	if(parser.isSet(facesOption))
		window.SimplifyTo(parser.value(facesOption).toInt());
	if(parser.isSet(shotOption))
	{
		// Let the window settle, then save a picture of it.
		QTimer::singleShot(2500, [&]() {
			QPixmap shot = window.grab();
			if(!shot.isNull())
				shot.save(parser.value(shotOption));
			else
				fprintf(stderr, "could not grab the window\n");
			QApplication::quit();
		});
	}

	return app.exec();
}
