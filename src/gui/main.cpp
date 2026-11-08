//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the graphical blueprint generator.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include <QApplication>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTimer>
#include <cstdio>

#include "auglobal.h"
#include "building.h"
#include "dgdata.h"
#include "logging.h"
#include "mainwindow.h"
#include "preview.h"
#include "version.h"

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
	QApplication::setApplicationName("Shipgen");
	// What the 3D view needs, checked before any window opens.
	const QString glProblem = ChooseOpenGLFormat();
	QApplication::setApplicationVersion(SHIPGEN_VERSION);
	QApplication::setApplicationDisplayName("Shipgen - Shores of Hazeron blueprint generator");

	// The engine's report and design code reaches for the global data object.
	// It is normally created by the game client, so make a bare one here.
	static DgData g_data;
	if(!gp_AuGlobal)
		gp_AuGlobal = new AuGlobal("Shipgen", "Hazeron Freehold", AuGlobal::WebCGI, &g_data);
	InstallIndustryTable(&g_data);

	// A few options that make the tool easier to drive from a script.
	QString openFile;
	QString screenShot;
	QString previewShot;
	bool showWizard = false;
	int		wizardPage = 0;
	int		wizardKind = 0;
	QString	wizardTemplate;
	QString generateFile;
	QString generateBuildingFile;
	QString hullFile;
	QStringList modelFiles;
	double modelFit = 0.0;
	const QStringList args = app.arguments();
	for(int i = 1; i < args.size(); ++i)
	{
		if(args.at(i) == "--open" && i + 1 < args.size())
			openFile = args.at(++i);
		else if(args.at(i) == "--screenshot" && i + 1 < args.size())
			screenShot = args.at(++i);
		else if(args.at(i) == "--screenshot-preview" && i + 1 < args.size())
			previewShot = args.at(++i);
		else if(args.at(i) == "--wizard")
			showWizard = true;
		else if(args.at(i) == "--wizard-template" && i + 1 < args.size())
			wizardTemplate = args.at(++i);
		else if(args.at(i) == "--wizard-page" && i + 1 < args.size())
		{
			showWizard = true;
			wizardPage = args.at(++i).toInt();
		}
		else if(args.at(i) == "--wizard-kind" && i + 1 < args.size())
		{
			showWizard = true;
			wizardKind = args.at(++i) == "building" ? 1 : 0;
		}
		else if(args.at(i) == "--generate" && i + 1 < args.size())
			generateFile = args.at(++i);
		else if(args.at(i) == "--generate-building" && i + 1 < args.size())
			generateBuildingFile = args.at(++i);
		else if(args.at(i) == "--hull" && i + 1 < args.size())
			hullFile = args.at(++i);
		else if(args.at(i) == "--model" && i + 1 < args.size())
			modelFiles << args.at(++i);
		else if(args.at(i) == "--fit" && i + 1 < args.size())
			modelFit = args.at(++i).toDouble();
	}

	if(!glProblem.isEmpty() && screenShot.isEmpty() && previewShot.isEmpty() && generateFile.isEmpty() && generateBuildingFile.isEmpty())
	{
		std::fprintf(stderr, "warning: %s\n", qPrintable(glProblem));
		if(!WarnAboutPrerequisites(glProblem))
			return 1;
	}

	MainWindow window;
	window.show();
	if(!openFile.isEmpty())
		window.OpenFile(openFile);
	if(showWizard)
	{
		window.SetWizardTemplate(wizardTemplate);
		window.OpenWizardPage(wizardPage, wizardKind);
	}
	if(!generateFile.isEmpty())
		window.GenerateFromBriefFile(generateFile, hullFile, modelFiles);
	if(!generateBuildingFile.isEmpty())
		window.GenerateBuildingFromBriefFile(generateBuildingFile, modelFiles, modelFit);

	if(!screenShot.isEmpty() || !previewShot.isEmpty())
	{
		// Let the window settle, then save the pictures.
		QTimer::singleShot(1500, [&]() {
			if(!previewShot.isEmpty())
			{
				const QImage preview = window.PreviewImage();
				if(preview.isNull())
					fprintf(stderr, "could not grab the 3D view\n");
				else
					preview.save(previewShot);
			}
			if(!screenShot.isEmpty())
			{
				QPixmap shot = window.Screenshot();
				if(shot.isNull())
				{
					QScreen* screen = QApplication::primaryScreen();
					if(screen)
						shot = screen->grabWindow(0);
				}
				if(!shot.isNull())
					shot.save(screenShot);
				else
					fprintf(stderr, "could not grab the screen\n");
			}
			QApplication::quit();
		});
	}
	return app.exec();
}
