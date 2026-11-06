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
	// For hull doors placed from now on, or right clicked: as in PlanDoor.
	void	SetDoorExit(const QString& exit, bool openOnly, double rampRun)
	{
		m_doorExit = exit;
		m_doorExitOpenOnly = openOnly;
		m_doorRampRun = rampRun;
	}
	enum Mode { HullMode, RoomMode, DoorMode, LadderMode, PaintMode, WindowMode, GroundParkingMode, SpaceParkingMode, StairMode,
			ContrailMode, WeaponMode, SkylightMode };
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

	QVector<QVector<bool> >	m_roomable;
	AuScTriList	m_hullTris;
	mutable QHash<qint64, QPointF>	m_contrailSpots;

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

	// A door being dragged along a wall into a row.
	bool	m_doorDrag = false;
	bool	m_doorMoved = false;
	bool	m_doorHadOne = false;
	TriRef	m_doorStart;
	int		m_doorAlong = 0;
	int		m_doorRow = 0;
	int		m_doorMirrorRow = 0;
	int		m_doorTexture = -1;
	QString	m_doorExit;
	bool	m_doorExitOpenOnly = false;
	double	m_doorRampRun = 0;
};

#endif
