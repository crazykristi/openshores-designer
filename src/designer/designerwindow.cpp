//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - draw a ship with triangles and turn it into a blueprint.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "mesh.h"
#include "designerwindow.h"
#include "effects.h"
#include "texturepreview.h"
#include "updater.h"
#include "interior.h"
#include "auscface.h"
#include <QStandardItemModel>


#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
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
#include <QMimeData>
#include <QMutex>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QImage>
#include <QToolButton>
#include <QProgressDialog>
#include <QPushButton>
#include <QLocale>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QThread>
#include <QTreeWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <atomic>

#include "aufilever.h"
#include "auscdesign.h"
#include "building.h"
#include "decal.h"
#include "generate.h"
#include "names.h"
#include "plantools.h"
#include "planview.h"
#include "stats.h"
#include "preview.h"
#include "shipgen.h"
#include "version.h"

// Metric unless the system's locale measures in US units: a guess at what
// the user prefers, until they choose under View > Metric units.
static bool LocaleIsMetric()
{
	return QLocale::system().measurementSystem() != QLocale::ImperialUSSystem;
}

static QString MetresText(double feet)
{
	const double m = feet * 0.3048;
	return m < 10 ? QString::number(m, 'f', 1).replace(".0", "") : QString::number(qRound(m));
}

// The lengths in feet in a text shown to the user, in meters (lengths are
// kept, and the build's report written, in feet, the game's own unit).
static QString MetricText(QString t)
{
	static const QString num = "(\\d+(?:[.,]\\d+)?)";
	auto feet = [](const QString& n) { QString c = n; c.remove(','); return c.toDouble(); };
	// Where meters are already given beside the feet, only the meters.
	t.replace(QRegularExpression(num + " ft \\(about " + num + " m\\)"), "\\2 m");
	t.replace(QRegularExpression(" m \\(" + num + " ft\\)"), " m");
	t.replace(QRegularExpression(", about " + num + " by " + num + " ft"), "");
	QRegularExpression re(num + "(?: by " + num + ")? (sq ft|ft|feet|foot)\\b");
	QString out;
	int at = 0;
	QRegularExpressionMatchIterator it = re.globalMatch(t);
	while(it.hasNext())
	{
		const QRegularExpressionMatch m = it.next();
		out += t.mid(at, m.capturedStart() - at);
		if(m.captured(3) == "sq ft")
			out += QString("%L1 m²").arg(qRound(feet(m.captured(1)) * 0.09290304));
		else
		{
			out += MetresText(feet(m.captured(1)));
			if(!m.captured(2).isEmpty())
				out += " by " + MetresText(feet(m.captured(2)));
			out += " m";
		}
		at = m.capturedEnd();
	}
	return out + t.mid(at);
}

// The room types of a spacecraft, in three groups by what the game does with
// them, each with a tip saying what (ShipRoomUse). The game only reads a room's
// hold access (SetRoomUse); the name is a label (2026-11-07).
static void FillShipRoomTypes(QComboBox* box, bool building = false)
{
	// The list is longer than a screen (a building's above all): a popup that
	// scrolls with the wheel and a scroll bar, not one that shows every item.
	box->setStyleSheet("QComboBox { combobox-popup: 0; }");
	box->setMaxVisibleItems(20);
	// The engine's room table has no turbolift (audef.h, RoomTypeNameInSpacecraft):
	// the designer offers it itself, or it can never be picked.
	QStringList roomTypes = SpacecraftRoomTypes();
	roomTypes << "Turbo Lift";
	if(building)
	{
		// A building's rooms: the names the game's own buildings use, and
		// the types that get a shape (turbolifts, open rooms).
		QStringList shapes;
		for(const QString& type : roomTypes + QStringList("Public Lift"))
			if(IsOpenToDeckBelowType(type) || IsZeroGRoomType(type) || IsLiftRoomType(type))
				shapes << type;
		box->addItem("Building rooms");
		if(QStandardItemModel* model = qobject_cast<QStandardItemModel*>(box->model()))
			if(QStandardItem* item = model->item(box->count() - 1))
				item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		for(const QString& type : BuildingRoomTypes())
			if(!shapes.contains(type))
				box->addItem("    " + type, type);
		box->insertSeparator(box->count());
		box->addItem("Gets a shape");
		if(QStandardItemModel* model = qobject_cast<QStandardItemModel*>(box->model()))
			if(QStandardItem* item = model->item(box->count() - 1))
				item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		for(const QString& type : shapes)
			box->addItem("    " + type, type);
		return;
	}
	QStringList uses, stations, names;
	for(const QString& type : roomTypes)
	{
		const QString t = type.toLower();
		if(ShipRoomHoldAccess(type) != HOLD_Inaccessible)
			uses << type;
		else if(t == "bridge" || t == "transporter room" || t == "sensor room" || t == "shield room" || t == "weapon bay"
				|| IsOpenToDeckBelowType(type) || IsZeroGRoomType(type) || IsLiftRoomType(type))
			stations << type;
		else
			names << type;
	}
	auto group = [box](const QString& title, const QStringList& types) {
		if(types.isEmpty())
			return;
		if(box->count() > 0)
			box->insertSeparator(box->count());
		box->addItem(title);
		// A heading, not a choice.
		if(QStandardItemModel* model = qobject_cast<QStandardItemModel*>(box->model()))
			if(QStandardItem* item = model->item(box->count() - 1))
				item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		for(const QString& type : types)
		{
			box->addItem("    " + type, type);
			box->setItemData(box->count() - 1, ShipRoomUse(type), Qt::ToolTipRole);
		}
	};
	group("Does something in the game", uses);
	group("Gets its stations, or a shape", stations);
	group("Only a name (a corridor in the game)", names);
}

// Picks a room type in such a list, adding it at the end if it is not there
// (an older or a building's type).
static void PickRoomType(QComboBox* box, const QString& type)
{
	int i = box->findData(type);
	if(i < 0)
	{
		box->addItem("    " + type, type);
		box->setItemData(box->count() - 1, ShipRoomUse(type), Qt::ToolTipRole);
		i = box->count() - 1;
	}
	box->setCurrentIndex(i);
}

namespace {

enum Step { StepShip, StepHull, StepRooms, StepCrew, StepPaint, StepBuild };

// A few words on a colour's finish, for the list.
QString FinishSummary(const PlanPaint& p)
{
	QStringList parts;
	if(p.opacity < 1.f)
		parts << QString("%1 % see-through").arg(qRound((1.f - p.opacity) * 100.f));
	if(p.reflect > 0.f)
		parts << (p.reflect >= .8f && p.shine >= 50.f ? "polished" : "sheen");
	if(p.selfLit > 0.f)
		parts << "self-lit";
	if(p.HasGlow())
		parts << "glows";
	if(p.effect > 0 && p.effect < EFF_Count)
		parts << SurfaceEffectName(SURFACE_EFFECT(p.effect));
	return parts.join(", ");
}

QIcon Swatch(const QColor& colour)
{
	QPixmap pm(16, 16);
	pm.fill(colour);
	return QIcon(pm);
}

QWidget* Scrolled(QWidget* page)
{
	QScrollArea* area = new QScrollArea;
	area->setWidget(page);
	area->setWidgetResizable(true);
	area->setFrameShape(QFrame::NoFrame);
	return area;
}

// The 'index'th item of a Build page kind (TriPlan::BuildKinds): its skip
// flag, and its deck and a line about it. 0 past the end, or for a kind
// that has no single items.
bool* BuildItemSkip(TriPlan& plan, const QString& key, int index, int* deck, QString* text)
{
	int n = 0;
	auto cell = [](int d, int x, int y) { return QString("deck %1, cell %2,%3").arg(d + 1).arg(x).arg(y); };
	auto found = [&](bool& skip, int d, const QString& line) -> bool* {
		if(deck)
			*deck = d;
		if(text)
			*text = line;
		return &skip;
	};
	if(key == "doors" || key == "innerWindows" || key == "balconies" || key == "doorButtons")
	{
		const QString kind = key == "innerWindows" ? "window" : key == "balconies" ? "balcony" : "";
		for(PlanDoor& d : plan.doors)
			if(d.kind == kind && (key != "doorButtons" || !d.buttons.isEmpty()) && n++ == index)
			{
				if(key == "doorButtons")
					return 0;		// Buttons go with their door.
				return found(d.skip, d.deck, cell(d.deck, d.cx, d.cy) + (d.row ? QString(", row %1").arg(d.row) : QString()));
			}
		return 0;
	}
	if(key == "decals")
	{
		for(DecalOptions& d : plan.decals)
			if(n++ == index)
				return found(d.skip, 0, QString("%1, %2 ft, %3").arg(QFileInfo(d.image).fileName())
						.arg(d.width, 0, 'f', 0).arg(DecalProjectionName(d.projection)));
		return 0;
	}
	if(key == "windows")
	{
		for(PlanDoor& w : plan.windows)
			if(n++ == index)
				return found(w.skip, w.deck, cell(w.deck, w.cx, w.cy) + (plan.WindowIsHull(w) ? ", follows the hull" : ", framed"));
		return 0;
	}
	if(key == "skylights")
	{
		for(PlanSkylight& w : plan.skylights)
			if(n++ == index)
				return found(w.skip, w.deck, cell(w.deck, w.cx, w.cy));
		return 0;
	}
	if(key == "ladders")
	{
		for(PlanLadder& l : plan.ladders)
			if(n++ == index)
				return found(l.skip, l.deck, cell(l.deck, l.cx, l.cy) + QString(" up to deck %1").arg(l.deck + 2));
		return 0;
	}
	if(key == "hatches")
	{
		for(PlanHatch& h : plan.hatches)
			if(n++ == index)
				return found(h.skip, h.deck, cell(h.deck, h.cx, h.cy) + QString(" to deck %1").arg(h.deck + 2)
						+ (h.row ? QString(", door %1").arg(h.row) : QString()) + (h.open ? ", starts open" : ""));
		return 0;
	}
	if(key == "stairs")
	{
		for(PlanStair& st : plan.stairs)
			if(n++ == index)
				return found(st.skip, st.deck, cell(st.deck, st.cx, st.cy) + QString(" up to deck %1").arg(st.deck + 2)
						+ (st.IsRamp() ? ", ramp" : "") + (st.width > 1 ? QString(", %1 wide").arg(st.width) : QString())
						+ (st.slope.isEmpty() ? QString() : ", " + st.slope));
		return 0;
	}
	if(key == "parking")
	{
		for(PlanParking& p : plan.parking)
			if(n++ == index)
				return found(p.skip, p.deck, QString(p.spacecraft ? "landing spot" : p.space ? "space vehicle" : "ground vehicle")
						+ ", " + cell(p.deck, p.cx, p.cy));
		return 0;
	}
	if(key == "contrails" || key == "weapons" || key == "markings")
	{
		for(PlanEmitter& e : plan.emitters)
		{
			const QString kind = e.marking ? "markings" : e.weapon ? "weapons" : "contrails";
			if(kind == key && n++ == index)
				return found(e.skip, e.deck, cell(e.deck, e.cx, e.cy)
						+ (e.marking == 1 ? ", flag" : e.marking == 2 ? ", name" : QString()));
		}
		return 0;
	}
	if(key == "stations")
	{
		for(PlanStation& st : plan.stations)
			if(n++ == index)
				return found(st.skip, st.deck, st.type + (st.seat.isEmpty() ? QString() : " (" + st.seat + ")")
						+ QString(", deck %1").arg(st.deck + 1));
		return 0;
	}
	if(key == "berths")
	{
		for(PlanBerth& b : plan.berths)
			if(n++ == index)
				return found(b.skip, b.deck, b.kind + QString(" berth, deck %1").arg(b.deck + 1));
		return 0;
	}
	if(key == "pads")
	{
		for(PlanPad& pad : plan.pads)
			if(n++ == index)
				return found(pad.skip, pad.deck, QString("pad, deck %1").arg(pad.deck + 1));
		return 0;
	}
	if(key == "paths")
	{
		for(PlanPath& path : plan.paths)
			if(n++ == index)
				return found(path.skip, path.deck, QString("deck %1, cell %2,%3 to %4,%5").arg(path.deck + 1)
						.arg(path.ax).arg(path.ay).arg(path.bx).arg(path.by));
		return 0;
	}
	return 0;
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
	setAcceptDrops(true);

	m_view = new PlanView;
	m_preview = new DesignPreview;
	m_preview->SetHullOpacity(0.9f);

	QSplitter* split = new QSplitter(Qt::Horizontal);
	m_split = split;
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
	m_steps->addTab(Scrolled(MakeCrewPage()), "4 Crew");
	m_steps->addTab(Scrolled(MakePaintPage()), "5 Paint");
	m_steps->addTab(Scrolled(MakeBuildPage()), "6 Build");
	m_steps->setTabToolTip(StepShip, "The ship's name, kind, grid and decks; turning or flipping it round.");
	m_steps->setTabToolTip(StepHull, "Drawing the outside of the ship, and how the hull is shaped.");
	m_steps->setTabToolTip(StepRooms, "Rooms, doors, windows, ladders, stairs, parking and emitters.");
	m_steps->setTabToolTip(StepCrew, "Optional: stations, berths, transporter pads and walk paths placed by hand; "
										"whatever is not placed here is put in automatically.");
	m_steps->setTabToolTip(StepPaint, "The colours of the hull.");
	m_steps->setTabToolTip(StepBuild, "Turning the plan into a blueprint the game can use, and saving it.");
	sideLayout->addWidget(m_steps, 1);
	side->setMinimumWidth(320);
	side->setMaximumWidth(420);

	QWidget* central = new QWidget;
	QHBoxLayout* centralLayout = new QHBoxLayout(central);
	centralLayout->setContentsMargins(0, 0, 0, 0);
	m_side = side;
	centralLayout->addWidget(side);
	centralLayout->addWidget(split, 1);
	setCentralWidget(central);

	m_status = new QLabel;
	statusBar()->addWidget(m_status, 1);
	m_aiStatus = new QLabel;
	m_aiStatus->setStyleSheet("QLabel { color: #2a7; font-weight: bold; }");
	m_aiStatus->hide();
	statusBar()->addPermanentWidget(m_aiStatus);
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
	file->addAction("&New spaceship", this, SLOT(OnNew()), QKeySequence::New);
	file->addAction("New spaceship from a 3D &model...", this, SLOT(OnNewFromModel()));
	file->addAction("New &fighter (experimental)", this, SLOT(OnNewFighter()));
	file->addAction("New fighter from a 3D model (experimental)...", this, SLOT(OnNewFighterFromModel()));
	file->addAction("New &building...", this, SLOT(OnNewBuilding()));
	file->addAction("New building from a 3D model...", this, [this]() {
		QStringList labels, keys;
		for(const BuildingTemplate& t : AllBuildingTemplates())
		{
			labels << t.label;
			keys << t.key;
		}
		bool ok = false;
		const QString picked = QInputDialog::getItem(this, "New building from a 3D model", "What kind of building? (It "
				"can be changed later, on the first page.)", labels, qMax(0, keys.indexOf("office_block")), false, &ok);
		if(!ok)
			return;
		m_importBuilding = keys.value(labels.indexOf(picked), "office_block");
		NewFromModel(false);
		m_importBuilding.clear();
	});
	file->addAction("&Open plan...", this, SLOT(OnOpen()), QKeySequence::Open);
	file->addAction("Open &blueprint (.SoH)...", this, SLOT(OnOpenBlueprint()));
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
	edit->addSeparator();
	edit->addAction("Turn the ship 90° &clockwise", this, [this]() { Rotate(true, false); });
	edit->addAction("Turn the ship 90° a&nticlockwise", this, [this]() { Rotate(false, false); });
	edit->addAction("Turn this deck 90° clock&wise", this, [this]() { Rotate(true, true); });
	edit->addAction("Turn this deck 90° anticloc&kwise", this, [this]() { Rotate(false, true); });
	QMenu* view = menuBar()->addMenu("&View");
	QAction* metric = view->addAction("&Metric units");
	metric->setCheckable(true);
	metric->setChecked(QSettings().value("designer/metric", LocaleIsMetric()).toBool());
	connect(metric, &QAction::toggled, this, &DesignerWindow::OnMetric);
	m_metric = metric->isChecked();
	QTimer::singleShot(0, this, [this]() { ApplyTextUnits(); });
	view->addSeparator();
	QAction* autoRedraw = view->addAction("Redraw the 3D view &while drawing");
	autoRedraw->setCheckable(true);
	autoRedraw->setChecked(m_autoRedraw);
	connect(autoRedraw, &QAction::toggled, this, &DesignerWindow::OnAutoRedraw);
	connect(m_autoRedrawBox, &QCheckBox::toggled, autoRedraw, &QAction::setChecked);
	connect(autoRedraw, &QAction::toggled, m_autoRedrawBox, &QCheckBox::setChecked);
	QMenu* help = menuBar()->addMenu("&Help");
	m_updater = new Updater(this, [this]() { return SaveIfChanged(); });
	help->addAction("Check for a &new version...", this, [this]() { m_updater->Check(false); });
	QAction* updateAtStart = help->addAction("Look for new versions at start");
	updateAtStart->setCheckable(true);
	updateAtStart->setChecked(Updater::CheckAtStart());
	updateAtStart->setToolTip("Once a day, when the designer starts, it asks GitHub whether there is a newer "
			"release, and says so if there is.");
	connect(updateAtStart, &QAction::toggled, this, [](bool on) { Updater::SetCheckAtStart(on); });
	connect(help, &QMenu::aboutToShow, updateAtStart, [updateAtStart]() {
		updateAtStart->setChecked(Updater::CheckAtStart());
	});
	help->addSeparator();
	help->addAction("&About", this, [this]() {
		QMessageBox::about(this, "About the Shipgen designer",
				SHIPGEN_ABOUT_HTML("Shipgen designer", "Draw a ship with triangles and turn it into a Shores of Hazeron "
								   "blueprint."));
	});
	view->addAction("&Redraw the 3D view", this, SLOT(OnShape()), QKeySequence(Qt::Key_F5));
	QAction* fly = view->addAction("&Fly through the 3D view");
	fly->setCheckable(true);
	fly->setShortcut(QKeySequence(Qt::Key_F6));
	fly->setToolTip("Fly through the model in the 3D view: W A S D move, Q and E down and up, Shift faster, "
					"drag to look round, the wheel sets the speed, Esc leaves.");
	connect(fly, &QAction::toggled, m_preview, &DesignPreview::SetFreeFly);
	connect(m_preview, &DesignPreview::FreeFlyChanged, fly, &QAction::setChecked);
	{
		// The game's animated surfaces in the 3D view: each runs only as the
		// game would run it, in the situation chosen here.
		QMenu* effects = view->addMenu("Surface &effects");
		effects->setToolTipsVisible(true);
		QSettings settings;
		auto option = [&](const QString& text, const QString& key, bool on, const QString& tip) {
			QAction* a = effects->addAction(text);
			a->setCheckable(true);
			a->setChecked(settings.value("designer/effects/" + key, on).toBool());
			a->setToolTip(tip);
			return a;
		};
		QAction* animate = option("&Animate surface effects", "animate", true,
				"Shows the game's animated surfaces (Paint page, Effect) moving in the 3D view, each only when the "
				"game would run it in the situation ticked below.");
		effects->addSeparator();
		QAction* power = option("Systems &powered up", "power", true,
				"The power plant, capacitor, life support and sensors work; the drives and transporters stand by. "
				"Off, nothing runs, as in a ship laid up.");
		QAction* shields = option("&Shields up", "shields", true, "The shield effect runs while the shields are up.");
		QAction* underWay = option("&Under way", "underway", false,
				"The maneuver drive at full power (its bright look); otherwise it holds still (its dim look). "
				"An aerial platform always runs it dim.");
		QAction* ftl = option("&FTL drives charged", "ftl", false,
				"The FTL drives charged: their bright look, and the hull's FTL Hot look (warp or wormhole, by the "
				"drive). Otherwise they are on: their dim look. Buildings never run them.");
		QAction* weapons = option("&Weapons firing", "weapons", false,
				"A weapon's effect runs for two seconds after it fires: this shows them all firing.");
		QAction* transporting = option("&Transporters sending", "transporting", false,
				"The transporter pads' bright look; otherwise the transporters are on: their dim look.");
		effects->addSeparator();
		QAction* mark = option("&Tint effects that never run", "mark", true,
				"Faces with an effect the game never runs on this design (no fire control station for that weapon, "
				"an FTL effect on a building, ...) are tinted pink, and the view says why.");
		effects->addAction("&Which effects run, and when...", this, [this]() {
			const DesignEffects& fx = m_preview->Effects();
			QStringList lines;
			for(int e = 1; e < EFF_Count; ++e)
				if(fx.faces[e])
					lines << QString("%1 (%2 faces): %3").arg(SurfaceEffectName(SURFACE_EFFECT(e))).arg(fx.faces[e])
							.arg(fx.possible[e] ? fx.need[e] : "never: " + fx.need[e]);
			QMessageBox::information(this, "Surface effects", lines.isEmpty()
					? QString("No face of the design in the 3D view has an effect. Effects are set on the Paint page "
							  "(Effect), and show once the 3D view is redrawn.")
					: lines.join("\n"));
		});
		QList<QAction*> all;
		all << animate << power << shields << underWay << ftl << weapons << transporting << mark;
		auto apply = [this, animate, power, shields, underWay, ftl, weapons, transporting, mark]() {
			EffectSituation situation;
			situation.power = power->isChecked();
			situation.shieldsUp = shields->isChecked();
			situation.underWay = underWay->isChecked();
			situation.ftlCharged = ftl->isChecked();
			situation.weaponsFiring = weapons->isChecked();
			situation.transporting = transporting->isChecked();
			for(QAction* a : { power, shields, underWay, ftl, weapons, transporting, mark })
				a->setEnabled(animate->isChecked());
			m_preview->SetEffects(animate->isChecked(), situation, mark->isChecked());
			QSettings settings;
			settings.setValue("designer/effects/animate", animate->isChecked());
			settings.setValue("designer/effects/power", power->isChecked());
			settings.setValue("designer/effects/shields", shields->isChecked());
			settings.setValue("designer/effects/underway", underWay->isChecked());
			settings.setValue("designer/effects/ftl", ftl->isChecked());
			settings.setValue("designer/effects/weapons", weapons->isChecked());
			settings.setValue("designer/effects/transporting", transporting->isChecked());
			settings.setValue("designer/effects/mark", mark->isChecked());
		};
		for(QAction* a : all)
			connect(a, &QAction::toggled, this, apply);
		apply();
	}
	view->addAction("The game's &texture pictures...", this, [this]() {
		const QString now = GameTextureLibrary();
		if(!now.isEmpty())
		{
			QMessageBox::information(this, "The game's textures", "The texture boxes show the game's textures, from " + now + ".");
			return;
		}
		const QString file = QFileDialog::getOpenFileName(this, "The game's texture library (libAuTexture.so.7 or "
				"AuTexture.dll, in the game's folder)", QDir::homePath(), "Libraries (*.so* *.dll);;All files (*)");
		if(file.isEmpty())
			return;
		QString error;
		if(!SetGameTextureLibrary(file, error))
		{
			QMessageBox::warning(this, "The game's textures", error);
			return;
		}
		ApplyTextureIcons();
		m_status->setText("The texture boxes now show the game's textures.");
	});

	m_shapeTimer = new QTimer(this);
	m_shapeTimer->setSingleShot(true);
	m_shapeTimer->setInterval(400);
	connect(m_shapeTimer, &QTimer::timeout, this, &DesignerWindow::ShapeSoon);

	connect(m_view, &PlanView::AboutToChange, this, &DesignerWindow::OnAboutToChange);
	connect(m_view, &PlanView::PlanChanged, this, &DesignerWindow::OnPlanChanged);
	connect(m_view, &PlanView::Hover, this, [this](const QString& t) { m_status->setText(m_metric ? MetricText(t) : t); });
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

// The deck buttons, on the Ship page and again on the Hull page, so that
// decks can be added while drawing the hull.
QGroupBox* DesignerWindow::MakeDeckBox()
{
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
	QPushButton* deckCw = new QPushButton("Turn 90° ↻");
	deckCw->setToolTip("Turns the deck shown a quarter turn clockwise (seen from above) about the grid's middle, "
					   "with its rooms, doors, windows and paint. The grid is made bigger when the turned deck "
					   "would not fit. Ladders and stairs to the decks next to it go when they no longer meet.");
	QPushButton* deckCcw = new QPushButton("Turn 90° ↺");
	deckCcw->setToolTip("Turns the deck shown a quarter turn anticlockwise (seen from above).");
	deckButtons->addWidget(turnDeck, 1, 0, 1, 2);
	deckButtons->addWidget(mirrorDeck, 1, 2);
	deckButtons->addWidget(deckCw, 2, 0);
	deckButtons->addWidget(deckCcw, 2, 1);
	QPushButton* copyDeck = new QPushButton("Copy this deck...");
	copyDeck->setToolTip("Puts copies of the deck shown straight on top of it, as many as you say; the decks "
						 "above move up. The copies have its hull, paint, rooms (as new rooms with their own "
						 "names; a turbolift stays one shaft and goes on up), doors, windows, parking, "
						 "stations and the rest.");
	QPushButton* copyFloor = new QPushButton("Copy the floor...");
	copyFloor->setToolTip("Puts copies of the deck shown's hull outline and paint straight on top of it, as many "
						  "as you say, with no rooms: the same shape, deck after deck, to draw rooms into.");
	deckButtons->addWidget(copyDeck, 3, 0, 1, 2);
	deckButtons->addWidget(copyFloor, 3, 2);
	connect(copyDeck, &QPushButton::clicked, this, [this]() { OnCopyDeck(true); });
	connect(copyFloor, &QPushButton::clicked, this, [this]() { OnCopyDeck(false); });
	connect(deckCw, &QPushButton::clicked, this, [this]() { Rotate(true, true); });
	connect(deckCcw, &QPushButton::clicked, this, [this]() { Rotate(false, true); });
	connect(above, &QPushButton::clicked, this, &DesignerWindow::OnAddDeckAbove);
	connect(below, &QPushButton::clicked, this, &DesignerWindow::OnAddDeckBelow);
	connect(remove, &QPushButton::clicked, this, &DesignerWindow::OnRemoveDeck);
	connect(turnDeck, &QPushButton::clicked, this, [this]() { Flip(true, true, false, true); });
	connect(mirrorDeck, &QPushButton::clicked, this, [this]() { Flip(false, true, false, true); });
	return decks;
}

QWidget* DesignerWindow::MakeShipPage()
{
	QWidget* page = new QWidget;
	QVBoxLayout* layout = new QVBoxLayout(page);
	m_fighterBanner = new QLabel("<b>Fighter mode - EXPERIMENTAL.</b> For small craft such as fighters: small cells "
			"and low decks, one cockpit (a Bridge room) for the pilot, no FTL drive and no automatic berths. Draw the "
			"cockpit, give it a hull door on its side (a ramp or ladder goes down from it, and the game puts a button "
			"by it), a canopy of windows or ceiling windows that follow the hull, and place the pilot's seat in the "
			"Crew step. A hatch in the belly is not possible yet. Try every fighter in the game before relying on it.");
	m_fighterBanner->setWordWrap(true);
	m_fighterBanner->setStyleSheet("QLabel { background: #fff3c4; color: #5a4300; border: 1px solid #d8b13a; "
								   "border-radius: 4px; padding: 6px; }");
	m_fighterBanner->setVisible(false);
	layout->addWidget(m_fighterBanner);
	m_buildingBanner = new QLabel("<b>Building mode.</b> Deck 1 stands on the ground. Draw the outside, the rooms "
			"(building room types) and the doors; a hull door on deck 1 opens at ground level. What the building is "
			"for, its volumes and its parking are set in the Building box below; the build makes a building "
			"blueprint with a site.");
	m_buildingBanner->setWordWrap(true);
	m_buildingBanner->setStyleSheet("QLabel { background: #e3f0ff; color: #1d3c66; border: 1px solid #8fb3e0; "
									"border-radius: 4px; padding: 6px; }");
	m_buildingBanner->setVisible(false);
	layout->addWidget(m_buildingBanner);
	layout->addWidget(Help("The size of the drawing grid and what the ship is for. A cell is a square of four "
						   "triangles; a deck is one floor of the ship."));
	QFormLayout* form = new QFormLayout;
	m_fighter = new QCheckBox("Fighter mode (experimental)");
	m_fighter->setToolTip("A small craft for one pilot (or a few, with seats placed by hand): the fighter kind of "
						  "ship, no FTL drive (it would not fit), windows that follow the hull for the canopy, no "
						  "automatic berths, and decks as low as 4 ft. The cell and deck sizes stay as they are; File, "
						  "New fighter sets them for a new plan (2.5 ft cells, 4.5 ft decks). Experimental: try it in "
						  "the game.");
	form->addRow(m_fighter);
	connect(m_fighter, &QCheckBox::toggled, this, [this](bool on) {
		if(m_updating || on == m_plan.IsFighter())
			return;
		OnAboutToChange();
		if(on)
			m_plan.MakeFighter(false);
		else
		{
			m_plan.mode.clear();
			if(m_plan.templateKey == "fighter")
				m_plan.templateKey = "shuttle";
			m_plan.autoBerths = true;
		}
		RefreshAll();
		OnPlanChanged(false);
		m_status->setText(on ? "Fighter mode is on (experimental)." : "Fighter mode is off.");
	});
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
	m_ftl->setToolTip("The FTL drive the engineer station runs.");
	form->addRow("FTL drive", m_ftl);
	m_maneuver = new QComboBox;
	m_maneuver->addItem("Gravity drive", "gravity");
	m_maneuver->addItem("Rocket drive", "rocket");
	m_maneuver->setToolTip("The drive that moves the ship at sublight speed. A gravity drive runs on the power plant; "
						   "a rocket drive burns fuel and uses no power, so the ship needs more fuel and less power. "
						   "The Expected stats on the Build page show the difference.");
	form->addRow("Maneuver drive", m_maneuver);
	m_station = new QCheckBox("Space station");
	m_station->setToolTip("A space station: the game builds it in space, and it gets no FTL drive (experimental).");
	form->addRow(m_station);
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
	m_fireControl->setToolTip("What the fire control station runs, and every weapon emitter whose bay is 'the "
							  "ship's'. It is harder to change in the game's designer.");
	form->addRow("Fire control", m_fireControl);
	connect(m_fireControl, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DesignerWindow::OnSettingsChanged);
	m_shipForm = form;
	// The fire control choice stays for buildings: a weapons base's bays.
	m_shipOnly = { m_fighter, m_template, m_ftl, m_maneuver, m_station, m_noShields, m_noWeapons, m_noTransporter };
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
	layout->addWidget(MakeBuildingBox());

	QGroupBox* grid = new QGroupBox("Grid");
	QFormLayout* gridForm = new QFormLayout(grid);
	m_width = new QSpinBox;
	m_width->setRange(4, TriPlan::kMaxCells);
	m_width->setToolTip("Cells from aft to fore. At most 400 cells each way (610 m at 5 ft cells); for a longer "
						"ship make the cells bigger.");
	gridForm->addRow("Length (cells)", m_width);
	m_depth = new QSpinBox;
	m_depth->setRange(4, TriPlan::kMaxCells);
	m_depth->setToolTip("Cells from starboard to port.");
	gridForm->addRow("Beam (cells)", m_depth);
	QPushButton* resizeButton = new QPushButton("Change the grid size");
	resizeButton->setToolTip("Makes the grid this many cells long and wide. What is drawn stays in the middle; "
							 "whatever falls off the edge of a smaller grid is lost (Undo brings it back).");
	gridForm->addRow(resizeButton);
	connect(resizeButton, &QPushButton::clicked, this, &DesignerWindow::OnResize);
	layout->addWidget(grid);

	layout->addWidget(MakeDeckBox());

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
	QPushButton* shipCw = new QPushButton("Turn 90° clockwise");
	shipCw->setToolTip("A quarter turn clockwise, seen from above: what pointed fore points to starboard. For a "
					   "3D model that came in sideways. The grid's width and depth swap; everything drawn, and "
					   "the model itself when it is the hull, goes with it.");
	QPushButton* shipCcw = new QPushButton("Turn 90° anticlockwise");
	shipCcw->setToolTip("A quarter turn anticlockwise, seen from above: what pointed fore points to port.");
	flipButtons->addWidget(shipCw, 2, 0);
	flipButtons->addWidget(shipCcw, 2, 1);
	connect(shipCw, &QPushButton::clicked, this, [this]() { Rotate(true, false); });
	connect(shipCcw, &QPushButton::clicked, this, [this]() { Rotate(false, false); });
	connect(turn, &QPushButton::clicked, this, [this]() { Flip(true, true, false, false); });
	connect(mirror, &QPushButton::clicked, this, [this]() { Flip(false, true, false, false); });
	connect(upside, &QPushButton::clicked, this, [this]() { Flip(false, false, true, false); });
	layout->addWidget(flips);
	layout->addStretch(1);

	connect(m_name, &QLineEdit::editingFinished, this, &DesignerWindow::OnSettingsChanged);
	connect(m_template, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DesignerWindow::OnSettingsChanged);
	connect(m_ftl, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DesignerWindow::OnSettingsChanged);
	connect(m_maneuver, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DesignerWindow::OnSettingsChanged);
	for(QCheckBox* box : { m_station, m_noShields, m_noWeapons, m_noTransporter })
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
	layout->addWidget(MakeDeckBox());
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
	// Each tool has a key (Qt::UserRole) that ApplyDrawMode goes by.
	const QList<QPair<QString, QString> > tools = {
		{ "Draw rooms", "The left button paints the triangles of the room picked in the list below; the right "
						"button takes them out of it." },
		{ "Place doors", "Click the triangle that stands on the cell edge the door goes on: between two rooms, "
						 "or between a room and the outside for a hull door. Click again to take it away." },
		{ "Place windows between rooms", "Click the triangle of a room cell that stands on the edge between two "
										 "rooms: a pane from the ship's window sill to its top (drawn light blue and "
										 "dashed). Nobody walks through it. Drag along the wall for a wide one; click "
										 "it again to take it away. A click on a door or balcony makes it a window." },
		{ "Place balconies", "Click the triangle of a room cell that stands on the edge between two rooms: open above "
							 "a wall a third of the room's height (over a room open to the deck below, a gallery; "
							 "drawn orange and dotted). Nobody walks through it. Drag for a wide one; click it again "
							 "to take it away. A click on a door or window makes it a balcony." },
		{ "Place ladders", "Click a cell of a room; the ladder goes up into the room above it." },
		{ "Place doors in the ceiling", "Click a cell of a room with a room over it, on the deck above: a door in the "
										"ceiling (a hatch, drawn amber and dashed). Drag over cells for a rectangle "
										"that becomes one door. A ladder may stand in the same cell: it can only be "
										"climbed while the door is open. Click a door again to take it (all of it) "
										"away; a right click gives it the door texture and the start open or closed "
										"picked below." },
		{ "Place doors in the floor", "Click a cell of a room with a room under it, on the deck below: a door in the "
									  "floor (a hatch, drawn amber). Drag over cells for a rectangle that becomes "
									  "one door. Place a ladder up from the deck below to climb through it. Click a "
									  "door again to take it (all of it) away; a right click gives it the door "
									  "texture and the start open or closed picked below." },
		{ "Place windows", "Click near the edge between a room and the outside, or drag along the walls for a row of "
						   "windows that builds as one window. A click or drag from a window takes them out." },
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
		{ "Place weapon emitters", "Click a hull triangle: a weapon bay that fires from the nearest point of the "
								   "hull (a ring with a cross), of the type picked below. Each gets a fire control "
								   "station of its own running that bay. A right click gives an emitter the type "
								   "picked. A ship whose fire control is left out (Ship page) has no weapon bays." },
		{ "Place ceiling windows (experimental)", "Click a triangle of a room with no room on any deck above it: a window "
												  "goes straight up through the hull there (Shift+click: the whole cell). "
												  "Click one again to take it away. Following the hull, neighbouring "
												  "triangles make one window of any shape; flat, with Join windows, "
												  "neighbouring whole cells make one pane. Try these in the game before "
												  "relying on them." },
		{ "Place spacecraft landing spots", "A 238 ft (about 73 m) square where a whole ship can land, centred on the "
											"grid corner nearest the click: on a deck, or on top of the hull from the "
											"deck above it. It does not need a room. Click on one to take it away." },
		{ "Place empire flags and names", "Click a hull triangle: the owner's flag (or the empire's name) goes flat on "
										  "the nearest point of the hull at mid-deck height, upright, facing out. The "
										  "game paints the owner's own flag or name on it. Click again to take it away." },
		{ "Place rocket pads (buildings)", "A 10 by 10 m space rocket spot, centred on the grid corner nearest the click: "
										   "outside on the ground, on a roof, or on a room's floor. The aircraft "
										   "factory wants one. Click it again to take it away." },
		{ "Place water vehicle parking (buildings)", "A 24 by 12 m spot for a boat, centred on the grid corner nearest "
													 "the click, outside or on a room's floor; R turns it." },
		{ "Draw ground areas (buildings)", "Drag a rectangle on the grid: a field (the game counts one per 4,000 m², "
										   "about 216 by 216 ft), a lawn, paving, cobbles, tarmac, or a build zone "
										   "(platforms only, on top of the deck below). Click an area of the same "
										   "kind, or right click any, to take it away." },
		{ "Place vehicle spots", "A spot for one vehicle, picked below, as the game designer's generic Vehicle "
								 "Spot: a vehicle from the road or air vehicle factory. The box is the vehicle's "
								 "size. Centred on the grid corner nearest the click, on a room's floor (or, for a "
								 "building, outside); R turns it. Click it again to take it away." },
		{ "Place trees, lamps and posts (buildings)", "Click to put the thing picked below on the ground, a roof or a "
													  "room's floor: a tree, a shrub, a street lamp, or a post for a "
													  "citizen, a guard or livestock. Click one again to take it away." },
	};
	const QStringList toolKeys = { "rooms", "doors", "innerWindows", "balconies", "ladders", "ceilingHatches",
			"floorHatches", "windows", "groundParking",
			"spaceParking", "stairs", "contrails", "weapons", "skylights", "spacecraftParking", "markings",
			"rocketParking", "waterParking", "areas", "vehicleParking", "props" };
	for(int i = 0; i < tools.size(); ++i)
	{
		m_roomTool->addItem(tools.at(i).first, toolKeys.at(i));
		m_roomTool->setItemData(m_roomTool->count() - 1, tools.at(i).second, Qt::ToolTipRole);
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
	m_weaponRow = new QWidget;
	{
		QFormLayout* weaponForm = new QFormLayout(m_weaponRow);
		weaponForm->setContentsMargins(0, 0, 0, 0);
		m_weaponBay = new QComboBox;
		m_weaponBay->addItem("The ship's (Ship page)", 0);
		for(const QPair<int, QString>& choice : FireControlModules())
			m_weaponBay->addItem(choice.second, choice.first);
		m_weaponBay->setToolTip("Each weapon emitter is a weapon bay of its own, with its own fire control station "
								"running this. For the emitters placed from now on; a right click on an emitter "
								"gives it this one. Shown as a letter by the emitter.");
		weaponForm->addRow("Weapon bay", m_weaponBay);
		connect(m_weaponBay, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
			m_view->SetWeaponModule(m_weaponBay->currentData().toInt());
		});
	}
	m_weaponRow->setVisible(false);
	layout->addWidget(m_weaponRow);
	m_markingRow = new QWidget;
	{
		QFormLayout* markingForm = new QFormLayout(m_markingRow);
		markingForm->setContentsMargins(0, 0, 0, 0);
		m_markingKind = new QComboBox;
		m_markingKind->addItem("Empire flag", 1);
		m_markingKind->addItem("Empire name", 2);
		m_markingKind->setToolTip("What goes on the hull: the owner's flag (a rectangle 2/3 as high as it is wide; the "
								  "game keeps the flag's own shape inside it) or the empire's name (1/4 as high).");
		markingForm->addRow("Marking", m_markingKind);
		m_markingSize = new QDoubleSpinBox;
		m_markingSize->setRange(1, 500);
		m_markingSize->setDecimals(1);
		m_markingSize->setValue(10);
		m_markingSize->setSuffix(" ft");
		m_markingSize->setToolTip("How wide the flag or name is, for the ones placed from now on.");
		markingForm->addRow("Width", m_markingSize);
		auto apply = [this]() {
			const double v = m_markingSize->value();
			m_view->SetMarking(m_markingKind->currentData().toInt(), m_metric ? v / 0.3048 : v);
		};
		connect(m_markingKind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, apply);
		connect(m_markingSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, apply);
	}
	m_markingRow->setVisible(false);
	layout->addWidget(m_markingRow);
	m_areaRow = new QWidget;
	{
		QFormLayout* f = new QFormLayout(m_areaRow);
		f->setContentsMargins(0, 0, 0, 0);
		QComboBox* box = new QComboBox;
		for(const auto& c : QList<QPair<QString, QString> >{ { "field", "Field (cracked earth)" }, { "grass", "Lawn" },
				{ "paving", "Paving (concrete)" }, { "cobbles", "Cobbles" }, { "tarmac", "Tarmac" },
				{ "buildzone", "Build zone (platforms)" }, { "site", "Site (the building's plot)" } })
			box->addItem(c.second, c.first);
		box->setToolTip("What the rectangles drawn from now on are.\n\nSite: the building's plot, the game's build site. "
				"There is one, on every deck; drawing a new one moves it, a click on it takes it away. Everything "
				"drawn has to be inside it (the hull with its skin, parking, areas, things on the ground): what is "
				"outside shows in red and stops the build. Without a site, the build makes the closest rectangle "
				"round all of it. Sea and aerial platforms have no site.");
		f->addRow("Area", box);
		connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, box](int) {
			m_view->SetAreaKind(box->currentData().toString());
		});
	}
	m_areaRow->setVisible(false);
	layout->addWidget(m_areaRow);
	m_propRow = new QWidget;
	{
		QFormLayout* f = new QFormLayout(m_propRow);
		f->setContentsMargins(0, 0, 0, 0);
		QComboBox* box = new QComboBox;
		for(const auto& c : QList<QPair<QString, QString> >{ { "tree", "Tree" }, { "shrub", "Shrub" },
				{ "lamp", "Street lamp" }, { "citizen", "Citizen post" }, { "guard", "Guard post" },
				{ "livestock", "Livestock post" } })
			box->addItem(c.second, c.first);
		box->setToolTip("What a click puts in from now on. Posts placed here take the place of the Building box's counts.");
		f->addRow("Thing", box);
		connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, box](int) {
			m_view->SetPropKind(box->currentData().toString());
		});
	}
	m_propRow->setVisible(false);
	layout->addWidget(m_propRow);
	m_vehicleRow = new QWidget;
	{
		QFormLayout* f = new QFormLayout(m_vehicleRow);
		f->setContentsMargins(0, 0, 0, 0);
		QComboBox* box = new QComboBox;
		static const QHash<QString, QString> kinds = { { "ground", "road" }, { "space", "air" }, { "water", "water" },
				{ "rocket", "rocket" } };
		for(const PlanVehicle& v : PlanVehicles())
		{
			box->addItem(QString("%1 (%2, %3 x %4 ft%5)").arg(v.name).arg(kinds.value(v.kind)).arg(v.length, 0, 'f', 0)
					.arg(v.width, 0, 'f', 0).arg(v.measured ? "" : ", size of the typed spot"), QString(v.key));
		}
		box->setCurrentIndex(box->findData(QString("SUV")));
		box->setToolTip("The vehicle the spots placed from now on are for. The sizes are the game's own models with "
						"a foot to spare; where the model is not known, the typed spot's size.");
		f->addRow("Vehicle", box);
		connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, box](int) {
			m_view->SetParkingVehicle(box->currentData().toString());
		});
	}
	m_vehicleRow->setVisible(false);
	layout->addWidget(m_vehicleRow);
	m_wallWindowRow = new QWidget;
	{
		QFormLayout* winForm = new QFormLayout(m_wallWindowRow);
		winForm->setContentsMargins(0, 0, 0, 0);
		m_wallWindowStyle = new QComboBox;
		m_wallWindowStyle->addItem("As the ship (Hull page)", "");
		m_wallWindowStyle->addItem("Window frame", "frame");
		m_wallWindowStyle->addItem("Follow the hull (Hull page's glass)", "hull");
		m_wallWindowStyle->addItem("Follow the hull: flat pane, upright", "pane");
		m_wallWindowStyle->addItem("Follow the hull: flat pane on its slope", "slope");
		m_wallWindowStyle->addItem("Follow the hull: its own faces", "faces");
		m_wallWindowStyle->setToolTip("For windows placed from now on. A window frame is cut through the hull as a box, "
									  "with a frame round it; following the hull, the hull's own triangles inside the "
									  "window's outline become the glass, so it keeps the hull's shape and edges, with no "
									  "frame outside (where too few whole triangles fit, it is cut with a frame). As the "
									  "ship: the Hull page's setting. Drawn dotted when following the hull, dashed when "
									  "framed. To change a window, take it out and put it in again.");
		winForm->addRow("Glass", m_wallWindowStyle);
		connect(m_wallWindowStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
			m_view->SetWindowStyle(m_wallWindowStyle->currentData().toString());
		});
	}
	m_wallWindowRow->setVisible(false);
	layout->addWidget(m_wallWindowRow);
	m_skylightRow = new QWidget;
	{
		QFormLayout* skyForm = new QFormLayout(m_skylightRow);
		skyForm->setContentsMargins(0, 0, 0, 0);
		m_skylightShape = new QComboBox;
		m_skylightShape->addItem("Like the wall windows", "");
		m_skylightShape->addItem("Follow the hull", "hull");
		m_skylightShape->addItem("Flat", "flat");
		m_skylightShape->addItem("Canopy", "canopy");
		m_skylightShape->setToolTip("For ceiling windows placed from now on. Following the hull, the glass is the "
									"hull's own surface over the window, in the hull's shape; flat, a level pane in a "
									"cutout. Like the wall windows follows Windows follow the hull's own triangles. "
									"Drawn dotted when following the hull, dashed when flat.\n\n"
									"Canopy: for a cockpit under a bubble of glass (a fighter's). Draw it over the "
									"room's cells under the hull's canopy. The hull over them is glass from the window "
									"sill up, its sides too, and the room's walls there are open above the sill, so "
									"the pilot sees out all round. Drawn filled.");
		skyForm->addRow("Glass", m_skylightShape);
		connect(m_skylightShape, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
			m_view->SetSkylightShape(m_skylightShape->currentData().toString());
		});
	}
	m_skylightRow->setVisible(false);
	layout->addWidget(m_skylightRow);
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
	FillShipRoomTypes(m_roomType);
	m_roomType->setToolTip("What the room picked in the list is for. In the game only a few types do something "
						   "(hover over a type to see what); the stations of the ship go into rooms of the right type.");
	form->addRow("Type", m_roomType);
	auto textureBox = [this](const QString& tip, const QString& none = "Same as the ship") {
		QComboBox* box = new QComboBox;
		box->setIconSize(QSize(32, 32));
		box->addItem(none, -1);
		for(const QPair<int, QString>& texture : InteriorTextures())
			box->addItem(GameTextureIcon(texture.first), texture.second, texture.first);
		box->setToolTip(tip + " The pictures are the game's own textures, when the designer finds the game "
				"(View, The game's texture pictures).");
		m_textureBoxes << box;
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
	m_carHeight = new QDoubleSpinBox;
	m_carHeight->setRange(0, 60);
	m_carHeight->setDecimals(1);
	m_carHeight->setSuffix(" ft");
	m_carHeight->setSpecialValueText("The deck's height");
	m_carHeight->setToolTip("A turbolift is drawn on the same cells of every deck it serves, and stops on each deck "
							"where it has a door (place them with the door tool; doors side by side make one wide "
							"door, for vehicles). Its car carries everyone and everything standing in it. For "
							"vehicles taller than a deck, make the car taller: then draw the lift's cells on the "
							"decks above its highest stop as well, as high as the car reaches.");
	m_carHeightLabel = new QLabel("Car height");
	form->addRow(m_carHeightLabel, m_carHeight);
	m_carHeight->setVisible(false);
	m_carHeightLabel->setVisible(false);
	connect(m_carHeight, &QDoubleSpinBox::editingFinished, this, &DesignerWindow::OnRoomEdited);
	{
		QWidget* row = new QWidget;
		QHBoxLayout* lay = new QHBoxLayout(row);
		lay->setContentsMargins(0, 0, 0, 0);
		const char* names[4] = { "Officers", "Crew", "Passengers", "Troops" };
		for(int i = 0; i < 4; ++i)
		{
			m_access[i] = new QCheckBox(names[i]);
			m_access[i]->setChecked(true);
			m_access[i]->setToolTip("Who may be in this room: officers (the captain and officers), crew, passengers, "
									"troops. All four is everyone, as before. The game keeps this on the room and shows "
									"it in its own designer (\"Restricted to ...\"), but the game client does not hold "
									"anyone to it yet.");
			lay->addWidget(m_access[i]);
			connect(m_access[i], &QCheckBox::toggled, this, &DesignerWindow::OnRoomEdited);
		}
		lay->addStretch();
		form->addRow("Who may enter", row);
	}
	m_doorTex = textureBox("The texture for doors you place from now on. With the door tool, a right click on a door "
			"gives it and the rest of its row this texture, and the way down below.");
	form->addRow("Door texture", m_doorTex);
	connect(m_doorTex, QOverload<int>::of(&QComboBox::activated), this, [this]() {
		m_view->SetDoorTexture(m_doorTex->currentData().toInt());
	});
	const QString doorExitTip = "For hull doors you place from now on. With the door tool, a right click on a door gives "
			"it and the rest of its row these settings, and its texture. A door with a way of its own shows a letter in the "
			"plan: L ladder, R ramp, D docking, A ramp or ladder, S same as the ship.";
	m_doorExit = new QComboBox;
	m_doorExit->addItem("Same as the ship", "");
	m_doorExit->addItem("A ramp or a ladder down", "auto");
	m_doorExit->addItem("A ladder down", "ladder");
	m_doorExit->addItem("A ramp down", "ramp");
	m_doorExit->addItem("None (docking door)", "none");
	m_doorExit->setToolTip(doorExitTip);
	form->addRow("Door's way down", m_doorExit);
	m_doorRampRun = new QDoubleSpinBox;
	m_doorRampRun->setRange(1.0, 8.0);
	m_doorRampRun->setSingleStep(.5);
	m_doorRampRun->setDecimals(1);
	m_doorRampRun->setValue(SHIPGEN_RAMP_RUN_PER_RISE);
	m_doorRampRun->setToolTip(doorExitTip + " Feet of ramp for each foot it falls: more is longer and gentler.");
	form->addRow("Ramp length per height", m_doorRampRun);
	auto doorExitChanged = [this]() {
		const double run = m_doorRampRun->value();
		m_view->SetDoorExit(m_doorExit->currentData().toString(),
				qFuzzyCompare(run, SHIPGEN_RAMP_RUN_PER_RISE) ? 0.0 : run);
	};
	connect(m_doorExit, QOverload<int>::of(&QComboBox::activated), this, doorExitChanged);
	connect(m_doorRampRun, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, doorExitChanged);
	m_doorOpen = new QComboBox;
	m_doorOpen->addItem("Closed", false);
	m_doorOpen->addItem("Open", true);
	m_doorOpen->setToolTip("How doors you place from now on (in walls, floors and ceilings) are when the blueprint is "
			"built. With the door tool or a floor or ceiling door tool, a right click on a door gives it and the rest of "
			"its row this. A door that starts open shows an O in the plan.");
	form->addRow("Doors start", m_doorOpen);
	connect(m_doorOpen, QOverload<int>::of(&QComboBox::activated), this, [this]() {
		m_view->SetDoorOpen(m_doorOpen->currentData().toBool());
	});
	{
		const QString tip = "For the Stairs tool: what the stairs placed from now on are like. A right click on stairs "
				"or a ramp gives it this, where it still fits. A flight climbs from the cell clicked towards the "
				"side clicked; more cells wide go to the left of the way up.";
		QWidget* row = new QWidget;
		QHBoxLayout* h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		m_stairKind = new QComboBox;
		m_stairKind->addItem("Stairs", QString());
		m_stairKind->addItem("Ramp", QString("ramp"));
		m_stairKind->setToolTip(tip);
		m_stairWidth = new QSpinBox;
		m_stairWidth->setRange(1, 3);
		m_stairWidth->setSuffix(" wide");
		m_stairWidth->setToolTip(tip + " Cells across.");
		m_stairSlope = new QComboBox;
		m_stairSlope->addItem("Steep", QString("steep"));
		m_stairSlope->addItem("Normal", QString());
		m_stairSlope->addItem("Gentle", QString("gentle"));
		m_stairSlope->setCurrentIndex(1);
		m_stairSlope->setToolTip(tip + " Steeper is shorter.");
		h->addWidget(m_stairKind);
		h->addWidget(m_stairWidth);
		h->addWidget(m_stairSlope);
		form->addRow("Stairs", row);
		m_stairInfo = new QLabel;
		m_stairInfo->setToolTip(tip);
		form->addRow("", m_stairInfo);
		connect(m_stairKind, QOverload<int>::of(&QComboBox::activated), this, [this]() { UpdateStairStyle(); });
		connect(m_stairSlope, QOverload<int>::of(&QComboBox::activated), this, [this]() { UpdateStairStyle(); });
		connect(m_stairWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { UpdateStairStyle(); });
	}
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
	m_hullWindows = new QCheckBox("Windows follow the hull's own triangles (unless set per window)");
	m_hullWindows->setToolTip("For a hull from a 3D model: the hull's own triangles inside a window's outline become "
			"the glass, so the window follows the model's shape and edges and blends in, with no frame on the outside. "
			"Where too few whole triangles fit, the window is cut as usual.");
	windowForm->addRow(m_hullWindows);
	connect(m_hullWindows, &QCheckBox::toggled, this, &DesignerWindow::OnSettingsChanged);
	m_hullWindowGlass = new QComboBox;
	m_hullWindowGlass->addItem("Flat pane, upright (hull pressed flat round it)", "pane");
	m_hullWindowGlass->addItem("Flat pane on the hull's slope", "slope");
	m_hullWindowGlass->addItem("The hull's own faces as they are", "faces");
	m_hullWindowGlass->setToolTip("How a window that follows the hull gets its glass.\n"
			"Flat pane, upright: the hull in front of the window is pressed flat onto one upright plane, set in to the "
			"nearest point of the hull, and the glass is one clean pane; the hull round it bends in to meet it. Full "
			"view out, nothing of the hull in the middle of the window.\n"
			"Flat pane on the hull's slope: the same, on a plane that leans with the hull there (at most 50 degrees).\n"
			"The hull's own faces: the faces inside the outline become the glass as they are. Keeps the hull's shape "
			"exactly, but a model's thinned hull has big odd faces, which can stand in the view.");
	windowForm->addRow("Hull window glass", m_hullWindowGlass);
	connect(m_hullWindowGlass, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DesignerWindow::OnSettingsChanged);
	m_windowLining = new QCheckBox("Line the hull seen from inside through windows");
	m_windowLining->setToolTip("The game does not draw the hull from inside, so the sky shows through it where it is "
			"seen through a window. On, each such face gets an inward copy (a mesh part, Window Lining). Off saves "
			"those faces.");
	windowForm->addRow(m_windowLining);
	connect(m_windowLining, &QCheckBox::toggled, this, &DesignerWindow::OnSettingsChanged);
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
	QGroupBox* shipTexBox = new QGroupBox("Textures (whole ship)");
	QFormLayout* shipTexForm = new QFormLayout(shipTexBox);
	const QString shipTexTip = "A texture from the game's library for every room or door that does not have one of "
			"its own (the room's own choice comes first).";
	m_shipWallTex = textureBox(shipTexTip, "Usual");
	m_shipFloorTex = textureBox(shipTexTip, "Usual");
	m_shipCeilingTex = textureBox(shipTexTip, "Usual");
	m_shipDoorTex = textureBox(shipTexTip, "Usual");
	shipTexForm->addRow("Walls", m_shipWallTex);
	shipTexForm->addRow("Floors", m_shipFloorTex);
	shipTexForm->addRow("Ceilings", m_shipCeilingTex);
	shipTexForm->addRow("Doors", m_shipDoorTex);
	for(QComboBox* box : { m_shipWallTex, m_shipFloorTex, m_shipCeilingTex, m_shipDoorTex })
		connect(box, QOverload<int>::of(&QComboBox::activated), this, [this](int) { OnSettingsChanged(); });
	layout->addWidget(shipTexBox);
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

QWidget* DesignerWindow::MakeCrewPage()
{
	QWidget* page = new QWidget;
	QVBoxLayout* layout = new QVBoxLayout(page);
	layout->addWidget(Help("<b>All of this is optional.</b> Left alone, the build puts in the stations, berths, "
						   "transporter pads and walk paths the kind of ship needs (Ship page) by itself. Here you can "
						   "place any of them by hand instead: what you place is kept where you put it, and the build "
						   "adds only what is still missing, unless you switch that kind off below. Pick a tool, then "
						   "click on a room's floor; the outline shows green where it fits in one room. R turns it (Shift+R "
						   "the other way), a click on one takes it away. Ladders and stairs always get their paths."));
	m_crewTool = new QComboBox;
	m_crewTool->addItem("Place stations", "stations");
	m_crewTool->addItem("Place berths", "berths");
	m_crewTool->addItem("Place transporter pads", "pads");
	m_crewTool->addItem("Draw walk paths", "paths");
	m_crewTool->setItemData(0, "A station (console) with its seat, facing the way of the arrow.", Qt::ToolTipRole);
	m_crewTool->setItemData(1, "A bed for one of the crew, officers, captain, passengers or troops.", Qt::ToolTipRole);
	m_crewTool->setItemData(2, "A 3 ft pad for the transporter station of the same room. A room with pads placed by hand "
							   "gets no others.", Qt::ToolTipRole);
	m_crewTool->setItemData(3, "Drag from one triangle of a room to another of the same room: people walk straight "
							   "between their middles, on the floor. The automatic paths run through triangle middles "
							   "too, so the two meet.", Qt::ToolTipRole);
	m_crewTool->setToolTip("What the mouse puts on the plan. Hover over a choice in the list for how it works.");
	layout->addWidget(m_crewTool);

	m_stationRow = new QWidget;
	{
		QFormLayout* form = new QFormLayout(m_stationRow);
		form->setContentsMargins(0, 0, 0, 0);
		m_stationType = new QComboBox;
		const QList<QPair<QString, QString> > types = {
			{ "helm", "Helm" }, { "navigator", "Navigator" }, { "command", "Command (captain)" },
			{ "engineer", "Engineer" }, { "firecontrol", "Fire control" }, { "sensor", "Sensors" },
			{ "shield", "Shields" }, { "medic", "Medic" }, { "transporter", "Transporter" },
			{ "powerrelay", "Power relay" } };
		for(const QPair<QString, QString>& t : types)
			m_stationType->addItem(t.second, t.first);
		m_stationType->setToolTip("Which station. One placed by hand counts towards those the ship needs: the build "
								  "puts in only the rest.");
		form->addRow("Station", m_stationType);
		m_stationSeat = new QComboBox;
		m_stationSeat->addItem("As the bridge style (Build page)", "");
		m_stationSeat->addItem("Swivel chair", "chair");
		m_stationSeat->addItem("Integrated desk", "desk");
		m_stationSeat->setToolTip("How this station looks: the game's swivel chair with its console, or an integrated "
								  "desk with built-in screens.");
		form->addRow("Seat", m_stationSeat);
		m_stationModule = new QComboBox;
		m_stationModule->addItem("The ship's (Ship page)", 0);
		for(const QPair<int, QString>& choice : FireControlModules())
			m_stationModule->addItem(choice.second, choice.first);
		m_stationModule->setToolTip("Fire control only: the weapon bay this station runs.");
		form->addRow("Weapon bay", m_stationModule);
		m_stationDrive = new QComboBox;
		m_stationDrive->addItem("The ship's (Ship page)", 0);
		for(const auto& choice : FtlDriveChoices())
			if(FtlDriveModule(choice.first) != CID_Invalid)
				m_stationDrive->addItem(choice.second, int(FtlDriveModule(choice.first)));
		m_stationDrive->setToolTip("Engineer only: the FTL drive this station runs. The ship's is the Ship page's "
								   "FTL drive. A drive picked here is fitted even when the Ship page says none, as "
								   "long as the hull is big enough for one.");
		form->addRow("FTL drive", m_stationDrive);
	}
	layout->addWidget(m_stationRow);
	m_berthRow = new QWidget;
	{
		QFormLayout* form = new QFormLayout(m_berthRow);
		form->setContentsMargins(0, 0, 0, 0);
		m_berthKind = new QComboBox;
		m_berthKind->addItem("Crew", "crew");
		m_berthKind->addItem("Officer", "officer");
		m_berthKind->addItem("Captain", "capt");
		m_berthKind->addItem("Passenger", "passenger");
		m_berthKind->addItem("Troops", "troop");
		m_berthKind->setToolTip("Who sleeps in it. Berths placed by hand count towards those the ship needs.");
		form->addRow("Berth for", m_berthKind);
	}
	m_berthRow->setVisible(false);
	layout->addWidget(m_berthRow);

	QGroupBox* autoBox = new QGroupBox("Put in the rest automatically");
	QVBoxLayout* autoLayout = new QVBoxLayout(autoBox);
	m_autoStations = new QCheckBox("Stations");
	m_autoStations->setToolTip("The stations the kind of ship needs that are not placed by hand, wherever the build "
							   "finds room. Off: only the stations placed by hand.");
	m_autoBerths = new QCheckBox("Berths");
	m_autoBerths->setToolTip("Berths for the rest of the crew and passengers. Off: only the berths placed by hand.");
	m_autoPads = new QCheckBox("Transporter pads");
	m_autoPads->setToolTip("Pads for a transporter room with none placed by hand. Off: only the pads placed by hand.");
	m_autoPaths = new QCheckBox("Walk paths through doors, ladders and stairs");
	m_autoPaths->setToolTip("Off: only the paths drawn by hand (and those up ladders and stairs). Without paths "
							"people in the game cannot find their way round the ship.");
	for(QCheckBox* box : { m_autoStations, m_autoBerths, m_autoPads, m_autoPaths })
	{
		autoLayout->addWidget(box);
		connect(box, &QCheckBox::toggled, this, [this](bool) {
			if(m_updating)
				return;
			OnAboutToChange();
			m_plan.autoStations = m_autoStations->isChecked();
			m_plan.autoBerths = m_autoBerths->isChecked();
			m_plan.autoPads = m_autoPads->isChecked();
			m_plan.autoPaths = m_autoPaths->isChecked();
			OnPlanChanged(false);
		});
	}
	layout->addWidget(autoBox);
	m_crewCounts = new QLabel;
	m_crewCounts->setWordWrap(true);
	layout->addWidget(m_crewCounts);
	m_crewBerthBanner = new QLabel;
	m_crewBerthBanner->setWordWrap(true);
	m_crewBerthBanner->setStyleSheet("QLabel { background: #fff1c2; color: #5a3d00; border: 1px solid #d9a400; "
			"border-radius: 4px; padding: 6px; }");
	m_crewBerthBanner->setVisible(false);
	layout->addWidget(m_crewBerthBanner);
	layout->addStretch(1);

	connect(m_crewTool, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { ApplyDrawMode(); });
	for(QComboBox* box : { m_stationType, m_stationSeat, m_stationModule, m_stationDrive, m_berthKind })
		connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { ApplyCrewChoice(); });
	ApplyCrewChoice();
	return page;
}

void DesignerWindow::ApplyCrewChoice()
{
	const QString type = m_stationType->currentData().toString();
	m_stationModule->setEnabled(type == "firecontrol");
	m_stationDrive->setEnabled(type == "engineer");
	m_view->SetStationChoice(type, m_stationSeat->currentData().toString(),
			type == "engineer" ? m_stationDrive->currentData().toInt() : m_stationModule->currentData().toInt());
	m_view->SetBerthKind(m_berthKind->currentData().toString());
}

void DesignerWindow::RefreshCrew()
{
	if(!m_crewCounts)
		return;
	const bool was = m_updating;
	m_updating = true;
	m_autoStations->setChecked(m_plan.autoStations);
	m_autoBerths->setChecked(m_plan.autoBerths);
	m_autoPads->setChecked(m_plan.autoPads);
	m_autoPaths->setChecked(m_plan.autoPaths);
	m_updating = was;
	m_crewCounts->setText(QString("Placed by hand: %1 stations, %2 berths, %3 transporter pads, %4 walk paths.")
			.arg(m_plan.stations.size()).arg(m_plan.berths.size()).arg(m_plan.pads.size()).arg(m_plan.paths.size()));
	RefreshBerthBanners();
}

// Before a build: with the automatic berths off, only the berths placed by
// hand are built, so they are counted against the stations placed by hand
// (the game counts every console but the transporter as a crew station).
QStringList DesignerWindow::HandBerthCheck() const
{
	return HandBerthWarnings(m_plan);
}

void DesignerWindow::RefreshBerthBanners()
{
	QStringList lines = HandBerthCheck();
	if(!m_builtBerthWarnings.isEmpty())
	{
		if(!lines.isEmpty())
			lines << "";
		lines << "At the last build:";
		lines << m_builtBerthWarnings;
	}
	const QString text = lines.isEmpty() ? QString() :
			"<b>Berths may be short for the stations.</b> You can still build and save; the ship may not work "
			"as intended in the game.<br>" + lines.join("<br>").toHtmlEscaped().replace("&lt;br&gt;", "<br>");
	for(QLabel* banner : { m_berthBanner, m_crewBerthBanner })
		if(banner)
		{
			banner->setText(text);
			banner->setVisible(!lines.isEmpty());
		}
}

void DesignerWindow::RefreshVolumeBox()
{
	if(!m_volumeBox)
		return;
	const bool was = m_updating;
	m_updating = true;
	m_statsQ->setValue(m_plan.statsQuality);
	m_volumeBox->setVisible(!m_plan.IsBuilding());
	const AuScDesign* sc = m_designIsBlueprint ? dynamic_cast<const AuScDesign*>(m_design) : 0;
	const QList<HandModuleKey>& keys = HandModuleKeys();
	quint64 sum = 0;
	for(int i = 0; i < keys.size() && i < m_volumeSpins.size(); ++i)
	{
		m_volumeSpins[i]->setValue(int(m_plan.handModules.*keys.at(i).field));
		quint32 now = 0;
		if(sc)
		{
			const QString key = keys.at(i).key;
			now = key == "powerPlant" ? sc->PowerPlantVolume() : key == "maneuverDrive" ? sc->ManeuverDriveVolume()
				: key == "capacitor" ? sc->CapacitorVolume() : key == "fuel" ? sc->FuelVolume()
				: key == "lifeSupport" ? sc->LifeSupportVolume() : key == "sensors" ? sc->SensorVolume()
				: key == "shields" ? sc->ShieldVolume() : key == "armor" ? sc->ArmorVolume()
				: key == "hold" ? sc->HoldVolume() : sc->SkeletonVolume();
			sum += now;
		}
		m_volumeNow[i]->setText(sc ? QString("now %L1").arg(now) : QString());
		m_volumeNow[i]->setToolTip("Cubic meters in the built blueprint.");
	}
	if(sc)
		m_volumeTotal->setText(QString("Hull %L1 m\u00b3; these systems %L2 m\u00b3 (the FTL drive and weapon bays "
				"have the rest).").arg(sc->HullVolume()).arg(sum));
	else
		m_volumeTotal->setText("Build the blueprint to see the volumes it has.");
	m_updating = was;
}

void DesignerWindow::RefreshStats()
{
	RefreshVolumeBox();
	if(!m_statsText)
		return;
	if(!m_design || !m_designIsBlueprint)
	{
		m_statsText->setHtml("<p>Build the blueprint (above) to see what it is expected to do.</p>");
		return;
	}
	QApplication::setOverrideCursor(Qt::WaitCursor);
	const DesignStats stats = WorkOutStats(*m_design, m_plan.statsQuality);
	QApplication::restoreOverrideCursor();
	QString html;
	const QStringList berth = BerthWarnings(stats);
	if(!berth.isEmpty())
	{
		html += "<p style=\"color:#b06000;\"><b>Berths may be short:</b><br>";
		for(const QString& w : berth)
			html += w.toHtmlEscaped() + "<br>";
		html += "</p>";
	}
	html += QString("<p><b>MATERIALS</b> (used up, at Q%1 or better)<br>").arg(stats.quality);
	for(const QPair<QString, int>& m : stats.materials)
		html += QString("%L1 &times; %2<br>").arg(m.second).arg(m.first.toHtmlEscaped());
	if(!stats.tools.isEmpty())
		html += QString("<i>Tools, not used up:</i> %1").arg(stats.tools.join(", ").toHtmlEscaped());
	html += "</p><hr>";
	html += stats.html;
	m_statsText->setHtml(html);
}

void DesignerWindow::RefreshBuildList()
{
	if(!m_buildList)
		return;
	const bool was = m_updating;
	m_updating = true;
	// Keep which kinds are open.
	QSet<QString> open;
	for(int i = 0; i < m_buildList->topLevelItemCount(); ++i)
		if(m_buildList->topLevelItem(i)->isExpanded())
			open << m_buildList->topLevelItem(i)->data(0, Qt::UserRole).toString();
	m_buildList->clear();
	// Stations, berths, pads and paths are there when placed by hand or put
	// in by the build; the rest only when the plan has some.
	const QSet<QString> always = { "stations", "berths", "pads", "systems", "paths", "liveries", "ramps" };
	for(const QPair<QString, QString>& kind : TriPlan::BuildKinds())
	{
		QList<QTreeWidgetItem*> children;
		QString line;
		int deck;
		for(int i = 0; BuildItemSkip(m_plan, kind.first, i, &deck, &line); ++i)
		{
			QTreeWidgetItem* child = new QTreeWidgetItem(QStringList() << line);
			child->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
			child->setData(0, Qt::UserRole, kind.first);
			child->setData(0, Qt::UserRole + 1, i);
			child->setCheckState(0, *BuildItemSkip(m_plan, kind.first, i, 0, 0) ? Qt::Unchecked : Qt::Checked);
			children << child;
		}
		bool any = !children.isEmpty() || always.contains(kind.first);
		if(kind.first == "doorButtons")
			for(const PlanDoor& d : m_plan.doors)
				any = any || !d.buttons.isEmpty();
		if(kind.first == "liveries")
		{
			any = false;
			for(const PlanPaint& p : m_plan.palette)
				any = any || p.livery;
		}
		if(!any)
		{
			qDeleteAll(children);
			continue;
		}
		QString label = kind.second;
		if(!children.isEmpty())
			label += QString(" (%1)").arg(children.size());
		const bool automatic = kind.first == "stations" ? m_plan.autoStations : kind.first == "berths" ? m_plan.autoBerths
				: kind.first == "pads" ? m_plan.autoPads : kind.first == "paths" ? m_plan.autoPaths : false;
		if(kind.first == "stations" || kind.first == "berths" || kind.first == "pads" || kind.first == "paths")
			label += children.isEmpty() ? (automatic ? QString(" - automatic") : QString(" - none (automatic is off, Crew page)"))
					: automatic ? QString(" - by hand, and the rest automatic") : QString(" - by hand only");
		QTreeWidgetItem* top = new QTreeWidgetItem(QStringList() << label);
		top->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
		top->setData(0, Qt::UserRole, kind.first);
		top->setData(0, Qt::UserRole + 1, -1);
		top->setCheckState(0, m_plan.buildLeaveOut.contains(kind.first) ? Qt::Unchecked : Qt::Checked);
		top->addChildren(children);
		m_buildList->addTopLevelItem(top);
		top->setExpanded(open.contains(kind.first));
	}
	m_updating = was;
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
	m_livery = new QCheckBox("Livery: keep this colour's edges sharp");
	m_livery->setToolTip("For a painted pattern, stripe or insignia that has to stay clear. On a hull shaped from a "
						 "model, or an angular hull, the hull is cut along the outline drawn in this colour, so the "
						 "edge is where you drew it instead of stepping along the hull's own triangles. It costs "
						 "faces. (A hull made from the drawn triangles alone keeps every colour edge already.) "
						 "Livery colours are marked with an L in the list.");
	layout->addWidget(m_livery);
	connect(m_livery, &QCheckBox::toggled, this, [this](bool on) {
		const int row = m_palette->currentRow();
		if(m_updating || row < 0 || row >= m_plan.palette.size() || m_plan.palette[row].livery == on)
			return;
		OnAboutToChange();
		m_plan.palette[row].livery = on;
		RefreshPalette();
		OnPlanChanged(true);
	});

	// The brush: paint straight on the hull in the 3D view.
	QGroupBox* brush = new QGroupBox("Brush in the 3D view");
	QFormLayout* brushForm = new QFormLayout(brush);
	m_brushOn = new QPushButton("Paint in the 3D view");
	m_brushOn->setCheckable(true);
	m_brushOn->setToolTip("Paint straight on the hull in the 3D view with the colour (or texture) picked above: drag "
			"with the left button to paint, the right button turns the view, the wheel zooms. Only the first face "
			"in view under the brush is painted, never what is behind it. The brushed paint stays where it was "
			"brushed when the hull is shaped again.");
	brushForm->addRow(m_brushOn);
	m_brushShape = new QComboBox;
	m_brushShape->addItem("Round", false);
	m_brushShape->addItem("Square", true);
	brushForm->addRow("Shape:", m_brushShape);
	m_brushSize = new QDoubleSpinBox;
	m_brushSize->setRange(0.25, 200);
	m_brushSize->setDecimals(2);
	m_brushSize->setSingleStep(0.5);
	m_brushSize->setSuffix(" ft");
	m_brushSize->setValue(Shown(QSettings().value("designer/brushFeet", 4.0).toDouble()));
	m_brushSize->setToolTip("Across the brush, where it meets the hull. Every face the brush touches is painted "
			"whole, so the hull's own faces set how fine it can paint.");
	brushForm->addRow("Size:", m_brushSize);
	m_brushRubOut = new QCheckBox("Rub out (back to the paint drawn on the plan)");
	brushForm->addRow(m_brushRubOut);
	QPushButton* clearBrushed = new QPushButton("Remove all brushed paint");
	brushForm->addRow(clearBrushed);
	m_brushInfo = new QLabel;
	m_brushInfo->setWordWrap(true);
	brushForm->addRow(m_brushInfo);
	layout->addWidget(brush);
	m_brushTimer = new QTimer(this);
	m_brushTimer->setSingleShot(true);
	m_brushTimer->setInterval(60);
	connect(m_brushTimer, &QTimer::timeout, this, &DesignerWindow::RefreshBrushedHull);
	connect(m_brushOn, &QPushButton::toggled, this, &DesignerWindow::SetBrushOn);
	auto brushChanged = [this]() {
		if(m_updating)
			return;
		QSettings().setValue("designer/brushFeet", Feet(m_brushSize->value()));
		if(m_brushOn->isChecked())
			m_preview->SetBrush(true, Feet(m_brushSize->value()), m_brushShape->currentData().toBool());
	};
	connect(m_brushShape, QOverload<int>::of(&QComboBox::currentIndexChanged), this, brushChanged);
	connect(m_brushSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, brushChanged);
	connect(clearBrushed, &QPushButton::clicked, this, [this]() {
		if(m_plan.strokes.isEmpty())
			return;
		OnAboutToChange();
		m_plan.strokes.clear();
		OnPlanChanged(false);
		RefreshBrushedHull();
		RefreshBrushInfo();
	});
	connect(m_preview, &DesignPreview::BrushBegan, this, [this]() {
		const int row = m_palette->currentRow();
		if(row < 0 || row >= m_plan.palette.size())
			return;
		OnAboutToChange();
		PlanStroke stroke;
		stroke.paint = m_brushRubOut->isChecked() ? -1 : row;
		stroke.square = m_brushShape->currentData().toBool();
		stroke.size = Feet(m_brushSize->value());
		m_plan.strokes << stroke;
		m_stroking = true;
	});
	connect(m_preview, &DesignPreview::BrushStamp, this,
			[this](const QVector3D& at, const QVector3D& eye, const QVector3D& up) {
		if(!m_stroking || m_plan.strokes.isEmpty())
			return;
		// From the game's axes (the 3D view) back to plan space: the turn is
		// a rotation, so its inverse is its transpose.
		const AuMatrix4x4 turn = QuarterTurnToFore();
		auto axis = [&turn](double x, double y, double z) {
			const AuPoint p = turn.Project(AuPoint(x, y, z));
			return QVector3D(float(p.x), float(p.y), float(p.z));
		};
		const QVector3D ex = axis(1, 0, 0), ey = axis(0, 1, 0), ez = axis(0, 0, 1);
		auto back = [&](const QVector3D& v) {
			return QVector3D(QVector3D::dotProduct(ex, v), QVector3D::dotProduct(ey, v), QVector3D::dotProduct(ez, v));
		};
		PlanStroke& stroke = m_plan.strokes.last();
		PlanStamp st;
		st.at = back(at);
		st.eye = back(eye);
		st.up = back(up);
		// A stamp about every third of the brush along the drag.
		if(!stroke.stamps.isEmpty() && (stroke.stamps.last().at - st.at).length() < stroke.size * 0.3)
			return;
		stroke.stamps << st;
		if(!m_brushTimer->isActive())
			m_brushTimer->start();
	});
	connect(m_preview, &DesignPreview::BrushEnded, this, [this]() {
		if(!m_stroking)
			return;
		m_stroking = false;
		if(!m_plan.strokes.isEmpty() && m_plan.strokes.last().stamps.isEmpty())
			m_plan.strokes.removeLast();
		m_brushTimer->stop();
		RefreshBrushedHull();
		OnPlanChanged(false);
		RefreshBrushInfo();
	});

	// The finish: what the game's own designer calls Transparency, Self
	// Illumination, Polish, Glow and the surface effects, for every face
	// painted in this colour.
	QGroupBox* finish = new QGroupBox("Finish of this colour");
	QFormLayout* form = new QFormLayout(finish);
	m_finishPreset = new QComboBox;
	m_finishPreset->addItems(QStringList() << "Choose a ready finish..." << "Matte (plain paint)" << "Satin metal"
							 << "Polished metal" << "Tinted glass" << "Light (self-lit)" << "Energy field");
	m_finishPreset->setToolTip("Sets all the values below at once; change any of them afterwards. The colour "
							   "itself stays.");
	form->addRow("Ready finish:", m_finishPreset);
	auto percent = [](const QString& tip) {
		QSpinBox* box = new QSpinBox;
		box->setRange(0, 100);
		box->setSuffix(" %");
		box->setKeyboardTracking(false);
		box->setToolTip(tip);
		return box;
	};
	m_transparency = percent("How much you see through it: 0 % is solid, 100 % invisible. The game's windows "
							 "are 75 %. On the hull, a see-through colour shows the rooms and the inside of the "
							 "ship from outside, so keep it for glass, lights and energy fields.");
	form->addRow("Transparency:", m_transparency);
	m_selfLit = percent("How much it lights itself: 0 % is lit only by the sun and lamps, 100 % looks the same "
						"in the dark. For lights, screens and glowing parts.");
	form->addRow("Self-lit:", m_selfLit);
	m_shine = new QDoubleSpinBox;
	m_shine->setRange(0, 5000);
	m_shine->setDecimals(0);
	m_shine->setSingleStep(10);
	m_shine->setKeyboardTracking(false);
	m_shine->setToolTip("Polish, part one: how small and sharp the highlight is. Low numbers give a wide soft "
						"sheen, high numbers (up to 1000 or more) a small bright spot, like chrome or glass.");
	form->addRow("Shininess:", m_shine);
	m_reflect = percent("Polish, part two: how bright the highlight is. 0 % is dull paint, 100 % a mirror "
						"finish.");
	form->addRow("Reflectivity:", m_reflect);
	m_glow = new QPushButton;
	m_glow->setToolTip("A colour of light added on top of everything else, as the game's Glow. The brighter the "
					   "colour, the stronger the glow. Black is no glow.");
	form->addRow("Glow:", m_glow);
	m_effect = new QComboBox;
	for(int e = 0; e < EFF_Count; ++e)
		m_effect->addItem(SurfaceEffectName(SURFACE_EFFECT(e)));
	m_effect->setToolTip("One of the game's animated surfaces. They move when the system they are named after "
						 "works (the shield effect when the shields are up, for instance).");
	form->addRow("Effect:", m_effect);
	m_finishNote = new QLabel;
	m_finishNote->setWordWrap(true);
	m_finishNote->setStyleSheet("color: #c08000;");
	form->addRow(m_finishNote);
	layout->addWidget(finish);

	// A texture instead of plain paint: one of the game's or an image of one's
	// own, and how it is wrapped round the hull.
	QGroupBox* texture = new QGroupBox("Texture of this colour");
	QFormLayout* texForm = new QFormLayout(texture);
	m_texKind = new QComboBox;
	m_texKind->addItem("None (plain paint)", "none");
	m_texKind->addItem("A texture of the game's", "game");
	m_texKind->addItem("An image of my own", "file");
	m_texKind->setToolTip("Paint the hull in this colour with a texture instead: one from the game's library, or an "
			"image file of your own (the game holds at most 256 pixels a side; bigger ones are made smaller). Each "
			"texture takes one of the 15 places in the ship's palette of textures, shared with rooms, doors and "
			"decals.");
	texForm->addRow("Texture:", m_texKind);
	m_texLib = new QComboBox;
	m_texLib->setIconSize(QSize(32, 32));
	for(const QPair<int, QString>& t : GameLibraryTextures())
		m_texLib->addItem(GameTextureIcon(t.first), QString("%1 (%2)").arg(t.second).arg(t.first), t.first);
	m_texLib->setToolTip("A texture from the game's own library (the pictures show when the designer finds the "
			"game: View, The game's texture pictures).");
	m_textureBoxes << m_texLib;
	texForm->addRow("Game texture:", m_texLib);
	m_texFile = new QPushButton("Choose an image...");
	m_texFile->setToolTip("An image file of your own (PNG, JPG, ...). It repeats over the hull, so one that tiles "
			"without seams looks best.");
	texForm->addRow("Image:", m_texFile);
	m_texWrap = new QComboBox;
	m_texWrap->addItem("Per face (no stretching)", "face");
	m_texWrap->addItem("Flat, from above", "top");
	m_texWrap->addItem("Flat, from the side", "side");
	m_texWrap->addItem("Flat, from the front", "front");
	m_texWrap->addItem("Round the length (cylinder)", "cylinder");
	m_texWrap->addItem("Round a ball (sphere)", "sphere");
	m_texWrap->setToolTip("How the texture is wrapped round the hull.\n"
			"Per face: each face gets the texture laid flat on it, all the same size; no stretching, but it does "
			"not run on across edges.\n"
			"Flat from above, the side or the front: projected straight through the hull from that way, like a "
			"slide projector; even on faces square to it, stretched on faces that slant away.\n"
			"Round the length: wrapped round the hull's long axis, for a round fuselage or a tower.\n"
			"Round a ball: for round hulls.");
	texForm->addRow("Wrapping:", m_texWrap);
	m_texFeet = new QDoubleSpinBox;
	m_texFeet->setRange(0.25, 1000);
	m_texFeet->setDecimals(2);
	m_texFeet->setSuffix(" ft");
	m_texFeet->setKeyboardTracking(false);
	m_texFeet->setToolTip("How far one repeat of the texture reaches: smaller makes the pattern smaller.");
	texForm->addRow("Size of one repeat:", m_texFeet);
	m_texRotate = new QDoubleSpinBox;
	m_texRotate->setRange(-180, 180);
	m_texRotate->setDecimals(1);
	m_texRotate->setSuffix(" °");
	m_texRotate->setKeyboardTracking(false);
	m_texRotate->setToolTip("Turns the texture on the hull.");
	texForm->addRow("Turn:", m_texRotate);
	m_texTint = percent("How much of the colour itself is laid over the texture: 0 % shows the texture as it is, "
			"100 % only the colour. The game mixes them this way.");
	texForm->addRow("Colour over it:", m_texTint);
	layout->addWidget(texture);
	connect(m_texKind, QOverload<int>::of(&QComboBox::activated), this, [this]() {
		const QString kind = m_texKind->currentData().toString();
		if(kind == "file")
		{
			const QString file = QFileDialog::getOpenFileName(this, "An image for the texture", QString(),
					"Images (*.png *.jpg *.jpeg *.bmp *.gif *.tga *.webp);;All files (*)");
			if(file.isEmpty())
			{
				RefreshTexture();
				return;
			}
			EditTexture([file](PlanPaint& p) { p.texLib = -1; p.texFile = file; });
			return;
		}
		const int lib = m_texLib->currentData().toInt();
		EditTexture([kind, lib](PlanPaint& p) {
			p.texFile.clear();
			p.texLib = kind == "game" ? qMax(0, lib) : -1;
		});
	});
	connect(m_texLib, QOverload<int>::of(&QComboBox::activated), this, [this]() {
		const int lib = m_texLib->currentData().toInt();
		EditTexture([lib](PlanPaint& p) { p.texFile.clear(); p.texLib = lib; });
	});
	connect(m_texFile, &QPushButton::clicked, this, [this]() {
		const QString file = QFileDialog::getOpenFileName(this, "An image for the texture", QString(),
				"Images (*.png *.jpg *.jpeg *.bmp *.gif *.tga *.webp);;All files (*)");
		if(!file.isEmpty())
			EditTexture([file](PlanPaint& p) { p.texLib = -1; p.texFile = file; });
	});
	connect(m_texWrap, QOverload<int>::of(&QComboBox::activated), this, [this]() {
		const QString wrap = m_texWrap->currentData().toString();
		EditTexture([wrap](PlanPaint& p) { p.wrap = wrap; });
	});
	connect(m_texFeet, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
		const double feet = Feet(v);
		EditTexture([feet](PlanPaint& p) { p.texFeet = feet; });
	});
	connect(m_texRotate, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
		EditTexture([v](PlanPaint& p) { p.texRotate = v; });
	});
	connect(m_texTint, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
		EditTexture([v](PlanPaint& p) { p.tint = float(v) / 100.f; });
	});

	connect(m_finishPreset, QOverload<int>::of(&QComboBox::activated), this, [this](int which) {
		if(which <= 0)
			return;
		m_finishPreset->setCurrentIndex(0);
		EditFinish([which](PlanPaint& p) {
			const QColor colour = p.colour;
			PlanPaint plain;
			p.opacity = plain.opacity;
			p.selfLit = plain.selfLit;
			p.shine = plain.shine;
			p.reflect = plain.reflect;
			p.glow = QColor();
			p.effect = 0;
			switch(which)
			{
				case 2:	p.shine = 20; p.reflect = .3f; break;							// Satin metal.
				case 3:	p.shine = 100; p.reflect = 1.f; break;							// Polished metal.
				case 4:	p.opacity = .4f; p.shine = 200; p.reflect = .8f; break;			// Tinted glass.
				case 5:	p.selfLit = 1.f; break;											// Light.
				case 6:	p.opacity = .5f; p.selfLit = 1.f; p.glow = colour; p.effect = EFF_Shield; break;
				default: break;															// Matte.
			}
		});
	});
	connect(m_transparency, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
		EditFinish([v](PlanPaint& p) { p.opacity = float(100 - v) / 100.f; });
	});
	connect(m_selfLit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
		EditFinish([v](PlanPaint& p) { p.selfLit = float(v) / 100.f; });
	});
	connect(m_shine, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
		EditFinish([v](PlanPaint& p) { p.shine = float(v); });
	});
	connect(m_reflect, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
		EditFinish([v](PlanPaint& p) { p.reflect = float(v) / 100.f; });
	});
	connect(m_effect, QOverload<int>::of(&QComboBox::activated), this, [this](int v) {
		EditFinish([v](PlanPaint& p) { p.effect = v; });
	});
	connect(m_glow, &QPushButton::clicked, this, [this]() {
		const int row = m_palette->currentRow();
		if(row < 0 || row >= m_plan.palette.size())
			return;
		const PlanPaint& p = m_plan.palette.at(row);
		const QColor c = QColorDialog::getColor(p.HasGlow() ? p.glow : QColor(Qt::black), this,
												"The glow (black for none)");
		if(!c.isValid())
			return;
		EditFinish([c](PlanPaint& p) { p.glow = (c.red() || c.green() || c.blue()) ? c : QColor(); });
	});
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
	layout->addWidget(new QLabel("What gets built (kept in the plan either way):"));
	m_buildList = new QTreeWidget;
	m_buildList->setHeaderHidden(true);
	m_buildList->setMinimumHeight(220);
	m_buildList->setToolTip("Untick a kind to leave all of it out of the blueprint, or open it and untick single "
							"items. Nothing is deleted: the plan keeps them, and ticking them again builds them. "
							"Clicking an item shows its deck. Saved with the plan.");
	layout->addWidget(m_buildList);
	connect(m_buildList, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int) {
		if(m_updating)
			return;
		const QString key = item->data(0, Qt::UserRole).toString();
		const int index = item->data(0, Qt::UserRole + 1).toInt();
		const bool on = item->checkState(0) == Qt::Checked;
		OnAboutToChange();
		if(index < 0)
		{
			m_plan.buildLeaveOut.removeAll(key);
			if(!on)
				m_plan.buildLeaveOut << key;
		}
		else if(bool* skip = BuildItemSkip(m_plan, key, index, 0, 0))
			*skip = !on;
		m_dirty = true;
		RefreshTitle();
		m_view->update();
	});
	connect(m_buildList, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
		int deck = -1;
		if(item->data(0, Qt::UserRole + 1).toInt() >= 0
				&& BuildItemSkip(m_plan, item->data(0, Qt::UserRole).toString(), item->data(0, Qt::UserRole + 1).toInt(), &deck, 0)
				&& deck >= 0 && deck != m_view->Deck())
			SetDeck(deck);
	});
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
	layout->addWidget(MakeDecalBox());
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

	m_berthBanner = new QLabel;
	m_berthBanner->setWordWrap(true);
	m_berthBanner->setStyleSheet("QLabel { background: #fff1c2; color: #5a3d00; border: 1px solid #d9a400; "
			"border-radius: 4px; padding: 6px; }");
	m_berthBanner->setVisible(false);
	layout->addWidget(m_berthBanner);

	m_buildTabs = new QTabWidget;
	m_buildTabs->addTab(m_reportText, "Report");
	m_buildTabs->addTab(MakeStatsTab(), "Expected stats");
	m_buildTabs->setTabToolTip(1, "What the blueprint is expected to do with modules and materials of a quality you "
			"choose, and what it is made of, as the game's own report code works it out. Only a guide: check it in "
			"the game.");
	layout->addWidget(m_buildTabs, 1);
	connect(build, &QPushButton::clicked, this, &DesignerWindow::OnBuild);
	connect(save, &QPushButton::clicked, this, &DesignerWindow::OnExport);
	return page;
}

QWidget* DesignerWindow::MakeStatsTab()
{
	QWidget* page = new QWidget;
	QVBoxLayout* layout = new QVBoxLayout(page);
	QLabel* caveat = new QLabel(QString("<b>Only a guide.</b> ") + QString(kStatsCaveat).toHtmlEscaped());
	caveat->setWordWrap(true);
	caveat->setStyleSheet("QLabel { background: #fff1c2; color: #5a3d00; border: 1px solid #d9a400; "
			"border-radius: 4px; padding: 6px; }");
	layout->addWidget(caveat);

	QHBoxLayout* qRow = new QHBoxLayout;
	qRow->addWidget(new QLabel("Quality (Q) of the modules and materials"));
	m_statsQ = new QSpinBox;
	m_statsQ->setRange(1, 255);
	m_statsQ->setKeyboardTracking(false);
	m_statsQ->setToolTip("The stats are worked out as if every module and material were of this quality. In the "
			"game the quality comes from what you build with. Q87 is the game's baseline: drives, capacitors and "
			"sensors do what their size says; lower is weaker, higher stronger. Only for the stats: it is not put "
			"in the blueprint. Saved with the plan.");
	qRow->addWidget(m_statsQ);
	qRow->addStretch(1);
	layout->addLayout(qRow);
	connect(m_statsQ, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int q) {
		if(m_updating || q == m_plan.statsQuality)
			return;
		m_plan.statsQuality = q;
		m_dirty = true;
		RefreshTitle();
		RefreshStats();
	});

	m_volumeBox = new QGroupBox("System volumes (ships)");
	QGridLayout* grid = new QGridLayout(m_volumeBox);
	m_volumeHelp = new QLabel("Auto: the build's share. A number is used as it is; the structure makes up the "
			"difference.");
	m_volumeHelp->setWordWrap(true);
	m_volumeHelp->setMinimumHeight(m_volumeHelp->fontMetrics().lineSpacing() * 2 + 4);
	m_volumeBox->setToolTip("Left on auto, the build shares the hull out the way the kind of ship has it. A number "
			"is put in as it is, and the structure (the framework) grows or shrinks to make up the difference. "
			"Changes show in the stats at once and go into the blueprint when you save it; going back to auto "
			"takes effect at the next build. The FTL drive and weapon bays are sized by the build.");
	grid->addWidget(m_volumeHelp, 0, 0, 1, 3);
	int row = 1;
	for(const HandModuleKey& k : HandModuleKeys())
	{
		QSpinBox* spin = new QSpinBox;
		spin->setRange(0, 10000000);
		spin->setSpecialValueText("auto");
		spin->setSuffix(QString::fromUtf8(" m\u00b3"));
		spin->setKeyboardTracking(false);
		spin->setGroupSeparatorShown(true);
		spin->setToolTip(QString("Cubic meters of %1, or auto for the build's share.").arg(QString(k.label).toLower()));
		QLabel* now = new QLabel;
		grid->addWidget(new QLabel(k.label), row, 0);
		grid->addWidget(spin, row, 1);
		grid->addWidget(now, row, 2);
		m_volumeSpins << spin;
		m_volumeNow << now;
		const int index = row - 1;
		connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, index](int value) {
			if(m_updating)
				return;
			const HandModuleKey& k = HandModuleKeys().at(index);
			if(m_plan.handModules.*k.field == quint32(value))
				return;
			OnAboutToChange();
			m_plan.handModules.*k.field = quint32(value);
			m_dirty = true;
			RefreshTitle();
			// Into the built blueprint at once, so that what is saved is what
			// the stats show.
			if(m_designIsBlueprint)
				if(AuScDesign* sc = dynamic_cast<AuScDesign*>(m_design))
				{
					QStringList warnings;
					ApplyHandVolumes(*sc, m_plan.handModules, warnings);
					if(!warnings.isEmpty())
						m_status->setText(warnings.first());
				}
			RefreshStats();
		});
		++row;
	}
	m_volumeTotal = new QLabel;
	m_volumeTotal->setWordWrap(true);
	grid->addWidget(m_volumeTotal, row, 0, 1, 3);
	grid->setColumnStretch(2, 1);
	layout->addWidget(m_volumeBox);

	m_statsText = new QTextBrowser;
	m_statsText->setMinimumHeight(300);
	m_statsText->setOpenLinks(false);
	layout->addWidget(m_statsText, 1);
	return page;
}

// The directions a decal can be projected from, as in DecalFromDirection.
static const struct { const char* label; double x, y, z; } kDecalSides[] = {
	{ "Starboard (+x)", 1, 0, 0 }, { "Port (-x)", -1, 0, 0 }, { "Top", 0, 0, 1 }, { "Bottom", 0, 0, -1 },
	{ "Bow (+y)", 0, 1, 0 }, { "Stern (-y)", 0, -1, 0 } };
static const int kDecalSideCount = 6;	// The last entry of the combo box is "Square to the surface".

QWidget* DesignerWindow::MakeDecalBox()
{
	QGroupBox* box = new QGroupBox("Decals: images on the hull");
	m_decalBox = box;
	box->setAcceptDrops(true);
	box->installEventFilter(this);
	box->setToolTip("Drop an image file here to add it as a decal.");
	QVBoxLayout* layout = new QVBoxLayout(box);
	QLabel* help = new QLabel("An image laid over the hull, at most 256 pixels (bigger ones are made smaller). "
			"A transparent background leaves only the picture. Add one, then click the 3D view of the built "
			"blueprint where it should go.");
	help->setWordWrap(true);
	layout->addWidget(help);
	QHBoxLayout* row = new QHBoxLayout;
	m_decalList = new QListWidget;
	m_decalList->setMaximumHeight(80);
	m_decalList->setToolTip("The decals of this plan. Pick one to change it. The Build list above can leave one out.");
	row->addWidget(m_decalList, 1);
	QVBoxLayout* listButtons = new QVBoxLayout;
	QPushButton* add = new QPushButton("Add image...");
	add->setToolTip("Pick an image file (PNG keeps a transparent background) for a new decal.");
	QPushButton* remove = new QPushButton("Remove");
	remove->setToolTip("Takes the decal picked in the list away.");
	listButtons->addWidget(add);
	listButtons->addWidget(remove);
	listButtons->addStretch();
	row->addLayout(listButtons);
	layout->addLayout(row);

	m_decalForm = new QWidget;
	QFormLayout* form = new QFormLayout(m_decalForm);
	form->setContentsMargins(0, 0, 0, 0);
	m_decalPlace = new QPushButton("Place: click the 3D view");
	m_decalPlace->setCheckable(true);
	m_decalPlace->setToolTip("While this is down, a click on the hull in the 3D view puts the decal's centre "
			"there. Dragging still turns the view.");
	form->addRow(m_decalPlace);
	m_decalProjection = new QComboBox;
	m_decalProjection->addItem("Wrap round the hull", "wrap");
	m_decalProjection->addItem("Flat, like a projector", "flat");
	m_decalProjection->setToolTip("Wrap: round the hull's long axis, so the image keeps its width round a curved "
			"hull (good for ships). Flat: straight along one direction (good for flat sides and buildings); it "
			"stretches on steep slopes.");
	form->addRow("Projection", m_decalProjection);
	m_decalFrom = new QComboBox;
	for(int i = 0; i < kDecalSideCount; ++i)
		m_decalFrom->addItem(kDecalSides[i].label);
	m_decalFrom->addItem("Square to the surface");
	m_decalFrom->setToolTip("Which way the image is projected onto the hull. A click on the 3D view picks the "
			"side nearest to the way the hull faces there; pick Square to the surface to follow it exactly.");
	form->addRow("From", m_decalFrom);
	m_decalWidth = new QDoubleSpinBox;
	m_decalWidth->setRange(0.5, 2000);
	m_decalWidth->setDecimals(1);
	m_decalWidth->setSuffix(" ft");
	m_decalWidth->setToolTip("How wide the image is; its height follows from its shape.");
	form->addRow("Width", m_decalWidth);
	m_decalRotate = new QDoubleSpinBox;
	m_decalRotate->setRange(-360, 360);
	m_decalRotate->setDecimals(0);
	m_decalRotate->setSuffix(" deg");
	m_decalRotate->setToolTip("Turns the image anticlockwise, as seen looking at it.");
	form->addRow("Rotate", m_decalRotate);
	m_decalMirror = new QCheckBox("Mirror the image");
	form->addRow(m_decalMirror);
	m_decalSides = new QComboBox;
	m_decalSides->addItem("One side", "one");
	m_decalSides->addItem("Both sides, reading the right way", "both");
	m_decalSides->addItem("Both sides, mirror image", "both-mirrored");
	m_decalSides->setToolTip("A copy on the other side (port and starboard). Reading the right way suits text and "
			"logos; a mirror image suits arrows and stripes that should point the same way on both sides.");
	form->addRow("Sides", m_decalSides);
	m_decalSelfLit = new QDoubleSpinBox;
	m_decalSelfLit->setRange(0, 1);
	m_decalSelfLit->setSingleStep(.05);
	m_decalSelfLit->setToolTip("How much the decal lights itself, from 0 (only lit by the sun) to 1.");
	form->addRow("Self-lit", m_decalSelfLit);
	m_decalOpacity = new QDoubleSpinBox;
	m_decalOpacity->setRange(.05, 1);
	m_decalOpacity->setSingleStep(.05);
	m_decalOpacity->setToolTip("Below 1 the hull shows through the decal. An image with soft edges is blended "
			"either way.");
	form->addRow("Opacity", m_decalOpacity);
	layout->addWidget(m_decalForm);
	m_decalStatus = new QLabel;
	m_decalStatus->setWordWrap(true);
	layout->addWidget(m_decalStatus);

	connect(m_decalList, &QListWidget::currentRowChanged, this, [this](int) {
		if(!m_updating)
			RefreshDecals();
	});
	connect(add, &QPushButton::clicked, this, [this]() {
		const QString file = QFileDialog::getOpenFileName(this, "A decal image", QString(),
				"Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)");
		if(!file.isEmpty())
			AddDecalImage(file);
	});
	connect(remove, &QPushButton::clicked, this, [this]() {
		const int i = m_decalList->currentRow();
		if(i < 0 || i >= m_plan.decals.size())
			return;
		OnAboutToChange();
		m_plan.decals.removeAt(i);
		m_dirty = true;
		RefreshTitle();
		RefreshDecals();
		RefreshBuildList();
		ReapplyDecals(false);
	});
	connect(m_decalPlace, &QPushButton::toggled, this, [this](bool on) {
		if(on && !m_designIsBlueprint)
		{
			m_decalPlace->setChecked(false);
			m_decalStatus->setText("Build the blueprint first: the decal is placed on the built hull.");
			return;
		}
		m_preview->SetPicking(on);
	});
	connect(m_preview, &DesignPreview::HullPicked, this, &DesignerWindow::OnDecalPicked);
	auto changed = [this]() {
		if(m_updating)
			return;
		DecalFormToPlan();
	};
	connect(m_decalProjection, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
	connect(m_decalFrom, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
	connect(m_decalSides, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
	connect(m_decalMirror, &QCheckBox::toggled, this, changed);
	for(QDoubleSpinBox* spin : { m_decalWidth, m_decalRotate, m_decalSelfLit, m_decalOpacity })
		connect(spin, &QDoubleSpinBox::editingFinished, this, changed);
	RefreshDecals();
	return box;
}

void DesignerWindow::AddDecalImage(const QString& file)
{
	OnAboutToChange();
	DecalOptions d;
	d.image = QFileInfo(file).absoluteFilePath();
	d.projection = m_plan.IsBuilding() ? DecalProjection::Flat : DecalProjection::Wrap;
	m_plan.decals << d;
	m_dirty = true;
	RefreshTitle();
	RefreshDecals();
	m_decalList->setCurrentRow(m_plan.decals.size() - 1);
	if(!ReapplyDecals(true))
		return;
	RefreshBuildList();
	if(m_designIsBlueprint)
		m_decalPlace->setChecked(true);
}

static const QStringList kDecalImageSuffixes = { "png", "jpg", "jpeg", "bmp", "gif", "webp" };
static const QStringList kPlanSuffixes = { "json", "soh" };

QString DesignerWindow::DroppedFile(const QMimeData* data, const QStringList& suffixes)
{
	if(!data || !data->hasUrls())
		return QString();
	for(const QUrl& url : data->urls())
	{
		if(!url.isLocalFile())
			continue;
		const QString file = url.toLocalFile();
		if(suffixes.contains(QFileInfo(file).suffix().toLower()) && QFileInfo(file).isFile())
			return file;
	}
	return QString();
}

void DesignerWindow::dragEnterEvent(QDragEnterEvent* event)
{
	if(!DroppedFile(event->mimeData(), kPlanSuffixes).isEmpty())
		event->acceptProposedAction();
}

void DesignerWindow::dragMoveEvent(QDragMoveEvent* event)
{
	if(!DroppedFile(event->mimeData(), kPlanSuffixes).isEmpty())
		event->acceptProposedAction();
}

void DesignerWindow::dropEvent(QDropEvent* event)
{
	const QString file = DroppedFile(event->mimeData(), kPlanSuffixes);
	if(file.isEmpty())
		return;
	event->acceptProposedAction();
	// Out of the drop first: the file manager waits until the drop returns.
	QTimer::singleShot(0, this, [this, file]() { OpenDropped(file); });
}

bool DesignerWindow::eventFilter(QObject* watched, QEvent* event)
{
	if(watched == m_decalBox && m_decalBox)
	{
		if(event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove)
		{
			QDragMoveEvent* drag = static_cast<QDragMoveEvent*>(event);
			if(!DroppedFile(drag->mimeData(), kDecalImageSuffixes).isEmpty())
			{
				drag->acceptProposedAction();
				return true;
			}
			// Not an image: let the window take it (a plan dropped on the box).
			return false;
		}
		if(event->type() == QEvent::Drop)
		{
			QDropEvent* drop = static_cast<QDropEvent*>(event);
			const QString file = DroppedFile(drop->mimeData(), kDecalImageSuffixes);
			if(!file.isEmpty())
			{
				drop->acceptProposedAction();
				QTimer::singleShot(0, this, [this, file]() { AddDecalImage(file); });
				return true;
			}
			return false;
		}
	}
	return QMainWindow::eventFilter(watched, event);
}

void DesignerWindow::OpenDropped(const QString& fileName)
{
	const bool blueprint = fileName.endsWith(".soh", Qt::CaseInsensitive);
	QString text = blueprint
			? QString("Open the blueprint %1 as a new plan?").arg(QFileInfo(fileName).fileName())
			: QString("Open the plan %1?").arg(QFileInfo(fileName).fileName());
	if(m_dirty)
		text += "\n\nThe changes to the plan open now are not saved and will be lost.";
	if(QMessageBox::question(this, "Shipgen designer", text, QMessageBox::Open | QMessageBox::Cancel,
			QMessageBox::Open) != QMessageBox::Open)
		return;
	if(blueprint)
		OpenBlueprint(fileName);
	else
		OpenPlan(fileName);
}

void DesignerWindow::RefreshDecals()
{
	if(!m_decalList)
		return;
	const bool was = m_updating;
	m_updating = true;
	int row = m_decalList->currentRow();
	m_decalList->clear();
	for(const DecalOptions& d : m_plan.decals)
		m_decalList->addItem(QString("%1  (%2 ft, %3%4)").arg(QFileInfo(d.image).fileName()).arg(d.width, 0, 'f', 0)
				.arg(DecalProjectionName(d.projection)).arg(d.haveAim ? QString() : QString(", not placed yet")));
	if(row >= m_plan.decals.size())
		row = m_plan.decals.size() - 1;
	if(row < 0 && !m_plan.decals.isEmpty())
		row = 0;
	m_decalList->setCurrentRow(row);
	m_decalForm->setEnabled(row >= 0);
	if(row >= 0)
	{
		const DecalOptions& d = m_plan.decals.at(row);
		m_decalProjection->setCurrentIndex(d.projection == DecalProjection::Wrap ? 0 : 1);
		int from = kDecalSideCount;
		for(int i = 0; i < kDecalSideCount; ++i)
			if(qAbs(d.fromX - kDecalSides[i].x) < 1e-6 && qAbs(d.fromY - kDecalSides[i].y) < 1e-6
					&& qAbs(d.fromZ - kDecalSides[i].z) < 1e-6)
				from = i;
		m_decalFrom->setCurrentIndex(from);
		m_decalSides->setCurrentIndex(m_decalSides->findData(DecalSidesName(d.sides)));
		m_decalWidth->setValue(Shown(d.width));
		m_decalRotate->setValue(d.rotate);
		m_decalMirror->setChecked(d.mirror);
		m_decalSelfLit->setValue(d.selfLit);
		m_decalOpacity->setValue(d.opacity);
	}
	else
		m_decalPlace->setChecked(false);
	m_updating = was;
}

void DesignerWindow::DecalFormToPlan()
{
	const int row = m_decalList->currentRow();
	if(row < 0 || row >= m_plan.decals.size())
		return;
	OnAboutToChange();
	DecalOptions& d = m_plan.decals[row];
	DecalProjectionFromName(m_decalProjection->currentData().toString(), d.projection);
	const int from = m_decalFrom->currentIndex();
	if(from >= 0 && from < kDecalSideCount)
	{
		d.fromX = kDecalSides[from].x;
		d.fromY = kDecalSides[from].y;
		d.fromZ = kDecalSides[from].z;
	}
	DecalSidesFromName(m_decalSides->currentData().toString(), d.sides);
	d.width = Feet(m_decalWidth->value());
	d.rotate = m_decalRotate->value();
	d.mirror = m_decalMirror->isChecked();
	d.selfLit = m_decalSelfLit->value();
	d.opacity = m_decalOpacity->value();
	m_dirty = true;
	RefreshTitle();
	RefreshDecals();
	RefreshBuildList();
	ReapplyDecals(true);
}

void DesignerWindow::OnDecalPicked(const QVector3D& point, const QVector3D& normal, const QVector3D& toward)
{
	Q_UNUSED(toward);
	const int row = m_decalList ? m_decalList->currentRow() : -1;
	if(!m_decalPlace || !m_decalPlace->isChecked() || row < 0 || row >= m_plan.decals.size())
		return;
	OnAboutToChange();
	DecalOptions& d = m_plan.decals[row];
	d.haveAim = true;
	d.aimX = point.x();
	d.aimY = point.y();
	d.aimZ = point.z();
	if(m_decalFrom->currentIndex() == kDecalSideCount)
	{
		// Square to the surface.
		d.fromX = normal.x();
		d.fromY = normal.y();
		d.fromZ = normal.z();
	}
	else
	{
		// The side nearest to the way the hull faces there.
		int best = 0;
		double bestDot = -2;
		for(int i = 0; i < kDecalSideCount; ++i)
		{
			const double dot = normal.x() * kDecalSides[i].x + normal.y() * kDecalSides[i].y + normal.z() * kDecalSides[i].z;
			if(dot > bestDot)
			{
				bestDot = dot;
				best = i;
			}
		}
		d.fromX = kDecalSides[best].x;
		d.fromY = kDecalSides[best].y;
		d.fromZ = kDecalSides[best].z;
	}
	m_dirty = true;
	RefreshTitle();
	RefreshDecals();
	ReapplyDecals(true);
}

bool DesignerWindow::ReapplyDecals(bool ask)
{
	if(!m_design || !m_designIsBlueprint)
	{
		if(m_decalStatus)
			m_decalStatus->setText(m_plan.decals.isEmpty() ? QString()
					: QString("Build the blueprint to see the decals and place them."));
		return true;
	}
	const int facesBefore = DesignFaceCount(*m_design);
	RemoveDecals(*m_design);
	DecalsApplied applied;
	if(!m_plan.buildLeaveOut.contains("decals"))
		applied = ApplyDecals(*m_design, m_plan.decals);
	const int faces = DesignFaceCount(*m_design);
	const double size = DesignSizePercent(*m_design);
	const bool overFaces = applied.faces > 0 && m_plan.maxFaces > 0 && m_report.hullFaces + applied.faces > m_plan.maxFaces;
	if(ask && faces > facesBefore && (overFaces || size > 100))
	{
		const QString why = size > 100
				? QString("The blueprint would be at %1 % of the game's size limit.").arg(size, 0, 'f', 0)
				: QString("The decals add %1 faces; with the hull's %2 that is over the face budget of %3.")
						.arg(applied.faces).arg(m_report.hullFaces).arg(m_plan.maxFaces);
		if(QMessageBox::question(this, "Shipgen designer", why + "\n\nKeep the change anyway?",
				QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
		{
			// Back to the plan before the change, keeping the built blueprint.
			if(!m_undo.isEmpty())
				m_plan = m_undo.takeLast();
			RefreshDecals();
			RefreshBuildList();
			ReapplyDecals(false);
			return false;
		}
	}
	QStringList lines;
	if(applied.parts)
		lines << QString("Decals: %1 faces. The blueprint has %2 faces and is at %3 % of the game's size limit.")
				.arg(applied.faces).arg(faces).arg(size, 0, 'f', 1);
	lines << applied.warnings;
	m_decalStatus->setText(lines.join("\n"));
	m_preview->SetDesign(m_design, false);
	return true;
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
	setWindowTitle(QString("Shipgen designer %1 - %2 (%3)%4%5").arg(SHIPGEN_VERSION, m_plan.name, file, m_dirty ? " *" : "",
			m_plan.IsFighter() ? "  [fighter mode - experimental]" : m_plan.IsBuilding() ? "  [building]" : ""));
}

void DesignerWindow::RefreshAll()
{
	RefreshBrushInfo();
	m_updating = true;
	m_name->setText(m_plan.name);
	m_template->setCurrentIndex(qMax(0, m_template->findData(m_plan.templateKey)));
	m_ftl->setCurrentIndex(qMax(0, m_ftl->findData(m_plan.ftlDrive)));
	m_maneuver->setCurrentIndex(qMax(0, m_maneuver->findData(m_plan.maneuverDrive)));
	m_station->setChecked(m_plan.station);
	m_noShields->setChecked(m_plan.leaveOut.contains("shield"));
	m_noWeapons->setChecked(m_plan.leaveOut.contains("firecontrol"));
	m_fireControl->setCurrentIndex(qMax(0, m_fireControl->findData(m_plan.fireControlModule)));
	m_fireControl->setEnabled(m_plan.IsBuilding() || !m_noWeapons->isChecked());
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
	m_shipWallTex->setCurrentIndex(qMax(0, m_shipWallTex->findData(m_plan.wallTex)));
	m_shipFloorTex->setCurrentIndex(qMax(0, m_shipFloorTex->findData(m_plan.floorTex)));
	m_shipCeilingTex->setCurrentIndex(qMax(0, m_shipCeilingTex->findData(m_plan.ceilingTex)));
	m_shipDoorTex->setCurrentIndex(qMax(0, m_shipDoorTex->findData(m_plan.doorTex)));
	m_flatWindows->setChecked(m_plan.flatWindows);
	m_hullWindows->setChecked(m_plan.windowStyle == "hull");
	m_hullWindowGlass->setCurrentIndex(qMax(0, m_hullWindowGlass->findData(m_plan.hullWindowGlass)));
	m_windowLining->setChecked(m_plan.windowLining);
	m_bridgeStyle->setCurrentIndex(qMax(0, m_bridgeStyle->findData(m_plan.bridgeStyle)));
	m_deskPosture->setCurrentIndex(m_plan.deskStanding ? 1 : 0);
	m_deskHeight->setValue(Shown(m_plan.DeskTopFeet()));
	m_deskPosture->setEnabled(m_plan.bridgeStyle == "desks");
	m_deskHeight->setEnabled(m_plan.bridgeStyle == "desks");
	m_fighter->setChecked(m_plan.IsFighter());
	m_fighterBanner->setVisible(m_plan.IsFighter());
	m_updating = false;
	RefreshBuilding();
	RefillRoomTypes();
	RefreshDecks();
	RefreshRooms();
	RefreshPalette();
	RefreshCrew();
	RefreshBuildList();
	RefreshVolumeBox();
	RefreshDecals();
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

void DesignerWindow::UpdateStairStyle()
{
	if(!m_stairKind)
		return;
	const QString kind = m_stairKind->currentData().toString(), slope = m_stairSlope->currentData().toString();
	m_view->SetStairStyle(kind, m_stairWidth->value(), slope);
	const int n = m_plan.StairCells(kind, slope);
	const double run = n * m_plan.cellFeet;
	const double degrees = atan2(m_plan.deckFeet, run) * 180.0 / M_PI;
	const QString info = QString("%1 cells long (%2 ft) and %3 wide, about %4 degrees, and a landing cell.")
			.arg(n).arg(run, 0, 'f', 0).arg(m_stairWidth->value()).arg(degrees, 0, 'f', 0);
	m_stairInfo->setText(m_metric ? MetricText(info) : info);
}

void DesignerWindow::RefreshRooms()
{
	UpdateStairStyle();
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
		if(p.livery)
			text += "  (L: livery)";
		if(p.HasFinish())
			text += "  (F: finish)";
		if(p.HasTexture())
		{
			QString what = QFileInfo(p.texFile).fileName();
			if(p.texLib >= 0)
			{
				const int at = m_texLib ? m_texLib->findData(p.texLib) : -1;
				what = at >= 0 ? m_texLib->itemText(at) : QString("texture %1").arg(p.texLib);
			}
			text += "  (T: " + what + ")";
		}
		QIcon icon = Swatch(p.colour);
		if(p.texLib >= 0 && !GameTextureIcon(p.texLib).isNull())
			icon = GameTextureIcon(p.texLib);
		else if(!p.texFile.isEmpty())
		{
			const QImage image(p.texFile);
			if(!image.isNull())
				icon = QIcon(QPixmap::fromImage(image.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
		}
		QListWidgetItem* item = new QListWidgetItem(icon, text);
		if(p.HasFinish())
			item->setToolTip("Finish: " + FinishSummary(p));
		m_palette->addItem(item);
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
	{
		const QString tool = m_roomTool->currentData().toString();
		static const QHash<QString, PlanView::Mode> modes = {
			{ "doors", PlanView::DoorMode }, { "innerWindows", PlanView::DoorMode }, { "balconies", PlanView::DoorMode },
			{ "ladders", PlanView::LadderMode }, { "windows", PlanView::WindowMode },
			{ "groundParking", PlanView::GroundParkingMode }, { "spaceParking", PlanView::SpaceParkingMode },
			{ "stairs", PlanView::StairMode }, { "contrails", PlanView::ContrailMode }, { "weapons", PlanView::WeaponMode },
			{ "skylights", PlanView::SkylightMode }, { "spacecraftParking", PlanView::SpacecraftParkingMode },
			{ "markings", PlanView::MarkingMode }, { "rocketParking", PlanView::RocketParkingMode },
			{ "waterParking", PlanView::WaterParkingMode }, { "areas", PlanView::AreaMode }, { "props", PlanView::PropMode },
			{ "vehicleParking", PlanView::VehicleParkingMode },
			{ "ceilingHatches", PlanView::HatchMode }, { "floorHatches", PlanView::HatchMode } };
		m_view->SetHatchFloor(tool == "floorHatches");
		// The door tool puts in doors, windows between rooms or balconies.
		m_view->SetDoorKind(tool == "innerWindows" ? "window" : tool == "balconies" ? "balcony" : "");
		m_view->SetMode(modes.value(tool, PlanView::RoomMode));
		m_emitterRow->setVisible(tool == "contrails");
		m_weaponRow->setVisible(tool == "weapons");
		m_markingRow->setVisible(tool == "markings");
		m_skylightRow->setVisible(tool == "skylights");
		m_wallWindowRow->setVisible(tool == "windows");
		m_areaRow->setVisible(tool == "areas");
		m_propRow->setVisible(tool == "props");
		m_vehicleRow->setVisible(tool == "vehicleParking");
	}
		break;
	case StepCrew:
	{
		const QString tool = m_crewTool->currentData().toString();
		m_view->SetMode(tool == "berths" ? PlanView::BerthMode : tool == "pads" ? PlanView::PadMode
				: tool == "paths" ? PlanView::PathMode : PlanView::StationMode);
		m_stationRow->setVisible(tool == "stations");
		m_berthRow->setVisible(tool == "berths");
	}
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
	if(step != StepPaint && m_brushOn && m_brushOn->isChecked())
		m_brushOn->setChecked(false);
	if(step == StepPaint)
		RefreshBrushInfo();
	if(step == StepCrew)
		RefreshCrew();
	if(step == StepBuild)
	{
		RefreshBuildList();
		RefreshBerthBanners();
		RefreshStats();
		if(m_design)
		{
			m_preview->SetHullOpacity(0.25f);
			m_preview->SetDesign(m_design, false);
		}
		return;
	}
	m_preview->SetHullOpacity(step == StepRooms || step == StepCrew ? 0.35f : 0.9f);
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
	if(m_steps->currentIndex() == StepCrew)
		RefreshCrew();
	if(m_steps->currentIndex() == StepRooms)
	{
		const QStringList lifts = m_plan.LiftProblems();
		if(!lifts.isEmpty())
			m_status->setText(lifts.first() + (lifts.size() > 1 ? QString(" (and %1 more)").arg(lifts.size() - 1) : QString()));
	}
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
	// The grid's size: a cell size and cells each way, with what that comes
	// to shown beside them (at most kMaxCells each way).
	QDialog dialog(this);
	dialog.setWindowTitle("New ship");
	QFormLayout* form = new QFormLayout(&dialog);
	form->addRow(Help(QString("How big the grid is. It can be changed later (Ship step), and the hull is drawn "
							  "on it. At most %1 cells each way; bigger cells make a bigger ship fit.")
			.arg(int(TriPlan::kMaxCells))));
	QDoubleSpinBox* cellSize = new QDoubleSpinBox;
	cellSize->setRange(Shown(2), Shown(20));
	cellSize->setSingleStep(Shown(0.5));
	cellSize->setDecimals(m_metric ? 2 : 1);
	cellSize->setSuffix(m_metric ? " m" : " ft");
	cellSize->setValue(Shown(plan.cellFeet));
	cellSize->setToolTip("The side of one cell. A door is as wide as a cell, so 5 feet or more is best.");
	form->addRow("Cell size", cellSize);
	QSpinBox* length = new QSpinBox;
	length->setRange(4, TriPlan::kMaxCells);
	length->setValue(plan.Width());
	length->setToolTip("Cells from aft to fore.");
	form->addRow("Length (cells)", length);
	QSpinBox* beam = new QSpinBox;
	beam->setRange(4, TriPlan::kMaxCells);
	beam->setValue(plan.Depth());
	beam->setToolTip("Cells from starboard to port.");
	form->addRow("Beam (cells)", beam);
	QLabel* size = new QLabel;
	form->addRow("Grid", size);
	auto showSize = [this, cellSize, length, beam, size]() {
		const double cell = Feet(cellSize->value());
		auto one = [this](double feet) {
			return m_metric ? QString("%1 m").arg(feet * 0.3048, 0, 'f', 0) : QString("%1 ft").arg(qRound(feet));
		};
		size->setText(QString("%1 long, %2 across").arg(one(length->value() * cell), one(beam->value() * cell)));
	};
	connect(cellSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), size, showSize);
	connect(length, QOverload<int>::of(&QSpinBox::valueChanged), size, showSize);
	connect(beam, QOverload<int>::of(&QSpinBox::valueChanged), size, showSize);
	showSize();
	QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	form->addRow(buttons);
	if(dialog.exec() != QDialog::Accepted)
		return;
	plan.cellFeet = Feet(cellSize->value());
	plan.Resize(length->value(), beam->value());
	SetPlan(plan);
	m_planFile.clear();
	m_dirty = false;
	ShowStep(StepHull);
	RefreshTitle();
}

void DesignerWindow::OnNewFighter()
{
	TriPlan plan;
	plan.name = "New fighter";
	plan.MakeFighter(true);
	// About 60 by 30 ft, three decks of 4.5 ft: room for a fighter's hull.
	plan.Resize(24, 12);
	while(plan.Decks() < 3)
		plan.AddDeck();
	SetPlan(plan);
	m_planFile.clear();
	m_dirty = false;
	ShowStep(StepHull);
	RefreshTitle();
	m_status->setText("A new fighter (experimental): draw the hull, then the cockpit (Rooms), its door and canopy.");
}

void DesignerWindow::OnNewBuilding()
{
	QStringList labels;
	QStringList keys;
	for(const BuildingTemplate& t : AllBuildingTemplates())
	{
		labels << QString("%1 - %2").arg(t.label, t.description);
		keys << t.key;
	}
	bool ok = false;
	const QString picked = QInputDialog::getItem(this, "New building", "What kind of building? (It can be changed "
			"later, on the first page.)", labels, qMax(0, keys.indexOf("office_block")), false, &ok);
	if(!ok)
		return;
	const BuildingTemplate* t = FindBuildingTemplate(keys.value(labels.indexOf(picked)));
	TriPlan plan;
	plan.MakeBuilding(t ? t->key : QString("office_block"), true);
	plan.name = t ? t->label : QString("New building");
	// The template's footprint and floors, and a cell of room round it.
	const double fx = t ? t->footprintX : 140, fy = t ? t->footprintY : 100;
	plan.Resize(qBound(8, int(ceil(fx / plan.cellFeet)) + 2, int(TriPlan::kMaxCells)), qBound(8, int(ceil(fy / plan.cellFeet)) + 2, int(TriPlan::kMaxCells)));
	while(plan.Decks() < qBound(1, t ? t->floors : 1, 20))
		plan.AddDeck();
	SetPlan(plan);
	m_planFile.clear();
	m_dirty = false;
	ShowStep(StepHull);
	RefreshTitle();
	m_status->setText("A new building: draw its outside on deck 1 and up, then the rooms and a hull door on deck 1.");
}

// The structures a building can be.
static const QList<QPair<QString, QString> >& StructureChoices()
{
	static const QList<QPair<QString, QString> > list = {
		{ "building", "Building" }, { "biodome", "Biodome" }, { "groundplatform", "Ground platform" },
		{ "seaplatform", "Sea platform" }, { "aerialplatform", "Aerial platform" }, { "statue", "Statue" },
		{ "streetlight", "Street light" } };
	return list;
}

// The counts of the building box: key, label, tip, maximum.
struct BuildingCount { const char* key; const char* label; const char* tip; int max; };
static const BuildingCount kBuildingCounts[] = {
	{ "offices", "Offices", "Cubic meters of office.", 100000 },
	{ "shops", "Workshop", "Cubic meters of workshop (factories, mines, churches).", 100000 },
	{ "store", "Store", "Cubic meters of retail store (a shop that sells). For storage use Hold.", 100000 },
	{ "lounge", "Lounge", "Cubic meters of lounge.", 100000 },
	{ "homesLarge", "Large homes", "Cubic meters of large homes.", 100000 },
	{ "homesMedium", "Medium homes", "Cubic meters of medium homes (a house has 4, about 480).", 100000 },
	{ "homesSmall", "Small homes", "Cubic meters of small homes.", 100000 },
	{ "powerPlant", "Power plant", "Cubic meters of generator.", 100000 },
	{ "lifeSupport", "Life support", "Cubic meters of life support (harsh and airless worlds).", 100000 },
	{ "capacitor", "Battery", "Cubic meters of battery (power plants want one).", 100000 },
	{ "sensors", "Sensors", "Cubic meters of sensors (a sensor console is put in).", 100000 },
	{ "shields", "Shields", "Cubic meters of shields (a shield station is put in; a shield base needs them).", 100000 },
	{ "weaponBays", "Weapon bays", "Weapon bays, each run by a fire control station of its own (a weapons base needs "
			"one). What they are is the Fire control choice above; each weapon emitter drawn makes one too.", 20 },
	{ "weaponBayVolume", "Each weapon bay", "Cubic meters for each weapon bay; 0 gives each 2 % of the building.", 100000 },
	{ "hold", "Hold", "Cubic meters of hold: storage for commodities.", 100000 },
	{ "auditorium", "Auditorium", "Cubic meters of auditorium.", 100000 },
	{ "arena", "Arena", "Cubic meters of arena.", 100000 },
	{ "field", "Fields", "Fields of cracked earth, 4,000 square meters each.", 50 },
	{ "groundParking", "Ground parking", "Parking spots for ground vehicles, in front of the building.", 50 },
	{ "spaceParking", "Space pads", "Landing pads for space vehicles, in front of the building.", 50 },
	{ "spacecraftParking", "Spacecraft berths", "Landing berths for whole spacecraft (238 ft square), beside it.", 10 },
	{ "rocketParking", "Rocket pads", "Space rocket parking spots (the aircraft factory wants one).", 10 },
	{ "citizenPosts", "Citizen posts", "Posts for citizens who keep a store or a workshop.", 50 },
	{ "guardPosts", "Guard posts", "Posts for guards.", 50 },
};

static int* BuildingCountField(BuildingBrief& b, const QString& key)
{
	BuildingVolumes& v = b.volumes;
	static int scratch;
	if(key == "groundParking") return &b.groundParking;
	if(key == "spaceParking") return &b.spaceParking;
	if(key == "spacecraftParking") return &b.spacecraftParking;
	if(key == "rocketParking") return &b.rocketParking;
	if(key == "citizenPosts") return &b.citizenPosts;
	if(key == "guardPosts") return &b.guardPosts;
	if(key == "field") return &v.field;
	if(key == "weaponBays") return &v.weaponBays;
	if(key == "weaponBayVolume") return reinterpret_cast<int*>(&v.weaponBayVolume);
	quint32* u = key == "offices" ? &v.offices : key == "shops" ? &v.shops : key == "store" ? &v.store
			: key == "lounge" ? &v.lounge : key == "homesLarge" ? &v.homesLarge : key == "homesMedium" ? &v.homesMedium
			: key == "homesSmall" ? &v.homesSmall : key == "powerPlant" ? &v.powerPlant : key == "lifeSupport" ? &v.lifeSupport
			: key == "capacitor" ? &v.capacitor : key == "sensors" ? &v.sensors : key == "shields" ? &v.shields
			: key == "hold" ? &v.hold : key == "auditorium" ? &v.auditorium : key == "arena" ? &v.arena : 0;
	if(u)
		return reinterpret_cast<int*>(u);		// Both 32 bits; the boxes keep them in range.
	return &scratch;
}

QGroupBox* DesignerWindow::MakeBuildingBox()
{
	m_buildingBox = new QGroupBox("Building");
	QVBoxLayout* outer = new QVBoxLayout(m_buildingBox);
	QFormLayout* form = new QFormLayout;
	outer->addLayout(form);
	m_bTemplate = new QComboBox;
	m_bTemplate->addItem("(keep as it is)", QString());
	for(const BuildingTemplate& t : AllBuildingTemplates())
		m_bTemplate->addItem(t.label, t.key);
	m_bTemplate->setToolTip("Fill in the industry, volumes and parking of a kind of building. The drawing stays.");
	form->addRow("Start from", m_bTemplate);
	connect(m_bTemplate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
		const QString key = m_bTemplate->currentData().toString();
		if(m_updating || key.isEmpty())
			return;
		OnAboutToChange();
		const QString description = m_plan.building.description;
		m_plan.building = BuildingBrief();
		ApplyBuildingTemplate(m_plan.building, key);
		m_plan.building.rooms.clear();
		if(m_plan.building.description.isEmpty())
			m_plan.building.description = description;
		RefreshBuilding();
		PlanEdited();
		m_status->setText(QString("The building now has what a %1 has.").arg(m_bTemplate->currentText()));
	});
	m_bIndustry = new QComboBox;
	m_bIndustry->addItem("(the first that can use it)", QString("auto"));
	QStringList industries = KnownIndustryNames();
	industries.sort(Qt::CaseInsensitive);
	for(const QString& n : industries)
		m_bIndustry->addItem(n, n.toLower());
	m_bIndustry->setToolTip("What the building is for. The game checks that it has what this industry needs.");
	form->addRow("Industry", m_bIndustry);
	m_bStructure = new QComboBox;
	for(const auto& c : StructureChoices())
		m_bStructure->addItem(c.second, c.first);
	m_bStructure->setToolTip("The kind of structure. Platforms and biodomes follow from their industry too; sea "
							 "and aerial platforms have no site.");
	form->addRow("Structure", m_bStructure);
	m_bDescription = new QLineEdit;
	m_bDescription->setToolTip("The description the game shows for the blueprint.");
	form->addRow("Description", m_bDescription);
	QGridLayout* grid = new QGridLayout;
	int i = 0;
	for(const BuildingCount& c : kBuildingCounts)
	{
		QSpinBox* box = new QSpinBox;
		box->setRange(0, c.max);
		box->setToolTip(c.tip);
		if(c.max >= 1000)
		{
			box->setSuffix(" m³");
			box->setSingleStep(50);
		}
		m_bCounts.insert(c.key, box);
		grid->addWidget(new QLabel(c.label), i / 2, (i % 2) * 2);
		grid->addWidget(box, i / 2, (i % 2) * 2 + 1);
		connect(box, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { OnBuildingChanged(); });
		++i;
	}
	outer->addLayout(grid);
	QFormLayout* more = new QFormLayout;
	m_bStackFields = new QCheckBox("Stack the fields");
	m_bStackFields->setToolTip("Lay the fields on top of each other: the game adds up the area of every field, so "
							   "four fields take the ground of one.");
	more->addRow(m_bStackFields);
	m_bBuildZone = new QCheckBox("Build zone over the deck (platforms)");
	m_bBuildZone->setToolTip("Platforms: the top of the deck is a build zone, for other buildings on it.");
	more->addRow(m_bBuildZone);
	m_bLightRange = new QDoubleSpinBox;
	m_bLightRange->setRange(0, 2000);
	m_bLightRange->setSuffix(" ft");
	m_bLightRange->setSpecialValueText("the game's own (45 ft for a street light)");
	m_bLightRange->setToolTip("How far the building's lamps light: a street lamp, not a whole block.");
	more->addRow("Lamps reach", m_bLightRange);
	outer->addLayout(more);
	m_bWarnings = new QLabel;
	m_bWarnings->setWordWrap(true);
	m_bWarnings->setStyleSheet("QLabel { color: #8a4b00; }");
	outer->addWidget(m_bWarnings);
	connect(m_bIndustry, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { OnBuildingChanged(); });
	connect(m_bStructure, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { OnBuildingChanged(); });
	connect(m_bDescription, &QLineEdit::editingFinished, this, [this]() { OnBuildingChanged(); });
	connect(m_bStackFields, &QCheckBox::toggled, this, [this](bool) { OnBuildingChanged(); });
	connect(m_bBuildZone, &QCheckBox::toggled, this, [this](bool) { OnBuildingChanged(); });
	connect(m_bLightRange, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { OnBuildingChanged(); });
	m_buildingBox->setVisible(false);
	return m_buildingBox;
}

void DesignerWindow::RefreshBuilding()
{
	const bool building = m_plan.IsBuilding();
	m_buildingBox->setVisible(building);
	m_buildingBanner->setVisible(building);
	for(QWidget* w : m_shipOnly)
	{
		w->setVisible(!building);
		if(QWidget* label = m_shipForm->labelForField(w))
			label->setVisible(!building);
	}
	if(QStandardItemModel* model = qobject_cast<QStandardItemModel*>(m_roomTool->model()))
		for(int i = 0; i < m_roomTool->count(); ++i)
		{
			const QString key = m_roomTool->itemData(i).toString();
			const bool buildingOnly = key == "rocketParking" || key == "waterParking" || key == "areas" || key == "props";
			// Weapon emitters: a military weapons base has weapon bays too.
			const bool shipOnly = key == "contrails" || key == "markings";
			if(QStandardItem* item = model->item(i))
			{
				const bool on = building ? !shipOnly : !buildingOnly;
				item->setFlags(on ? item->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable
						: item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
			}
		}
	if(!building)
		return;
	const bool was = m_updating;
	m_updating = true;
	BuildingBrief& b = m_plan.building;
	m_bTemplate->setCurrentIndex(0);
	int ind = m_bIndustry->findData(b.industry.toLower());
	m_bIndustry->setCurrentIndex(qMax(0, ind));
	m_bStructure->setCurrentIndex(qMax(0, m_bStructure->findData(b.structure.toLower())));
	m_bDescription->setText(b.description);
	for(auto it = m_bCounts.begin(); it != m_bCounts.end(); ++it)
		it.value()->setValue(*BuildingCountField(b, it.key()));
	m_bStackFields->setChecked(b.stackFields);
	m_bBuildZone->setChecked(b.buildZone);
	m_bLightRange->setValue(Shown(b.lightRange));
	const QStringList warnings = BuildingRuleWarnings(b);
	m_bWarnings->setText(warnings.isEmpty() ? QString() : "⚠ " + warnings.join("\n⚠ "));
	m_bWarnings->setVisible(!warnings.isEmpty());
	m_updating = was;
}

void DesignerWindow::OnBuildingChanged()
{
	if(m_updating || !m_plan.IsBuilding())
		return;
	OnAboutToChange();
	BuildingBrief& b = m_plan.building;
	b.industry = m_bIndustry->currentData().toString();
	b.structure = m_bStructure->currentData().toString();
	b.description = m_bDescription->text().trimmed();
	for(auto it = m_bCounts.constBegin(); it != m_bCounts.constEnd(); ++it)
		*BuildingCountField(b, it.key()) = it.value()->value();
	b.stackFields = m_bStackFields->isChecked();
	b.buildZone = m_bBuildZone->isChecked();
	b.lightRange = Feet(m_bLightRange->value());
	const QStringList warnings = BuildingRuleWarnings(b);
	m_bWarnings->setText(warnings.isEmpty() ? QString() : "⚠ " + warnings.join("\n⚠ "));
	m_bWarnings->setVisible(!warnings.isEmpty());
	PlanEdited();
}

// The room type box follows the mode: a building's rooms or a ship's.
void DesignerWindow::RefillRoomTypes()
{
	if(!m_roomType || m_roomTypesForBuilding == m_plan.IsBuilding())
		return;
	m_roomTypesForBuilding = m_plan.IsBuilding();
	const bool was = m_updating;
	m_updating = true;
	m_roomType->clear();
	FillShipRoomTypes(m_roomType, m_roomTypesForBuilding);
	m_updating = was;
	RefreshRooms();
}

void DesignerWindow::OnNewFighterFromModel()
{
	NewFromModel(true);
}

void DesignerWindow::OnMetric(bool metric)
{
	m_metric = metric;
	QSettings().setValue("designer/metric", metric);
	m_updating = true;
	RefreshAll();
	m_updating = false;
	ApplyTextUnits();
}

// The tooltips and labels in the unit chosen. Each keeps the text it was
// given in feet, taken again whenever the widget was given a new one.
void DesignerWindow::ApplyTextUnits()
{
	auto convert = [this](QObject* o, const char* key, const QString& now) -> QString {
		const QByteArray shown = QByteArray(key) + "Shown", orig = QByteArray(key) + "Feet";
		if(now != o->property(shown.constData()).toString())
			o->setProperty(orig.constData(), now);
		const QString base = o->property(orig.constData()).toString();
		const QString text = m_metric ? MetricText(base) : base;
		o->setProperty(shown.constData(), text);
		return text;
	};
	for(QWidget* w : findChildren<QWidget*>())
	{
		if(!w->toolTip().isEmpty())
			w->setToolTip(convert(w, "tip", w->toolTip()));
		if(QLabel* label = qobject_cast<QLabel*>(w))
			if(label != m_status && label != m_stairInfo && !label->text().isEmpty())
				label->setText(convert(label, "text", label->text()));
		if(QComboBox* combo = qobject_cast<QComboBox*>(w))
			for(int i = 0; i < combo->count(); ++i)
			{
				const QString tip = combo->itemData(i, Qt::ToolTipRole).toString();
				if(tip.isEmpty())
					continue;
				const QByteArray key = "item" + QByteArray::number(i);
				combo->setItemData(i, convert(combo, key.constData(), tip), Qt::ToolTipRole);
			}
	}
}

// The suffixes, ranges and steps of the length boxes, in feet or meters.
void DesignerWindow::ApplyUnits()
{
	struct Box { QDoubleSpinBox* box; double lo, hi, step; };
	const Box boxes[] = {
		{ m_cellFeet, 2, 20, .5 }, { m_deckFeet, m_plan.IsFighter() ? 4.0 : 8.0, 40, .5 }, { m_floorFeet, .5, 4, .5 }, { m_skin, .5, 10, .5 },
		{ m_windowSill, 0, 6, .5 }, { m_deskHeight, 2.2, 5.5, .1 }, { m_windowTop, 3, 20, .5 }, { m_emitterSize, 1, 200, .5 },
		{ m_markingSize, 1, 500, 1 },
	};
	const bool wasUpdating = m_updating;
	m_updating = true;
	// The emitter width is the view's own setting, not the plan's: kept in feet across the change.
	const double emitterFeet = m_emitterSize->suffix() == " m" ? m_emitterSize->value() / 0.3048 : m_emitterSize->value();
	const double markingFeet = m_markingSize->suffix() == " m" ? m_markingSize->value() / 0.3048 : m_markingSize->value();
	const double brushFeet = m_brushSize->suffix() == " m" ? m_brushSize->value() / 0.3048 : m_brushSize->value();
	// Kept in the plan, so set again from it after this; only their unit here.
	const Box more[] = { { m_brushSize, .25, 200, .5 }, { m_texFeet, .25, 1000, .5 }, { m_decalWidth, .5, 2000, 1 },
						 { m_bLightRange, 0, 2000, 5 } };
	for(const Box& b : more)
	{
		b.box->setDecimals(2);
		b.box->setRange(Shown(b.lo), Shown(b.hi));
		b.box->setSingleStep(m_metric ? .1 : b.step);
		b.box->setSuffix(m_metric ? " m" : " ft");
	}
	m_bLightRange->setSpecialValueText(m_metric ? "the game's own (14 m for a street light)" : "the game's own (45 ft for a street light)");
	for(const Box& b : boxes)
	{
		b.box->setDecimals(m_metric ? 2 : 1);
		b.box->setRange(Shown(b.lo), Shown(b.hi));
		b.box->setSingleStep(m_metric ? .1 : b.step);
		b.box->setSuffix(m_metric ? " m" : " ft");
	}
	m_emitterSize->setValue(Shown(emitterFeet));
	m_markingSize->setValue(Shown(markingFeet));
	m_brushSize->setValue(Shown(brushFeet));
	m_updating = wasUpdating;
	if(m_rooms && m_rooms->currentRow() >= 0)
		OnRoomSelected(m_rooms->currentRow());		// The car height, in the new unit.
}

void DesignerWindow::OnNewFromModel()
{
	NewFromModel(false);
}

void DesignerWindow::NewFromModel(bool fighter)
{
	const QString modelFile = QFileDialog::getOpenFileName(this, fighter ? "A fighter from a 3D model" : "Fill the hull from a 3D model",
			QString(), ModelFileFilter() + ";;All files (*)");
	if(modelFile.isEmpty())
		return;
	QDialog dialog(this);
	dialog.setWindowTitle(fighter ? "New fighter from a 3D model (experimental)" : "New spaceship from a 3D model");
	QFormLayout* form = new QFormLayout(&dialog);
	if(fighter)
	{
		QLabel* warn = new QLabel("<b>Fighter mode is experimental.</b> The model itself is the hull, at its real "
								  "length; the plan gets 2.5 ft cells and 4.5 ft decks for the cockpit. Try every "
								  "fighter in the game before relying on it.");
		warn->setWordWrap(true);
		warn->setStyleSheet("QLabel { background: #fff3c4; color: #5a4300; border: 1px solid #d8b13a; padding: 6px; }");
		form->addRow(warn);
	}
	form->addRow(Help(fighter ? "Give the craft's real length (an F-35 is 15.7 m, a Gripen 14.1 m). Thin parts such "
							  "as wings stay on the hull but have no cells for rooms; draw the cockpit where the "
							  "body is thick enough."
							: "The model is cut into slices, one per deck, and the hull triangles of each deck are filled "
							  "in where the model is. Parts thinner than about half a cell are lost, and slopes come out "
							  "in steps of a deck, which the shaping smooths. Touch the grid up by hand afterwards."));
	PlanFromModelOptions options;
	// The cell size, the length and the cells along it go together: a length
	// gives its cells, cells give their length, and a new cell size keeps the
	// length and changes the cells.
	QDoubleSpinBox* cellSize = new QDoubleSpinBox;
	cellSize->setRange(Shown(2), Shown(20));
	cellSize->setSingleStep(Shown(0.5));
	cellSize->setDecimals(m_metric ? 2 : 1);
	cellSize->setSuffix(m_metric ? " m" : " ft");
	cellSize->setValue(Shown(m_plan.cellFeet));
	cellSize->setToolTip("The side of one cell in the new plan. A door is as wide as a cell, so 5 feet or more is "
						 "best; bigger cells make a longer ship fit the grid.");
	if(!fighter)
		form->addRow("Cell size", cellSize);
	QDoubleSpinBox* length = new QDoubleSpinBox;
	length->setRange(Shown(fighter ? 5 : 10), Shown(TriPlan::kMaxCells * 20.0));
	length->setSuffix(m_metric ? " m" : " ft");
	length->setValue(Shown(fighter ? 50.0 : options.lengthFeet));
	form->addRow("Length", length);
	QSpinBox* cells = new QSpinBox;
	cells->setRange(4, TriPlan::kMaxCells);
	cells->setToolTip(QString("Cells from aft to fore, two of them a margin round the model. At most %1.")
			.arg(int(TriPlan::kMaxCells)));
	form->addRow("Length (cells)", cells);
	// Past the grid's most cells the ends of the model are cut off.
	QLabel* tooLong = new QLabel;
	tooLong->setWordWrap(true);
	tooLong->setStyleSheet("QLabel { color: #8a4b00; }");
	form->addRow(tooLong);
	auto cellFeet = [this, cellSize, fighter]() {
		return fighter ? TriPlan::kFighterCellFeet : (m_metric ? cellSize->value() / 0.3048 : cellSize->value());
	};
	auto syncing = std::make_shared<bool>(false);
	auto checkLength = [this, length, cells, tooLong, cellFeet, syncing]() {
		const double feet = m_metric ? length->value() / 0.3048 : length->value();
		const double cell = cellFeet();
		const int need = int(ceil(feet / cell)) + 2;
		*syncing = true;
		cells->setValue(qMin(need, int(TriPlan::kMaxCells)));
		*syncing = false;
		cells->setEnabled(length->isEnabled());
		const double most = (TriPlan::kMaxCells - 2) * cell;
		tooLong->setVisible(length->isEnabled() && need > TriPlan::kMaxCells);
		tooLong->setText(QString("Longer than the grid holds: at most %1 cells, %2 at %3 cells. The ends will be cut "
								 "off; make the cells bigger.").arg(int(TriPlan::kMaxCells))
				.arg(m_metric ? QString("%1 m").arg(qRound(most * 0.3048)) : QString("%1 ft").arg(qRound(most)))
				.arg(m_metric ? QString("%1 m").arg(cell * 0.3048, 0, 'f', 2) : QString("%1 ft").arg(cell)));
	};
	connect(length, QOverload<double>::of(&QDoubleSpinBox::valueChanged), tooLong, checkLength);
	connect(cellSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), tooLong, checkLength);
	connect(cells, QOverload<int>::of(&QSpinBox::valueChanged), length, [this, length, cellFeet, syncing](int n) {
		if(!*syncing)
			length->setValue(Shown((n - 2) * cellFeet()));
	});
	checkLength();
	QSpinBox* decks = new QSpinBox;
	decks->setRange(0, 60);
	decks->setSpecialValueText("From the length");
	decks->setToolTip("How many decks (slices) the model is cut into. The decks stay their height, so more decks "
					  "make a longer ship, which keeps more of a detailed model. The import recommends a length "
					  "when much of the model is lost.");
	form->addRow("Decks", decks);
	connect(decks, QOverload<int>::of(&QSpinBox::valueChanged), length, [length, checkLength](int n) {
		length->setEnabled(n == 0);
		checkLength();
	});
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
	QCheckBox* keepModel = new QCheckBox("Keep the model (its own surface can be the hull)");
	keepModel->setChecked(true);
	keepModel->setToolTip("Unticked: the model is only sliced into decks and then let go. The plan is drawn triangles "
						  "alone, its hull shaped from them (smooth, angular or classic), with far fewer faces, and the "
						  "plan no longer needs the model file.");
	form->addRow(keepModel);
	QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	form->addRow(buttons);
	if(dialog.exec() != QDialog::Accepted)
		return;
	options.lengthFeet = Feet(length->value());
	options.cellFeet = fighter ? TriPlan::kFighterCellFeet : Feet(cellSize->value());
	options.decks = decks->value();
	options.slicesPerDeck = perDeck->value();
	options.method = method->currentIndex() == 0 ? ModelInside::Slices : ModelInside::Solid;
	options.threshold = threshold->value();
	options.flip = flip->isChecked();
	options.yUp = yUp->isChecked();
	if(ImportModel(modelFile, options, true, fighter) && !keepModel->isChecked())
	{
		// Slices only: the plan keeps the drawn triangles and lets go of the model.
		m_plan.model = PlanModel();
		m_hullShaped = false;
		RefreshAll();
		PlanEdited();
		m_status->setText("Sliced the model into decks; the plan's hull is now made from the drawn triangles alone.");
	}
}

bool DesignerWindow::ImportModel(const QString& modelFile, PlanFromModelOptions options, bool tell, bool fighter)
{
	// 0 for the cell size of the plan open now (the dialog gives its own).
	if(fighter)
		options.cellFeet = TriPlan::kFighterCellFeet;
	else if(options.cellFeet <= 0)
		options.cellFeet = m_plan.cellFeet;
	options.deckFeet = fighter ? TriPlan::kFighterDeckFeet : m_plan.deckFeet;
	if(fighter)
		options.decks = 0;		// A fighter keeps its real length.

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
	// A blueprint (.SoH): its hull as it is, with its rooms and doors.
	const bool blueprint = modelFile.endsWith(".soh", Qt::CaseInsensitive);
	QThread* worker = QThread::create([&]() {
		made = blueprint ? PlanFromBlueprint(modelFile, options, plan, notes, error)
				: PlanFromModel(modelFile, options, plan, notes, error, &report);
	});
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
	if(blueprint)
	{
		// Its size is the blueprint's: no longer ship is offered.
		notes = notes.filter(QRegularExpression("^(?!Recommended|Only a little)"));
		report.suggestedLength = 0;
	}
	if(fighter)
	{
		plan.MakeFighter(true);
		// The real length is what it is: no longer ship is offered.
		notes = notes.filter(QRegularExpression("^(?!Recommended)"));
		report.suggestedLength = 0;
		notes << "Fighter mode (experimental): draw the cockpit room where the body is thick enough, a hull door on "
				 "its side, and a canopy of windows or ceiling windows that follow the hull; place the seat in the "
				 "Crew step.";
	}
	if(m_importBuilding.isEmpty() ? false : true)
	{
		plan.MakeBuilding(m_importBuilding, false);
		plan.hullStyle = "smooth";
		notes << "Building mode: deck 1 stands on the ground. Draw the rooms where the model is thick enough, a hull "
				 "door on deck 1, and set what the building is for in the Building box (first page).";
	}
	m_importBuilding.clear();
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
			return ImportModel(modelFile, options, tell, fighter);
		}
	}
	return true;
}

void DesignerWindow::OnOpen()
{
	const QString fileName = QFileDialog::getOpenFileName(this, "Open a plan", QString(),
			"Triangle plans and blueprints (*.json *.SoH *.soh);;Triangle plans (*.json);;Blueprints (*.SoH *.soh)");
	if(fileName.isEmpty())
		return;
	if(fileName.endsWith(".soh", Qt::CaseInsensitive))
		OpenBlueprint(fileName);
	else
		OpenPlan(fileName);
}

void DesignerWindow::OnOpenBlueprint()
{
	const QString fileName = QFileDialog::getOpenFileName(this, "Open a blueprint", QString(), "Blueprints (*.SoH *.soh)");
	if(!fileName.isEmpty())
		OpenBlueprint(fileName);
}

// A blueprint becomes a new plan: its hull as it is, its rooms and doors;
// the template furnishes it again when it is built.
void DesignerWindow::OpenBlueprint(const QString& fileName)
{
	m_importBuilding.clear();
	PlanFromModelOptions blueprintOptions;
	blueprintOptions.cellFeet = 0;		// The open plan's.
	ImportModel(fileName, blueprintOptions, true, false);
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

bool DesignerWindow::SaveIfChanged()
{
	if(!m_dirty)
		return true;
	const QMessageBox::StandardButton answer = QMessageBox::question(this, "Shipgen designer",
			"The plan has changes that are not saved. Save them first?",
			QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
	if(answer == QMessageBox::Cancel)
		return false;
	if(answer == QMessageBox::Save)
	{
		OnSave();
		return !m_dirty;
	}
	return true;
}

void DesignerWindow::CheckForUpdatesAtStart()
{
	if(m_updater && Updater::CheckAtStart())
		m_updater->Check(true);
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
	if(!SaveDesign(*m_design, fileName, QList<OpaquePart>(),
			m_designIsBlueprint ? m_report.vehicleSpots : QList<VehicleSpotSave>()))
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

// Puts the game's texture pictures in the texture boxes (after the game's
// texture library has been picked).
void DesignerWindow::ApplyTextureIcons()
{
	for(QComboBox* box : m_textureBoxes)
		for(int i = 0; i < box->count(); ++i)
			box->setItemIcon(i, GameTextureIcon(box->itemData(i).toInt()));
}

void DesignerWindow::OnCopyDeck(bool withRooms)
{
	const int deck = m_view->Deck();
	bool ok = false;
	const int times = QInputDialog::getInt(this, withRooms ? "Copy this deck" : "Copy the floor",
			QString(withRooms ? "How many copies of deck %1 go on top of it (with its rooms and all on it)?"
					: "How many copies of deck %1's hull and paint go on top of it (no rooms)?").arg(deck + 1),
			1, 1, 50, 1, &ok);
	if(!ok)
		return;
	OnAboutToChange();
	const int rooms = m_plan.CopyDeck(deck, times, withRooms);
	m_view->SetDeck(deck + times);
	RefreshDecks();
	RefreshRooms();
	OnPlanChanged(true);
	m_status->setText(withRooms ? QString("Deck %1 copied %2 time(s) on top of it, with %3 new room(s).")
			.arg(deck + 1).arg(times).arg(rooms)
			: QString("Deck %1's floor copied %2 time(s) on top of it.").arg(deck + 1).arg(times));
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

void DesignerWindow::Rotate(bool clockwise, bool deckOnly)
{
	OnAboutToChange();
	const int deck = m_view->Deck();
	bool grew = false;
	const int lost = m_plan.Rotate(clockwise, deckOnly ? deck : -1, &grew);
	RefreshAll();
	if(!deckOnly || grew)
		m_view->FitToWindow();
	m_view->update();
	const QString what = clockwise ? "Turned 90° clockwise" : "Turned 90° anticlockwise";
	QString text = deckOnly ? QString("%1: deck %2.").arg(what).arg(deck + 1) : what + ".";
	if(grew)
		text += QString(" The grid was made %1 by %2 cells so the turned deck fits.").arg(m_plan.Width()).arg(m_plan.Depth());
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
	m_plan.maneuverDrive = m_maneuver->currentData().toString();
	m_plan.station = m_station->isChecked();
	m_plan.leaveOut.clear();
	if(m_noShields->isChecked())
		m_plan.leaveOut << "shield";
	if(m_noWeapons->isChecked())
		m_plan.leaveOut << "firecontrol";
	m_plan.fireControlModule = m_fireControl->currentData().toInt();
	m_fireControl->setEnabled(m_plan.IsBuilding() || !m_noWeapons->isChecked());
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
	m_plan.wallTex = m_shipWallTex->currentData().toInt();
	m_plan.floorTex = m_shipFloorTex->currentData().toInt();
	m_plan.ceilingTex = m_shipCeilingTex->currentData().toInt();
	m_plan.doorTex = m_shipDoorTex->currentData().toInt();
	m_plan.flatWindows = m_flatWindows->isChecked();
	m_plan.windowStyle = m_hullWindows->isChecked() ? QString("hull") : QString();
	m_plan.hullWindowGlass = m_hullWindowGlass->currentData().toString();
	m_plan.windowLining = m_windowLining->isChecked();
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
	FillShipRoomTypes(typeBox, m_plan.IsBuilding());
	typeBox->setToolTip("The engine's own room types. The stations of the ship go into rooms of the right type: "
						"the helm on the bridge, the engineer in engineering, and so on.");
	static QString lastType;
	if(!lastType.isEmpty())
		PickRoomType(typeBox, lastType);
	else
		PickRoomType(typeBox, "Hall");
	form->addRow("Type", typeBox);
	QLineEdit* nameEdit = new QLineEdit(freeName(typeBox->currentData().toString()));
	nameEdit->setToolTip("The room's name in the blueprint. Every room needs its own.");
	form->addRow("Name", nameEdit);
	// The name follows the type until it is typed in.
	bool typed = false;
	connect(nameEdit, &QLineEdit::textEdited, &dialog, [&typed]() { typed = true; });
	connect(typeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [&](int) {
		if(!typed)
			nameEdit->setText(freeName(typeBox->currentData().toString()));
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
	const QString type = typeBox->currentData().toString();
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
	PickRoomType(m_roomType, m_plan.rooms.at(row).type);
	m_roomLabel->setText(m_plan.rooms.at(row).label);
	m_roomWallTex->setCurrentIndex(qMax(0, m_roomWallTex->findData(m_plan.rooms.at(row).wallTex)));
	m_roomFloorTex->setCurrentIndex(qMax(0, m_roomFloorTex->findData(m_plan.rooms.at(row).floorTex)));
	m_roomCeilingTex->setCurrentIndex(qMax(0, m_roomCeilingTex->findData(m_plan.rooms.at(row).ceilingTex)));
	const bool lift = IsLiftRoomType(m_plan.rooms.at(row).type);
	m_carHeight->setVisible(lift);
	m_carHeightLabel->setVisible(lift);
	m_carHeight->setSuffix(m_metric ? " m" : " ft");
	m_carHeight->setMaximum(Shown(60));
	m_carHeight->setValue(Shown(m_plan.rooms.at(row).carFeet));
	for(int i = 0; i < 4; ++i)
		m_access[i]->setChecked(m_plan.rooms.at(row).access & (1 << i));
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
	m_plan.rooms[row].type = m_roomType->currentData().toString();
	m_plan.rooms[row].wallTex = m_roomWallTex->currentData().toInt();
	m_plan.rooms[row].floorTex = m_roomFloorTex->currentData().toInt();
	m_plan.rooms[row].ceilingTex = m_roomCeilingTex->currentData().toInt();
	m_plan.rooms[row].carFeet = m_metric ? m_carHeight->value() / 0.3048 : m_carHeight->value();
	int access = 0;
	for(int i = 0; i < 4; ++i)
		if(m_access[i]->isChecked())
			access |= 1 << i;
	if(!access)
	{
		m_status->setText("A room has to let someone in; it is open to everyone again.");
		access = 15;
	}
	m_plan.rooms[row].access = access;
	if(!label.isEmpty())
		m_plan.rooms[row].label = label;
	RefreshRooms();
	OnRoomSelected(row);		// A lift shows its car height.
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
	for(PlanStroke& s : m_plan.strokes)
		if(s.paint == row)
			s.paint = 0;
		else if(s.paint > row)
			--s.paint;
	m_plan.palette.removeAt(row);
	RefreshPalette();
	OnPlanChanged(true);
}

void DesignerWindow::OnColourSelected(int row)
{
	if(row >= 0)
		m_view->SetPaintIndex(row);
	if(m_livery)
	{
		const bool was = m_updating;
		m_updating = true;
		m_livery->setChecked(row >= 0 && row < m_plan.palette.size() && m_plan.palette.at(row).livery);
		m_updating = was;
	}
	RefreshFinish();
	RefreshTexture();
}

// Puts the finish of the colour picked in the controls.
void DesignerWindow::RefreshFinish()
{
	if(!m_transparency)
		return;
	const int row = m_palette->currentRow();
	const bool valid = row >= 0 && row < m_plan.palette.size();
	const PlanPaint p = valid ? m_plan.palette.at(row) : PlanPaint();
	const bool was = m_updating;
	m_updating = true;
	// A named list: a range-for over 'QList<QWidget*>() << ...' would loop
	// over a temporary that is already gone (operator<< returns a reference).
	QList<QWidget*> controls;
	controls << m_finishPreset << m_transparency << m_selfLit << m_shine << m_reflect << m_glow << m_effect;
	for(QWidget* w : controls)
		w->setEnabled(valid);
	m_transparency->setValue(qRound((1.f - p.opacity) * 100.f));
	m_selfLit->setValue(qRound(p.selfLit * 100.f));
	m_shine->setValue(p.shine);
	m_reflect->setValue(qRound(p.reflect * 100.f));
	m_effect->setCurrentIndex(qBound(0, p.effect, m_effect->count() - 1));
	if(p.HasGlow())
	{
		m_glow->setIcon(Swatch(p.glow));
		m_glow->setText(p.glow.name());
	}
	else
	{
		m_glow->setIcon(QIcon());
		m_glow->setText("None - choose...");
	}
	QString note;
	if(valid && p.opacity < 1.f)
		note = row == 0 ? "The base coat is see-through: from outside, the whole ship shows its rooms and "
						  "insides. Usually only glass, lights and fields should be."
						: "See-through: from outside, the rooms behind it show through.";
	m_finishNote->setText(note);
	m_finishNote->setVisible(!note.isEmpty());
	m_updating = was;
}

// Changes the finish of the colour picked, as one step to undo.
// Puts the texture of the colour picked in the controls.
void DesignerWindow::RefreshTexture()
{
	if(!m_texKind)
		return;
	const int row = m_palette->currentRow();
	const bool valid = row >= 0 && row < m_plan.palette.size();
	const PlanPaint p = valid ? m_plan.palette.at(row) : PlanPaint();
	const bool was = m_updating;
	m_updating = true;
	const QString kind = p.texLib >= 0 ? "game" : (!p.texFile.isEmpty() ? "file" : "none");
	m_texKind->setEnabled(valid);
	m_texKind->setCurrentIndex(qMax(0, m_texKind->findData(kind)));
	m_texLib->setEnabled(valid && kind == "game");
	if(p.texLib >= 0)
		m_texLib->setCurrentIndex(qMax(0, m_texLib->findData(p.texLib)));
	m_texFile->setEnabled(valid && kind != "game");
	m_texFile->setText(kind == "file" ? QFileInfo(p.texFile).fileName() : QString("Choose an image..."));
	m_texFile->setToolTip(kind == "file" ? p.texFile : QString("An image file of your own (PNG, JPG, ...). It "
			"repeats over the hull, so one that tiles without seams looks best."));
	const bool textured = valid && kind != "none";
	m_texWrap->setEnabled(textured);
	m_texWrap->setCurrentIndex(qMax(0, m_texWrap->findData(p.wrap)));
	m_texFeet->setEnabled(textured && p.wrap != "sphere");
	m_texFeet->setValue(Shown(p.texFeet));
	m_texRotate->setEnabled(textured);
	m_texRotate->setValue(p.texRotate);
	m_texTint->setEnabled(textured);
	m_texTint->setValue(qRound(p.tint * 100.f));
	m_updating = was;
}

void DesignerWindow::EditTexture(const std::function<void(PlanPaint&)>& change)
{
	const int row = m_palette->currentRow();
	if(m_updating || row < 0 || row >= m_plan.palette.size())
		return;
	PlanPaint p = m_plan.palette.at(row);
	change(p);
	const PlanPaint& old = m_plan.palette.at(row);
	if(p.texLib == old.texLib && p.texFile == old.texFile && p.wrap == old.wrap && p.texFeet == old.texFeet &&
			p.texRotate == old.texRotate && p.tint == old.tint)
		return;
	OnAboutToChange();
	m_plan.palette[row] = p;
	RefreshPalette();
	OnPlanChanged(true);
}

void DesignerWindow::EditFinish(const std::function<void(PlanPaint&)>& change)
{
	const int row = m_palette->currentRow();
	if(m_updating || row < 0 || row >= m_plan.palette.size())
		return;
	PlanPaint p = m_plan.palette.at(row);
	change(p);
	const PlanPaint& old = m_plan.palette.at(row);
	if(p.opacity == old.opacity && p.selfLit == old.selfLit && p.shine == old.shine && p.reflect == old.reflect &&
			p.glow == old.glow && p.effect == old.effect)
		return;
	OnAboutToChange();
	m_plan.palette[row] = p;
	RefreshPalette();
	OnPlanChanged(true);
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
		// The brushed paint on a copy: the hull itself stays as shaped, so
		// that brushing again needs no shaping (RefreshBrushedHull).
		PlanHull painted = job.hull;
		ApplyPaintStrokes(job.plan, painted);
		job.design = BuildPlanHullDesign(job.plan, painted);
	}
}

void DesignerWindow::SetBrushOn(bool on)
{
	if(on && m_preview->Picking())
		m_preview->SetPicking(false);
	if(on && m_preview->FreeFly())
		m_preview->SetFreeFly(false);
	m_preview->SetBrush(on, Feet(m_brushSize->value()), m_brushShape->currentData().toBool());
	if(on)
	{
		if(!m_hullShaped)
			ShapeSoon();
		else if(m_designIsBlueprint || !m_design)
			RefreshBrushedHull();
		m_status->setText("Brush: drag with the left button on the hull to paint; the right button turns the view.");
	}
	RefreshBrushInfo();
}

// The shaped hull with the brushed paint on it, in the 3D view, without
// shaping it again.
void DesignerWindow::RefreshBrushedHull()
{
	if(!m_hullShaped || m_steps->currentIndex() == StepBuild)
		return;
	PlanHull painted = m_hull;
	ApplyPaintStrokes(m_plan, painted);
	AuScDesign* design = BuildPlanHullDesign(m_plan, painted);
	m_preview->SetDesign(design, false);
	delete m_design;
	m_design = design;
	m_designIsBlueprint = false;
}

void DesignerWindow::RefreshBrushInfo()
{
	if(!m_brushInfo)
		return;
	int stamps = 0;
	for(const PlanStroke& s : m_plan.strokes)
		stamps += s.stamps.size();
	m_brushInfo->setText(m_plan.strokes.isEmpty() ? QString("Nothing brushed yet.")
			: QString("%1 stroke(s) brushed (%2 dabs). Undo takes back the last.").arg(m_plan.strokes.size()).arg(stamps));
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
	// What is left out is in the plan (buildLeaveOut, skip); furnishing is
	// off only when every kind it puts in is.
	const QStringList& out = m_plan.buildLeaveOut;
	options.furnish = !(out.contains("stations") && out.contains("berths") && out.contains("pads") && out.contains("systems"));
	options.walkPaths = true;
	AuDesign* design = BuildPlanDesign(m_plan, options, m_report);
	m_preview->SetDesign(0);
	delete m_design;
	m_design = design;
	m_designIsBlueprint = true;
	if(m_decalStatus)
		m_decalStatus->setText(m_report.decals ? QString("Decals: %1 faces. The blueprint is at %2 % of the game's size limit.")
				.arg(m_report.decalFaces).arg(DesignSizePercent(*m_design), 0, 'f', 1) : QString());
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
	lines << QString("Stations: %1   Berths: %2   Contrail emitters: %3   Weapon emitters: %4   Flags and names: %5").arg(m_report.stations)
			.arg(m_report.berths).arg(m_report.contrails).arg(m_report.weaponEmitters).arg(m_report.markings);
	if(m_report.decals)
		lines << QString("Decals: %1 (%2 faces)").arg(m_report.decals).arg(m_report.decalFaces);
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
	// The berths against the stations, as the game counts them (it says
	// nothing when there are none at all), at the top where they are seen.
	m_builtBerthWarnings.clear();
	if(m_design && !m_plan.IsBuilding())
		m_builtBerthWarnings = BerthWarnings(WorkOutStats(*m_design, m_plan.statsQuality));
	if(!m_builtBerthWarnings.isEmpty())
	{
		QStringList top;
		top << "BERTHS MAY BE SHORT FOR THE STATIONS (you can still save; the ship may not work as intended):";
		for(const QString& w : m_builtBerthWarnings)
			top << "  " + w;
		top << "";
		lines = top + lines;
	}
	m_reportText->setPlainText(m_metric ? MetricText(lines.join("\n")) : lines.join("\n"));
	RefreshBerthBanners();
	RefreshStats();
	if(m_metric)
		ApplyTextUnits();		// The stats and banners, written in feet.
	return m_report.ok;
}

void DesignerWindow::ShowStatsTab()
{
	m_steps->setCurrentIndex(StepBuild);
	m_buildTabs->setCurrentIndex(1);
}

void DesignerWindow::OnBuild()
{
	BuildNow();
	m_preview->FrameAll();
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// For an AI assistant (mcpserver.h).

void DesignerWindow::ApplyAiChange(const TriPlan& plan, bool hullChanged, bool newPlan, int deck, const QString& what)
{
	Q_UNUSED(hullChanged);		// Rooms reshape the hull too (the smoothing gives way to them).
	OnAboutToChange();
	m_plan = plan;
	if(newPlan)
		m_hullShaped = false;
	m_view->SetDeck(qBound(0, deck >= 0 ? deck : m_view->Deck(), m_plan.Decks() - 1));
	RefreshAll();
	m_view->update();
	m_dirty = true;
	RefreshTitle();
	m_status->setText("AI assistant: " + what);
	if(newPlan)
	{
		m_view->FitToWindow();
		m_frameAfterShape = true;
	}
	PlanEdited();
}

bool DesignerWindow::AiBuild(QString& report)
{
	const bool ok = BuildNow();
	report = m_reportText->toPlainText();
	return ok;
}

QString DesignerWindow::AiStats(int quality)
{
	BuildNow();
	if(!m_design)
		return "The blueprint could not be built, so there are no stats.";
	return StatsText(WorkOutStats(*m_design, qBound(1, quality, 255)));
}

bool DesignerWindow::AiSavePlan(const QString& fileName, QString& error)
{
	if(!m_plan.Save(fileName, error))
		return false;
	m_planFile = fileName;
	m_dirty = false;
	RefreshTitle();
	m_status->setText("AI assistant: saved " + fileName);
	return true;
}

bool DesignerWindow::AiOpenPlan(const QString& fileName, QString& error)
{
	TriPlan plan;
	if(!plan.Load(fileName, error))
		return false;
	ApplyAiChange(plan, true, true, 0, "opened " + fileName);
	m_planFile = fileName;
	m_dirty = false;
	RefreshTitle();
	return true;
}

bool DesignerWindow::AiSaveBlueprint(const QString& fileName, QString& report, QString& error)
{
	AiBuild(report);
	if(!m_design)
	{
		error = "The blueprint could not be built.";
		return false;
	}
	if(!SaveDesign(*m_design, fileName, QList<OpaquePart>(),
			m_designIsBlueprint ? m_report.vehicleSpots : QList<VehicleSpotSave>()))
	{
		error = "Could not write " + fileName;
		return false;
	}
	m_status->setText("AI assistant: saved the blueprint " + fileName);
	return true;
}

QImage DesignerWindow::AiPicture(bool threeD, int deck, float yaw, float pitch, bool blueprint)
{
	if(!threeD)
	{
		// The rooms, doors and the rest are drawn on the Rooms and Crew steps.
		if(m_steps->currentIndex() != StepRooms && m_steps->currentIndex() != StepCrew)
			ShowStep(StepRooms);
		SetDeck(qBound(0, deck, m_plan.Decks() - 1));
		// The plan view gets the whole window while the picture is taken.
		const QList<int> sizes = m_split->sizes();
		const bool sideShown = m_side->isVisible(), previewShown = m_preview->isVisible();
		m_side->hide();
		m_preview->hide();
		centralWidget()->layout()->activate();
		m_view->FitToWindow();
		const QImage image = m_view->grab().toImage();
		m_side->setVisible(sideShown);
		m_preview->setVisible(previewShown);
		centralWidget()->layout()->activate();
		m_split->setSizes(sizes);
		m_view->FitToWindow();
		return image;
	}
	if(blueprint)
	{
		BuildNow();
		ShowStep(StepBuild);
	}
	else
	{
		if(m_steps->currentIndex() == StepBuild)
			ShowStep(StepRooms);
		if(m_shapeStale || m_shapeTimer->isActive() || m_designIsBlueprint || !m_hullShaped)
			ShapeNow();
	}
	// Wide enough to see a long ship: the 3D view gets the whole window
	// while the picture is taken, then the panels come back as they were.
	const QList<int> sizes = m_split->sizes();
	const bool sideShown = m_side->isVisible(), viewShown = m_view->isVisible();
	m_side->hide();
	m_view->hide();
	// Lay out now, not on the next pass of the event loop.
	centralWidget()->layout()->activate();
	m_preview->FrameAll();
	m_preview->SetViewAngles(yaw, pitch);
	const QImage image = m_preview->grabFramebuffer();
	m_side->setVisible(sideShown);
	m_view->setVisible(viewShown);
	centralWidget()->layout()->activate();
	m_split->setSizes(sizes);
	return image;
}

bool DesignerWindow::AiUndo()
{
	if(m_undo.isEmpty())
		return false;
	OnUndo();
	m_status->setText("AI assistant: undid the last change");
	return true;
}

void DesignerWindow::SetAiStatus(const QString& text)
{
	m_aiStatus->setText(text);
	m_aiStatus->setVisible(!text.isEmpty());
}
