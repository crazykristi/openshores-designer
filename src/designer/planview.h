//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - the triangle grid of one deck, seen from above.
//
// The left button draws and the right button rubs out. What is drawn
// depends on the mode: hull triangles, the triangles of the chosen room, a
// door on the cell edge under the mouse (the triangle under the mouse stands
// on that edge), a ladder up from the cell, a window, a vehicle parking spot
// (on the grid corner nearest the mouse), or paint. The wheel zooms, the
// middle button pans. With the mirror on, everything drawn on one side of
// the ship is drawn on the other side as well.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_PLANVIEW_H
#define SHIPGEN_PLANVIEW_H

#include <QPointF>
#include <QVector>
#include <QWidget>

#include "triplan.h"

class PlanView : public QWidget
{
	Q_OBJECT

public:
	enum Mode { HullMode, RoomMode, DoorMode, LadderMode, PaintMode, WindowMode, GroundParkingMode, SpaceParkingMode, StairMode };
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

	// The triangles a room fits in, from the last time the hull was shaped.
	// The ones that do not are marked.
	void	SetRoomable(const QVector<QVector<bool> >& roomable);
	void	ClearRoomable();

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
	void	wheelEvent(QWheelEvent* event) override;
	void	resizeEvent(QResizeEvent* event) override;

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

	TriPlan*	m_plan = 0;
	int			m_deck = 0;
	Mode		m_mode = HullMode;
	Brush		m_brush = CellBrush;
	bool		m_mirror = true;
	int			m_room = 0;
	int			m_paintIndex = 0;

	QVector<QVector<bool> >	m_roomable;

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
};

#endif
