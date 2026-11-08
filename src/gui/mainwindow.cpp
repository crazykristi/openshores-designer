//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the main window.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include "version.h"
#include <QPainter>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <cstdio>

#include "aufilever.h"
#include "auscparts.h"
#include "auscdesign.h"
#include "audesign.h"
#include "aubddesign.h"
#include "building.h"
#include "generate.h"
#include "preview.h"

#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include "shipgen.h"
#include "wizard.h"

namespace {

// Works out whether this machine can give us an OpenGL context. The 3D
// preview needs one. Without it the tool still works, it just cannot show
// the model.
bool OpenGLAvailable()
{
	QOpenGLContext context;
	context.setFormat(QSurfaceFormat::defaultFormat());
	if(!context.create())
		return false;
	QOffscreenSurface surface;
	surface.setFormat(context.format());
	surface.create();
	if(!surface.isValid())
		return false;
	if(!context.makeCurrent(&surface))
		return false;
	// The preview needs OpenGL 3.3 for its shaders.
	QOpenGLFunctions* functions = context.functions();
	const char* versionText = functions ? reinterpret_cast<const char*>(functions->glGetString(GL_VERSION)) : 0;
	const bool available = versionText && QString(versionText).section(' ', 0, 0).toFloat() >= 3.3;
	context.doneCurrent();
	return available;
}

QString HumanSize(qint64 bytes)
{
	if(bytes < 1024)
		return QString("%1 bytes").arg(bytes);
	if(bytes < 1024 * 1024)
		return QString("%1 kB").arg(bytes / 1024.0, 0, 'f', 1);
	return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

} // namespace

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

MainWindow::MainWindow(QWidget* parent) :
	QMainWindow(parent)
{
	setWindowTitle(tr("Shipgen - Shores of Hazeron blueprint generator"));
	resize(1100, 720);

	CreateActions();
	CreateDocks();

	m_hasOpenGL = OpenGLAvailable();
	if(m_hasOpenGL)
	{
		m_preview = new DesignPreview(this);
		setCentralWidget(m_preview);
	}
	else
	{
		QLabel* label = new QLabel(tr("This machine has no usable OpenGL, so the model cannot be shown.\n"
				"Everything else works as usual."), this);
		label->setAlignment(Qt::AlignCenter);
		label->setWordWrap(true);
		setCentralWidget(label);
	}

	m_statusLabel = new QLabel(tr("Ready."), this);
	statusBar()->addPermanentWidget(m_statusLabel);
}

MainWindow::~MainWindow()
{
	delete m_design;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

void MainWindow::CreateActions()
{
	QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
	QAction* openAction = fileMenu->addAction(tr("&Open blueprint..."));
	openAction->setShortcut(QKeySequence::Open);
	openAction->setStatusTip(tr("Open a blueprint that was made before."));
	connect(openAction, &QAction::triggered, this, &MainWindow::OnOpen);
	QAction* saveAction = fileMenu->addAction(tr("&Save"));
	saveAction->setShortcut(QKeySequence::Save);
	saveAction->setStatusTip(tr("Save the blueprint over the file it came from."));
	connect(saveAction, &QAction::triggered, this, &MainWindow::OnSave);
	QAction* saveAsAction = fileMenu->addAction(tr("Save &as..."));
	saveAsAction->setShortcut(QKeySequence::SaveAs);
	saveAsAction->setStatusTip(tr("Save the blueprint under a new name."));
	connect(saveAsAction, &QAction::triggered, this, &MainWindow::OnSaveAs);
	fileMenu->addSeparator();
	QAction* quitAction = fileMenu->addAction(tr("&Quit"));
	quitAction->setShortcut(QKeySequence::Quit);
	connect(quitAction, &QAction::triggered, this, &QWidget::close);

	QMenu* generateMenu = menuBar()->addMenu(tr("&Generate"));
	QAction* shipAction = generateMenu->addAction(tr("New &spacecraft blueprint..."));
	shipAction->setStatusTip(tr("Go through the wizard and make a new spacecraft blueprint."));
	connect(shipAction, &QAction::triggered, this, &MainWindow::OnNewSpacecraft);
	QAction* buildingAction = generateMenu->addAction(tr("New &building blueprint..."));
	buildingAction->setStatusTip(tr("Go through the wizard and make a new building blueprint."));
	connect(buildingAction, &QAction::triggered, this, &MainWindow::OnNewBuilding);

	QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
	QAction* frameAction = viewMenu->addAction(tr("&Frame the model"));
	frameAction->setStatusTip(tr("Move the view so that the whole model is in it."));
	frameAction->setShortcut(QKeySequence(Qt::Key_F));
	connect(frameAction, &QAction::triggered, this, [this]() { if(m_preview) m_preview->FrameAll(); });
	QAction* roomsAction = viewMenu->addAction(tr("Show &rooms"));
	roomsAction->setStatusTip(tr("Whether the rooms inside the hull are drawn."));
	roomsAction->setCheckable(true);
	roomsAction->setChecked(true);
	connect(roomsAction, &QAction::toggled, this, &MainWindow::OnShowRooms);

	QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
	QAction* aboutAction = helpMenu->addAction(tr("&About"));
	aboutAction->setStatusTip(tr("What this program is and where it came from."));
	connect(aboutAction, &QAction::triggered, this, &MainWindow::OnAbout);

	QToolBar* bar = addToolBar(tr("Main"));
	bar->setObjectName("mainToolBar");
	bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	bar->addAction(openAction);
	bar->addAction(saveAction);
	bar->addSeparator();
	bar->addAction(shipAction);
	bar->addAction(buildingAction);
	bar->addSeparator();
	bar->addAction(frameAction);
}

void MainWindow::CreateDocks()
{
	// The left dock: the source files and the candidates.
	QDockWidget* leftDock = new QDockWidget(tr("Project"), this);
	leftDock->setObjectName("projectDock");
	QWidget* leftWidget = new QWidget(leftDock);
	QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);

	leftLayout->addWidget(new QLabel(tr("Source files"), leftWidget));
	m_fileList = new QListWidget(leftWidget);
	leftLayout->addWidget(m_fileList);

	leftLayout->addWidget(new QLabel(tr("Generated blueprints"), leftWidget));
	m_candidateList = new QListWidget(leftWidget);
	connect(m_candidateList, &QListWidget::currentTextChanged, this, &MainWindow::OnCandidateSelected);
	leftLayout->addWidget(m_candidateList);

	leftDock->setWidget(leftWidget);
	addDockWidget(Qt::LeftDockWidgetArea, leftDock);

	// The right dock: the validation report.
	QDockWidget* rightDock = new QDockWidget(tr("Validation"), this);
	rightDock->setObjectName("validationDock");
	m_validationText = new QPlainTextEdit(rightDock);
	m_validationText->setReadOnly(true);
	rightDock->setWidget(m_validationText);
	addDockWidget(Qt::RightDockWidgetArea, rightDock);

	// The bottom dock: what is in the blueprint.
	QDockWidget* bottomDock = new QDockWidget(tr("Contents"), this);
	bottomDock->setObjectName("contentsDock");
	m_contentsTree = new QTreeWidget(bottomDock);
	m_contentsTree->setHeaderLabels(QStringList() << tr("Part") << tr("Type") << tr("Group"));
	bottomDock->setWidget(m_contentsTree);
	addDockWidget(Qt::BottomDockWidgetArea, bottomDock);
}

void MainWindow::AddFileRow(const QString& label, const QString& fileName)
{
	QListWidgetItem* item = new QListWidgetItem(label, m_fileList);
	if(fileName.isEmpty())
		item->setText(label + tr(": (not set)"));
	else
	{
		item->setText(label + ": " + QFileInfo(fileName).fileName());
		item->setToolTip(fileName);
	}
}

void MainWindow::RefreshFileList()
{
	m_fileList->clear();
	AddFileRow(tr("Blueprint"), m_designFile);
	if(m_modelFiles.isEmpty())
		AddFileRow(tr("Ship model"), QString());
	else
	{
		for(const QString& file : m_modelFiles)
			AddFileRow(tr("Ship model"), file);
	}
	AddFileRow(tr("Existing hull"), m_hullFile);
	AddFileRow(tr("Texture"), m_textureFile);
	AddFileRow(tr("Brief"), m_briefFile);
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---|

void MainWindow::OnOpen()
{
	const QString fileName = QFileDialog::getOpenFileName(this, tr("Open a blueprint"), QString(),
			tr("Blueprints (*.SoH);;All files (*)"));
	if(fileName.isEmpty())
		return;
	OpenFile(fileName);
}

void MainWindow::OpenFile(const QString& fileName)
{
	LoadDesign(fileName);
}

void MainWindow::LoadDesign(const QString& fileName)
{
	DesignLoadReport report;
	AuDesign* design = ::LoadDesign(fileName, &report);
	if(!design)
	{
		QMessageBox::warning(this, tr("Shipgen"), tr("Could not read %1.").arg(fileName));
		return;
	}
	if(!report.notes.isEmpty())
		QMessageBox::information(this, tr("Shipgen"), report.notes.join("\n\n"));
	delete m_design;
	m_design = design;
	m_designFile = fileName;
	m_opaqueParts = report.opaque;

	setWindowTitle(tr("Shipgen - %1").arg(design->Name()));
	if(m_preview)
	{
		m_preview->SetDesign(m_design);
		m_preview->FrameAll();
	}
	RefreshContents();
	RefreshValidation();
	RefreshFileList();
	m_statusLabel->setText(tr("%1 - %2").arg(QFileInfo(fileName).fileName()).arg(HumanSize(QFileInfo(fileName).size())));
}

void MainWindow::OnSave()
{
	if(!m_design)
		return;
	if(m_designFile.isEmpty())
	{
		OnSaveAs();
		return;
	}
	if(SaveDesign(*m_design, m_designFile, m_opaqueParts))
		m_statusLabel->setText(tr("Saved %1").arg(m_designFile));
	else
		QMessageBox::warning(this, tr("Shipgen"), tr("Could not write %1.").arg(m_designFile));
}

void MainWindow::OnSaveAs()
{
	if(!m_design)
		return;
	const QString fileName = QFileDialog::getSaveFileName(this, tr("Save the blueprint"), m_designFile,
			tr("Blueprints (*.SoH)"));
	if(fileName.isEmpty())
		return;
	QString name = fileName;
	if(!name.endsWith(".SoH", Qt::CaseInsensitive))
		name += ".SoH";
	if(SaveDesign(*m_design, name, m_opaqueParts))
	{
		m_designFile = name;
		m_statusLabel->setText(tr("Saved %1").arg(name));
	}
	else
		QMessageBox::warning(this, tr("Shipgen"), tr("Could not write %1.").arg(name));
}

void MainWindow::OnAbout()
{
	QMessageBox::about(this, tr("About Shipgen"),
			SHIPGEN_ABOUT_HTML("Shipgen", "A layout generator for Shores of Hazeron blueprints.</p>"
			"<p>Give it a ship model, say what the ship is for, and it produces "
			"spacecraft and building blueprints with a complete interior: rooms, "
			"crew stations, berths and the hull volume that goes with them. "
			"Every blueprint it writes is checked with the game's own report "
			"engine."));
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---|

void MainWindow::OnShowRooms(bool show)
{
	if(m_preview)
		m_preview->SetShowRooms(show);
}

void MainWindow::OnCandidateSelected()
{
	const QString text = m_candidateList->currentItem()->text();
	for(const Candidate& c : m_candidates)
	{
		if(text.startsWith(c.strategy))
		{
			ShowCandidate(c.fileName);
			return;
		}
	}
}

void MainWindow::ShowCandidate(const QString& fileName)
{
	LoadDesign(fileName);
}

void MainWindow::RefreshContents()
{
	m_contentsTree->clear();
	if(!m_design)
		return;

	const AuScParts& parts = m_design->Parts();
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* part = it.value();
		if(!part)
			continue;
		QTreeWidgetItem* item = new QTreeWidgetItem(m_contentsTree);
		item->setText(0, QString::number(it.key()));
		item->setText(1, part->TypeName());
		item->setText(2, part->GroupName());
	}
	m_contentsTree->resizeColumnToContents(0);
	m_contentsTree->resizeColumnToContents(1);
}

void MainWindow::RefreshValidation()
{
	m_validationText->clear();
	if(!m_design)
		return;

	QStringList errors, warnings;
	AuScDesign* sc = dynamic_cast<AuScDesign*>(m_design);
	AuBdDesign* bd = dynamic_cast<AuBdDesign*>(m_design);
	if(sc)
		ValidateDesign(*sc, errors, warnings);
	else if(bd)
		ValidateBuilding(*bd, errors, warnings);

	if(errors.isEmpty() && warnings.isEmpty())
	{
		m_validationText->appendPlainText(tr("The blueprint is valid. No errors and no warnings."));
		return;
	}
	if(!errors.isEmpty())
	{
		m_validationText->appendPlainText(tr("%1 error(s):").arg(errors.size()));
		for(const QString& e : errors)
			m_validationText->appendPlainText("  " + e);
		m_validationText->appendPlainText(QString());
	}
	if(!warnings.isEmpty())
	{
		m_validationText->appendPlainText(tr("%1 warning(s):").arg(warnings.size()));
		for(const QString& w : warnings)
			m_validationText->appendPlainText("  " + w);
	}
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---|

void MainWindow::RunWizard(BlueprintKind kind)
{
	GenerationWizard wizard(kind, this);
	if(wizard.exec() != QDialog::Accepted)
		return;
	RunWizardRequest(wizard.Request());
}

void MainWindow::RunWizardRequest(const GenerationRequest& request)
{
	m_candidates.clear();
	m_candidateList->clear();

	if(request.kind == BlueprintKind::Spacecraft)
	{
		// The wizard keeps the brief up to date as you type.
		Brief brief = request.shipBrief;
		if(brief.name.isEmpty())
			brief.name = request.name;
		if(brief.name.isEmpty())
			brief.name = tr("Spacecraft");

		// The layout options are the same for every variation.
		LayoutOptions opts;
		opts.wallThickness = request.wall;
		opts.deckThickness = request.deck;

		// Generate one candidate per layout, for each variation of the shape.
		QStringList strategies = request.strategies;
		if(strategies.isEmpty())
			strategies = LayoutStrategies();

		if(!m_tempDir)
			m_tempDir = new QTemporaryDir();

		const int variations = qMax(1, request.variations);
		for(int variation = 0; variation < variations; ++variation)
		{
			// The hull.
			AuScDesign base;
			base.SetId(1);
			base.SetArchitectId(AuId(0));
			base.SetArchitectName("shipgen");
			base.SetName(variations > 1 ? QString("%1 %2").arg(brief.name).arg(variation + 1) : brief.name);
			base.SetStructure(SC_Spaceship);
			base.SetStreamline(SLINE_None);
			if(!brief.description.isEmpty())
				base.SetDescription(brief.description);

			QString error;
			if(!request.hullFile.isEmpty())
			{
				AuDesign* src = ::LoadDesign(request.hullFile);
				AuScDesign* srcSc = dynamic_cast<AuScDesign*>(src);
				if(!srcSc || !CopyHull(*srcSc, base, error))
				{
					QMessageBox::warning(this, tr("Shipgen"), error.isEmpty() ? tr("That file has no hull.") : error);
					return;
				}
				delete src;
				if(request.paint.IsSet())
				{
					QStringList notes;
					if(!PaintDesign(base, request.paint, &notes, error))
					{
						QMessageBox::warning(this, tr("Shipgen"), error);
						return;
					}
				}
			}
			else if(!request.modelFiles.isEmpty() || request.base.IsSet())
			{
				QStringList importNotes;
				ModelImportOptions modelOpts;
				modelOpts.scale = request.scale;
				modelOpts.fit = request.fit;
				modelOpts.decimate = request.decimate;
				modelOpts.shape = VaryShape(request.shape, variation);
				modelOpts.base = request.base;
				modelOpts.paint = request.paint;
				modelOpts.repair = request.repair;
				modelOpts.notes = &importNotes;
				AuPoint modelMin, modelMax;
				if(!ImportHullModel(base, request.modelFiles, modelOpts, modelMin, modelMax, error))
				{
					QMessageBox::warning(this, tr("Shipgen"), error);
					return;
				}
			}
			else
			{
				QMessageBox::warning(this, tr("Shipgen"), tr("Choose a ship model, a base shape or an "
						"existing hull first."));
				return;
			}

			HullAnalysis hull;
			if(!AnalyseHull(base, opts, hull, error))
			{
				QMessageBox::warning(this, tr("Shipgen"), error);
				return;
			}
			m_statusLabel->setText(tr("Hull is %1 x %2 x %3 feet, about %4 cubic meters.")
					.arg(qRound(hull.max.x - hull.min.x)).arg(qRound(hull.max.y - hull.min.y))
					.arg(qRound(hull.max.z - hull.min.z)).arg(qRound(hull.volumeM3)));

			for(const QString& strategy : strategies)
			{
				AuScDesign design = base;
				LayoutOptions sOpts = opts;
				sOpts.strategy = strategy;
				const LayoutResult result = GenerateLayout(design, brief, hull, sOpts);

				QStringList errors, warnings;
				ValidateDesign(design, errors, warnings);

				const QString tag = variations > 1 ? QString("%1 %2").arg(strategy).arg(variation + 1) : strategy;
				const QString fileName = QDir(m_tempDir->path()).filePath(tag + ".SoH");
				design.SaveToFile(fileName, SOH_FILE_TYPE);

				Candidate candidate;
				candidate.fileName = fileName;
				candidate.strategy = tag;
				candidate.rooms = result.roomsPlaced;
				candidate.stations = result.stationsPlaced;
				candidate.ok = errors.isEmpty();
				m_candidates << candidate;

				QListWidgetItem* item = new QListWidgetItem(m_candidateList);
				item->setText(QString("%1 - %2 rooms, %3 stations%4")
						.arg(tag).arg(result.roomsPlaced).arg(result.stationsPlaced)
						.arg(candidate.ok ? tr("") : tr(" - errors")));
				item->setToolTip(errors.join("\n"));
			}
		}
	}
	else
	{
		BuildingBrief brief = request.buildingBrief;
		if(brief.name.isEmpty())
			brief.name = request.name;
		if(brief.name.isEmpty())
			brief.name = tr("Building");

		if(!m_tempDir)
			m_tempDir = new QTemporaryDir();
		QStringList strategies = request.strategies;
		if(strategies.isEmpty())
			strategies << "grid" << "corridor";
		const int variations = qMax(1, request.variations);
		for(int variation = 0; variation < variations; ++variation)
		{
			for(const QString& strategy : strategies)
			{
				AuBdDesign design;
				design.SetId(1);
				design.SetArchitectId(AuId(0));
				design.SetArchitectName("shipgen");
				design.SetName(variations > 1 ? QString("%1 %2").arg(brief.name).arg(variation + 1) : brief.name);
				if(!request.buildingModelFiles.isEmpty() || request.base.IsSet())
				{
					// The model, or the primitive base, becomes the shell.
					ModelImportOptions modelOpts;
					modelOpts.fit = request.buildingFit;
					modelOpts.shape = VaryShape(request.buildingShape, variation);
					modelOpts.base = request.base;
					modelOpts.paint = request.paint;
					AuPoint modelMin, modelMax;
					QString importError;
					if(!ImportHullModel(design, request.buildingModelFiles, modelOpts, modelMin, modelMax, importError))
					{
						QMessageBox::warning(this, tr("Shipgen"), importError);
						return;
					}
				}
				const BuildingResult result = GenerateBuilding(design, brief, strategy);

				QStringList errors, warnings;
				ValidateBuilding(design, errors, warnings);

				const QString tag = variations > 1 ? QString("%1 %2").arg(strategy).arg(variation + 1) : strategy;
				const QString fileName = QDir(m_tempDir->path()).filePath(tag + ".SoH");
				design.SaveToFile(fileName, SOH_FILE_TYPE);

				Candidate candidate;
				candidate.fileName = fileName;
				candidate.strategy = tag;
				candidate.rooms = result.roomsPlaced;
				candidate.stations = result.doorsPlaced;
				candidate.ok = errors.isEmpty();
				m_candidates << candidate;

				QListWidgetItem* item = new QListWidgetItem(m_candidateList);
				item->setText(QString("%1 - %2 rooms%3").arg(tag).arg(result.roomsPlaced)
						.arg(candidate.ok ? tr("") : tr(" - errors")));
				item->setToolTip(errors.join("\n"));
			}
		}

		// The other buildings in the set, made from the same model.
		for(const BuildingSetEntry& entry : request.buildingSet)
		{
			BuildingBrief entryBrief;
			if(!entry.templateKey.isEmpty())
				ApplyBuildingTemplate(entryBrief, entry.templateKey);
			if(!entry.name.isEmpty())
				entryBrief.name = entry.name;
			else if(entryBrief.name.isEmpty())
				entryBrief.name = entry.templateKey;

			ModelShape shape;
			if(!entry.shapeKey.isEmpty())
			{
				ShapePreset preset;
				if(FindShapePreset(entry.shapeKey, preset))
					shape = preset.shape;
			}

			for(int variation = 0; variation < qMax(1, entry.variations); ++variation)
			{
				AuBdDesign design;
				design.SetId(1);
				design.SetArchitectId(AuId(0));
				design.SetArchitectName("shipgen");
				design.SetName(entry.variations > 1 ? QString("%1 %2").arg(entryBrief.name).arg(variation + 1)
						: entryBrief.name);
				if(!request.buildingModelFiles.isEmpty() || request.base.IsSet())
				{
					QStringList importNotes;
					ModelImportOptions modelOpts;
					modelOpts.fit = request.buildingFit;
					modelOpts.shape = VaryShape(shape, variation);
					modelOpts.base = request.base;
					modelOpts.paint = request.paint;
					modelOpts.repair = request.repair;
					modelOpts.notes = &importNotes;
					AuPoint modelMin, modelMax;
					QString importError;
					if(!ImportHullModel(design, request.buildingModelFiles, modelOpts, modelMin, modelMax, importError))
					{
						QMessageBox::warning(this, tr("Shipgen"), importError);
						return;
					}
				}
				const BuildingResult result = GenerateBuilding(design, entryBrief, entryBrief.layout);

				QStringList errors, warnings;
				ValidateBuilding(design, errors, warnings);

				const QString tag = entry.variations > 1
						? QString("%1 %2").arg(entryBrief.name).arg(variation + 1) : entryBrief.name;
				const QString fileName = QDir(m_tempDir->path()).filePath(SanitiseName(tag) + ".SoH");
				design.SaveToFile(fileName, SOH_FILE_TYPE);

				Candidate candidate;
				candidate.fileName = fileName;
				candidate.strategy = tag;
				candidate.rooms = result.roomsPlaced;
				candidate.stations = result.doorsPlaced;
				candidate.ok = errors.isEmpty();
				m_candidates << candidate;

				QListWidgetItem* item = new QListWidgetItem(m_candidateList);
				item->setText(QString("%1 - %2 rooms%3").arg(tag).arg(result.roomsPlaced)
						.arg(candidate.ok ? tr("") : tr(" - errors")));
				item->setToolTip(errors.join("\n"));
			}
		}
	}

	if(m_candidates.isEmpty())
		return;
	m_candidateList->setCurrentRow(0);
	m_statusLabel->setText(tr("%1 candidate(s) generated.").arg(m_candidates.size()));
}

QImage MainWindow::PreviewImage()
{
	if(!m_preview)
		return QImage();
	return m_preview->grabFramebuffer();
}

QPixmap MainWindow::Screenshot()
{
	if(m_wizard && m_wizard->isVisible())
		return m_wizard->grab();

	QPixmap shot = grab();
	// A QOpenGLWidget draws into its own buffer, which grab() does not always
	// copy on every platform. Taking the frame directly and putting it where
	// the widget sits keeps the 3D view in the picture.
	if(m_preview && m_preview->isVisible())
	{
		const QImage frame = m_preview->grabFramebuffer();
		if(!frame.isNull())
		{
			const qreal dpr = shot.devicePixelRatio();
			const QPoint corner = m_preview->mapTo(this, QPoint(0, 0));
			const QRect target(qRound(corner.x() * dpr), qRound(corner.y() * dpr),
					qRound(m_preview->width() * dpr), qRound(m_preview->height() * dpr));
			QPainter painter(&shot);
			painter.drawImage(target, frame);
		}
	}
	return shot;
}

void MainWindow::OpenWizard()
{
	OpenWizardPage(0);
}

void MainWindow::OpenWizardPage(int page, int kind)
{
	if(!m_wizard || m_wizardKind != kind)
	{
		if(m_wizard)
			m_wizard->close();
		m_wizardKind = kind;
		m_wizard = new GenerationWizard(kind == 1 ? BlueprintKind::Building : BlueprintKind::Spacecraft, this);
		m_wizard->setAttribute(Qt::WA_DeleteOnClose);
	}
	m_wizard->setStartId(page);
	if(!m_wizardTemplate.isEmpty())
		m_wizard->ChooseTemplate(m_wizardTemplate);
	m_wizard->restart();
	m_wizard->resize(880, 660);
	m_wizard->move(160, 80);
	m_wizard->show();
	m_wizard->raise();
	m_wizard->activateWindow();
}

void MainWindow::GenerateFromBriefFile(const QString& briefFile, const QString& hullFile, const QStringList& modelFiles)
{
	Brief brief;
	QString error;
	if(!LoadBrief(briefFile, brief, error))
	{
		// This path is used from scripts, so do not block on a message box.
		fprintf(stderr, "%s\n", qPrintable(error));
		return;
	}
	if(!brief.templateName.isEmpty())
		ApplyTemplate(brief, brief.templateName);

	GenerationRequest request;
	request.kind = BlueprintKind::Spacecraft;
	request.shipBrief = brief;
	request.name = brief.name;
	request.description = brief.description;
	request.crew = brief.crew;
	request.strategies = LayoutStrategies();
	request.hullFile = hullFile;
	request.modelFiles = modelFiles;
	m_hullFile = hullFile;
	m_modelFiles = modelFiles;
	m_briefFile = briefFile;
	RunWizardRequest(request);
}

void MainWindow::GenerateBuildingFromBriefFile(const QString& briefFile, const QStringList& modelFiles, double fit)
{
	BuildingBrief brief;
	QString error;
	if(!LoadBuildingBrief(briefFile, brief, error))
	{
		fprintf(stderr, "%s\n", qPrintable(error));
		return;
	}

	GenerationRequest request;
	request.kind = BlueprintKind::Building;
	request.buildingBrief = brief;
	request.name = brief.name;
	request.description = brief.description;
	request.strategies = QStringList() << "grid" << "corridor";
	request.buildingModelFiles = modelFiles;
	request.buildingFit = fit;
	m_briefFile = briefFile;
	m_modelFiles = modelFiles;
	RunWizardRequest(request);
}

void MainWindow::OnNewSpacecraft()
{
	RunWizard(BlueprintKind::Spacecraft);
}

void MainWindow::OnNewBuilding()
{
	RunWizard(BlueprintKind::Building);
}
