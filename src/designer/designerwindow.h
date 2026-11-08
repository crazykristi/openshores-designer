//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - draw a ship with triangles and turn it into a blueprint.
//
// The window has the plan on the left (planview.h), the 3D view on the
// right, and the settings down the side. The work goes in steps, which are
// the tabs of the side panel: draw the hull, shape it, draw the rooms and
// doors, paint it, and build the blueprint. Any step can be gone back to.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_DESIGNERWINDOW_H
#define SHIPGEN_DESIGNERWINDOW_H

#include <QElapsedTimer>
#include <QList>
#include <QMainWindow>
#include <QMutex>
#include <atomic>
#include <functional>
#include <memory>

#include "planbuild.h"
#include "triplan.h"

class AuScDesign;
class DesignPreview;
class PlanView;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QProgressBar;
class QThread;
class QTreeWidget;
class QToolButton;
class QSlider;
class QSpinBox;
class QTabWidget;
class QTimer;

class DesignerWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit DesignerWindow(QWidget* parent = 0);
	~DesignerWindow();

	bool	OpenPlan(const QString& fileName);
	void	SetPlan(const TriPlan& plan);
	// Shapes the hull now rather than after the pause, and waits for it.
	void	ShapeNow();
	// Shapes the hull in the background; the 3D view follows when it is done.
	void	ShapeSoon();
	// Builds the blueprint and shows it. Returns whether the engine accepted it.
	bool	BuildNow();
	// With 'ask', a blueprint the engine does not accept is only saved if the user says so.
	bool	SaveBlueprint(const QString& fileName, bool ask = true);
	void	ShowStep(int step);
	void	SetDeck(int deck);
	// Fits the plan and the 3D view to their windows.
	void	FrameView();
	// Starts a new plan from a 3D model, in the background with a progress
	// window; 'tell' shows what went wrong or needs looking at in a message
	// box (off for scripts). Returns whether a plan was made.
	// 'fighter': a plan in fighter mode (experimental, TriPlan::MakeFighter),
	// at the model's real length with small cells and low decks.
	bool	ImportModel(const QString& modelFile, PlanFromModelOptions options, bool tell = true, bool fighter = false);

private slots:
	void	OnNew();
	void	OnNewFromModel();
	void	OnNewFighter();
	void	OnNewFighterFromModel();
	void	OnMetric(bool metric);
	void	OnOpen();
	void	OnSave();
	void	OnSaveAs();
	void	OnExport();
	void	OnUndo();
	void	OnRedo();
	void	OnAboutToChange();
	void	OnPlanChanged(bool hullChanged);
	void	OnStepChanged(int step);
	void	OnDeckChanged(int deck);
	void	OnAddDeckAbove();
	void	OnAddDeckBelow();
	void	OnRemoveDeck();
	void	OnResize();
	void	OnSettingsChanged();
	void	OnAddRoom();
	void	OnRemoveRoom();
	void	OnRoomSelected(int row);
	void	OnRoomEdited();
	void	OnAddColour();
	void	OnAddEmpireColour();
	void	OnRemoveColour();
	void	OnColourSelected(int row);
	void	OnShape();
	void	OnBuild();

private:
	QWidget*	MakeShipPage();
	QWidget*	MakeHullPage();
	QWidget*	MakeRoomPage();
	QWidget*	MakeCrewPage();
	void		ApplyCrewChoice();
	QWidget*	MakePaintPage();
	QWidget*	MakeBuildPage();
	void		RefreshAll();
	void		RefreshRooms();
	void		RefreshPalette();
	void		RefreshFinish();
	void		EditFinish(const std::function<void(PlanPaint&)>& change);
	void		RefreshDecks();
	void		RefreshTitle();
	void		ApplyDrawMode();
	// Flips the plan (TriPlan::Flip), or with 'deckOnly' the deck shown.
	void		Flip(bool foreAft, bool portStarboard, bool upsideDown, bool deckOnly);
	void		Rotate(bool clockwise, bool deckOnly);	// A quarter turn (TriPlan::Rotate).

	// The hull is shaped on a worker thread (a big model takes long), from a
	// copy of the plan; edits made meanwhile shape it again after.
	struct ShapeJob
	{
		TriPlan					plan;
		bool					makeDesign = true;
		PlanHull				hull;
		QVector<QVector<bool> >	roomable;
		AuScDesign*				design = 0;
	};
	static void	RunShapeJob(ShapeJob& job, const PlanProgress& progress);
	void		ApplyShapeJob(ShapeJob& job);
	void		ShapingDone();
	// The plan changed: shape it after the pause, or mark the 3D view out of
	// date when redrawing while drawing is off.
	void		PlanEdited();
	void		OnAutoRedraw(bool on);

	// The task bar at the bottom of the window: what is being done, how far
	// along, how long it has taken, and a Stop button where it can stop.
	// SetTask may be called from any thread.
	void		BeginTask(const QString& title, bool canStop);
	void		SetTask(const QString& step, int done, int total);
	void		EndTask(const QString& finalText = QString());
	void		RefreshTask();

	TriPlan			m_plan;
	QString			m_planFile;
	bool			m_dirty = false;
	QList<TriPlan>	m_undo;
	QList<TriPlan>	m_redo;
	bool			m_updating = false;		// Filling the controls; their signals are not edits.

	PlanHull		m_hull;
	bool			m_hullShaped = false;
	AuScDesign*		m_design = 0;
	PlanBuildReport	m_report;

	PlanView*		m_view = 0;
	DesignPreview*	m_preview = 0;
	QTabWidget*		m_steps = 0;
	QTimer*			m_shapeTimer = 0;
	QLabel*			m_status = 0;
	bool			m_autoRedraw = true;
	bool			m_shapeStale = false;		// The 3D view does not show the plan as it is.
	QThread*		m_shapeWorker = 0;
	std::shared_ptr<ShapeJob>	m_shapeJob;
	bool			m_shapeAgain = false;		// Edited while shaping.
	bool			m_frameAfterShape = false;
	bool			m_designIsBlueprint = false;	// m_design is the built blueprint, not the hull.
	int				m_shapeGeneration = 0;

	QWidget*		m_taskBox = 0;
	QLabel*			m_taskLabel = 0;
	QProgressBar*	m_taskBar = 0;
	QToolButton*	m_taskStop = 0;
	QTimer*			m_taskTimer = 0;
	QElapsedTimer	m_taskClock;
	QString			m_taskTitle;
	QMutex			m_taskLock;
	QString			m_taskStep;
	int				m_taskDone = 0;
	int				m_taskTotal = 0;
	int				m_taskSteps = 0;			// Steps seen so far.
	std::atomic<bool>	m_taskStopAsked{false};
	QCheckBox*		m_autoRedrawBox = 0;

	// The ship page.
	QLineEdit*		m_name = 0;
	QComboBox*		m_template = 0;
	QComboBox*		m_ftl = 0;
	// Lengths are kept in feet; with 'm_metric' they are shown in meters.
	bool			m_metric = false;
	double			Shown(double feet) const { return m_metric ? feet * 0.3048 : feet; }
	double			Feet(double shown) const { return m_metric ? shown / 0.3048 : shown; }
	void			ApplyUnits();
	QDoubleSpinBox*	m_cellFeet = 0;
	QDoubleSpinBox*	m_deckFeet = 0;
	QDoubleSpinBox*	m_floorFeet = 0;
	QSpinBox*		m_width = 0;
	QSpinBox*		m_depth = 0;

	// Shared drawing controls.
	QSpinBox*		m_deck = 0;
	QLabel*			m_deckLabel = 0;
	QComboBox*		m_brush = 0;
	QCheckBox*		m_mirror = 0;

	// The hull page.
	QDoubleSpinBox*	m_skin = 0;
	QSlider*		m_smoothing = 0;
	QSpinBox*		m_maxFaces = 0;
	QSpinBox*		m_panels = 0;
	QCheckBox*		m_modelHull = 0;
	QComboBox*		m_modelHullShape = 0;
	QLabel*			m_modelHullShapeLabel = 0;
	QDoubleSpinBox*	m_windowSill = 0;
	QDoubleSpinBox*	m_windowTop = 0;
	QCheckBox*		m_joinWindows = 0;
	QComboBox*		m_shipWallTex = 0;
	QComboBox*		m_shipFloorTex = 0;
	QComboBox*		m_shipCeilingTex = 0;
	QComboBox*		m_shipDoorTex = 0;
	QCheckBox*		m_flatWindows = 0;
	QCheckBox*		m_hullWindows = 0;		// PlanTri::windowStyle "hull".
	QComboBox*		m_roomWallTex = 0;
	QComboBox*		m_roomFloorTex = 0;
	QComboBox*		m_roomCeilingTex = 0;
	QDoubleSpinBox*	m_carHeight = 0;		// A turbolift's car; shown for lifts only.
	QWidget*		m_carHeightLabel = 0;
	QCheckBox*		m_access[4] = {};		// Who may be in the room: officers, crew, passengers, troops.
	QComboBox*		m_doorTex = 0;
	QComboBox*		m_doorExit = 0;
	QDoubleSpinBox*	m_doorRampRun = 0;
	QCheckBox*		m_slopedDecks = 0;
	QComboBox*		m_hullStyle = 0;
	QComboBox*		m_hullDoorExit = 0;
	QCheckBox*		m_noShields = 0;
	QCheckBox*		m_noWeapons = 0;
	QComboBox*		m_fireControl = 0;
	QCheckBox*		m_noTransporter = 0;
	QLabel*			m_hullInfo = 0;

	// The room page.
	QListWidget*	m_rooms = 0;
	QComboBox*		m_roomType = 0;
	QLineEdit*		m_roomLabel = 0;
	QComboBox*		m_roomTool = 0;
	QWidget*		m_emitterRow = 0;		// Shown with the contrail tool.
	QDoubleSpinBox*	m_emitterSize = 0;
	QWidget*		m_weaponRow = 0;		// Shown with the weapon emitter tool.
	QComboBox*		m_weaponBay = 0;
	QWidget*		m_markingRow = 0;		// Shown with the empire flag tool.
	QComboBox*		m_markingKind = 0;
	QWidget*		m_wallWindowRow = 0;	// Shown with the window tool.
	QComboBox*		m_wallWindowStyle = 0;	// PlanDoor::style of windows put in.
	QWidget*		m_skylightRow = 0;		// Shown with the ceiling window tool.
	QComboBox*		m_skylightShape = 0;
	QCheckBox*		m_livery = 0;			// The colour picked is a livery colour.
	// The finish of the colour picked (PlanPaint opacity, selfLit, shine,
	// reflect, glow, effect).
	QComboBox*		m_finishPreset = 0;
	QSpinBox*		m_transparency = 0;		// Percent; 100 - opacity.
	QSpinBox*		m_selfLit = 0;			// Percent.
	QDoubleSpinBox*	m_shine = 0;
	QSpinBox*		m_reflect = 0;			// Percent.
	QPushButton*	m_glow = 0;
	QComboBox*		m_effect = 0;
	QLabel*			m_finishNote = 0;
	QDoubleSpinBox*	m_markingSize = 0;

	// The paint page.
	QListWidget*	m_palette = 0;

	// The build page.
	QPlainTextEdit*	m_reportText = 0;
	void			NewFromModel(bool fighter);
	QCheckBox*		m_fighter = 0;			// TriPlan::mode "fighter".
	QLabel*			m_fighterBanner = 0;	// Says fighter mode is experimental.
	// The Crew step.
	QComboBox*		m_crewTool = 0;
	QWidget*		m_stationRow = 0;
	QComboBox*		m_stationType = 0;
	QComboBox*		m_stationSeat = 0;
	QComboBox*		m_stationModule = 0;
	QWidget*		m_berthRow = 0;
	QComboBox*		m_berthKind = 0;
	QCheckBox*		m_autoStations = 0;
	QCheckBox*		m_autoBerths = 0;
	QCheckBox*		m_autoPads = 0;
	QCheckBox*		m_autoPaths = 0;
	QLabel*			m_crewCounts = 0;
	void			RefreshCrew();
	// The Build step: what gets built (TriPlan::buildLeaveOut, and each item's skip).
	QTreeWidget*	m_buildList = 0;
	void			RefreshBuildList();
	QComboBox*		m_bridgeStyle = 0;
	QComboBox*		m_deskPosture = 0;
	QDoubleSpinBox*	m_deskHeight = 0;
};

#endif
