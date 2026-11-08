//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - the triangle grid of one deck, seen from above.
//
// The left button draws and the right button rubs out. What is drawn
// depends on the mode: hull triangles, the triangles of the chosen room, a
// door on the cell edge under the mouse (the triangle under the mouse stands
// on that edge), a ladder up from the cell, a window, a vehicle parking spot
// (on the grid corner nearest the mouse), a contrail or weapon emitter (on
// the hull triangle clicked), or paint. The wheel zooms, the
// middle button pans. With the mirror on, everything drawn on one side of
// the ship is drawn on the other side as well.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_PLANVIEW_H
#define SHIPGEN_PLANVIEW_H

#include <QPointF>
#include <QPolygonF>
#include <QVector>
#include <QWidget>

#include <QHash>
#include "auscface.h"
#include "triplan.h"

class PlanView : public QWidget
{
	Q_OBJECT

public:
	void	SetDoorTexture(int texture) { m_doorTexture = texture; }
	// What the door tool puts in from now on (PlanDoor::kind): "" a door,
	// "window" a window between two rooms, "balcony".
	void	SetDoorKind(const QString& kind) { m_doorKind = kind; }
	void	SetWindowStyle(const QString& style) { m_windowStyle = style; }	// PlanDoor::style of wall windows put in from now on.
	void	SetSkylightShape(const QString& shape) { m_skylightShape = shape; }	// PlanSkylight::shape.
	// For hull doors placed from now on, or right clicked: as in PlanDoor.
	void	SetDoorExit(const QString& exit, double rampRun)
	{
		m_doorExit = exit;
		m_doorRampRun = rampRun;
	}
	enum Mode { HullMode, RoomMode, DoorMode, LadderMode, PaintMode, WindowMode, GroundParkingMode, SpaceParkingMode, SpacecraftParkingMode, StairMode,
			ContrailMode, WeaponMode, SkylightMode, MarkingMode,
			// The Crew step: stations, berths, transporter pads and walk paths placed by hand.
			StationMode, BerthMode, PadMode, PathMode };
	enum Brush { TriangleBrush, CellBrush, WideBrush };

	explicit PlanView(QWidget* parent = 0);

	void	SetPlan(TriPlan* plan);		// Not owned.
	void	SetDeck(int deck);
	int		Deck() const { return m_deck; }
	void	SetMode(Mode mode);
	Mode	CurrentMode() const { return m_mode; }
	void	SetBrush(Brush brush) { m_brush = brush; }
	void	SetMirror(bool mirror) { m_mirror = mirror; update(); }
	void	SetRoom(int room) { m_room = room; }
	void	SetPaintIndex(int paint) { m_paintIndex = paint; }
	void	SetEmitterSize(double feet) { m_emitterFeet = feet; }		// For contrail emitters placed from now on.
	// The weapon bay of weapon emitters placed from now on, or right clicked:
	// a fire control module, 0 for the ship's (PlanEmitter::module).
	void	SetWeaponModule(int module) { m_weaponModule = module; }
	// What the Crew step's tools put in from now on (PlanStation, PlanBerth).
	void	SetStationChoice(const QString& type, const QString& seat, int module)
	{
		m_stationType = type;
		m_stationSeat = seat;
		m_stationModule = module;
		update();
	}
	void	SetBerthKind(const QString& kind) { m_berthKind = kind; }
	void	SetMarking(int marking, double widthFeet) { m_marking = marking; m_markingFeet = widthFeet; }	// PlanEmitter::marking.

	// The triangles a room fits in, from the last time the hull was shaped.
	// The ones that do not are marked.
	void	SetRoomable(const QVector<QVector<bool> >& roomable);
	void	ClearRoomable();

	// The shaped hull, to show contrail emitters where they end up on it.
	void	SetHullTris(const AuScTriList& tris);

	void	FitToWindow();

signals:
	void	AboutToChange();				// Before an edit, for the undo stack.
	void	PlanChanged(bool hullChanged);
	void	Hover(const QString& text);

protected:
	void	paintEvent(QPaintEvent* event) override;
	void	mousePressEvent(QMouseEvent* event) override;
	void	mouseMoveEvent(QMouseEvent* event) override;
	void	mouseReleaseEvent(QMouseEvent* event) override;
	bool	PlaceDoorInRow(int deck, int cx, int cy, int side);
	void	wheelEvent(QWheelEvent* event) override;
	void	resizeEvent(QResizeEvent* event) override;
	void	keyPressEvent(QKeyEvent* event) override;
	void	leaveEvent(QEvent* event) override;

private:
	QPointF	ToScreen(const QPointF& world) const;
	QPointF	ToWorld(const QPointF& screen) const;
	bool	TriangleAt(const QPointF& screen, TriRef& out) const;
	int		SpacecraftSpotUnder(const QPointF& world) const;	// Index in parking, -1 if none.
	void	Apply(const TriRef& t, bool erase);
	void	ApplyOne(const TriRef& t, bool erase);
	QColor	TriangleColour(const TriRef& t) const;
	// Says why a click did nothing. The text stays until the mouse leaves
	// the triangle, rather than being replaced by the hover text at once.
	void	Notice(const TriRef& t, const QString& text);
	// The grid corner nearest a point of the plan.
	void	NearestCorner(const QPointF& world, int& ix, int& iy) const;
	// Where a contrail emitter is drawn: where it ends up on the hull.
	QPointF	ContrailSpot(const PlanEmitter& e) const;

	TriPlan*	m_plan = 0;
	int			m_deck = 0;
	Mode		m_mode = HullMode;
	Brush		m_brush = CellBrush;
	bool		m_mirror = true;
	int			m_room = 0;
	int			m_paintIndex = 0;
	double		m_emitterFeet = 6.5616798;
	int			m_weaponModule = 0;
	int			m_marking = 1;
	double		m_markingFeet = 10.;

	QVector<QVector<bool> >	m_roomable;
	AuScTriList	m_hullTris;
	mutable QHash<qint64, QPointF>	m_contrailSpots;

	// The Crew step. Items are kept in cell units from the grid's corner 0,0.
	QString		m_stationType = "helm";
	QString		m_stationSeat;
	int			m_stationModule = 0;
	QString		m_berthKind = "crew";
	double		m_itemYaw = 0;				// Degrees from fore, towards port; R turns it 45 degrees.
	bool		m_pathDrag = false;
	TriRef		m_pathStart;
	QPointF		m_pathEnd;					// Plan point the dragged path ends at, for drawing it.
	bool	CrewMode() const { return m_mode == StationMode || m_mode == BerthMode || m_mode == PadMode || m_mode == PathMode; }
	QPointF	ToCells(const QPointF& world) const;
	// The outline of an item on the floor, in plan feet: kind 0 a station
	// (with 'seat' "desk" or a chair), 1 a berth, 2 a pad.
	QPolygonF	ItemOutline(int kind, const QPointF& world, double yawDegrees, const QString& seat) const;
	// The room the whole outline is in, -1 if it is not in one room.
	int		OutlineRoom(int deck, const QPolygonF& outline) const;
	// The hand-placed item of the mode under a plan point, index in its list or -1.
	int		CrewItemAt(const QPointF& world) const;
	int		PathAt(const QPointF& world) const;
	void	CrewClick(const QPointF& world, const TriRef& t, bool erase);
	void	PaintCrew(QPainter& p, double cellPixels);
	bool		m_parkingAcross = false;	// Spots placed from now on lie across the ship (R turns them).
	bool		m_hovering = false;
	QPointF		m_hoverWorld;

	double		m_scale = 12.0;			// Pixels per foot.
	QPointF		m_offset;				// Screen position of the world origin.
	bool		m_drawing = false;
	bool		m_erasing = false;
	bool		m_panning = false;
	bool		m_hullTouched = false;
	QPointF		m_lastMouse;
	TriRef		m_lastTri;
	bool		m_haveLast = false;
	QString		m_notice;
	TriRef		m_noticeTri;

	// Windows being dragged along walls: put in (or, from a window, taken out)
	// wherever the mouse passes; those put in share a row.
	bool	m_windowDrag = false;
	bool	m_windowAdding = false;
	int		m_windowRow = 0;
	bool	WindowEdgeAt(const QPointF& world, const TriRef& t, TriRef& w, int& we) const;
	bool	WindowOuter(const TriRef& a, int edge) const;
	void	DragWindow(const TriRef& w, int we);
	// A door being dragged along a wall into a row.
	bool	m_doorDrag = false;
	bool	m_doorMoved = false;
	bool	m_doorHadOne = false;
	TriRef	m_doorStart;
	int		m_doorAlong = 0;
	int		m_doorRow = 0;
	int		m_doorMirrorRow = 0;
	int		m_doorTexture = -1;
	QString	m_doorKind;
	QString	m_skylightShape;
	QString	m_windowStyle;
	QString	m_doorExit;
	double	m_doorRampRun = 0;

	// After a door is placed: its buttons (PlanDoor::buttons), clicked on a
	// wall near it, until two are placed, Esc, or a click elsewhere.
	int		m_buttonDoor = -1;		// A door of the row, in doors; -1 when not placing buttons.
	bool	m_buttonHover = false;
	QPoint	m_buttonAt;				// Where a click would put one, in eighths of a cell.
	QPointF	m_buttonWallA, m_buttonWallB;	// The wall edge it would go on.
	QList<int> ButtonRow(int door) const;	// The doors of that door's row.
	bool	ButtonSpot(const QPointF& world, QPoint& eighths, QPointF& wallA, QPointF& wallB, QString* why) const;
	void	StartButtons(int door);
	void	StopButtons();
};

#endif
