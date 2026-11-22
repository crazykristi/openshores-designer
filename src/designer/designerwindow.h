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
#include <QMap>
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
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMimeData;
class QFormLayout;
class QImage;
class QGroupBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QProgressBar;
class QThread;
class QTreeWidget;
class QSpinBox;
class QTextBrowser;
class QToolButton;
class QSlider;
class QSpinBox;
class QTabWidget;
class QTimer;
class QVector3D;

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
	void	ShowStatsTab();		// The Expected stats tab of the Build page.
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
	void	OpenBlueprint(const QString& fileName);
	// Asks to save changes that are not saved yet: false if the user cancels.
	bool	SaveIfChanged();
	// Looks for a new version on GitHub (Updater), quietly and at most once a
	// day, unless that is turned off in the Help menu.
	void	CheckForUpdatesAtStart();
	bool	ImportModel(const QString& modelFile, PlanFromModelOptions options, bool tell = true, bool fighter = false);

	// For an AI assistant through the MCP server (mcpserver.h). None of these
	// opens a dialog: the AI is told what went wrong instead.
	const TriPlan&	Plan() const { return m_plan; }
	// Puts in a plan the AI changed, as one step the user can undo, and shows
	// 'deck' (from 0; -1 leaves the deck shown as it is).
	void	ApplyAiChange(const TriPlan& plan, bool hullChanged, bool newPlan, int deck, const QString& what);
	// Builds the blueprint: the report as the Build page shows it; returns
	// whether the engine accepted it.
	bool	AiBuild(QString& report);
	// Builds, then the expected stats at 'quality' (stats.h) as text.
	QString	AiStats(int quality);
	bool	AiSavePlan(const QString& fileName, QString& error);
	bool	AiOpenPlan(const QString& fileName, QString& error);
	// Builds and saves the blueprint, even when the engine has errors (they
	// are in 'report').
	bool	AiSaveBlueprint(const QString& fileName, QString& report, QString& error);
	// A picture of the plan (a deck) or of the 3D view, from 'yaw' and 'pitch'
	// in degrees; with 'blueprint' the 3D view shows the built blueprint.
	QImage	AiPicture(bool threeD, int deck, float yaw, float pitch, bool blueprint);
	bool	AiUndo();
	// The words in the status bar that say an AI is connected.
	void	SetAiStatus(const QString& text);

private slots:
	void	OnNew();
	void	OnNewFromModel();
	void	OnNewFighter();
	void	OnNewBuilding();
	void	OnNewFighterFromModel();
	void	OnMetric(bool metric);
	void	OnOpen();
	void	OnOpenBlueprint();
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
	void	OnCopyDeck(bool withRooms);		// The whole deck, or only its hull and paint.
	void	ApplyTextureIcons();
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

protected:
	// A .SoH or .json plan dropped on the window is opened (after asking);
	// an image dropped on the Decals box becomes a new decal (eventFilter).
	void	dragEnterEvent(QDragEnterEvent* event) override;
	void	dragMoveEvent(QDragMoveEvent* event) override;
	void	dropEvent(QDropEvent* event) override;
	bool	eventFilter(QObject* watched, QEvent* event) override;

private:
	// The first local file in a drag whose suffix is one of 'suffixes' (lower case), or empty.
	static QString	DroppedFile(const QMimeData* data, const QStringList& suffixes);
	// Asks, then opens a dropped plan or blueprint.
	void	OpenDropped(const QString& fileName);
	QWidget*	MakeShipPage();
	QWidget*	MakeHullPage();
	QGroupBox*	MakeDeckBox();
	QWidget*	MakeRoomPage();
	QWidget*	MakeCrewPage();
	void		ApplyCrewChoice();
	QWidget*	MakePaintPage();
	QWidget*	MakeBuildPage();
	QWidget*	MakeStatsTab();
	void		RefreshAll();
	void		RefreshRooms();
	void		RefreshPalette();
	void		RefreshFinish();
	void		EditFinish(const std::function<void(PlanPaint&)>& change);
	void		RefreshTexture();
	// The paint brush in the 3D view (TriPlan::strokes).
	void		SetBrushOn(bool on);
	void		RefreshBrushedHull();
	void		RefreshBrushInfo();
	void		EditTexture(const std::function<void(PlanPaint&)>& change);
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
	class Updater*	m_updater = 0;
	QList<TriPlan>	m_undo;
	QList<TriPlan>	m_redo;
	bool			m_updating = false;		// Filling the controls; their signals are not edits.

	PlanHull		m_hull;
	bool			m_hullShaped = false;
	AuDesign*		m_design = 0;
	PlanBuildReport	m_report;

	PlanView*		m_view = 0;
	DesignPreview*	m_preview = 0;
	QTabWidget*		m_steps = 0;
	QTimer*			m_shapeTimer = 0;
	QLabel*			m_status = 0;
	QLabel*			m_aiStatus = 0;			// Shown while an AI assistant is connected (--mcp).
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
	QComboBox*		m_maneuver = 0;			// TriPlan::maneuverDrive.
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
	QComboBox*		m_hullWindowGlass = 0;	// TriPlan::hullWindowGlass.
	QCheckBox*		m_windowLining = 0;		// TriPlan::windowLining.
	QComboBox*		m_roomWallTex = 0;
	QComboBox*		m_roomFloorTex = 0;
	QComboBox*		m_roomCeilingTex = 0;
	QDoubleSpinBox*	m_carHeight = 0;		// A turbolift's car; shown for lifts only.
	QWidget*		m_carHeightLabel = 0;
	QCheckBox*		m_access[4] = {};		// Who may be in the room: officers, crew, passengers, troops.
	QComboBox*		m_doorTex = 0;
	QComboBox*		m_doorExit = 0;
	QDoubleSpinBox*	m_doorRampRun = 0;
	QComboBox*		m_doorOpen = 0;		// Doors placed start open or closed (PlanDoor::open, PlanHatch::open).
	QComboBox*		m_stairKind = 0;	// Stairs or a ramp, for those placed or right clicked.
	QSpinBox*		m_stairWidth = 0;	// Cells across.
	QComboBox*		m_stairSlope = 0;	// Steep, normal or gentle.
	QLabel*			m_stairInfo = 0;	// How long and steep that makes it.
	void			UpdateStairStyle();
	QCheckBox*		m_slopedDecks = 0;
	QComboBox*		m_hullStyle = 0;
	QComboBox*		m_hullDoorExit = 0;
	QCheckBox*		m_noShields = 0;
	QCheckBox*		m_station = 0;
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
	// The texture of the colour picked (PlanPaint::texLib, texFile, wrap, ...).
	QComboBox*		m_texKind = 0;		// Plain paint, a game texture, an image of one's own.
	QComboBox*		m_texLib = 0;
	QPushButton*	m_texFile = 0;
	QComboBox*		m_texWrap = 0;
	QDoubleSpinBox*	m_texFeet = 0;
	QDoubleSpinBox*	m_texRotate = 0;
	QSpinBox*		m_texTint = 0;
	QPushButton*	m_brushOn = 0;
	QComboBox*		m_brushShape = 0;
	QDoubleSpinBox*	m_brushSize = 0;
	QCheckBox*		m_brushRubOut = 0;
	QLabel*			m_brushInfo = 0;
	class QTimer*	m_brushTimer = 0;
	bool			m_stroking = false;
	QLabel*			m_finishNote = 0;
	QDoubleSpinBox*	m_markingSize = 0;

	// The paint page.
	QListWidget*	m_palette = 0;

	// The build page.
	QPlainTextEdit*	m_reportText = 0;
	// Expected stats (stats.h): the quality they are for, the ship's system
	// volumes set by hand, the game's report and the materials.
	QTabWidget*		m_buildTabs = 0;
	QSpinBox*		m_statsQ = 0;
	QGroupBox*		m_volumeBox = 0;
	QLabel*			m_volumeHelp = 0;
	QList<QSpinBox*>	m_volumeSpins;		// In the order of HandModuleKeys().
	QList<QLabel*>	m_volumeNow;
	QLabel*			m_volumeTotal = 0;
	QTextBrowser*	m_statsText = 0;
	// Berths short for the stations: on the Build page and the Crew page.
	QLabel*			m_berthBanner = 0;
	QLabel*			m_crewBerthBanner = 0;
	QStringList		m_builtBerthWarnings;	// From the last build.
	void			RefreshStats();
	void			RefreshVolumeBox();
	void			RefreshBerthBanners();
	QStringList		HandBerthCheck() const;
	void			NewFromModel(bool fighter);
	QCheckBox*		m_fighter = 0;			// TriPlan::mode "fighter".
	QLabel*			m_fighterBanner = 0;	// Says fighter mode is experimental.
	// Building mode (TriPlan::mode "building"): the building box on the
	// first page, and the ship-only rows it hides.
	QLabel*			m_buildingBanner = 0;
	QWidget*		m_areaRow = 0;
	QString			m_importBuilding;		// A building template while a model is imported as a building.
	QWidget*		m_propRow = 0;
	QWidget*		m_vehicleRow = 0;	// The vehicle a generic vehicle spot is for.
	QList<QComboBox*>	m_textureBoxes;	// Every box that picks a library texture.
	QGroupBox*		m_buildingBox = 0;
	QComboBox*		m_bTemplate = 0;
	QComboBox*		m_bIndustry = 0;
	QComboBox*		m_bStructure = 0;
	QLineEdit*		m_bDescription = 0;
	QMap<QString, QSpinBox*>	m_bCounts;		// Volumes, parking and posts by key.
	QCheckBox*		m_bStackFields = 0;
	QCheckBox*		m_bBuildZone = 0;
	QDoubleSpinBox*	m_bLightRange = 0;
	QLabel*			m_bWarnings = 0;
	QFormLayout*	m_shipForm = 0;
	QList<QWidget*>	m_shipOnly;
	bool			m_roomTypesForBuilding = false;
	QGroupBox*		MakeBuildingBox();
	void			RefreshBuilding();
	void			OnBuildingChanged();
	void			RefillRoomTypes();
	// The Crew step.
	QComboBox*		m_crewTool = 0;
	QWidget*		m_stationRow = 0;
	QComboBox*		m_stationType = 0;
	QComboBox*		m_stationSeat = 0;
	QComboBox*		m_stationModule = 0;
	QComboBox*		m_stationDrive = 0;		// Engineer: the FTL drive it runs (PlanStation::module).
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
	// Decals: images laid over the built hull (decal.h, TriPlan::decals).
	QWidget*		MakeDecalBox();
	QWidget*		m_decalBox = 0;
	// A new decal from an image file (the Add button, or an image dropped on the box).
	void			AddDecalImage(const QString& file);
	QListWidget*	m_decalList = 0;
	QWidget*		m_decalForm = 0;
	QComboBox*		m_decalProjection = 0;
	QComboBox*		m_decalFrom = 0;
	QComboBox*		m_decalSides = 0;
	QDoubleSpinBox*	m_decalWidth = 0;
	QDoubleSpinBox*	m_decalRotate = 0;
	QDoubleSpinBox*	m_decalSelfLit = 0;
	QDoubleSpinBox*	m_decalOpacity = 0;
	QCheckBox*		m_decalMirror = 0;
	QPushButton*	m_decalPlace = 0;
	QLabel*			m_decalStatus = 0;
	void			RefreshDecals();
	void			DecalFormToPlan();
	void			OnDecalPicked(const QVector3D& point, const QVector3D& normal, const QVector3D& toward);
	// Puts the plan's decals on the built blueprint again. With 'ask', a
	// result over the face budget or the size limit asks whether to keep it.
	// Returns false when the user chose to undo.
	bool			ReapplyDecals(bool ask);
};

#endif
