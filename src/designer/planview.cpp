//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - the triangle grid of one deck, seen from above.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "planview.h"

#include <QHash>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <cmath>

namespace {

const QColor kBackground(34, 38, 46);
const QColor kHull(110, 140, 180);
const QColor kHullInRoomMode(92, 102, 118);
const QColor kGhost(58, 66, 80);
const QColor kGridLine(20, 22, 28);
const QColor kDiagonal(20, 22, 28, 90);
const QColor kInteriorDoor(250, 220, 60);
const QColor kHullDoor(255, 140, 40);
const QColor kWindow(140, 220, 255);
const QColor kNotRoomable(220, 50, 50);

const char* kSideNames[4] = { "starboard", "fore", "port", "aft" };

}	// namespace

PlanView::PlanView(QWidget* parent) : QWidget(parent)
{
	setMouseTracking(true);
	setMinimumSize(400, 300);
	setFocusPolicy(Qt::StrongFocus);
}

void PlanView::SetPlan(TriPlan* plan)
{
	m_plan = plan;
	m_deck = qBound(0, m_deck, plan ? plan->Decks() - 1 : 0);
	ClearRoomable();
	FitToWindow();
}

void PlanView::SetDeck(int deck)
{
	if(!m_plan)
		return;
	m_deck = qBound(0, deck, m_plan->Decks() - 1);
	update();
}

void PlanView::SetMode(Mode mode)
{
	m_mode = mode;
	update();
}

void PlanView::SetRoomable(const QVector<QVector<bool> >& roomable)
{
	m_roomable = roomable;
	update();
}

void PlanView::ClearRoomable()
{
	m_roomable.clear();
	update();
}

void PlanView::FitToWindow()
{
	if(!m_plan)
		return;
	const double w = m_plan->Width() * m_plan->cellFeet;
	const double d = m_plan->Depth() * m_plan->cellFeet;
	m_scale = qMax(0.5, qMin(width() / w, height() / d) * 0.92);
	m_offset = QPointF(width() / 2.0, height() / 2.0);
	update();
}

QPointF PlanView::ToScreen(const QPointF& world) const
{
	return QPointF(m_offset.x() + world.x() * m_scale, m_offset.y() - world.y() * m_scale);
}

QPointF PlanView::ToWorld(const QPointF& screen) const
{
	return QPointF((screen.x() - m_offset.x()) / m_scale, -(screen.y() - m_offset.y()) / m_scale);
}

bool PlanView::TriangleAt(const QPointF& screen, TriRef& out) const
{
	if(!m_plan)
		return false;
	const QPointF w = ToWorld(screen);
	const double s = m_plan->cellFeet;
	const double gx = w.x() / s + m_plan->Width() / 2.0;
	const double gy = w.y() / s + m_plan->Depth() / 2.0;
	const int cx = int(floor(gx)), cy = int(floor(gy));
	if(cx < 0 || cy < 0 || cx >= m_plan->Width() || cy >= m_plan->Depth())
		return false;
	// Which of the four triangles: compare with the two diagonals.
	const double u = gx - cx, v = gy - cy;
	const double d1 = v - u, d2 = v + u - 1;
	int k;
	if(d1 < 0 && d2 < 0)
		k = TRI_S;
	else if(d1 < 0)
		k = TRI_E;
	else if(d2 >= 0)
		k = TRI_N;
	else
		k = TRI_W;
	out = TriRef(m_deck, cx, cy, k);
	return true;
}

QColor PlanView::TriangleColour(const TriRef& t) const
{
	const bool hull = m_plan->Hull(t);
	if(m_mode == HullMode)
	{
		if(hull)
			return kHull;
		if(m_plan->Hull(TriRef(t.deck - 1, t.cx, t.cy, t.k)))
			return kGhost;
		return QColor();
	}
	if(!hull)
		return QColor();
	if(m_mode == PaintMode)
	{
		const int p = m_plan->Paint(t);
		return p < m_plan->palette.size() ? m_plan->palette.at(p).colour : kHull;
	}
	const int room = m_plan->Room(t);
	if(room >= 0 && room < m_plan->rooms.size())
		return m_plan->rooms.at(room).colour;
	return kHullInRoomMode;
}

void PlanView::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.fillRect(rect(), kBackground);
	if(!m_plan)
		return;
	p.setRenderHint(QPainter::Antialiasing, false);

	const int W = m_plan->Width(), D = m_plan->Depth();
	const double s = m_plan->cellFeet;
	const double cellPixels = s * m_scale;
	const bool haveRoomable = m_mode != HullMode && m_mode != PaintMode && m_roomable.size() == m_plan->Decks();

	// The grid area.
	const QPointF g0 = ToScreen(m_plan->Corner(0, 0)), g1 = ToScreen(m_plan->Corner(W, D));
	p.fillRect(QRectF(g0, g1).normalized(), QColor(28, 31, 38));

	for(int cy = 0; cy < D; ++cy)
	{
		for(int cx = 0; cx < W; ++cx)
		{
			const QPointF corner = ToScreen(m_plan->Corner(cx, cy + 1));
			if(corner.x() > width() || corner.y() > height() || corner.x() + cellPixels < 0 || corner.y() + cellPixels < 0)
				continue;
			for(int k = 0; k < 4; ++k)
			{
				const TriRef t(m_deck, cx, cy, k);
				const QColor colour = TriangleColour(t);
				QPointF c[3];
				m_plan->TriCorners(t, c);
				QPolygonF poly;
				poly << ToScreen(c[0]) << ToScreen(c[1]) << ToScreen(c[2]);
				if(colour.isValid())
				{
					p.setPen(Qt::NoPen);
					p.setBrush(colour);
					p.drawPolygon(poly);
				}
				if(haveRoomable && m_plan->Hull(t) && !m_roomable[m_deck][(cy * W + cx) * 4 + k])
				{
					// Outside the shaped hull: no room can go here.
					p.setPen(Qt::NoPen);
					p.setBrush(QBrush(kNotRoomable, m_plan->Room(t) >= 0 ? Qt::DiagCrossPattern : Qt::BDiagPattern));
					p.drawPolygon(poly);
				}
			}
		}
	}

	// The lines of the grid.
	if(cellPixels > 6)
	{
		p.setPen(QPen(kGridLine, 1));
		for(int ix = 0; ix <= W; ++ix)
			p.drawLine(ToScreen(m_plan->Corner(ix, 0)), ToScreen(m_plan->Corner(ix, D)));
		for(int iy = 0; iy <= D; ++iy)
			p.drawLine(ToScreen(m_plan->Corner(0, iy)), ToScreen(m_plan->Corner(W, iy)));
	}
	if(cellPixels > 18)
	{
		p.setPen(QPen(kDiagonal, 1));
		for(int cy = 0; cy < D; ++cy)
			for(int cx = 0; cx < W; ++cx)
			{
				p.drawLine(ToScreen(m_plan->Corner(cx, cy)), ToScreen(m_plan->Corner(cx + 1, cy + 1)));
				p.drawLine(ToScreen(m_plan->Corner(cx + 1, cy)), ToScreen(m_plan->Corner(cx, cy + 1)));
			}
	}

	// The centre line, which the mirror works about.
	if(m_mirror)
	{
		p.setPen(QPen(QColor(200, 200, 220, 120), 1, Qt::DashLine));
		p.drawLine(ToScreen(QPointF(m_plan->Corner(0, 0).x(), 0)), ToScreen(QPointF(m_plan->Corner(W, 0).x(), 0)));
	}

	// The doors and the ladders, whatever the mode, once there are rooms.
	if(m_mode != HullMode)
	{
		for(const PlanDoor& door : m_plan->doors)
		{
			if(door.deck != m_deck)
				continue;
			const TriRef a(door.deck, door.cx, door.cy, door.side);
			const TriRef b = m_plan->AcrossCellEdge(door.deck, door.cx, door.cy, door.side);
			const bool hullDoor = m_plan->Room(a) < 0 || m_plan->Room(b) < 0;
			QPointF c[3];
			m_plan->TriCorners(a, c);
			p.setPen(QPen(hullDoor ? kHullDoor : kInteriorDoor, qMax(3.0, cellPixels / 5), Qt::SolidLine, Qt::FlatCap));
			const QPointF e0 = ToScreen(c[0]), e1 = ToScreen(c[1]);
			p.drawLine(e0 + (e1 - e0) * 0.12, e1 + (e0 - e1) * 0.12);
		}
		for(const PlanDoor& window : m_plan->windows)
		{
			if(window.deck != m_deck)
				continue;
			QPointF c0, c1;
			m_plan->EdgeEnds(TriRef(window.deck, window.cx, window.cy, window.side), window.edge, c0, c1);
			p.setPen(QPen(kWindow, qMax(2.0, cellPixels / 8), Qt::DashLine, Qt::FlatCap));
			const QPointF e0 = ToScreen(c0), e1 = ToScreen(c1);
			p.drawLine(e0 + (e1 - e0) * 0.1, e1 + (e0 - e1) * 0.1);
		}
		for(const PlanParking& spot : m_plan->parking)
		{
			if(spot.deck != m_deck)
				continue;
			const QPointF centre = m_plan->Corner(spot.cx, spot.cy), half = m_plan->ParkingSize(spot) / 2.0;
			const QRectF r = QRectF(ToScreen(centre - half), ToScreen(centre + half)).normalized();
			p.setPen(QPen(QColor(255, 210, 60), qMax(1.5, cellPixels / 12), Qt::DashDotLine));
			p.setBrush(Qt::NoBrush);
			p.drawRect(r);
			p.drawText(r, Qt::AlignCenter, spot.space ? "space vehicle" : "vehicle");
		}
		// Stairs: the steps on the deck they start from, the opening on the
		// deck above.
		for(const PlanStair& stair : m_plan->stairs)
		{
			if(stair.deck != m_deck && stair.deck + 1 != m_deck)
				continue;
			const int n = m_plan->StairCells();
			int x0, y0, x1, y1;
			m_plan->StairCell(stair, 0, x0, y0);
			m_plan->StairCell(stair, n - 1, x1, y1);
			const QPointF lo = ToScreen(m_plan->Corner(qMin(x0, x1), qMin(y0, y1)));
			const QPointF hi = ToScreen(m_plan->Corner(qMax(x0, x1) + 1, qMax(y0, y1) + 1));
			const QRectF r = QRectF(lo, hi).normalized().adjusted(cellPixels * .1, cellPixels * .1, -cellPixels * .1, -cellPixels * .1);
			const bool alongX = stair.side == TRI_E || stair.side == TRI_W;
			p.setBrush(Qt::NoBrush);
			if(stair.deck == m_deck)
			{
				p.setPen(QPen(Qt::white, qMax(1.5, cellPixels / 14)));
				p.drawRect(r);
				const int steps = 4 * n;
				for(int i = 1; i < steps; ++i)
				{
					if(alongX)
					{
						const double x = r.left() + r.width() * i / steps;
						p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
					}
					else
					{
						const double y = r.top() + r.height() * i / steps;
						p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
					}
				}
				// An arrow up the stairs.
				const QPointF c0 = ToScreen(m_plan->CellCentre(x0, y0)), c1 = ToScreen(m_plan->CellCentre(x1, y1));
				const QPointF d = (c1 - c0) / qMax(1.0, std::hypot(c1.x() - c0.x(), c1.y() - c0.y()));
				const QPointF side(-d.y(), d.x());
				p.setPen(QPen(QColor(255, 220, 120), qMax(2.0, cellPixels / 10)));
				p.drawLine(c0, c1);
				p.drawLine(c1, c1 - d * cellPixels * .3 + side * cellPixels * .2);
				p.drawLine(c1, c1 - d * cellPixels * .3 - side * cellPixels * .2);
			}
			else
			{
				p.setPen(QPen(QColor(255, 255, 255, 150), qMax(1.5, cellPixels / 14), Qt::DashLine));
				p.drawRect(r);
				p.drawText(r, Qt::AlignCenter, "stairs");
			}
		}
		for(const PlanLadder& ladder : m_plan->ladders)
		{
			if(ladder.deck != m_deck && ladder.deck + 1 != m_deck)
				continue;
			const QPointF lo = ToScreen(m_plan->Corner(ladder.cx, ladder.cy));
			const QPointF hi = ToScreen(m_plan->Corner(ladder.cx + 1, ladder.cy + 1));
			const QRectF r = QRectF(lo, hi).normalized().adjusted(cellPixels * .25, cellPixels * .1, -cellPixels * .25, -cellPixels * .1);
			p.setPen(QPen(ladder.deck == m_deck ? Qt::white : QColor(255, 255, 255, 110), qMax(1.5, cellPixels / 14)));
			p.drawLine(r.topLeft(), r.bottomLeft());
			p.drawLine(r.topRight(), r.bottomRight());
			for(int i = 1; i <= 3; ++i)
			{
				const double y = r.top() + r.height() * i / 4.0;
				p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
			}
		}

		// Room names in the middle of each room.
		QHash<int, QPointF> sum;
		QHash<int, int> count;
		for(int cy = 0; cy < D; ++cy)
			for(int cx = 0; cx < W; ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const int r = m_plan->Room(TriRef(m_deck, cx, cy, k));
					if(r < 0)
						continue;
					sum[r] += m_plan->TriCentre(TriRef(m_deck, cx, cy, k));
					count[r]++;
				}
		p.setPen(Qt::white);
		QFont font = p.font();
		font.setBold(true);
		p.setFont(font);
		for(QHash<int, QPointF>::const_iterator it = sum.constBegin(); it != sum.constEnd(); ++it)
		{
			if(it.key() >= m_plan->rooms.size())
				continue;
			const QPointF c = ToScreen(it.value() / count.value(it.key()));
			const QString text = m_plan->rooms.at(it.key()).label;
			const QRectF box(c.x() - 100, c.y() - 10, 200, 20);
			p.drawText(box, Qt::AlignCenter, text);
		}
	}

	// Which way is which.
	p.setPen(QColor(200, 205, 215));
	p.drawText(QRectF(8, 6, width() - 16, 20), Qt::AlignLeft,
			QString("Deck %1 of %2  -  fore is to the right, port is up").arg(m_deck + 1).arg(m_plan->Decks()));
}

void PlanView::ApplyOne(const TriRef& t, bool erase)
{
	if(!m_plan->Valid(t))
		return;
	switch(m_mode)
	{
	case HullMode:
		if(m_plan->Hull(t) != !erase)
		{
			m_plan->SetHull(t, !erase);
			m_hullTouched = true;
		}
		break;
	case RoomMode:
		m_plan->SetRoom(t, erase ? -1 : m_room);
		break;
	case PaintMode:
		if(m_plan->Hull(t))
		{
			m_plan->SetPaint(t, erase ? 0 : m_paintIndex);
			m_hullTouched = true;
		}
		break;
	default:
		break;
	}
}

void PlanView::Apply(const TriRef& t, bool erase)
{
	QList<TriRef> tris;
	if(m_brush == TriangleBrush)
		tris << t;
	else
	{
		const int r = (m_brush == WideBrush) ? 1 : 0;
		for(int dy = -r; dy <= r; ++dy)
			for(int dx = -r; dx <= r; ++dx)
				for(int k = 0; k < 4; ++k)
					tris << TriRef(t.deck, t.cx + dx, t.cy + dy, k);
	}
	for(const TriRef& one : tris)
	{
		ApplyOne(one, erase);
		if(m_mirror)
			ApplyOne(m_plan->Mirror(one), erase);
	}
}

void PlanView::Notice(const TriRef& t, const QString& text)
{
	m_notice = text;
	m_noticeTri = t;
	emit Hover(text);
}

void PlanView::mousePressEvent(QMouseEvent* event)
{
	m_lastMouse = event->pos();
	if(event->button() == Qt::MiddleButton)
	{
		m_panning = true;
		return;
	}
	if(!m_plan)
		return;
	TriRef t;
	if(!TriangleAt(event->pos(), t))
		return;
	const bool erase = event->button() == Qt::RightButton;

	if(m_mode == DoorMode)
	{
		emit AboutToChange();
		auto toggle = [this](int deck, int cx, int cy, int side) -> bool {
			const int existing = m_plan->DoorAt(deck, cx, cy, side);
			if(existing >= 0)
			{
				m_plan->doors.removeAt(existing);
				return true;
			}
			const TriRef a(deck, cx, cy, side);
			const TriRef b = m_plan->AcrossCellEdge(deck, cx, cy, side);
			if((m_plan->Room(a) < 0 && m_plan->Room(b) < 0) || m_plan->Room(a) == m_plan->Room(b))
				return false;
			PlanDoor d;
			d.deck = deck;
			d.cx = cx;
			d.cy = cy;
			d.side = side;
			m_plan->doors << d;
			return true;
		};
		if(!toggle(t.deck, t.cx, t.cy, t.k))
			Notice(t, "A door has to be on the edge between a room and another room, or the outside: click the "
					"triangle of the room cell that stands on that edge.");
		else if(m_mirror)
		{
			const TriRef m = m_plan->Mirror(t);
			if(!(m == t) && (m_plan->DoorAt(m.deck, m.cx, m.cy, m.k) >= 0) != (m_plan->DoorAt(t.deck, t.cx, t.cy, t.k) >= 0))
				toggle(m.deck, m.cx, m.cy, m.k);
		}
		emit PlanChanged(false);
		update();
		return;
	}
	if(m_mode == WindowMode)
	{
		emit AboutToChange();
		// An edge (the cell edge or a diagonal) is an outer wall when there
		// is a room on one side of it only, and no door on it.
		auto outer = [this](const TriRef& a, int edge) {
			const TriRef b = m_plan->Across(a, edge);
			return (m_plan->Room(a) < 0) != (m_plan->Room(b) < 0)
					&& (edge != 0 || m_plan->DoorAt(a.deck, a.cx, a.cy, a.k) < 0);
		};
		auto toggle = [this, &outer](const TriRef& a, int edge) -> bool {
			const int existing = m_plan->WindowAt(a.deck, a.cx, a.cy, a.k, edge);
			if(existing >= 0)
			{
				m_plan->windows.removeAt(existing);
				return true;
			}
			if(!outer(a, edge))
				return false;
			PlanDoor w;
			w.deck = a.deck;
			w.cx = a.cx;
			w.cy = a.cy;
			w.side = a.k;
			w.edge = edge;
			m_plan->windows << w;
			return true;
		};
		// The edge of the clicked triangle nearest the mouse, or, when that
		// is not an outer wall, the clicked cell's one outer wall if it has
		// only one.
		const QPointF at = ToWorld(event->pos());
		auto distance = [](const QPointF& p, const QPointF& a, const QPointF& b) {
			const QPointF ab = b - a;
			const double len2 = QPointF::dotProduct(ab, ab);
			const double u = len2 > 0 ? qBound(0.0, QPointF::dotProduct(p - a, ab) / len2, 1.0) : 0.0;
			const QPointF d = p - (a + ab * u);
			return sqrt(QPointF::dotProduct(d, d));
		};
		int edge = 0;
		double best = 1e30;
		for(int e = 0; e < 3; ++e)
		{
			QPointF p, q;
			m_plan->EdgeEnds(t, e, p, q);
			const double d = distance(at, p, q);
			if(d < best)
			{
				best = d;
				edge = e;
			}
		}
		TriRef w = t;
		int we = edge;
		if(m_plan->WindowAt(t.deck, t.cx, t.cy, t.k, edge) < 0 && !outer(t, edge))
		{
			int found = 0;
			for(int k = 0; k < 4; ++k)
				for(int e = 0; e < 2; ++e)		// Edge 2 of a triangle is edge 1 of the one before it.
					if(outer(TriRef(t.deck, t.cx, t.cy, k), e))
					{
						w = TriRef(t.deck, t.cx, t.cy, k);
						we = e;
						++found;
					}
			if(found != 1)
			{
				w = t;
				we = edge;
			}
		}
		if(!toggle(w, we))
			Notice(t, "A window has to be on an edge between a room and the outside, where there is no door: a "
					"cell edge or a diagonal. Click near that edge.");
		else if(m_mirror)
		{
			const TriRef m = m_plan->Mirror(w);
			const int me = we == 0 ? 0 : 3 - we;		// Mirroring swaps the two diagonals.
			if(!(m == w && me == we) && (m_plan->WindowAt(m.deck, m.cx, m.cy, m.k, me) >= 0)
					!= (m_plan->WindowAt(w.deck, w.cx, w.cy, w.k, we) >= 0))
				toggle(m, me);
		}
		emit PlanChanged(false);
		update();
		return;
	}
	if(m_mode == GroundParkingMode || m_mode == SpaceParkingMode)
	{
		// The grid corner nearest the mouse.
		const QPointF at = ToWorld(event->pos());
		const QPointF origin = m_plan->Corner(0, 0);
		const int ix = qBound(0, qRound((at.x() - origin.x()) / m_plan->cellFeet), m_plan->Width());
		const int iy = qBound(0, qRound((at.y() - origin.y()) / m_plan->cellFeet), m_plan->Depth());
		emit AboutToChange();
		auto toggle = [this](int deck, int cx, int cy) -> bool {
			const int existing = m_plan->ParkingAt(deck, cx, cy);
			if(existing >= 0)
			{
				m_plan->parking.removeAt(existing);
				return true;
			}
			PlanParking spot;
			spot.deck = deck;
			spot.cx = cx;
			spot.cy = cy;
			spot.space = (m_mode == SpaceParkingMode);
			for(int turn = 0; turn < 2; ++turn)
			{
				spot.across = (turn == 1);
				if(m_plan->ParkingFits(spot))
				{
					m_plan->parking << spot;
					return true;
				}
			}
			return false;
		};
		const bool had = m_plan->ParkingAt(m_deck, ix, iy) >= 0;
		if(!toggle(m_deck, ix, iy))
			Notice(t, QString("A %1 parking spot (%2) has to lie on the floor of one room, centred on the grid corner "
					"nearest the mouse. Make the room bigger, or click nearer its middle.")
					.arg(m_mode == SpaceParkingMode ? "space vehicle" : "ground vehicle")
					.arg(m_mode == SpaceParkingMode ? "12 by 17 m, about 40 by 56 ft" : "6 by 12 m, about 20 by 40 ft"));
		else if(m_mirror)
		{
			const int my = m_plan->Depth() - iy;
			if(my != iy && (m_plan->ParkingAt(m_deck, ix, my) >= 0) == had)
				toggle(m_deck, ix, my);
		}
		emit PlanChanged(false);
		update();
		return;
	}
	if(m_mode == StairMode)
	{
		emit AboutToChange();
		// The triangle clicked says which way the stairs climb.
		auto toggle = [this](int deck, int cx, int cy, int side, QString* why) -> bool {
			const int existing = m_plan->StairAt(deck, cx, cy);
			if(existing >= 0)
			{
				m_plan->stairs.removeAt(existing);
				return true;
			}
			PlanStair st;
			st.deck = deck;
			st.cx = cx;
			st.cy = cy;
			st.side = side;
			if(!m_plan->StairFits(st, why))
				return false;
			m_plan->stairs << st;
			return true;
		};
		QString why;
		const bool had = m_plan->StairAt(t.deck, t.cx, t.cy) >= 0;
		if(!toggle(t.deck, t.cx, t.cy, t.k, &why))
			Notice(t, why);
		else if(m_mirror)
		{
			const int my = m_plan->Depth() - 1 - t.cy;
			const int mside = t.k == TRI_N ? TRI_S : t.k == TRI_S ? TRI_N : t.k;
			if(my != t.cy && (m_plan->StairAt(t.deck, t.cx, my) >= 0) == had)
				toggle(t.deck, t.cx, my, mside, 0);
		}
		emit PlanChanged(false);
		update();
		return;
	}
	if(m_mode == LadderMode)
	{
		emit AboutToChange();
		auto toggle = [this](int deck, int cx, int cy) -> bool {
			const int existing = m_plan->LadderAt(deck, cx, cy);
			if(existing >= 0)
			{
				m_plan->ladders.removeAt(existing);
				return true;
			}
			if(m_plan->CellRoom(deck, cx, cy) < 0 || m_plan->CellRoom(deck + 1, cx, cy) < 0)
				return false;
			PlanLadder l;
			l.deck = deck;
			l.cx = cx;
			l.cy = cy;
			m_plan->ladders << l;
			return true;
		};
		if(!toggle(t.deck, t.cx, t.cy))
			Notice(t, "A ladder goes up from a whole cell of room into a whole cell of room on the deck above.");
		else if(m_mirror)
		{
			const int my = m_plan->Depth() - 1 - t.cy;
			if(my != t.cy && (m_plan->LadderAt(t.deck, t.cx, my) >= 0) != (m_plan->LadderAt(t.deck, t.cx, t.cy) >= 0))
				toggle(t.deck, t.cx, my);
		}
		emit PlanChanged(false);
		update();
		return;
	}

	emit AboutToChange();
	m_drawing = true;
	m_erasing = erase;
	m_hullTouched = false;
	Apply(t, erase);
	m_lastTri = t;
	m_haveLast = true;
	update();
}

void PlanView::mouseMoveEvent(QMouseEvent* event)
{
	if(m_panning)
	{
		m_offset += event->pos() - m_lastMouse;
		m_lastMouse = event->pos();
		update();
		return;
	}
	TriRef t;
	if(!TriangleAt(event->pos(), t))
	{
		emit Hover(QString());
		return;
	}
	if(!m_notice.isEmpty() && t == m_noticeTri)
		return;		// Keep saying why the last click did nothing.
	m_notice.clear();
	QString room = "no room";
	const int r = m_plan->Room(t);
	if(r >= 0 && r < m_plan->rooms.size())
		room = m_plan->rooms.at(r).label;
	emit Hover(QString("deck %1, cell %2,%3, %4 triangle - %5%6")
			.arg(t.deck + 1).arg(t.cx).arg(t.cy).arg(kSideNames[t.k])
			.arg(m_plan->Hull(t) ? "hull" : "outside").arg(m_plan->Hull(t) ? ", " + room : QString()));

	if(m_drawing && !(m_haveLast && t == m_lastTri))
	{
		Apply(t, m_erasing);
		m_lastTri = t;
		m_haveLast = true;
		update();
	}
}

void PlanView::mouseReleaseEvent(QMouseEvent*)
{
	if(m_drawing)
		emit PlanChanged(m_hullTouched || m_mode == HullMode);
	m_drawing = false;
	m_panning = false;
	m_haveLast = false;
}

void PlanView::wheelEvent(QWheelEvent* event)
{
	const QPointF at = event->position();
	const QPointF before = ToWorld(at);
	const double factor = event->angleDelta().y() > 0 ? 1.15 : 1 / 1.15;
	m_scale = qBound(0.5, m_scale * factor, 200.0);
	const QPointF after = ToScreen(before);
	m_offset += at - after;
	update();
}

void PlanView::resizeEvent(QResizeEvent*)
{
	FitToWindow();
}
