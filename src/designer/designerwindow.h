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

#include <QList>
#include <QMainWindow>

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
	// Shapes the hull now rather than after the pause.
	void	ShapeNow();
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
	bool	ImportModel(const QString& modelFile, PlanFromModelOptions options, bool tell = true);

private slots:
	void	OnNew();
	void	OnNewFromModel();
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
	QWidget*	MakePaintPage();
	QWidget*	MakeBuildPage();
	void		RefreshAll();
	void		RefreshRooms();
	void		RefreshPalette();
	void		RefreshDecks();
	void		RefreshTitle();
	void		ApplyDrawMode();
	// Flips the plan (TriPlan::Flip), or with 'deckOnly' the deck shown.
	void		Flip(bool foreAft, bool portStarboard, bool upsideDown, bool deckOnly);

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
	QDoubleSpinBox*	m_windowSill = 0;
	QDoubleSpinBox*	m_windowTop = 0;
	QCheckBox*		m_joinWindows = 0;
	QCheckBox*		m_slopedDecks = 0;
	QCheckBox*		m_noShields = 0;
	QCheckBox*		m_noWeapons = 0;
	QCheckBox*		m_noTransporter = 0;
	QLabel*			m_hullInfo = 0;

	// The room page.
	QListWidget*	m_rooms = 0;
	QComboBox*		m_roomType = 0;
	QLineEdit*		m_roomLabel = 0;
	QComboBox*		m_roomTool = 0;
	QWidget*		m_emitterRow = 0;		// Shown with the contrail tool.
	QDoubleSpinBox*	m_emitterSize = 0;

	// The paint page.
	QListWidget*	m_palette = 0;

	// The build page.
	QPlainTextEdit*	m_reportText = 0;
	QCheckBox*		m_furnish = 0;
	QCheckBox*		m_walkPaths = 0;
};

#endif
