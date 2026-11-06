//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - draw a ship with triangles and turn it into a blueprint.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "designerwindow.h"
#include "interior.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMutex>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QToolButton>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <atomic>

#include "aufilever.h"
#include "auscdesign.h"
#include "generate.h"
#include "names.h"
#include "planview.h"
#include "preview.h"
#include "shipgen.h"
#include "version.h"

namespace {

enum Step { StepShip, StepHull, StepRooms, StepPaint, StepBuild };

QIcon Swatch(const QColor& colour)
{
	QPixmap pm(16, 16);
	pm.fill(colour);
	return QIcon(pm);
}

// A colour for a new room, so that rooms next to each other can be told apart.
QColor RoomColour(int index)
{
	static const QColor colours[] = {
		QColor(90, 140, 220), QColor(220, 130, 70), QColor(120, 190, 110), QColor(200, 90, 160),
		QColor(80, 190, 190), QColor(210, 190, 80), QColor(150, 110, 210), QColor(200, 100, 100),
	};
	return colours[index % 8];
}

QWidget* Scrolled(QWidget* page)
{
	QScrollArea* area = new QScrollArea;
	area->setWidget(page);
	area->setWidgetResizable(true);
	area->setFrameShape(QFrame::NoFrame);
	return area;
}

QLabel* Help(const QString& text)
{
	QLabel* label = new QLabel(text);
	label->setWordWrap(true);
	label->setStyleSheet("color: palette(mid);");
	return label;
}

}	// namespace

DesignerWindow::DesignerWindow(QWidget* parent) : QMainWindow(parent)
{
	setWindowTitle("Shipgen designer " SHIPGEN_VERSION);

	m_view = new PlanView;
	m_preview = new DesignPreview;
	m_preview->SetHullOpacity(0.9f);

	QSplitter* split = new QSplitter(Qt::Horizontal);
	split->addWidget(m_view);
	split->addWidget(m_preview);
	split->setStretchFactor(0, 3);
	split->setStretchFactor(1, 2);

	// The side panel: the deck and the brush, which every step uses, and the steps.
	QWidget* side = new QWidget;
	QVBoxLayout* sideLayout = new QVBoxLayout(side);
	{
		QGroupBox* box = new QGroupBox("Drawing");
		QFormLayout* form = new QFormLayout(box);
		QHBoxLayout* deckRow = new QHBoxLayout;
		m_deck = new QSpinBox;
		m_deck->setMinimum(1);
		m_deck->setToolTip("The deck that is drawn on. Deck 1 is the lowest.");
		m_deckLabel = new QLabel;
		deckRow->addWidget(m_deck);
		deckRow->addWidget(m_deckLabel);
		form->addRow("Deck", deckRow);
		m_brush = new QComboBox;
		m_brush->addItem("One triangle");
		m_brush->addItem("One cell (4 triangles)");
		m_brush->addItem("Wide (3 x 3 cells)");
		m_brush->setCurrentIndex(1);
		m_brush->setToolTip("How much one stroke draws. The left button draws, the right button rubs out.");
		form->addRow("Brush", m_brush);
		m_mirror = new QCheckBox("Mirror port and starboard");
		m_mirror->setChecked(true);
		m_mirror->setToolTip("Whatever is drawn on one side of the centre line is drawn on the other side too.");
		form->addRow(m_mirror);
		sideLayout->addWidget(box);
	}
	m_steps = new QTabWidget;
	m_steps->addTab(Scrolled(MakeShipPage()), "1 Ship");
	m_steps->addTab(Scrolled(MakeHullPage()), "2 Hull");
	m_steps->addTab(Scrolled(MakeRoomPage()), "3 Rooms");
	m_steps->addTab(Scrolled(MakePaintPage()), "4 Paint");
	m_steps->addTab(Scrolled(MakeBuildPage()), "5 Build");
	m_steps->setTabToolTip(StepShip, "The ship's name, kind, grid and decks; turning or flipping it round.");
	m_steps->setTabToolTip(StepHull, "Drawing the outside of the ship, and how the hull is shaped.");
	m_steps->setTabToolTip(StepRooms, "Rooms, doors, windows, ladders, stairs, parking and emitters.");
	m_steps->setTabToolTip(StepPaint, "The colours of the hull.");
	m_steps->setTabToolTip(StepBuild, "Turning the plan into a blueprint the game can use, and saving it.");
	sideLayout->addWidget(m_steps, 1);
	side->setMinimumWidth(320);
	side->setMaximumWidth(420);

	QWidget* central = new QWidget;
	QHBoxLayout* centralLayout = new QHBoxLayout(central);
	centralLayout->setContentsMargins(0, 0, 0, 0);
	centralLayout->addWidget(side);
	centralLayout->addWidget(split, 1);
	setCentralWidget(central);

	m_status = new QLabel;
	statusBar()->addWidget(m_status, 1);
	// The task bar: hidden until something takes a while.
	m_taskBox = new QWidget;
	QHBoxLayout* taskLayout = new QHBoxLayout(m_taskBox);
	taskLayout->setContentsMargins(0, 0, 0, 0);
	m_taskLabel = new QLabel;
	m_taskBar = new QProgressBar;
	m_taskBar->setMinimumWidth(220);
	m_taskBar->setMaximumHeight(16);
	m_taskBar->setTextVisible(true);
	m_taskStop = new QToolButton;
	m_taskStop->setText("Stop");
	taskLayout->addWidget(m_taskLabel);
	taskLayout->addWidget(m_taskBar);
	taskLayout->addWidget(m_taskStop);
	statusBar()->addPermanentWidget(m_taskBox);
	m_taskBox->hide();
	m_taskTimer = new QTimer(this);
	m_taskTimer->setInterval(150);
	connect(m_taskTimer, &QTimer::timeout, this, &DesignerWindow::RefreshTask);
	connect(m_taskStop, &QToolButton::clicked, this, [this]() {
		m_taskStopAsked = true;
		m_taskStop->setEnabled(false);
		RefreshTask();
	});
	m_autoRedraw = QSettings().value("designer/autoRedraw", true).toBool();
	m_autoRedrawBox = new QCheckBox("Redraw 3D while drawing");
	m_autoRedrawBox->setToolTip("Reshape the hull and redraw the 3D view after every change. On a big model "
								"that takes a while; turn it off and press F5 (View, Redraw the 3D view) when "
								"you want to see the changes.");
	m_autoRedrawBox->setChecked(m_autoRedraw);
	statusBar()->addPermanentWidget(m_autoRedrawBox);

	// Menus.
	QMenu* file = menuBar()->addMenu("&File");
	file->addAction("&New plan", this, SLOT(OnNew()), QKeySequence::New);
	file->addAction("New plan from a 3D &model...", this, SLOT(OnNewFromModel()));
	file->addAction("&Open plan...", this, SLOT(OnOpen()), QKeySequence::Open);
	file->addAction("&Save plan", this, SLOT(OnSave()), QKeySequence::Save);
	file->addAction("Save plan &as...", this, SLOT(OnSaveAs()), QKeySequence::SaveAs);
	file->addSeparator();
	file->addAction("&Export blueprint (.SoH)...", this, SLOT(OnExport()), QKeySequence("Ctrl+E"));
	file->addSeparator();
	file->addAction("&Quit", this, SLOT(close()), QKeySequence::Quit);
	QMenu* edit = menuBar()->addMenu("&Edit");
	edit->addAction("&Undo", this, SLOT(OnUndo()), QKeySequence::Undo);
	edit->addAction("&Redo", this, SLOT(OnRedo()), QKeySequence::Redo);
	edit->addSeparator();
	edit->addAction("&Turn the ship around (fore and aft)", this, [this]() { Flip(true, true, false, false); });
	edit->addAction("&Mirror the ship (port and starboard)", this, [this]() { Flip(false, true, false, false); });
	edit->addAction("Turn the ship &upside down", this, [this]() { Flip(false, false, true, false); });
	edit->addAction("Turn this &deck around", this, [this]() { Flip(true, true, false, true); });
	edit->addAction("M&irror this deck", this, [this]() { Flip(false, true, false, true); });
	QMenu* view = menuBar()->addMenu("&View");
	QAction* metric = view->addAction("&Metric units");
	metric->setCheckable(true);
	metric->setChecked(QSettings().value("designer/metric", false).toBool());
	connect(metric, &QAction::toggled, this, &DesignerWindow::OnMetric);
	m_metric = metric->isChecked();
	view->addSeparator();
	QAction* autoRedraw = view->addAction("Redraw the 3D view &while drawing");
	autoRedraw->setCheckable(true);
	autoRedraw->setChecked(m_autoRedraw);
	connect(autoRedraw, &QAction::toggled, this, &DesignerWindow::OnAutoRedraw);
	connect(m_autoRedrawBox, &QCheckBox::toggled, autoRedraw, &QAction::setChecked);
	connect(autoRedraw, &QAction::toggled, m_autoRedrawBox, &QCheckBox::setChecked);
	view->addAction("&Redraw the 3D view", this, SLOT(OnShape()), QKeySequence(Qt::Key_F5));

	m_shapeTimer = new QTimer(this);
	m_shapeTimer->setSingleShot(true);
	m_shapeTimer->setInterval(400);
	connect(m_shapeTimer, &QTimer::timeout, this, &DesignerWindow::ShapeSoon);

	connect(m_view, &PlanView::AboutToChange, this, &DesignerWindow::OnAboutToChange);
	connect(m_view, &PlanView::PlanChanged, this, &DesignerWindow::OnPlanChanged);
	connect(m_view, &PlanView::Hover, m_status, &QLabel::setText);
	connect(m_steps, &QTabWidget::currentChanged, this, &DesignerWindow::OnStepChanged);
	connect(m_deck, QOverload<int>::of(&QSpinBox::valueChanged), this, &DesignerWindow::OnDeckChanged);
	connect(m_brush, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
		m_view->SetBrush(PlanView::Brush(i));
	});
	connect(m_mirror, &QCheckBox::toggled, m_view, &PlanView::SetMirror);

	resize(1500, 900);
	SetPlan(TriPlan::Example());
	m_dirty = false;
	RefreshTitle();
}

DesignerWindow::~DesignerWindow()
{
	if(m_shapeWorker)
	{
		m_shapeWorker->wait();
		delete m_shapeWorker;
		m_shapeWorker = 0;
		if(m_shapeJob)
			delete m_shapeJob->design;
	}
	m_preview->SetDesign(0);
	delete m_design;
}

QWidget* DesignerWindow::MakeShipPage()
{
	QWidget* page = new QWidget;
	QVBoxLayout* layout = new QVBoxLayout(page);
	layout->addWidget(Help("The size of the drawing grid and what the ship is for. A cell is a square of four "
						   "triangles; a deck is one floor of the ship."));
	QFormLayout* form = new QFormLayout;
	m_name = new QLineEdit;
	m_name->setToolTip("The ship's design name, as the game shows it in the blueprint list.");
	form->addRow("Name", m_name);
	m_template = new QComboBox;
	for(const ShipTemplate& t : AllTemplates())
		m_template->addItem(t.label, t.key);
	m_template->setToolTip("The stations, berths and systems are put in the way this kind of ship has them.");
	form->addRow("Kind of ship", m_template);
	m_ftl = new QComboBox;
	for(const auto& choice : FtlDriveChoices())
		m_ftl->addItem(choice.second, choice.first);
	m_ftl->setToolTip("The FTL drive the engineer station runs. The game fits a maneuver drive by itself.");
	form->addRow("FTL drive", m_ftl);
	m_noShields = new QCheckBox("No shields");
	m_noWeapons = new QCheckBox("No weapon bay");
	m_noTransporter = new QCheckBox("No transporter");
	for(QCheckBox* box : { m_noShields, m_noWeapons, m_noTransporter })
	{
		box->setToolTip("Leave this out of what the template puts in: its station and the system it runs.");
		form->addRow(box);
	}
	m_fireControl = new QComboBox;
	for(const QPair<int, QString>& choice : FireControlModules())
		m_fireControl->addItem(choice.second, choice.first);
	m_fireControl->setToolTip("What the fire control station runs. It is harder to change in the game's designer.");
	form->addRow("Fire control", m_fireControl);
	connect(m_fireControl, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DesignerWindow::OnSettingsChanged);
	m_cellFeet = new QDoubleSpinBox;
	m_cellFeet->setRange(2, 20);
	m_cellFeet->setSingleStep(0.5);
	m_cellFeet->setSuffix(" ft");
	m_cellFeet->setToolTip("The side of one cell. A door is as wide as a cell, so 5 feet or more is best.");
	form->addRow("Cell size", m_cellFeet);
	m_deckFeet = new QDoubleSpinBox;
	m_deckFeet->setRange(8, 40);
	m_deckFeet->setSuffix(" ft");
	m_deckFeet->setToolTip("From one deck floor to the next. A room needs 7 feet of headroom.");
	form->addRow("Deck height", m_deckFeet);
	m_floorFeet = new QDoubleSpinBox;
	m_floorFeet->setRange(0.5, 4);
	m_floorFeet->setSingleStep(0.5);
	m_floorFeet->setSuffix(" ft");
	m_floorFeet->setToolTip("The structure between the rooms of one deck and the next.");
	form->addRow("Floor thickness", m_floorFeet);
	layout->addLayout(form);

	QGroupBox* grid = new QGroupBox("Grid");
	QFormLayout* gridForm = new QFormLayout(grid);
	m_width = new QSpinBox;
	m_width->setRange(4, 400);
	m_width->setToolTip("Cells from aft to fore.");
	gridForm->addRow("Length (cells)", m_width);
	m_depth = new QSpinBox;
	m_depth->setRange(4, 400);
	m_depth->setToolTip("Cells from starboard to port.");
	gridForm->addRow("Beam (cells)", m_depth);
	QPushButton* resizeButton = new QPushButton("Change the grid size");
	resizeButton->setToolTip("Makes the grid this many cells long and wide. What is drawn stays in the middle; "
							 "whatever falls off the edge of a smaller grid is lost (Undo brings it back).");
	gridForm->addRow(resizeButton);
	connect(resizeButton, &QPushButton::clicked, this, &DesignerWindow::OnResize);
	layout->addWidget(grid);

	QGroupBox* decks = new QGroupBox("Decks");
	QGridLayout* deckButtons = new QGridLayout(decks);
	QPushButton* above = new QPushButton("Add above");
	above->setToolTip("A new, empty deck on top of the ship.");
	QPushButton* below = new QPushButton("Add below");
	below->setToolTip("A new, empty deck under the ship. The decks above it move up one.");
	QPushButton* remove = new QPushButton("Remove this deck");
	remove->setToolTip("Takes away the deck shown, with its rooms, doors, ladders and stairs.");
	QPushButton* turnDeck = new QPushButton("Turn this deck around");
	turnDeck->setToolTip("Swaps fore and aft on the deck shown only (and port and starboard, as a real half "
						 "turn does). Ladders and stairs to the decks next to it go when they no longer meet. "
						 "A hull that is the 3D model itself does not change: untick that on the hull page to "
						 "see it.");
	QPushButton* mirrorDeck = new QPushButton("Mirror this deck");
	mirrorDeck->setToolTip("Swaps port and starboard on the deck shown only.");
	deckButtons->addWidget(above, 0, 0);
	deckButtons->addWidget(below, 0, 1);
	deckButtons->addWidget(remove, 0, 2);
	deckButtons->addWidget(turnDeck, 1, 0, 1, 2);
	deckButtons->addWidget(mirrorDeck, 1, 2);
	connect(above, &QPushButton::clicked, this, &DesignerWindow::OnAddDeckAbove);
	connect(below, &QPushButton::clicked, this, &DesignerWindow::OnAddDeckBelow);
	connect(remove, &QPushButton::clicked, this, &DesignerWindow::OnRemoveDeck);
	connect(turnDeck, &QPushButton::clicked, this, [this]() { Flip(true, true, false, true); });
	connect(mirrorDeck, &QPushButton::clicked, this, [this]() { Flip(false, true, false, true); });
	layout->addWidget(decks);

	// The whole design: for a model that came in the wrong way round.
	QGroupBox* flips = new QGroupBox("Turn and flip the whole ship");
	QGridLayout* flipButtons = new QGridLayout(flips);
	QPushButton* turn = new QPushButton("Turn around (fore and aft)");
	turn->setToolTip("A half turn: the bow becomes the stern. For a 3D model that came in backwards. Every "
					 "deck, room, door, window, ladder, stairs, parking spot, emitter and the paint go with "
					 "it, and so does the model itself when it is the hull.");
	QPushButton* mirror = new QPushButton("Mirror port and starboard");
	mirror->setToolTip("Swaps the left and right sides of the ship, like a mirror image.");
	QPushButton* upside = new QPushButton("Turn upside down");
	upside->setToolTip("The top deck becomes the bottom one. For a 3D model that came in upside down. Stairs "
					   "would climb down, so they are taken away; draw them again.");
	flipButtons->addWidget(turn, 0, 0);
	flipButtons->addWidget(mirror, 0, 1);
	flipButtons->addWidget(upside, 1, 0);
	connect(turn, &QPushButton::clicked, this, [this]() { Flip(true, true, false, false); });
	connect(mirror, &QPushButton::clicked, this, [this]() { Flip(false, true, false, false); });
	connect(upside, &QPushButton::clicked, this, [this]() { Flip(false, false, true, false); });
	layout->addWidget(flips);
	layout->addStretch(1);

	connect(m_name, &QLineEdit::editingFinished, this, &DesignerWindow::OnSettingsChanged);
	connect(m_template, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DesignerWindow::OnSettingsChanged);
	connect(m_ftl, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DesignerWindow::OnSettingsChanged);
	for(QCheckBox* box : { m_noShields, m_noWeapons, m_noTransporter })
		connect(box, &QCheckBox::toggled, this, &DesignerWindow::OnSettingsChanged);
	for(QDoubleSpinBox* box : { m_cellFeet, m_deckFeet, m_floorFeet })
		connect(box, &QDoubleSpinBox::editingFinished, this, &DesignerWindow::OnSettingsChanged);
	return page;
}

QWidget* DesignerWindow::MakeHullPage()
{
	QWidget* page = new QWidget;
	QVBoxLayout* layout = new QVBoxLayout(page);
	layout->addWidget(Help("Draw the outside of the ship, deck by deck. The drawn triangles are the inside of the "
						   "hull; the skin goes round them, and the smoothing rounds its edges and the steps "
						   "between decks. The 3D view follows as you draw. The deck below shows faintly."));
	QFormLayout* form = new QFormLayout;
	m_hullStyle = new QComboBox;
	m_hullStyle->addItem("Smooth", "smooth");
	m_hullStyle->addItem("Angular (experimental)", "angular");
	m_hullStyle->addItem("Classic", "classic");
	m_hullStyle->setToolTip("How the hull is made from the drawn triangles (not used when the 3D model is the "
							"hull). Smooth: the hull is pushed out round the drawn triangles and its edges rounded. "
							"Angular: the hull slopes from deck to deck and keeps sharp edges, so flat areas "
							"stay flat panels (the smoothing is not used). Classic: like the old Shores of "
							"Hazeron designer, straight walls up from the drawn outline and flat tops and "
							"bottoms, deck by deck.");
	form->addRow("Hull style", m_hullStyle);
	m_skin = new QDoubleSpinBox;
	m_skin->setRange(0.5, 10);
	m_skin->setSingleStep(0.5);
	m_skin->setSuffix(" ft");
	m_skin->setToolTip("How far the hull surface stands out from the drawn triangles. More skin allows more rounding.");
	form->addRow("Skin", m_skin);
	m_smoothing = new QSlider(Qt::Horizontal);
	m_smoothing->setRange(0, 15);
	m_smoothing->setToolTip("How much the edges are rounded. 0 keeps every edge sharp.");
	form->addRow("Smoothing", m_smoothing);
	m_maxFaces = new QSpinBox;
	m_maxFaces->setRange(500, 15000);
	m_maxFaces->setSingleStep(500);
	m_maxFaces->setToolTip("The game copes with about 15,000 faces, is comfortable with 5,000 and happy with 2,000.");
	form->addRow("Face budget", m_maxFaces);
	m_panels = new QSpinBox;
	m_panels->setRange(0, 2000);
	m_panels->setSingleStep(25);
	m_panels->setSpecialValueText("Smooth");
	m_panels->setToolTip("Experimental. A hull of about this many flat panels, like armour plating: fewer "
						 "faces and a low-poly look, though the panels still come out a little crumpled. The "
						 "panels keep the paint edges and a wall round every room. Smooth (0) keeps the "
						 "rounded hull, and is the better choice for now.");
	form->addRow("Flat panels", m_panels);
	m_modelHull = new QCheckBox("Use the 3D model as the hull");
	m_modelHull->setToolTip("For a plan made from a 3D model: the hull is the model itself, with a wall "
							"round every room (a room drawn out past the model still gets its hull). Truer to the "
							"model than the triangles. Hull triangles drawn or rubbed out do not change it: untick "
							"this to go back to the drawn hull. Thin parts of the model (antennas) go, and a "
							"detailed model loses detail at a low face budget.");
	form->addRow(m_modelHull);
	m_modelHullShape = new QComboBox;
	m_modelHullShape->addItem("Sharp: follows the model's edges and faces", true);
	m_modelHullShape->addItem("Smooth: rounded", false);
	m_modelHullShape->setToolTip("How the 3D model is made the hull. Sharp keeps the model's edges sharp and its "
								 "flat faces flat, so the hull looks like the model. Smooth rounds it (edges by "
								 "about a quarter of a cell).");
	m_modelHullShapeLabel = new QLabel("Model hull");
	form->addRow(m_modelHullShapeLabel, m_modelHullShape);
	m_slopedDecks = new QCheckBox("Slope the hull between decks");
	m_slopedDecks->setToolTip("Where a deck is narrower than the one below, the hull slopes up to it instead of "
							  "stepping in (and under a wider deck it slopes down). The drawn triangles stay the "
							  "least the hull may be: sloping only adds hull. Sleeker, and usually fewer faces.");
	form->addRow(m_slopedDecks);
	layout->addLayout(form);
	QPushButton* shape = new QPushButton("Shape the hull now");
	shape->setToolTip("The hull is shaped by itself a moment after each change; this does it at once.");
	layout->addWidget(shape);
	m_hullInfo = new QLabel;
	m_hullInfo->setWordWrap(true);
	layout->addWidget(m_hullInfo);
	layout->addStretch(1);

	connect(shape, &QPushButton::clicked, this, &DesignerWindow::OnShape);
	connect(m_skin, &QDoubleSpinBox::editingFinished, this, &DesignerWindow::OnSettingsChanged);
	connect(m_smoothing, &QSlider::valueChanged, this, &DesignerWindow::OnSettingsChanged);
	connect(m_maxFaces, &QSpinBox::editingFinished, this, &DesignerWindow::OnSettingsChanged);
	connect(m_panels, &QSpinBox::editingFinished, this, &DesignerWindow::OnSettingsChanged);
	connect(m_modelHull, &QCheckBox::toggled, this, &DesignerWindow::OnSettingsChanged);
	connect(m_slopedDecks, &QCheckBox::toggled, this, &DesignerWindow::OnSettingsChanged);
	connect(m_hullStyle, QOverload<int>::of(&QComboBox::activated), this, [this](int) { OnSettingsChanged(); });
	connect(m_modelHullShape, QOverload<int>::of(&QComboBox::activated), this, [this](int) { OnSettingsChanged(); });
	return page;
}

QWidget* DesignerWindow::MakeRoomPage()
{
	QWidget* page = new QWidget;
	QVBoxLayout* layout = new QVBoxLayout(page);
	layout->addWidget(Help("Pick a room and draw it on the hull triangles; pick a tool for the rest (hover over a "
						   "control for what it does). Red hatching marks triangles that are "
						   "outside the shaped hull, where a room cannot go. Doors go on the edge between two "
						   "rooms, or between a room and the outside for a hull door: click the triangle that "
						   "stands on that edge. A ladder goes up from a cell into the room above it. Windows go on the "
						   "edge between a room and the outside; they are cut through the room wall and the hull. "
						   "Drag along a wall for one wide door, as tall as the room: a bay door for vehicles; drag over "
						   "separate doors to join them. A click makes a door of its own, and a click on a door takes it "
						   "away. A right click on a door gives its row the door texture picked below. Vehicle parking spots go on the floor of a room, centred on the "
						   "grid corner you click. Stairs climb to the deck above over a few cells of one room: click the "
						   "triangle of the first cell on the side they climb towards. On the deck above, the cells "
						   "over them and the cell they come out on have to be in one room."));
	m_roomTool = new QComboBox;
	const QList<QPair<QString, QString> > tools = {
		{ "Draw rooms", "The left button paints the triangles of the room picked in the list below; the right "
						"button takes them out of it." },
		{ "Place doors", "Click the triangle that stands on the cell edge the door goes on: between two rooms, "
						 "or between a room and the outside for a hull door. Click again to take it away." },
		{ "Place ladders", "Click a cell of a room; the ladder goes up into the room above it." },
		{ "Place windows", "Click the triangle on the edge between a room and the outside." },
		{ "Place ground vehicle parking", "A 6 by 12 m spot on the floor of a room, centred on the grid corner "
										  "nearest the click; its outline follows the mouse, and R turns it. The room "
										  "needs a bay door to get vehicles out." },
		{ "Place space vehicle parking", "A 12 by 17 m spot for a shuttle or fighter, centred on the grid "
										 "corner nearest the click; its outline follows the mouse, and R turns it." },
		{ "Place stairs", "Click the triangle of the first cell on the side the stairs climb towards." },
		{ "Place contrail emitters", "Click a hull triangle: a contrail emitter goes on the hull straight aft of "
									 "it at mid-deck height, where the engine trail comes out (a ball on the "
									 "plan at that spot, and on the 3D view, with an arrow the way the trail goes). "
									 "Click the triangle or the ball again to take it away." },
		{ "Place weapon emitters", "Click a hull triangle: the ship's guns fire from the nearest point of the "
								   "hull (a ring with a cross). The game keeps one on each fire control station, "
								   "so the first goes to the first station; a ship without fire control (see the "
								   "Ship page) has nowhere to put one." },
		{ "Place ceiling windows (experimental)", "Click a whole cell of a room with no room on any deck above it: a window "
												  "goes straight up through the hull. With Join windows, neighbouring "
												  "cells make one pane. Try these in the game before relying on them." },
	};
	for(const auto& tool : tools)
	{
		m_roomTool->addItem(tool.first);
		m_roomTool->setItemData(m_roomTool->count() - 1, tool.second, Qt::ToolTipRole);
	}
	m_roomTool->setToolTip("What the mouse puts on the plan. Hover over a choice in the list for how it works.");
	layout->addWidget(m_roomTool);
	m_emitterRow = new QWidget;
	{
		QFormLayout* emitterForm = new QFormLayout(m_emitterRow);
		emitterForm->setContentsMargins(0, 0, 0, 0);
		m_emitterSize = new QDoubleSpinBox;
		m_emitterSize->setRange(1, 200);
		m_emitterSize->setDecimals(1);
		m_emitterSize->setValue(6.6);
		m_emitterSize->setSuffix(" ft");
		m_emitterSize->setToolTip("How wide the contrail is, for the emitters placed from now on. The game's own "
								  "is 2 m (6.6 ft); match it to the engine nozzle.");
		emitterForm->addRow("Contrail width", m_emitterSize);
		connect(m_emitterSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
			m_view->SetEmitterSize(m_metric ? v / 0.3048 : v);
		});
	}
	m_emitterRow->setVisible(false);
	layout->addWidget(m_emitterRow);
	m_rooms = new QListWidget;
	m_rooms->setMinimumHeight(180);
	m_rooms->setToolTip("The rooms of the ship. Pick one to draw it, or to change its type and name below.");
	layout->addWidget(m_rooms);
	QHBoxLayout* buttons = new QHBoxLayout;
	QPushButton* add = new QPushButton("Add a room...");
	add->setToolTip("Asks for the new room's type and name, then picks it for drawing.");
	QPushButton* remove = new QPushButton("Remove it");
	remove->setToolTip("Takes away the room picked in the list, and its doors, ladders and parking.");
	buttons->addWidget(add);
	buttons->addWidget(remove);
	layout->addLayout(buttons);
	QGroupBox* selected = new QGroupBox("The room picked");
	QFormLayout* form = new QFormLayout(selected);
	m_roomType = new QComboBox;
	m_roomType->addItems(SpacecraftRoomTypes());
	m_roomType->setToolTip("The type of the room picked in the list (the engine's own room types). Changing it "
						   "changes that room. The stations of the ship go into rooms of the right type.");
	form->addRow("Type", m_roomType);
	auto textureBox = [](const QString& tip) {
		QComboBox* box = new QComboBox;
		box->addItem("Usual", -1);
		for(const QPair<int, QString>& texture : InteriorTextures())
			box->addItem(texture.second, texture.first);
		box->setToolTip(tip);
		return box;
	};
	const QString textureTip = "A texture from the game's library for this room. A ship's palette holds 15 textures "
			"between its hull, rooms and doors.";
	m_roomWallTex = textureBox(textureTip);
	m_roomFloorTex = textureBox(textureTip);
	m_roomCeilingTex = textureBox(textureTip);
	form->addRow("Walls", m_roomWallTex);
	form->addRow("Floor", m_roomFloorTex);
	form->addRow("Ceiling", m_roomCeilingTex);
	for(QComboBox* box : { m_roomWallTex, m_roomFloorTex, m_roomCeilingTex })
		connect(box, QOverload<int>::of(&QComboBox::activated), this, &DesignerWindow::OnRoomEdited);
	m_doorTex = textureBox("The texture for doors you place from now on. With the door tool, a right click on a door "
			"gives it and the rest of its row this texture, and the way down below.");
	form->addRow("Door texture", m_doorTex);
	connect(m_doorTex, QOverload<int>::of(&QComboBox::activated), this, [this]() {
		m_view->SetDoorTexture(m_doorTex->currentData().toInt());
	});
	const QString doorExitTip = "For hull doors you place from now on. With the door tool, a right click on a door gives "
			"it and the rest of its row these settings, and its texture. A door with a way of its own shows a letter in the "
			"plan: L ladder, R ramp, D docking, A ramp or ladder, S same as the ship; a star means only while it is open.";
	m_doorExit = new QComboBox;
	m_doorExit->addItem("Same as the ship", "");
	m_doorExit->addItem("A ramp or a ladder down", "auto");
	m_doorExit->addItem("A ladder down", "ladder");
	m_doorExit->addItem("A ramp down", "ramp");
	m_doorExit->addItem("None (docking door)", "none");
	m_doorExit->setToolTip(doorExitTip);
	form->addRow("Door's way down", m_doorExit);
	m_doorExitOpenOnly = new QCheckBox("Only while the door is open");
	m_doorExitOpenOnly->setToolTip(doorExitTip + " (Experimental.) A ramp stays in sight with the door closed, "
			"but cannot be stood on, and the game's crew do not use such a ladder to come aboard from the ground.");
	form->addRow(m_doorExitOpenOnly);
	m_doorRampRun = new QDoubleSpinBox;
	m_doorRampRun->setRange(1.0, 8.0);
	m_doorRampRun->setSingleStep(.5);
	m_doorRampRun->setDecimals(1);
	m_doorRampRun->setValue(SHIPGEN_RAMP_RUN_PER_RISE);
	m_doorRampRun->setToolTip(doorExitTip + " Feet of ramp for each foot it falls: more is longer and gentler.");
	form->addRow("Ramp length per height", m_doorRampRun);
	auto doorExitChanged = [this]() {
		const double run = m_doorRampRun->value();
		m_view->SetDoorExit(m_doorExit->currentData().toString(), m_doorExitOpenOnly->isChecked(),
				qFuzzyCompare(run, SHIPGEN_RAMP_RUN_PER_RISE) ? 0.0 : run);
	};
	connect(m_doorExit, QOverload<int>::of(&QComboBox::activated), this, doorExitChanged);
	connect(m_doorExitOpenOnly, &QCheckBox::toggled, this, doorExitChanged);
	connect(m_doorRampRun, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, doorExitChanged);
	m_roomLabel = new QLineEdit;
	m_roomLabel->setToolTip("The name of the room picked in the list, as the blueprint has it. Every room needs "
							"its own.");
	form->addRow("Name", m_roomLabel);
	layout->addWidget(selected);
	QGroupBox* windowBox = new QGroupBox("Windows (all rooms)");
	QFormLayout* windowForm = new QFormLayout(windowBox);
	m_windowSill = new QDoubleSpinBox;
	m_windowSill->setToolTip("How high above the floor the windows start. Lower suits short characters.");
	windowForm->addRow("Window sill", m_windowSill);
	m_windowTop = new QDoubleSpinBox;
	m_windowTop->setToolTip("How high above the floor the windows end (less in a low room).");
	windowForm->addRow("Window top", m_windowTop);
	m_joinWindows = new QCheckBox("Join windows side by side");
	m_joinWindows->setToolTip("Windows next to each other on one straight wall of a room become one long window.");
	windowForm->addRow(m_joinWindows);
	m_flatWindows = new QCheckBox("One flat pane across a zigzag wall");
	m_flatWindows->setToolTip("Joined windows along a wall that zigzags over the triangles become one flat pane on a "
			"straight line. Off, each tooth is cut by itself, with its own frame, and the panes follow the hull.");
	windowForm->addRow(m_flatWindows);
	m_hullDoorExit = new QComboBox;
	m_hullDoorExit->addItem("A ramp or a ladder down", "auto");
	m_hullDoorExit->addItem("A ladder down", "ladder");
	m_hullDoorExit->addItem("None (docking door)", "none");
	m_hullDoorExit->setToolTip("How people get from the hull doors down to the ground: a ramp where the door is "
							   "low enough and a ladder otherwise, always a ladder, or no way down at all, for a "
							   "ship that stays in space (no ladder, and nothing cleared below the door). A door can have "
							   "its own way down: see *Door's way down* above.");
	windowForm->addRow("Hull doors (ship)", m_hullDoorExit);
	layout->addWidget(windowBox);
	layout->addStretch(1);

	connect(m_roomTool, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { ApplyDrawMode(); });
	connect(m_rooms, &QListWidget::currentRowChanged, this, &DesignerWindow::OnRoomSelected);
	connect(m_roomType, QOverload<int>::of(&QComboBox::activated), this, &DesignerWindow::OnRoomEdited);
	connect(m_roomLabel, &QLineEdit::editingFinished, this, &DesignerWindow::OnRoomEdited);
	connect(m_windowSill, &QDoubleSpinBox::editingFinished, this, &DesignerWindow::OnSettingsChanged);
	connect(m_windowTop, &QDoubleSpinBox::editingFinished, this, &DesignerWindow::OnSettingsChanged);
	connect(m_joinWindows, &QCheckBox::toggled, this, &DesignerWindow::OnSettingsChanged);
	connect(m_flatWindows, &QCheckBox::toggled, this, &DesignerWindow::OnSettingsChanged);
	connect(m_hullDoorExit, QOverload<int>::of(&QComboBox::activated), this, [this](int) { OnSettingsChanged(); });
	connect(add, &QPushButton::clicked, this, &DesignerWindow::OnAddRoom);
	connect(remove, &QPushButton::clicked, this, &DesignerWindow::OnRemoveRoom);
	return page;
}

QWidget* DesignerWindow::MakePaintPage()
{
	QWidget* page = new QWidget;
	QVBoxLayout* layout = new QVBoxLayout(page);
	layout->addWidget(Help("Pick a colour and paint the hull triangles; the paint covers the hull over them on "
						   "that deck, sides included. The first colour is the base coat. An empire colour is "
						   "the owner's own colour in the game."));
	m_palette = new QListWidget;
	m_palette->setMinimumHeight(160);
	m_palette->setToolTip("Pick a colour to paint with. Double-click a plain colour to change it.");
	layout->addWidget(m_palette);
	QHBoxLayout* buttons = new QHBoxLayout;
	QPushButton* add = new QPushButton("Add a colour...");
	add->setToolTip("A plain colour, picked from the colour chooser.");
	QPushButton* empire = new QPushButton("Add an empire colour");
	empire->setToolTip("One of the owner's empire colours (up to three): the ship takes the colours of whoever "
					   "builds it.");
	QPushButton* remove = new QPushButton("Remove it");
	remove->setToolTip("Takes away the colour picked; what was painted with it goes back to the base coat. The "
					   "base coat itself stays.");
	buttons->addWidget(add);
	buttons->addWidget(empire);
	buttons->addWidget(remove);
	layout->addLayout(buttons);
	layout->addStretch(1);
	connect(m_palette, &QListWidget::currentRowChanged, this, &DesignerWindow::OnColourSelected);
	connect(m_palette, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
		const int row = m_palette->currentRow();
		if(row < 0 || row >= m_plan.palette.size() || m_plan.palette[row].theme >= 0)
			return;
		const QColor c = QColorDialog::getColor(m_plan.palette[row].colour, this, "The colour");
		if(!c.isValid())
			return;
		OnAboutToChange();
		m_plan.palette[row].colour = c;
		RefreshPalette();
		OnPlanChanged(true);
	});
	connect(add, &QPushButton::clicked, this, &DesignerWindow::OnAddColour);
	connect(empire, &QPushButton::clicked, this, &DesignerWindow::OnAddEmpireColour);
	connect(remove, &QPushButton::clicked, this, &DesignerWindow::OnRemoveColour);
	return page;
}

QWidget* DesignerWindow::MakeBuildPage()
{
	QWidget* page = new QWidget;
	QVBoxLayout* layout = new QVBoxLayout(page);
	layout->addWidget(Help("Turn the plan into a blueprint. The rooms get their voids, the doors their doorways, "
						   "and the stations, berths and systems are put in for the kind of ship chosen on the "
						   "first page. The engine's own report says whether the game will take it."));
	m_furnish = new QCheckBox("Put in stations, berths and systems");
	m_furnish->setChecked(true);
	m_furnish->setToolTip("Puts in what the kind of ship needs (Ship page): stations such as the helm, berths "
						  "for the crew, and volume for the power plant, drives and so on. Off: only the hull, "
						  "rooms, doors and the rest drawn.");
	layout->addWidget(m_furnish);
	m_walkPaths = new QCheckBox("Walk paths through the doors");
	m_walkPaths->setChecked(true);
	m_walkPaths->setToolTip("Paths the crew and passengers walk along through the doors, ladders and stairs. "
							"Without them people in the game cannot find their way round the ship.");
	layout->addWidget(m_walkPaths);
	QHBoxLayout* bridgeRow = new QHBoxLayout;
	bridgeRow->addWidget(new QLabel("Bridge style"));
	m_bridgeStyle = new QComboBox;
	for(const QPair<QString, QString>& style : BridgeStyles())
		m_bridgeStyle->addItem(style.second, style.first);
	m_bridgeStyle->setToolTip("How the stations look. Hazeron default: the game's swivel chairs. Integrated desks: "
			"curved rows of desks with built-in screens and the captain behind them; try it in the game first.");
	bridgeRow->addWidget(m_bridgeStyle, 1);
	layout->addLayout(bridgeRow);
	connect(m_bridgeStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DesignerWindow::OnSettingsChanged);
	QHBoxLayout* deskRow = new QHBoxLayout;
	deskRow->addWidget(new QLabel("Desks"));
	m_deskPosture = new QComboBox;
	m_deskPosture->addItem("Sitting", false);
	m_deskPosture->addItem("Standing", true);
	m_deskPosture->setToolTip("Whether the operator of an integrated desk sits on a stool or stands. For the whole ship.");
	deskRow->addWidget(m_deskPosture, 1);
	deskRow->addWidget(new QLabel("Height"));
	m_deskHeight = new QDoubleSpinBox;
	m_deskHeight->setToolTip("How high the top edge of the desks is above the floor, for the whole ship. "
			"Lower suits short characters. Up to 4.5 ft sitting; from 2.8 ft standing.");
	deskRow->addWidget(m_deskHeight);
	layout->addLayout(deskRow);
	connect(m_deskPosture, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DesignerWindow::OnSettingsChanged);
	connect(m_deskHeight, &QDoubleSpinBox::editingFinished, this, &DesignerWindow::OnSettingsChanged);
	QHBoxLayout* buttons = new QHBoxLayout;
	QPushButton* build = new QPushButton("Build the blueprint");
	build->setToolTip("Makes the blueprint and asks the game's own engine whether it would take it. The report "
					  "appears below, and the 3D view shows the result.");
	QPushButton* save = new QPushButton("Save it as .SoH...");
	save->setToolTip("Saves the blueprint as a .SoH file, which the game's design studio can open (File, "
					 "Export blueprint does the same).");
	buttons->addWidget(build);
	buttons->addWidget(save);
	layout->addLayout(buttons);
	m_reportText = new QPlainTextEdit;
	m_reportText->setReadOnly(true);
	m_reportText->setToolTip("What was built, and the engine's own errors and warnings. Errors have to be put "
							 "right before the game takes the blueprint.");
	m_reportText->setMinimumHeight(300);
	layout->addWidget(m_reportText, 1);
	connect(build, &QPushButton::clicked, this, &DesignerWindow::OnBuild);
	connect(save, &QPushButton::clicked, this, &DesignerWindow::OnExport);
	return page;
}

void DesignerWindow::SetPlan(const TriPlan& plan)
{
	m_plan = plan;
	m_undo.clear();
	m_redo.clear();
	m_view->SetPlan(&m_plan);
	m_hullShaped = false;
	RefreshAll();
	ShapeNow();		// Always, whether redrawing while drawing or not.
	m_preview->FrameAll();
}

bool DesignerWindow::OpenPlan(const QString& fileName)
{
	TriPlan plan;
	QString error;
	if(!plan.Load(fileName, error))
	{
		QMessageBox::warning(this, "Shipgen designer", error);
		return false;
	}
	SetPlan(plan);
	m_planFile = fileName;
	m_dirty = false;
	RefreshTitle();
	return true;
}

void DesignerWindow::RefreshTitle()
{
	const QString file = m_planFile.isEmpty() ? QString("unsaved") : QFileInfo(m_planFile).fileName();
	setWindowTitle(QString("Shipgen designer %1 - %2 (%3)%4").arg(SHIPGEN_VERSION, m_plan.name, file, m_dirty ? " *" : ""));
}

void DesignerWindow::RefreshAll()
{
	m_updating = true;
	m_name->setText(m_plan.name);
	m_template->setCurrentIndex(qMax(0, m_template->findData(m_plan.templateKey)));
	m_ftl->setCurrentIndex(qMax(0, m_ftl->findData(m_plan.ftlDrive)));
	m_noShields->setChecked(m_plan.leaveOut.contains("shield"));
	m_noWeapons->setChecked(m_plan.leaveOut.contains("firecontrol"));
	m_fireControl->setCurrentIndex(qMax(0, m_fireControl->findData(m_plan.fireControlModule)));
	m_fireControl->setEnabled(!m_noWeapons->isChecked());
	m_noTransporter->setChecked(m_plan.leaveOut.contains("transporter"));
	ApplyUnits();
	m_cellFeet->setValue(Shown(m_plan.cellFeet));
	m_deckFeet->setValue(Shown(m_plan.deckFeet));
	m_floorFeet->setValue(Shown(m_plan.floorFeet));
	m_width->setValue(m_plan.Width());
	m_depth->setValue(m_plan.Depth());
	m_skin->setValue(Shown(m_plan.skinFeet));
	m_smoothing->setValue(m_plan.smoothing);
	m_maxFaces->setValue(m_plan.maxFaces);
	m_panels->setValue(m_plan.panels);
	m_modelHull->setChecked(m_plan.model.asHull);
	m_modelHull->setVisible(m_plan.model.IsSet());
	m_modelHullShape->setCurrentIndex(m_plan.model.sharp ? 0 : 1);
	m_modelHullShape->setVisible(m_plan.model.IsSet());
	m_modelHullShapeLabel->setVisible(m_plan.model.IsSet());
	m_modelHullShape->setEnabled(m_plan.model.asHull);
	if(m_plan.model.IsSet())
		m_modelHull->setText(QString("Use the 3D model as the hull (%1)").arg(QFileInfo(m_plan.model.file).fileName()));
	m_slopedDecks->setChecked(m_plan.slopedDecks);
	m_hullStyle->setCurrentIndex(qMax(0, m_hullStyle->findData(m_plan.hullStyle)));
	m_hullDoorExit->setCurrentIndex(qMax(0, m_hullDoorExit->findData(m_plan.hullDoorExit)));
	m_windowSill->setValue(Shown(m_plan.windowSillFeet));
	m_windowTop->setValue(Shown(m_plan.windowTopFeet));
	m_joinWindows->setChecked(m_plan.joinWindows);
	m_flatWindows->setChecked(m_plan.flatWindows);
	m_bridgeStyle->setCurrentIndex(qMax(0, m_bridgeStyle->findData(m_plan.bridgeStyle)));
	m_deskPosture->setCurrentIndex(m_plan.deskStanding ? 1 : 0);
	m_deskHeight->setValue(Shown(m_plan.DeskTopFeet()));
	m_deskPosture->setEnabled(m_plan.bridgeStyle == "desks");
	m_deskHeight->setEnabled(m_plan.bridgeStyle == "desks");
	m_updating = false;
	RefreshDecks();
	RefreshRooms();
	RefreshPalette();
	ApplyDrawMode();
	RefreshTitle();
}

void DesignerWindow::RefreshDecks()
{
	m_updating = true;
	m_deck->setMaximum(m_plan.Decks());
	m_deck->setValue(m_view->Deck() + 1);
	m_deckLabel->setText(QString("of %1").arg(m_plan.Decks()));
	m_updating = false;
}

void DesignerWindow::RefreshRooms()
{
	m_updating = true;
	const int row = m_rooms->currentRow();
	m_rooms->clear();
	for(const PlanRoom& r : m_plan.rooms)
		m_rooms->addItem(new QListWidgetItem(Swatch(r.colour), QString("%1  (%2)").arg(r.label, r.type)));
	if(!m_plan.rooms.isEmpty())
		m_rooms->setCurrentRow(qBound(0, row, m_plan.rooms.size() - 1));
	m_updating = false;
	OnRoomSelected(m_rooms->currentRow());
}

void DesignerWindow::RefreshPalette()
{
	m_updating = true;
	const int row = m_palette->currentRow();
	m_palette->clear();
	for(int i = 0; i < m_plan.palette.size(); ++i)
	{
		const PlanPaint& p = m_plan.palette.at(i);
		QString text = p.name.isEmpty() ? p.colour.name() : p.name;
		if(p.theme >= 0)
			text += QString("  (empire colour %1)").arg(p.theme + 1);
		if(i == 0)
			text += "  - base coat";
		m_palette->addItem(new QListWidgetItem(Swatch(p.colour), text));
	}
	m_palette->setCurrentRow(qBound(0, row, m_plan.palette.size() - 1));
	m_updating = false;
	OnColourSelected(m_palette->currentRow());
}

void DesignerWindow::ApplyDrawMode()
{
	switch(m_steps->currentIndex())
	{
	case StepShip:
	case StepHull:
		m_view->SetMode(PlanView::HullMode);
		break;
	case StepRooms:
		m_view->SetMode(m_roomTool->currentIndex() == 1 ? PlanView::DoorMode :
				m_roomTool->currentIndex() == 2 ? PlanView::LadderMode :
				m_roomTool->currentIndex() == 3 ? PlanView::WindowMode :
				m_roomTool->currentIndex() == 4 ? PlanView::GroundParkingMode :
				m_roomTool->currentIndex() == 5 ? PlanView::SpaceParkingMode :
				m_roomTool->currentIndex() == 6 ? PlanView::StairMode :
				m_roomTool->currentIndex() == 7 ? PlanView::ContrailMode :
				m_roomTool->currentIndex() == 8 ? PlanView::WeaponMode :
				m_roomTool->currentIndex() == 9 ? PlanView::SkylightMode : PlanView::RoomMode);
		m_emitterRow->setVisible(m_roomTool->currentIndex() == 7);
		break;
	case StepPaint:
		m_view->SetMode(PlanView::PaintMode);
		break;
	default:
		m_view->SetMode(PlanView::RoomMode);
		break;
	}
}

void DesignerWindow::ShowStep(int step)
{
	m_steps->setCurrentIndex(qBound(0, step, m_steps->count() - 1));
}

void DesignerWindow::FrameView()
{
	m_view->FitToWindow();
	m_preview->FrameAll();
}

void DesignerWindow::SetDeck(int deck)
{
	m_view->SetDeck(deck);
	RefreshDecks();
}

void DesignerWindow::OnStepChanged(int step)
{
	ApplyDrawMode();
	if(step == StepBuild)
	{
		if(m_design)
		{
			m_preview->SetHullOpacity(0.25f);
			m_preview->SetDesign(m_design, false);
		}
		return;
	}
	m_preview->SetHullOpacity(step == StepRooms ? 0.35f : 0.9f);
	// The hull does not depend on the step: shaped again only when the view
	// shows the blueprint (from the build step) or nothing, or is out of date.
	if(!m_design || m_designIsBlueprint || (m_shapeStale && m_autoRedraw))
		ShapeSoon();
	else if(m_shapeStale)
		m_status->setText("The 3D view is out of date: press F5 to redraw it.");
}

void DesignerWindow::OnDeckChanged(int deck)
{
	if(m_updating)
		return;
	m_view->SetDeck(deck - 1);
}

void DesignerWindow::OnAboutToChange()
{
	m_undo << m_plan;
	if(m_undo.size() > 100)
		m_undo.removeFirst();
	m_redo.clear();
}

void DesignerWindow::OnPlanChanged(bool hullChanged)
{
	m_dirty = true;
	RefreshTitle();
	if(hullChanged || m_steps->currentIndex() == StepRooms)		// The smoothing gives way to rooms.
		PlanEdited();
}

void DesignerWindow::PlanEdited()
{
	if(m_autoRedraw)
	{
		m_shapeTimer->start();
		return;
	}
	m_shapeStale = true;
	m_hullInfo->setText("The 3D view is out of date: press F5 (View, Redraw the 3D view) to redraw it, "
						"or turn on redrawing while drawing.");
}

void DesignerWindow::OnAutoRedraw(bool on)
{
	if(on == m_autoRedraw)
		return;
	m_autoRedraw = on;
	QSettings().setValue("designer/autoRedraw", on);
	if(on && m_shapeStale)
		ShapeSoon();
}

void DesignerWindow::OnUndo()
{
	if(m_undo.isEmpty())
		return;
	m_redo << m_plan;
	m_plan = m_undo.takeLast();
	m_view->SetDeck(qMin(m_view->Deck(), m_plan.Decks() - 1));
	RefreshAll();
	m_view->update();
	PlanEdited();
}

void DesignerWindow::OnRedo()
{
	if(m_redo.isEmpty())
		return;
	m_undo << m_plan;
	m_plan = m_redo.takeLast();
	m_view->SetDeck(qMin(m_view->Deck(), m_plan.Decks() - 1));
	RefreshAll();
	m_view->update();
	PlanEdited();
}

void DesignerWindow::OnNew()
{
	TriPlan plan;
	plan.name = "New ship";
	SetPlan(plan);
	m_planFile.clear();
	m_dirty = false;
	ShowStep(StepHull);
	RefreshTitle();
}

void DesignerWindow::OnMetric(bool metric)
{
	m_metric = metric;
	QSettings().setValue("designer/metric", metric);
	m_updating = true;
	RefreshAll();
	m_updating = false;
}

// The suffixes, ranges and steps of the length boxes, in feet or meters.
void DesignerWindow::ApplyUnits()
{
	struct Box { QDoubleSpinBox* box; double lo, hi, step; };
	const Box boxes[] = {
		{ m_cellFeet, 2, 20, .5 }, { m_deckFeet, 8, 40, 1 }, { m_floorFeet, .5, 4, .5 }, { m_skin, .5, 10, .5 },
		{ m_windowSill, 0, 6, .5 }, { m_deskHeight, 2.2, 5.5, .1 }, { m_windowTop, 3, 20, .5 }, { m_emitterSize, 1, 200, .5 },
	};
	const bool wasUpdating = m_updating;
	m_updating = true;
	// The emitter width is the view's own setting, not the plan's: kept in feet across the change.
	const double emitterFeet = m_emitterSize->suffix() == " m" ? m_emitterSize->value() / 0.3048 : m_emitterSize->value();
	for(const Box& b : boxes)
	{
		b.box->setDecimals(m_metric ? 2 : 1);
		b.box->setRange(Shown(b.lo), Shown(b.hi));
		b.box->setSingleStep(m_metric ? .1 : b.step);
		b.box->setSuffix(m_metric ? " m" : " ft");
	}
	m_emitterSize->setValue(Shown(emitterFeet));
	m_updating = wasUpdating;
}

void DesignerWindow::OnNewFromModel()
{
	const QString modelFile = QFileDialog::getOpenFileName(this, "Fill the hull from a 3D model", QString(),
			"3D models (*.stl *.3ds *.obj)");
	if(modelFile.isEmpty())
		return;
	QDialog dialog(this);
	dialog.setWindowTitle("New plan from a 3D model");
	QFormLayout* form = new QFormLayout(&dialog);
	form->addRow(Help("The model is cut into slices, one per deck, and the hull triangles of each deck are filled "
					  "in where the model is. Parts thinner than about half a cell are lost, and slopes come out "
					  "in steps of a deck, which the shaping smooths. Touch the grid up by hand afterwards."));
	PlanFromModelOptions options;
	QDoubleSpinBox* length = new QDoubleSpinBox;
	length->setRange(Shown(10), Shown(2000));
	length->setSuffix(m_metric ? " m" : " ft");
	length->setValue(Shown(options.lengthFeet));
	form->addRow("Length", length);
	QSpinBox* decks = new QSpinBox;
	decks->setRange(0, 60);
	decks->setSpecialValueText("From the length");
	decks->setToolTip("How many decks (slices) the model is cut into. The decks stay their height, so more decks "
					  "make a longer ship, which keeps more of a detailed model. The import recommends a length "
					  "when much of the model is lost.");
	form->addRow("Decks", decks);
	connect(decks, QOverload<int>::of(&QSpinBox::valueChanged), length, [length](int n) { length->setEnabled(n == 0); });
	QSpinBox* perDeck = new QSpinBox;
	perDeck->setRange(0, 9);
	perDeck->setSpecialValueText(QString("Recommended (%1)").arg(RecommendedSlicesPerDeck(m_plan.deckFeet)));
	perDeck->setToolTip("How many heights in each deck a triangle is judged at. More follow the model's shape "
						"within a deck more evenly; the deck itself stays one step of the hull.");
	form->addRow("Slices per deck", perDeck);
	QComboBox* method = new QComboBox;
	method->addItem("Slices (works on open models)");
	method->addItem("Closed pieces only (the old way)");
	method->setToolTip("Slices: each slice of the model is flooded from outside, so models made of open panels "
					   "(most models from the internet) keep all their parts, and big models need no thinning. "
					   "Closed pieces: only the parts of the model that are closed solids count.");
	form->addRow("Inside found by", method);	QDoubleSpinBox* threshold = new QDoubleSpinBox;
	threshold->setRange(.05, 1);
	threshold->setSingleStep(.05);
	threshold->setValue(options.threshold);
	threshold->setToolTip("How much of a triangle has to be inside the model for it to be hull: lower for a fuller "
						  "hull, higher for a slimmer one.");
	form->addRow("Inside at least", threshold);
	QCheckBox* flip = new QCheckBox("The model's other end is fore");
	form->addRow(flip);
	QCheckBox* yUp = new QCheckBox("The model was made with y up");
	form->addRow(yUp);
	QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	form->addRow(buttons);
	if(dialog.exec() != QDialog::Accepted)
		return;
	options.lengthFeet = Feet(length->value());
	options.decks = decks->value();
	options.slicesPerDeck = perDeck->value();
	options.method = method->currentIndex() == 0 ? ModelInside::Slices : ModelInside::Solid;
	options.threshold = threshold->value();
	options.flip = flip->isChecked();
	options.yUp = yUp->isChecked();
	ImportModel(modelFile, options);
}

bool DesignerWindow::ImportModel(const QString& modelFile, PlanFromModelOptions options, bool tell)
{
	options.cellFeet = m_plan.cellFeet;
	options.deckFeet = m_plan.deckFeet;

	// A big model takes a while (half a million faces: about a minute and a
	// half), so it is read in the background, with the step it is on shown
	// and a way to stop. The window looked frozen to the user before.
	TriPlan plan;
	QStringList notes;
	QString error;
	bool made = false;
	PlanFromModelReport report;
	if(m_shapeWorker)		// The model caches are shared with the shaping.
	{
		m_shapeWorker->wait();
		ShapingDone();
	}
	options.progress = [this](const QString& what, int done, int total) {
		SetTask(what, done, total);
		return !m_taskStopAsked.load();
	};
	QThread* worker = QThread::create([&]() { made = PlanFromModel(modelFile, options, plan, notes, error, &report); });
	// The progress shows in the task bar at the bottom of the window, which
	// cannot be used meanwhile.
	BeginTask(QString("Reading %1").arg(QFileInfo(modelFile).fileName()), true);
	centralWidget()->setEnabled(false);
	menuBar()->setEnabled(false);
	m_autoRedrawBox->setEnabled(false);
	QEventLoop wait;
	connect(worker, &QThread::finished, &wait, &QEventLoop::quit);
	worker->start();
	wait.exec();
	worker->wait();
	delete worker;
	const bool stopped = m_taskStopAsked.load();
	const qint64 ms = m_taskClock.elapsed();
	EndTask();
	centralWidget()->setEnabled(true);
	menuBar()->setEnabled(true);
	m_autoRedrawBox->setEnabled(true);
	if(!made && stopped)
	{
		m_status->setText("Stopped reading the model.");
		return false;
	}
	if(!made)
	{
		const QString why = QString("The plan could not be made from %1.\n\n%2").arg(QFileInfo(modelFile).fileName(),
				(notes + QStringList(error)).join("\n\n"));
		m_status->setText(why.simplified());
		if(tell)
			QMessageBox::warning(this, "New plan from a 3D model", why);
		return false;
	}
	m_undo << m_plan;
	SetPlan(plan);
	m_planFile.clear();
	m_dirty = true;
	ShowStep(StepHull);
	RefreshTitle();
	m_status->setText(QString("Read %1 in %2 s. ").arg(QFileInfo(modelFile).fileName()).arg(ms / 1000.0, 0, 'f', 1)
			+ notes.join(" "));
	// What went less than well is worth more than a line in the status bar.
	// And a longer ship, when that would keep much more of the model, at the
	// press of a button.
	const QStringList worries = notes.filter(QRegularExpression("not closed|not quite closed|separate pieces|little of|Recommended"));
	if(tell && !worries.isEmpty())
	{
		QMessageBox box(QMessageBox::Information, "New plan from a 3D model",
				QString("The plan was made from %1, but look it over:\n\n%2").arg(QFileInfo(modelFile).fileName(),
						notes.join("\n\n")), QMessageBox::Ok, this);
		QPushButton* again = 0;
		if(report.suggestedLength > 0)
			again = box.addButton(QString("Make it %1 long").arg(m_metric
					? QString("%1 m").arg(Shown(report.suggestedLength), 0, 'f', 0)
					: QString("%1 ft").arg(qRound(report.suggestedLength)))
					+ (report.suggestedDecks > 0 ? QString(" (%1 decks)").arg(report.suggestedDecks) : QString()),
					QMessageBox::AcceptRole);
		box.exec();
		if(again && box.clickedButton() == again)
		{
			options.lengthFeet = report.suggestedLength;
			options.decks = 0;
			return ImportModel(modelFile, options, tell);
		}
	}
	return true;
}

void DesignerWindow::OnOpen()
{
	const QString fileName = QFileDialog::getOpenFileName(this, "Open a plan", QString(), "Triangle plans (*.json)");
	if(!fileName.isEmpty())
		OpenPlan(fileName);
}

void DesignerWindow::OnSave()
{
	if(m_planFile.isEmpty())
	{
		OnSaveAs();
		return;
	}
	QString error;
	if(!m_plan.Save(m_planFile, error))
	{
		QMessageBox::warning(this, "Shipgen designer", error);
		return;
	}
	m_dirty = false;
	RefreshTitle();
	m_status->setText("Saved " + m_planFile);
}

void DesignerWindow::OnSaveAs()
{
	QString fileName = QFileDialog::getSaveFileName(this, "Save the plan", SanitiseName(m_plan.name) + ".json",
			"Triangle plans (*.json)");
	if(fileName.isEmpty())
		return;
	if(!fileName.endsWith(".json", Qt::CaseInsensitive))
		fileName += ".json";
	m_planFile = fileName;
	OnSave();
}

void DesignerWindow::OnExport()
{
	QString fileName = QFileDialog::getSaveFileName(this, "Save the blueprint", SanitiseName(m_plan.name) + ".SoH",
			"Blueprints (*.SoH)");
	if(fileName.isEmpty())
		return;
	if(!fileName.endsWith(".SoH", Qt::CaseInsensitive))
		fileName += ".SoH";
	SaveBlueprint(fileName);
}

bool DesignerWindow::SaveBlueprint(const QString& fileName, bool ask)
{
	if(!BuildNow() && ask)
	{
		if(QMessageBox::question(this, "Shipgen designer",
				"The engine's report has errors, so the game may not take this blueprint. Save it anyway?")
				!= QMessageBox::Yes)
			return false;
	}
	if(!m_design->SaveToFile(fileName, SOH_FILE_TYPE))
	{
		QMessageBox::warning(this, "Shipgen designer", "Could not write " + fileName);
		return false;
	}
	m_status->setText("Saved " + fileName);
	return true;
}

void DesignerWindow::OnAddDeckAbove()
{
	OnAboutToChange();
	m_plan.AddDeck(false);
	m_view->SetDeck(m_plan.Decks() - 1);
	RefreshDecks();
	OnPlanChanged(true);
}

void DesignerWindow::OnAddDeckBelow()
{
	OnAboutToChange();
	m_plan.AddDeck(true);
	m_view->SetDeck(0);
	RefreshDecks();
	OnPlanChanged(true);
}

void DesignerWindow::OnRemoveDeck()
{
	if(m_plan.Decks() <= 1)
		return;
	OnAboutToChange();
	m_plan.RemoveDeck(m_view->Deck());
	m_view->SetDeck(qMin(m_view->Deck(), m_plan.Decks() - 1));
	RefreshDecks();
	OnPlanChanged(true);
}

void DesignerWindow::Flip(bool foreAft, bool portStarboard, bool upsideDown, bool deckOnly)
{
	OnAboutToChange();
	const int deck = m_view->Deck();
	const int lost = m_plan.Flip(foreAft, portStarboard, upsideDown, deckOnly ? deck : -1);
	if(upsideDown && !deckOnly)
		m_view->SetDeck(m_plan.Decks() - 1 - deck);
	RefreshDecks();
	RefreshRooms();
	m_view->update();
	QString what = foreAft && portStarboard ? "Turned around" : foreAft ? "Fore and aft swapped"
			: portStarboard ? "Mirrored" : "Turned upside down";
	QString text = deckOnly ? QString("%1: deck %2.").arg(what).arg(deck + 1) : what + ".";
	if(lost > 0)
		text += QString(" %1 ladder(s), stairs or parking spot(s) no longer fitted and were taken away (Undo brings "
						"them back).").arg(lost);
	if(deckOnly && m_plan.model.asHull)
		text += " The hull is the 3D model itself, which does not change with one deck.";
	m_status->setText(text);
	OnPlanChanged(true);
}

void DesignerWindow::OnResize()
{
	OnAboutToChange();
	m_plan.Resize(m_width->value(), m_depth->value());
	m_view->FitToWindow();
	OnPlanChanged(true);
}

void DesignerWindow::OnSettingsChanged()
{
	if(m_updating)
		return;
	OnAboutToChange();
	m_plan.name = m_name->text().trimmed().isEmpty() ? m_plan.name : m_name->text().trimmed();
	m_plan.templateKey = m_template->currentData().toString();
	m_plan.ftlDrive = m_ftl->currentData().toString();
	m_plan.leaveOut.clear();
	if(m_noShields->isChecked())
		m_plan.leaveOut << "shield";
	if(m_noWeapons->isChecked())
		m_plan.leaveOut << "firecontrol";
	m_plan.fireControlModule = m_fireControl->currentData().toInt();
	m_fireControl->setEnabled(!m_noWeapons->isChecked());
	if(m_noTransporter->isChecked())
		m_plan.leaveOut << "transporter";
	// Only what was changed: a length shown in meters and read back would
	// otherwise creep by the rounding.
	auto read = [this](QDoubleSpinBox* box, double& feet) {
		if(fabs(box->value() - Shown(feet)) > 1e-9)
			feet = Feet(box->value());
	};
	read(m_cellFeet, m_plan.cellFeet);
	read(m_deckFeet, m_plan.deckFeet);
	read(m_floorFeet, m_plan.floorFeet);
	read(m_skin, m_plan.skinFeet);
	m_plan.smoothing = m_smoothing->value();
	m_plan.maxFaces = m_maxFaces->value();
	m_plan.panels = m_panels->value();
	m_plan.model.asHull = m_plan.model.IsSet() && m_modelHull->isChecked();
	m_plan.model.sharp = m_modelHullShape->currentData().toBool();
	m_modelHullShape->setEnabled(m_plan.model.asHull);
	m_plan.slopedDecks = m_slopedDecks->isChecked();
	m_plan.hullStyle = m_hullStyle->currentData().toString();
	m_plan.hullDoorExit = m_hullDoorExit->currentData().toString();
	read(m_windowSill, m_plan.windowSillFeet);
	read(m_windowTop, m_plan.windowTopFeet);
	m_plan.joinWindows = m_joinWindows->isChecked();
	m_plan.flatWindows = m_flatWindows->isChecked();
	m_plan.bridgeStyle = m_bridgeStyle->currentData().toString();
	// A height is the plan's own only once it is changed: until then it follows
	// the usual height for sitting or standing.
	if(fabs(m_deskHeight->value() - Shown(m_plan.DeskTopFeet())) > 1e-9)
		m_plan.deskHeightFeet = Feet(m_deskHeight->value());
	m_plan.deskStanding = m_deskPosture->currentData().toBool();
	m_deskHeight->setValue(Shown(m_plan.DeskTopFeet()));
	m_deskPosture->setEnabled(m_plan.bridgeStyle == "desks");
	m_deskHeight->setEnabled(m_plan.bridgeStyle == "desks");
	if(m_plan.deckFeet - m_plan.floorFeet < 7.0)
		m_status->setText(m_metric ? "A room needs 2.1 m of headroom: the deck height less the floor is less than that."
				: "A room needs 7 feet of headroom: the deck height less the floor is less than that.");
	m_view->update();
	OnPlanChanged(true);
}

void DesignerWindow::OnAddRoom()
{
	// A dialog of its own, so that choosing the new room's type does not
	// change the room picked in the list (the type box there edits it).
	QStringList labels;
	for(const PlanRoom& r : m_plan.rooms)
		labels << r.label;
	auto freeName = [&labels](const QString& type) {
		int n = 1;
		while(labels.contains(QString("%1 %2").arg(type).arg(n)))
			++n;
		return QString("%1 %2").arg(type).arg(n);
	};
	QDialog dialog(this);
	dialog.setWindowTitle("Add a room");
	QFormLayout* form = new QFormLayout(&dialog);
	QComboBox* typeBox = new QComboBox;
	typeBox->addItems(SpacecraftRoomTypes());
	typeBox->setToolTip("The engine's own room types. The stations of the ship go into rooms of the right type: "
						"the helm on the bridge, the engineer in engineering, and so on.");
	static QString lastType;
	if(!lastType.isEmpty())
		typeBox->setCurrentText(lastType);
	form->addRow("Type", typeBox);
	QLineEdit* nameEdit = new QLineEdit(freeName(typeBox->currentText()));
	nameEdit->setToolTip("The room's name in the blueprint. Every room needs its own.");
	form->addRow("Name", nameEdit);
	// The name follows the type until it is typed in.
	bool typed = false;
	connect(nameEdit, &QLineEdit::textEdited, &dialog, [&typed]() { typed = true; });
	connect(typeBox, &QComboBox::currentTextChanged, &dialog, [&](const QString& type) {
		if(!typed)
			nameEdit->setText(freeName(type));
	});
	QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	box->button(QDialogButtonBox::Ok)->setText("Add");
	form->addRow(box);
	connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	QString label;
	while(true)
	{
		if(dialog.exec() != QDialog::Accepted)
			return;
		label = nameEdit->text().trimmed();
		if(!label.isEmpty() && !labels.contains(label))
			break;
		QMessageBox::information(this, "Add a room", label.isEmpty() ? "The room needs a name."
				: "Another room already has that name; every room needs its own.");
	}
	const QString type = typeBox->currentText();
	lastType = type;
	OnAboutToChange();
	PlanRoom room;
	room.type = type;
	room.label = label;
	room.colour = RoomColour(m_plan.rooms.size());
	m_plan.rooms << room;
	RefreshRooms();
	m_rooms->setCurrentRow(m_plan.rooms.size() - 1);
	m_roomTool->setCurrentIndex(0);
	OnPlanChanged(false);
}

void DesignerWindow::OnRemoveRoom()
{
	const int row = m_rooms->currentRow();
	if(row < 0 || row >= m_plan.rooms.size())
		return;
	OnAboutToChange();
	// Take the room off its triangles and move the higher room numbers down.
	for(int d = 0; d < m_plan.Decks(); ++d)
		for(int cy = 0; cy < m_plan.Depth(); ++cy)
			for(int cx = 0; cx < m_plan.Width(); ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const TriRef t(d, cx, cy, k);
					const int r = m_plan.Room(t);
					if(r == row)
						m_plan.SetRoom(t, -1);
					else if(r > row)
						m_plan.SetRoom(t, r - 1);
				}
	m_plan.rooms.removeAt(row);
	m_plan.Tidy();
	RefreshRooms();
	m_view->update();
	OnPlanChanged(false);
}

void DesignerWindow::OnRoomSelected(int row)
{
	if(row < 0 || row >= m_plan.rooms.size())
		return;
	const bool was = m_updating;
	m_updating = true;
	m_roomType->setCurrentText(m_plan.rooms.at(row).type);
	m_roomLabel->setText(m_plan.rooms.at(row).label);
	m_roomWallTex->setCurrentIndex(qMax(0, m_roomWallTex->findData(m_plan.rooms.at(row).wallTex)));
	m_roomFloorTex->setCurrentIndex(qMax(0, m_roomFloorTex->findData(m_plan.rooms.at(row).floorTex)));
	m_roomCeilingTex->setCurrentIndex(qMax(0, m_roomCeilingTex->findData(m_plan.rooms.at(row).ceilingTex)));
	m_updating = was;
	m_view->SetRoom(row);
}

void DesignerWindow::OnRoomEdited()
{
	if(m_updating)
		return;
	const int row = m_rooms->currentRow();
	if(row < 0 || row >= m_plan.rooms.size())
		return;
	const QString label = m_roomLabel->text().trimmed();
	for(int i = 0; i < m_plan.rooms.size(); ++i)
		if(i != row && m_plan.rooms.at(i).label == label)
		{
			m_status->setText("Another room already has that name; every room needs its own.");
			return;
		}
	OnAboutToChange();
	m_plan.rooms[row].type = m_roomType->currentText();
	m_plan.rooms[row].wallTex = m_roomWallTex->currentData().toInt();
	m_plan.rooms[row].floorTex = m_roomFloorTex->currentData().toInt();
	m_plan.rooms[row].ceilingTex = m_roomCeilingTex->currentData().toInt();
	if(!label.isEmpty())
		m_plan.rooms[row].label = label;
	RefreshRooms();
	m_view->update();
	OnPlanChanged(false);
}

void DesignerWindow::OnAddColour()
{
	const QColor c = QColorDialog::getColor(QColor(170, 40, 40), this, "A colour for the hull");
	if(!c.isValid())
		return;
	OnAboutToChange();
	PlanPaint p;
	p.colour = c;
	p.name = c.name();
	m_plan.palette << p;
	RefreshPalette();
	m_palette->setCurrentRow(m_plan.palette.size() - 1);
	OnPlanChanged(false);
}

void DesignerWindow::OnAddEmpireColour()
{
	bool ok = false;
	const int which = QInputDialog::getInt(this, "An empire colour", "Which of the owner's colours (1 to 3)?", 1, 1, 3, 1, &ok);
	if(!ok)
		return;
	OnAboutToChange();
	PlanPaint p;
	p.theme = which - 1;
	static const QColor shown[3] = { QColor(60, 90, 160), QColor(200, 200, 210), QColor(160, 60, 60) };
	p.colour = shown[which - 1];
	p.name = QString("Empire colour %1").arg(which);
	m_plan.palette << p;
	RefreshPalette();
	m_palette->setCurrentRow(m_plan.palette.size() - 1);
	OnPlanChanged(false);
}

void DesignerWindow::OnRemoveColour()
{
	const int row = m_palette->currentRow();
	if(row <= 0 || row >= m_plan.palette.size())
		return;			// The base coat stays.
	OnAboutToChange();
	for(int d = 0; d < m_plan.Decks(); ++d)
		for(int cy = 0; cy < m_plan.Depth(); ++cy)
			for(int cx = 0; cx < m_plan.Width(); ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const TriRef t(d, cx, cy, k);
					const int p = m_plan.Paint(t);
					if(p == row)
						m_plan.SetPaint(t, 0);
					else if(p > row)
						m_plan.SetPaint(t, p - 1);
				}
	m_plan.palette.removeAt(row);
	RefreshPalette();
	OnPlanChanged(true);
}

void DesignerWindow::OnColourSelected(int row)
{
	if(row >= 0)
		m_view->SetPaintIndex(row);
}

void DesignerWindow::RunShapeJob(ShapeJob& job, const PlanProgress& progress)
{
	job.hull = BuildPlanHull(job.plan, progress);
	if(!job.hull.tris.isEmpty())
	{
		if(progress)
			progress("Finding where rooms fit inside the hull", 0, 0);
		job.roomable = RoomableTriangles(job.plan, job.hull);
	}
	if(job.makeDesign)
	{
		if(progress)
			progress(QString("Making the 3D view of %1 faces").arg(job.hull.tris.size()), 0, 0);
		job.design = BuildPlanHullDesign(job.plan, job.hull);
	}
}

void DesignerWindow::ApplyShapeJob(ShapeJob& job)
{
	m_hull = job.hull;
	m_hullShaped = !m_hull.tris.isEmpty();
	// The roomable triangles fit the plan they were found for; after a
	// resize while shaping they wait for the next round.
	m_view->SetHullTris(m_hull.tris);
	if(m_hullShaped && job.plan.Width() == m_plan.Width() && job.plan.Depth() == m_plan.Depth() &&
			job.plan.Decks() == m_plan.Decks())
		m_view->SetRoomable(job.roomable);
	else if(!m_hullShaped)
		m_view->ClearRoomable();

	if(job.design)
	{
		if(m_steps->currentIndex() != StepBuild)
		{
			m_preview->SetDesign(job.design, false);
			delete m_design;
			m_design = job.design;
			m_designIsBlueprint = false;
		}
		else
			delete job.design;		// The blueprint is shown on the build step.
		job.design = 0;
	}
	m_shapeStale = false;

	QString info = m_hullShaped ? QString("%1 faces (%2 before the flat areas were merged), %3")
			.arg(m_hull.tris.size()).arg(m_hull.rawTriangles).arg(m_hull.closed ? "closed" : "NOT closed")
			: QString("No hull yet.");
	if(!m_hull.notes.isEmpty())
		info += "\n" + m_hull.notes.join("\n");
	m_hullInfo->setText(info);
	if(m_frameAfterShape)
	{
		m_frameAfterShape = false;
		m_preview->FrameAll();
	}
}

void DesignerWindow::ShapeSoon()
{
	m_shapeTimer->stop();
	if(m_shapeWorker)
	{
		m_shapeAgain = true;		// After the one under way.
		return;
	}
	m_shapeAgain = false;
	m_shapeJob = std::make_shared<ShapeJob>();
	m_shapeJob->plan = m_plan;
	m_shapeJob->makeDesign = m_steps->currentIndex() != StepBuild;
	BeginTask("Shaping the hull", false);
	std::shared_ptr<ShapeJob> job = m_shapeJob;
	m_shapeWorker = QThread::create([this, job]() {
		RunShapeJob(*job, [this](const QString& step, int done, int total) {
			SetTask(step, done, total);
			return true;
		});
	});
	const int generation = ++m_shapeGeneration;
	connect(m_shapeWorker, &QThread::finished, this, [this, generation]() {
		if(generation == m_shapeGeneration)
			ShapingDone();
	}, Qt::QueuedConnection);
	m_shapeWorker->start();
}

void DesignerWindow::ShapingDone()
{
	if(!m_shapeWorker)
		return;
	++m_shapeGeneration;		// A finished signal still on its way is for this one.
	m_shapeWorker->wait();
	delete m_shapeWorker;
	m_shapeWorker = 0;
	std::shared_ptr<ShapeJob> job = m_shapeJob;
	m_shapeJob.reset();
	const qint64 ms = m_taskClock.elapsed();
	ApplyShapeJob(*job);
	EndTask(ms > 1500 ? QString("Shaped the hull in %1 s.").arg(ms / 1000.0, 0, 'f', 1) : QString());
	if(m_shapeAgain)
	{
		if(m_autoRedraw)
			ShapeSoon();
		else
			PlanEdited();
	}
}

void DesignerWindow::ShapeNow()
{
	// Before the window is up (scripts, the first plan) there is nothing to
	// keep responsive: done here and now.
	if(!isVisible() && !m_shapeWorker)
	{
		m_shapeTimer->stop();
		ShapeJob job;
		job.plan = m_plan;
		job.makeDesign = m_steps->currentIndex() != StepBuild;
		RunShapeJob(job, PlanProgress());
		ApplyShapeJob(job);
		return;
	}
	ShapeSoon();
	// Waits, keeping the window drawn and the task bar moving; edits made
	// meanwhile shape it again after.
	QEventLoop wait;
	while(m_shapeWorker)
	{
		QTimer::singleShot(50, &wait, &QEventLoop::quit);
		wait.exec();
	}
}

void DesignerWindow::OnShape()
{
	m_frameAfterShape = true;
	ShapeSoon();
}

void DesignerWindow::BeginTask(const QString& title, bool canStop)
{
	{
		QMutexLocker locked(&m_taskLock);
		m_taskStep.clear();
		m_taskDone = m_taskTotal = 0;
		m_taskSteps = 0;
	}
	m_taskTitle = title;
	m_taskStopAsked = false;
	m_taskStop->setVisible(canStop);
	m_taskStop->setEnabled(true);
	m_taskClock.start();
	m_taskBox->show();
	RefreshTask();
	m_taskTimer->start();
}

void DesignerWindow::SetTask(const QString& step, int done, int total)
{
	QMutexLocker locked(&m_taskLock);
	if(step != m_taskStep && !(total > 0 && m_taskTotal > 0 && done > 0))
		++m_taskSteps;
	m_taskStep = step;
	m_taskDone = done;
	m_taskTotal = total;
}

void DesignerWindow::RefreshTask()
{
	QString step;
	int done, total, steps;
	{
		QMutexLocker locked(&m_taskLock);
		step = m_taskStep;
		done = m_taskDone;
		total = m_taskTotal;
		steps = m_taskSteps;
	}
	if(total > 0)
	{
		m_taskBar->setRange(0, 1000);
		m_taskBar->setValue(int(1000.0 * qBound(0, done, total) / total));
		m_taskBar->setFormat(QString("%1%").arg(100.0 * qBound(0, done, total) / total, 0, 'f', 0));
	}
	else
	{
		m_taskBar->setRange(0, 0);		// Busy, no count.
		m_taskBar->setFormat(QString());
	}
	QString text = m_taskTitle;
	if(!step.isEmpty())
		text += QString(" \u2013 step %1: %2").arg(qMax(1, steps)).arg(step);
	text += QString("  (%1 s)").arg(m_taskClock.elapsed() / 1000);
	if(m_taskStopAsked)
		text += "  Stopping after this step...";
	m_taskLabel->setText(text);
}

void DesignerWindow::EndTask(const QString& finalText)
{
	m_taskTimer->stop();
	m_taskBox->hide();
	if(!finalText.isEmpty())
		m_status->setText(finalText);
}

bool DesignerWindow::BuildNow()
{
	// The model caches are shared with the shaping.
	if(m_shapeWorker)
	{
		m_shapeWorker->wait();
		ShapingDone();
	}
	QApplication::setOverrideCursor(Qt::WaitCursor);
	BeginTask("Building the blueprint", false);
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	PlanBuildOptions options;
	options.furnish = m_furnish->isChecked();
	options.walkPaths = m_walkPaths->isChecked();
	AuScDesign* design = BuildPlanDesign(m_plan, options, m_report);
	m_preview->SetDesign(0);
	delete m_design;
	m_design = design;
	m_designIsBlueprint = true;
	if(m_steps->currentIndex() == StepBuild)
	{
		m_preview->SetHullOpacity(0.25f);
		m_preview->SetDesign(m_design, false);
	}
	EndTask(QString("Built the blueprint in %1 s.").arg(m_taskClock.elapsed() / 1000.0, 0, 'f', 1));
	QApplication::restoreOverrideCursor();

	QStringList lines;
	lines << QString("Hull: %1 faces, about %2 cubic meters").arg(m_report.hullFaces).arg(qRound(m_report.hullVolumeM3));
	lines << QString("Rooms: %1   Doors: %2   Windows: %3   Ladders: %4   Parking: %5   Ramps: %6   Stairs: %7").arg(m_report.rooms).arg(m_report.doors)
			.arg(m_report.windows).arg(m_report.ladders).arg(m_report.parking).arg(m_report.ramps).arg(m_report.stairs);
	lines << QString("Zero-G joins: %1   Open to the deck below: %2").arg(m_report.zeroGJoins).arg(m_report.openJoins);
	lines << QString("Stations: %1   Berths: %2   Contrail emitters: %3   Weapon emitters: %4").arg(m_report.stations)
			.arg(m_report.berths).arg(m_report.contrails).arg(m_report.weaponEmitters);
	lines << "";
	for(const QString& w : m_report.warnings)
		lines << "Warning: " + w;
	for(const QString& n : m_report.notes)
		lines << "Note: " + n;
	lines << "";
	lines << QString("The engine's report: %1 errors, %2 warnings").arg(m_report.errors.size()).arg(m_report.engineWarnings.size());
	for(const QString& e : m_report.errors)
		lines << "  Error: " + e;
	for(const QString& w : m_report.engineWarnings)
		lines << "  Warning: " + w;
	m_reportText->setPlainText(lines.join("\n"));
	return m_report.ok;
}

void DesignerWindow::OnBuild()
{
	BuildNow();
	m_preview->FrameAll();
}
