//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - the triangle grid of one deck, seen from above.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "planview.h"
#include "names.h"

#include <QHash>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <cmath>
#include "planbuild.h"

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
	if(mode != m_mode && m_buttonDoor >= 0)
		StopButtons();
	m_pathDrag = false;
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
	m_scale = qMax(0.5, qMin(width() / w, height() / d) * 0.88);
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

int PlanView::SpacecraftSpotUnder(const QPointF& world) const
{
	if(!m_plan)
		return -1;
	for(int i = 0; i < m_plan->parking.size(); ++i)
	{
		const PlanParking& spot = m_plan->parking.at(i);
		if(!spot.spacecraft || spot.deck != m_deck)
			continue;
		const QPointF c = m_plan->Corner(spot.cx, spot.cy), half = m_plan->ParkingSize(spot) / 2.0;
		if(qAbs(world.x() - c.x()) <= half.x() && qAbs(world.y() - c.y()) <= half.y())
			return i;
	}
	return -1;
}

QList<int> PlanView::ButtonRow(int door) const
{
	QList<int> row;
	if(!m_plan || door < 0 || door >= m_plan->doors.size())
		return row;
	row << door;
	for(int at = 0; at < row.size(); ++at)
		for(int i = 0; i < m_plan->doors.size(); ++i)
			if(!row.contains(i) && m_plan->DoorsJoin(m_plan->doors.at(i), m_plan->doors.at(row[at])))
				row << i;
	return row;
}

// A button goes on a wall of one of the door's rooms, its middle within 12 ft
// of the door: the wall edge of the triangle under the mouse nearest it.
bool PlanView::ButtonSpot(const QPointF& world, QPoint& eighths, QPointF& wallA, QPointF& wallB, QString* why) const
{
	const QList<int> row = ButtonRow(m_buttonDoor);
	if(row.isEmpty())
		return false;
	QList<int> rooms;
	QPointF middle(0, 0);
	for(int i : row)
	{
		const PlanDoor& d = m_plan->doors.at(i);
		const TriRef a(d.deck, d.cx, d.cy, d.side);
		const TriRef b = m_plan->AcrossCellEdge(d.deck, d.cx, d.cy, d.side);
		for(const TriRef& t : { a, b })
			if(m_plan->Valid(t) && m_plan->Room(t) >= 0 && !rooms.contains(m_plan->Room(t)))
				rooms << m_plan->Room(t);
		QPointF c[3];
		m_plan->TriCorners(a, c);
		middle += (c[0] + c[1]) / 2.0;
	}
	middle /= row.size();
	TriRef t;
	if(!m_plan->TriAt(m_plan->doors.at(row[0]).deck, world, t) || !rooms.contains(m_plan->Room(t)))
	{
		if(why)
			*why = "A door's button goes on a wall of one of the rooms the door opens into.";
		return false;
	}
	double best = 1e30;
	for(int e = 0; e < 3; ++e)
	{
		const TriRef across = m_plan->Across(t, e);
		if(m_plan->Valid(across) && m_plan->Room(across) == m_plan->Room(t))
			continue;
		// Not in a doorway (a cell edge with a door on it).
		if(e == 0 && m_plan->DoorAt(t.deck, t.cx, t.cy, t.k) >= 0)
			continue;
		QPointF p, q;
		m_plan->EdgeEnds(t, e, p, q);
		const QPointF off = (p + q) / 2.0 - world;
		const double d = QPointF::dotProduct(off, off);
		if(d < best)
		{
			best = d;
			wallA = p;
			wallB = q;
		}
	}
	if(best > 1e29)
	{
		if(why)
			*why = "Point at a wall of the room: a door's button goes on a wall.";
		return false;
	}
	const QPointF mid = (wallA + wallB) / 2.0, toDoor = mid - middle;
	if(QPointF::dotProduct(toDoor, toDoor) > 12.0 * 12.0)
	{
		if(why)
			*why = "A door's button has to be within 12 ft of the door (the game shows its switch within 3 m).";
		return false;
	}
	// An eighth of a cell in from the wall's middle, into the triangle.
	QPointF in = m_plan->TriCentre(t) - mid;
	in /= sqrt(QPointF::dotProduct(in, in));
	const QPointF at = mid + in * (m_plan->cellFeet / 8.0);
	const QPointF origin = m_plan->Corner(0, 0);
	eighths = QPoint(qRound((at.x() - origin.x()) / m_plan->cellFeet * 8.0), qRound((at.y() - origin.y()) / m_plan->cellFeet * 8.0));
	return true;
}

void PlanView::StartButtons(int door)
{
	m_buttonDoor = door;
	m_buttonHover = false;
	emit Hover("Now click a wall within 12 ft of the door for a button that opens and closes it, then one on the "
			"other side if you like. Esc, or a click anywhere else, for no (more) buttons.");
	m_notice = "button";
	setFocus();		// For Esc.
	update();
}

void PlanView::StopButtons()
{
	m_buttonDoor = -1;
	m_buttonHover = false;
	m_notice.clear();
	emit Hover(QString());
	update();
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
			// A window between rooms light blue and dashed, a balcony orange and dotted.
			if(door.kind == "window")
				p.setPen(QPen(QColor(150, 210, 255), qMax(3.0, cellPixels / 5), Qt::DashLine, Qt::FlatCap));
			else if(door.kind == "balcony")
				p.setPen(QPen(QColor(255, 170, 70), qMax(3.0, cellPixels / 5), Qt::DotLine, Qt::FlatCap));
			else
				p.setPen(QPen(hullDoor ? kHullDoor : kInteriorDoor, qMax(3.0, cellPixels / 5), Qt::SolidLine, Qt::FlatCap));
			const QPointF e0 = ToScreen(c[0]), e1 = ToScreen(c[1]);
			// A row is one door: no gap where it meets its neighbour.
			bool at0 = false, at1 = false;
			for(const PlanDoor& other : m_plan->doors)
				if(m_plan->DoorsJoin(door, other))
				{
					const QPointF o = m_plan->CellCentre(other.cx, other.cy);
					const QPointF near0 = c[0] - o, near1 = c[1] - o;
					(QPointF::dotProduct(near0, near0) < QPointF::dotProduct(near1, near1) ? at0 : at1) = true;
				}
			p.drawLine(at0 ? e0 : e0 + (e1 - e0) * 0.12, at1 ? e1 : e1 + (e0 - e1) * 0.12);
			// A hull door's own way down, if it has one: a letter on the door.
			if(hullDoor && !door.exit.isEmpty() && cellPixels >= 10)
			{
				const QString way = door.exit == "ladder" ? "L" : door.exit == "ramp" ? "R" : door.exit == "none" ? "D"
						: door.exit == "auto" ? "A" : "S";
				const QPointF middle = (e0 + e1) / 2.0;
				const QRectF box(middle.x() - cellPixels / 2, middle.y() - cellPixels / 2, cellPixels, cellPixels);
				p.setPen(Qt::black);
				p.drawText(box.translated(1, 1), Qt::AlignCenter, way);
				p.setPen(Qt::white);
				p.drawText(box, Qt::AlignCenter, way);
			}
			// Its buttons: small green squares on the wall.
			for(const QPoint& b : door.buttons)
			{
				const QPointF at = ToScreen(m_plan->EighthPoint(b));
				const double r = qMax(3.0, cellPixels * .12);
				p.setPen(QPen(Qt::black, 1));
				p.setBrush(QColor(70, 230, 110));
				p.drawRect(QRectF(at.x() - r, at.y() - r, 2 * r, 2 * r));
				p.setBrush(Qt::NoBrush);
			}
		}
		// Where a click would put a door's button.
		if(m_mode == DoorMode && m_buttonDoor >= 0 && m_buttonHover)
		{
			p.setPen(QPen(QColor(70, 230, 110), qMax(3.0, cellPixels / 6), Qt::SolidLine, Qt::RoundCap));
			p.drawLine(ToScreen(m_buttonWallA), ToScreen(m_buttonWallB));
		}
		for(const PlanDoor& window : m_plan->windows)
		{
			if(window.deck != m_deck)
				continue;
			QPointF c0, c1;
			m_plan->EdgeEnds(TriRef(window.deck, window.cx, window.cy, window.side), window.edge, c0, c1);
			// Dots for a window that follows the hull, dashes for a framed one.
			p.setPen(QPen(kWindow, qMax(2.0, cellPixels / 8), m_plan->WindowIsHull(window) ? Qt::DotLine : Qt::DashLine, Qt::FlatCap));
			const QPointF e0 = ToScreen(c0), e1 = ToScreen(c1);
			p.drawLine(e0 + (e1 - e0) * 0.1, e1 + (e0 - e1) * 0.1);
		}
		for(const PlanSkylight& window : m_plan->skylights)
		{
			if(window.deck != m_deck)
				continue;
			p.setPen(QPen(kWindow, qMax(1.5, cellPixels / 10), m_plan->SkylightIsHull(window) ? Qt::DotLine : Qt::DashLine));
			p.setBrush(Qt::NoBrush);
			if(window.k < 0)
			{
				const QRectF r = QRectF(ToScreen(m_plan->Corner(window.cx, window.cy)), ToScreen(m_plan->Corner(window.cx + 1, window.cy + 1)))
						.normalized().adjusted(cellPixels * .15, cellPixels * .15, -cellPixels * .15, -cellPixels * .15);
				p.drawRect(r);
				p.drawLine(r.topLeft(), r.bottomRight());
			}
			else
			{
				QPointF c[3];
				m_plan->TriCorners(TriRef(window.deck, window.cx, window.cy, window.k), c);
				const QPointF centre = (c[0] + c[1] + c[2]) / 3.0;
				QPolygonF tri;
				for(const QPointF& q : c)
					tri << ToScreen(centre + (q - centre) * .7);
				p.drawPolygon(tri);
			}
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
			p.drawText(r, Qt::AlignCenter, spot.spacecraft ? "spacecraft landing" : spot.space ? "space vehicle" : "vehicle");
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

		// Emitters: a contrail as an arrow pointing aft, a weapon emitter as
		// a ring with a cross.
		for(const PlanEmitter& e : m_plan->emitters)
		{
			if(e.deck != m_deck)
				continue;
			const QPointF c = ToScreen(m_plan->TriCentre(TriRef(e.deck, e.cx, e.cy, e.k)));
			const double r = qMax(6.0, cellPixels * .35);
			p.setBrush(Qt::NoBrush);
			if(e.marking)
			{
				// A flag on a pole, or a name plate.
				const QColor colour(240, 200, 255);
				p.setPen(QPen(colour, qMax(1.5, cellPixels / 14)));
				if(e.marking == 1)
				{
					p.drawLine(c + QPointF(-r * .6, r), c + QPointF(-r * .6, -r));
					p.setBrush(QColor(240, 200, 255, 120));
					p.drawRect(QRectF(c.x() - r * .6, c.y() - r, r * 1.4, r * .9));
				}
				else
				{
					const QRectF plate(c.x() - r * 1.2, c.y() - r * .45, r * 2.4, r * .9);
					p.drawRect(plate);
					if(cellPixels >= 10)
						p.drawText(plate, Qt::AlignCenter, "N");
				}
				p.setBrush(Qt::NoBrush);
			}
			else if(e.weapon)
			{
				p.setPen(QPen(QColor(255, 80, 80), qMax(1.5, cellPixels / 14)));
				p.drawEllipse(c, r, r);
				p.drawLine(c - QPointF(r * 1.4, 0), c + QPointF(r * 1.4, 0));
				p.drawLine(c - QPointF(0, r * 1.4), c + QPointF(0, r * 1.4));
				// Its bay, as a letter, when it has one of its own.
				if(e.module > 0 && cellPixels >= 10)
				{
					QString letter = "?";
					for(const QPair<int, QString>& choice : FireControlModules())
						if(choice.first == e.module)
							letter = choice.second.left(1);
					const QRectF box(c.x() + r * .6, c.y() - r * 1.9, r * 1.6, r * 1.3);
					p.setPen(Qt::black);
					p.drawText(box.translated(1, 1), Qt::AlignCenter, letter);
					p.setPen(QColor(255, 80, 80));
					p.drawText(box, Qt::AlignCenter, letter);
				}
			}
			else
			{
				// A ball where the emitter ends up on the hull, with an arrow
				// the way the trail goes (aft is to the left on the screen),
				// and a faint line back to the triangle it was put in from.
				const QPointF ball = ToScreen(ContrailSpot(e));
				const QColor colour(120, 200, 255);
				const double br = qMax(4.0, cellPixels * .22), length = qMax(16.0, cellPixels * 1.1), head = length * .28;
				p.setPen(QPen(colour, 1, Qt::DotLine));
				p.drawLine(c, ball);
				p.setPen(QPen(colour, qMax(2.0, cellPixels / 10), Qt::SolidLine, Qt::RoundCap));
				const QPointF tip = ball - QPointF(br + length, 0);
				p.drawLine(ball, tip);
				p.drawLine(tip, tip + QPointF(head, -head * .7));
				p.drawLine(tip, tip + QPointF(head, head * .7));
				p.setPen(Qt::NoPen);
				p.setBrush(colour);
				p.drawEllipse(ball, br, br);
				p.setBrush(Qt::NoBrush);
			}
		}

		// The outline of the parking spot the mouse would place.
		if((m_mode == GroundParkingMode || m_mode == SpaceParkingMode || m_mode == SpacecraftParkingMode) && m_hovering)
		{
			PlanParking spot;
			spot.deck = m_deck;
			NearestCorner(m_hoverWorld, spot.cx, spot.cy);
			spot.space = m_mode == SpaceParkingMode;
			spot.spacecraft = m_mode == SpacecraftParkingMode;
			spot.across = m_parkingAcross;
			const bool taken = SpacecraftSpotUnder(m_hoverWorld) >= 0 || (!spot.spacecraft && m_plan->ParkingAt(m_deck, spot.cx, spot.cy) >= 0);
			const bool fits = m_plan->ParkingFits(spot);
			const QColor colour = taken ? QColor(255, 160, 60) : fits ? QColor(90, 230, 120) : QColor(255, 90, 90);
			const QPointF centre = m_plan->Corner(spot.cx, spot.cy), half = m_plan->ParkingSize(spot) / 2.0;
			const QRectF r = QRectF(ToScreen(centre - half), ToScreen(centre + half)).normalized();
			p.setPen(QPen(colour, qMax(1.5, cellPixels / 10)));
			QColor fill = colour;
			fill.setAlpha(45);
			p.setBrush(fill);
			p.drawRect(r);
			p.setBrush(Qt::NoBrush);
			p.drawText(QRectF(r.center().x() - 300, r.bottom() + 2, 600, 40), Qt::AlignHCenter | Qt::AlignTop,
					taken ? "click takes this spot away" : fits ? "click to place, R turns it"
					: "does not fit on one room's floor, R turns it");
		}

		// Stations, berths, pads and paths placed by hand (Crew step).
		PaintCrew(p, cellPixels);

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

	if(CrewMode())
	{
		CrewClick(ToWorld(event->pos()), t, erase);
		return;
	}
	if(m_mode == DoorMode && m_buttonDoor >= 0)
	{
		// Placing a door's buttons: a click on a wall near it puts one there;
		// anywhere else stops, and the click does what it would have done.
		QPoint at;
		QPointF wa, wb;
		// A click on a door (this one or another) is a click on the door:
		// it stops placing buttons and takes the door away, as usual.
		const bool onDoor = m_plan->DoorAt(t.deck, t.cx, t.cy, t.k) >= 0;
		if(!erase && !onDoor && ButtonSpot(ToWorld(event->pos()), at, wa, wb, 0))
		{
			emit AboutToChange();
			const QList<int> row = ButtonRow(m_buttonDoor);
			PlanDoor& first = m_plan->doors[row[0]];
			int count = 0;
			for(int i : row)
				count += m_plan->doors.at(i).buttons.size();
			first.buttons << at;
			// The mirrored door, when drawing both sides, gets the mirrored button.
			if(m_mirror)
			{
				const PlanDoor& d = m_plan->doors.at(row[0]);
				const TriRef m = m_plan->Mirror(TriRef(d.deck, d.cx, d.cy, d.side));
				const int other = m_plan->DoorAt(m.deck, m.cx, m.cy, m.k);
				if(other >= 0 && !row.contains(other))
					m_plan->doors[ButtonRow(other)[0]].buttons << QPoint(at.x(), 8 * m_plan->Depth() - at.y());
			}
			emit PlanChanged(false);
			if(count + 1 >= 2)
				StopButtons();
			else
				StartButtons(m_buttonDoor);
			return;
		}
		StopButtons();
	}
	if(m_mode == DoorMode)
	{
		const int existing = m_plan->DoorAt(t.deck, t.cx, t.cy, t.k);
		// Shift and a click on a door: place its buttons again.
		if(!erase && existing >= 0 && (event->modifiers() & Qt::ShiftModifier) && m_plan->doors.at(existing).kind.isEmpty())
		{
			emit AboutToChange();
			for(int i : ButtonRow(existing))
				m_plan->doors[i].buttons.clear();
			emit PlanChanged(false);
			StartButtons(existing);
			return;
		}
		if(erase)
		{
			// The right button gives a door, and the rest of its row, the
			// texture and the way down picked for doors.
			if(existing < 0)
				return;
			emit AboutToChange();
			QList<int> row;
			row << existing;
			for(int at = 0; at < row.size(); ++at)
				for(int i = 0; i < m_plan->doors.size(); ++i)
					if(!row.contains(i) && m_plan->DoorsJoin(m_plan->doors.at(i), m_plan->doors.at(row[at])))
						row << i;
			for(int i : row)
			{
				PlanDoor& door = m_plan->doors[i];
				door.tex = m_doorTexture;
				door.exit = m_doorExit;
				door.rampRun = m_doorRampRun;
			}
			emit PlanChanged(false);
			update();
			return;
		}
		emit AboutToChange();
		m_doorDrag = true;
		m_doorMoved = false;
		m_doorStart = t;
		m_doorAlong = (t.k == TRI_E || t.k == TRI_W) ? t.cy : t.cx;
		m_doorHadOne = existing >= 0;
		m_doorRow = m_plan->NewDoorRow();
		m_doorMirrorRow = m_doorRow + 1;
		if(existing < 0 && !PlaceDoorInRow(t.deck, t.cx, t.cy, t.k))
		{
			m_doorDrag = false;
			Notice(t, m_doorKind.isEmpty() ? "A door has to be on the edge between a room and another room, or the outside: click the "
					"triangle of the room cell that stands on that edge." : "A window or a balcony between rooms goes on the "
					"edge between two rooms: click the triangle of a room cell that stands on that edge.");
		}
		emit PlanChanged(false);
		update();
		return;
	}
	if(m_mode == SkylightMode)
	{
		emit AboutToChange();
		// A triangle with the triangle brush, else (or with Shift) the whole
		// cell, and with the wide brush the cells round it. Taking one triangle out
		// of a whole cell leaves the other three; four triangles of one kind
		// become a whole cell again.
		const bool wholeCell = (event->modifiers() & Qt::ShiftModifier) || m_brush != TriangleBrush;
		auto toggle = [this, wholeCell](const TriRef& at) -> bool {
			const int existing = m_plan->SkylightOn(at);
			if(existing >= 0)
			{
				const PlanSkylight was = m_plan->skylights.at(existing);
				if(wholeCell)
				{
					for(int i = m_plan->skylights.size() - 1; i >= 0; --i)
					{
						const PlanSkylight& w = m_plan->skylights.at(i);
						if(w.deck == at.deck && w.cx == at.cx && w.cy == at.cy)
							m_plan->skylights.removeAt(i);
					}
					return true;
				}
				m_plan->skylights.removeAt(existing);
				if(was.k < 0)
					for(int k = 0; k < 4; ++k)
						if(k != at.k)
						{
							PlanSkylight w = was;
							w.k = k;
							m_plan->skylights << w;
						}
				return true;
			}
			PlanSkylight w;
			w.deck = at.deck;
			w.cx = at.cx;
			w.cy = at.cy;
			w.k = wholeCell ? -1 : at.k;
			w.shape = m_skylightShape;
			if(wholeCell)
				for(int i = m_plan->skylights.size() - 1; i >= 0; --i)
				{
					const PlanSkylight& o = m_plan->skylights.at(i);
					if(o.deck == at.deck && o.cx == at.cx && o.cy == at.cy)
						m_plan->skylights.removeAt(i);
				}
			if(!m_plan->SkylightFits(w))
				return false;
			m_plan->skylights << w;
			// Four triangles alike make the whole cell.
			QList<int> parts;
			for(int i = 0; i < m_plan->skylights.size(); ++i)
			{
				const PlanSkylight& o = m_plan->skylights.at(i);
				if(o.deck == w.deck && o.cx == w.cx && o.cy == w.cy && o.k >= 0 && o.shape == w.shape && o.floor == w.floor)
					parts << i;
			}
			if(parts.size() == 4)
			{
				for(int j = parts.size() - 1; j >= 0; --j)
					m_plan->skylights.removeAt(parts.at(j));
				w.k = -1;
				m_plan->skylights << w;
			}
			return true;
		};
		// The brush says how much: the triangle, its cell, or the cells round
		// it too. A click on a ceiling window takes them out, elsewhere puts
		// them in.
		const bool had = m_plan->SkylightOn(t) >= 0;
		QList<TriRef> targets;
		if(m_brush == WideBrush)
		{
			for(int dy = -1; dy <= 1; ++dy)
				for(int dx = -1; dx <= 1; ++dx)
				{
					const TriRef c(t.deck, t.cx + dx, t.cy + dy, t.k);
					if(m_plan->Valid(c))
						targets << c;
				}
		}
		else
			targets << t;
		int changed = 0;
		QList<TriRef> done;
		for(const TriRef& one : targets)
			for(const TriRef& at : { one, m_mirror ? m_plan->Mirror(one) : one })
			{
				if(done.contains(at))
					continue;
				done << at;
				if((m_plan->SkylightOn(at) >= 0) == had && toggle(at))
					++changed;
			}
		if(!changed)
			Notice(t, wholeCell ? "A whole-cell ceiling window goes in a whole cell of room with no room on any deck above it."
					: "A ceiling window goes in a triangle of a room with no room on any deck above it.");
		emit PlanChanged(false);
		update();
		return;
	}
	if(m_mode == WindowMode)
	{
		emit AboutToChange();
		TriRef w;
		int we;
		WindowEdgeAt(ToWorld(event->pos()), t, w, we);
		const bool had = m_plan->WindowAt(w.deck, w.cx, w.cy, w.k, we) >= 0;
		if(!had && !WindowOuter(w, we))
		{
			Notice(t, "A window has to be on an edge between a room and the outside, where there is no door: a "
					"cell edge or a diagonal. Click near that edge, or drag along the wall for a row of them.");
			return;
		}
		// A drag from here puts windows in (or, from a window, takes them out)
		// wherever the mouse passes.
		m_windowDrag = true;
		m_windowAdding = !had;
		m_windowRow = m_plan->NewWindowRow();
		DragWindow(w, we);
		emit PlanChanged(false);
		update();
		return;
	}
	if(m_mode == GroundParkingMode || m_mode == SpaceParkingMode || m_mode == SpacecraftParkingMode)
	{
		int ix, iy;
		NearestCorner(ToWorld(event->pos()), ix, iy);
		emit AboutToChange();
		const bool spacecraft = m_mode == SpacecraftParkingMode;
		if(spacecraft)
		{
			// A landing spot is large: a click anywhere on one takes it away.
			const int under = SpacecraftSpotUnder(ToWorld(event->pos()));
			if(under >= 0)
			{
				m_plan->parking.removeAt(under);
				emit PlanChanged(false);
				update();
				return;
			}
		}
		auto toggle = [this, spacecraft](int deck, int cx, int cy) -> bool {
			const int existing = spacecraft ? -1 : m_plan->ParkingAt(deck, cx, cy);
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
			spot.spacecraft = spacecraft;
			spot.across = m_parkingAcross;
			if(!m_plan->ParkingFits(spot))
				return false;
			m_plan->parking << spot;
			return true;
		};
		const bool had = m_plan->ParkingAt(m_deck, ix, iy) >= 0;
		if(spacecraft && !toggle(m_deck, ix, iy))
			Notice(t, "A spacecraft landing spot (238 ft, about 73 m, square) cannot overlap another one on the same deck.");
		else if(spacecraft)
			;
		else if(!toggle(m_deck, ix, iy))
			Notice(t, QString("A %1 parking spot (%2) has to lie on the floor of one room, centred on the grid corner "
					"nearest the mouse. Press R to turn it, make the room bigger, or click nearer its middle.")
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
	if(m_mode == WeaponMode && erase)
	{
		// The right button gives a weapon emitter the bay type picked.
		const int existing = m_plan->EmitterAt(t, true);
		if(existing < 0)
		{
			Notice(t, "A right click on a weapon emitter gives it the weapon bay type picked.");
			return;
		}
		emit AboutToChange();
		m_plan->emitters[existing].module = m_weaponModule;
		if(m_mirror)
		{
			const int other = m_plan->EmitterAt(m_plan->Mirror(t), true);
			if(other >= 0)
				m_plan->emitters[other].module = m_weaponModule;
		}
		emit PlanChanged(false);
		update();
		return;
	}
	if(m_mode == MarkingMode)
	{
		emit AboutToChange();
		auto toggle = [this](const TriRef& at) -> bool {
			// One marking per triangle: a click takes away a flag or a name.
			for(int marking = 1; marking <= 2; ++marking)
			{
				const int existing = m_plan->EmitterAt(at, false, marking);
				if(existing >= 0)
				{
					m_plan->emitters.removeAt(existing);
					return true;
				}
			}
			if(!m_plan->Hull(at))
				return false;
			PlanEmitter e;
			e.deck = at.deck;
			e.cx = at.cx;
			e.cy = at.cy;
			e.k = at.k;
			e.marking = m_marking;
			e.sizeFeet = m_markingFeet;
			m_plan->emitters << e;
			return true;
		};
		auto has = [this](const TriRef& at) {
			return m_plan->EmitterAt(at, false, 1) >= 0 || m_plan->EmitterAt(at, false, 2) >= 0;
		};
		const bool had = has(t);
		if(!toggle(t))
			Notice(t, "An empire flag or name goes on a hull triangle; it ends up on the nearest point of the hull.");
		else if(m_mirror)
		{
			const TriRef m = m_plan->Mirror(t);
			if(!(m == t) && has(m) == had)
				toggle(m);
		}
		emit PlanChanged(false);
		update();
		return;
	}
	if(m_mode == ContrailMode || m_mode == WeaponMode)
	{
		emit AboutToChange();
		const bool weapon = m_mode == WeaponMode;
		// A click on a contrail's ball is a click on the triangle it came from.
		if(!weapon)
		{
			const double reach = qMax(8.0, m_scale * m_plan->cellFeet * .3);
			for(const PlanEmitter& e : m_plan->emitters)
				if(!e.weapon && e.deck == m_deck)
				{
					const QPointF d = ToScreen(ContrailSpot(e)) - event->pos();
					if(d.x() * d.x() + d.y() * d.y() <= reach * reach)
					{
						t = TriRef(e.deck, e.cx, e.cy, e.k);
						break;
					}
				}
		}
		auto toggle = [this, weapon](const TriRef& at) -> bool {
			const int existing = m_plan->EmitterAt(at, weapon);
			if(existing >= 0)
			{
				m_plan->emitters.removeAt(existing);
				return true;
			}
			if(!m_plan->Hull(at))
				return false;
			PlanEmitter e;
			e.deck = at.deck;
			e.cx = at.cx;
			e.cy = at.cy;
			e.k = at.k;
			e.weapon = weapon;
			e.sizeFeet = m_emitterFeet;
			e.module = weapon ? m_weaponModule : 0;
			m_plan->emitters << e;
			return true;
		};
		const bool had = m_plan->EmitterAt(t, weapon) >= 0;
		if(!toggle(t))
			Notice(t, "An emitter goes on a hull triangle.");
		else if(m_mirror)
		{
			const TriRef m = m_plan->Mirror(t);
			if(!(m == t) && (m_plan->EmitterAt(m, weapon) >= 0) == had)
				toggle(m);
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

bool PlanView::PlaceDoorInRow(int deck, int cx, int cy, int side)
{
	auto place = [this](int deck, int cx, int cy, int side, int row) {
		const int existing = m_plan->DoorAt(deck, cx, cy, side);
		if(existing >= 0)
		{
			m_plan->doors[existing].row = row;
			return true;
		}
		const TriRef a(deck, cx, cy, side);
		const TriRef b = m_plan->AcrossCellEdge(deck, cx, cy, side);
		if((m_plan->Room(a) < 0 && m_plan->Room(b) < 0) || m_plan->Room(a) == m_plan->Room(b))
			return false;
		if(!m_doorKind.isEmpty() && (m_plan->Room(a) < 0 || m_plan->Room(b) < 0))
			return false;		// A window or a balcony goes between two rooms.
		PlanDoor d;
		d.deck = deck;
		d.cx = cx;
		d.cy = cy;
		d.side = side;
		d.row = row;
		d.tex = m_doorTexture;
		d.exit = m_doorExit;
		d.rampRun = m_doorRampRun;
		d.kind = m_doorKind;
		m_plan->doors << d;
		return true;
	};
	if(!place(deck, cx, cy, side, m_doorRow))
		return false;
	if(m_mirror)
	{
		const TriRef t(deck, cx, cy, side);
		const TriRef m = m_plan->Mirror(t);
		if(!(m == t))
			place(m.deck, m.cx, m.cy, m.k, m_doorMirrorRow);
	}
	return true;
}

// An edge (the cell edge or a diagonal) is an outer wall when there is a room
// on one side of it only, and no door on it.
bool PlanView::WindowOuter(const TriRef& a, int edge) const
{
	const TriRef b = m_plan->Across(a, edge);
	return (m_plan->Room(a) < 0) != (m_plan->Room(b) < 0)
			&& (edge != 0 || m_plan->DoorAt(a.deck, a.cx, a.cy, a.k) < 0);
}

// The edge of triangle t nearest the point, or, when that is not an outer
// wall, the cell's one outer wall if it has only one. False when neither.
bool PlanView::WindowEdgeAt(const QPointF& at, const TriRef& t, TriRef& w, int& we) const
{
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
	w = t;
	we = edge;
	if(m_plan->WindowAt(t.deck, t.cx, t.cy, t.k, edge) >= 0 || WindowOuter(t, edge))
		return true;
	int found = 0;
	TriRef fw;
	int fe = 0;
	for(int k = 0; k < 4; ++k)
		for(int e = 0; e < 2; ++e)		// Edge 2 of a triangle is edge 1 of the one before it.
			if(WindowOuter(TriRef(t.deck, t.cx, t.cy, k), e))
			{
				fw = TriRef(t.deck, t.cx, t.cy, k);
				fe = e;
				++found;
			}
	if(found != 1)
		return false;
	w = fw;
	we = fe;
	return true;
}

// Puts a window in on an edge, or takes it out, as the drag does, and the
// same on the mirrored edge.
void PlanView::DragWindow(const TriRef& w, int we)
{
	auto apply = [this](const TriRef& a, int edge) {
		const int existing = m_plan->WindowAt(a.deck, a.cx, a.cy, a.k, edge);
		if(!m_windowAdding)
		{
			if(existing >= 0)
				m_plan->windows.removeAt(existing);
			return;
		}
		if(existing >= 0 || !WindowOuter(a, edge))
			return;
		PlanDoor nw;
		nw.deck = a.deck;
		nw.cx = a.cx;
		nw.cy = a.cy;
		nw.side = a.k;
		nw.edge = edge;
		nw.row = m_windowRow;
		nw.style = m_windowStyle;
		m_plan->windows << nw;
	};
	apply(w, we);
	if(m_mirror)
	{
		const TriRef m = m_plan->Mirror(w);
		const int me = we == 0 ? 0 : 3 - we;		// Mirroring swaps the two diagonals.
		if(!(m == w && me == we))
			apply(m, me);
	}
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
	if(m_windowDrag)
	{
		TriRef t, w;
		int we;
		if(TriangleAt(event->pos(), t) && WindowEdgeAt(ToWorld(event->pos()), t, w, we))
		{
			const int before = m_plan->windows.size();
			DragWindow(w, we);
			if(m_plan->windows.size() != before)
			{
				emit PlanChanged(false);
				update();
			}
		}
		return;
	}
	if(m_doorDrag)
	{
		// Along the wall the door is on, however far the mouse strays from it.
		const QPointF world = ToWorld(event->pos());
		const bool xWall = m_doorStart.k == TRI_E || m_doorStart.k == TRI_W;
		int to = m_doorAlong;
		for(int i = 0; i < (xWall ? m_plan->Depth() : m_plan->Width()); ++i)
		{
			const QPointF c = xWall ? m_plan->CellCentre(m_doorStart.cx, i) : m_plan->CellCentre(i, m_doorStart.cy);
			const QPointF at = xWall ? m_plan->CellCentre(m_doorStart.cx, to) : m_plan->CellCentre(to, m_doorStart.cy);
			if(fabs((xWall ? c.y() : c.x()) - (xWall ? world.y() : world.x())) < fabs((xWall ? at.y() : at.x()) - (xWall ? world.y() : world.x())))
				to = i;
		}
		if(to != m_doorAlong || m_doorMoved)
		{
			m_doorMoved = true;
			for(int i = qMin(m_doorAlong, to); i <= qMax(m_doorAlong, to); ++i)
				PlaceDoorInRow(m_doorStart.deck, xWall ? m_doorStart.cx : i, xWall ? i : m_doorStart.cy, m_doorStart.k);
			emit PlanChanged(false);
			update();
		}
		return;
	}
	if(m_mode == DoorMode && m_buttonDoor >= 0)
	{
		QString why;
		const bool was = m_buttonHover;
		TriRef under;
		const bool onDoor = TriangleAt(event->pos(), under) && m_plan->DoorAt(under.deck, under.cx, under.cy, under.k) >= 0;
		m_buttonHover = !onDoor && ButtonSpot(ToWorld(event->pos()), m_buttonAt, m_buttonWallA, m_buttonWallB, &why);
		if(m_buttonHover || was)
			update();
		return;		// The hint about the buttons stays in the status bar.
	}
	if(m_mode == GroundParkingMode || m_mode == SpaceParkingMode || m_mode == SpacecraftParkingMode || CrewMode())
	{
		m_hovering = true;
		m_hoverWorld = ToWorld(event->pos());
		if(m_pathDrag)
			m_pathEnd = m_hoverWorld;
		setFocus();		// For R.
		update();
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
	m_windowDrag = false;
	if(m_pathDrag)
	{
		m_pathDrag = false;
		TriRef end;
		const int room = m_plan->Room(m_pathStart);
		if(m_plan->TriAt(m_deck, m_pathEnd, end) && !(end == m_pathStart) && room >= 0 && m_plan->Room(end) == room)
		{
			emit AboutToChange();
			auto add = [this](const TriRef& a, const TriRef& b) {
				PlanPath path;
				path.deck = a.deck;
				path.ax = a.cx, path.ay = a.cy, path.ak = a.k;
				path.bx = b.cx, path.by = b.cy, path.bk = b.k;
				m_plan->paths << path;
			};
			add(m_pathStart, end);
			if(m_mirror)
			{
				const TriRef ma = m_plan->Mirror(m_pathStart), mb = m_plan->Mirror(end);
				const bool same = (ma == m_pathStart && mb == end) || (ma == end && mb == m_pathStart);
				if(!same && m_plan->Room(ma) >= 0 && m_plan->Room(ma) == m_plan->Room(mb))
					add(ma, mb);
			}
			emit PlanChanged(false);
		}
		update();
	}
	if(m_doorDrag)
	{
		m_doorDrag = false;
		// A door was put in: offer its buttons.
		if(!(m_doorHadOne && !m_doorMoved))
		{
			const int placed = m_plan->DoorAt(m_doorStart.deck, m_doorStart.cx, m_doorStart.cy, m_doorStart.k);
			if(placed >= 0 && m_plan->doors.at(placed).kind.isEmpty())
				StartButtons(placed);
		}
		// A click on a door, not a drag from it, takes it away; with another
		// of the three tools (door, window between rooms, balcony) it becomes
		// what that tool puts in, its whole row.
		const int clicked = m_doorHadOne && !m_doorMoved
				? m_plan->DoorAt(m_doorStart.deck, m_doorStart.cx, m_doorStart.cy, m_doorStart.k) : -1;
		if(clicked >= 0 && m_plan->doors.at(clicked).kind != m_doorKind)
		{
			const PlanDoor& was = m_plan->doors.at(clicked);
			if(!m_doorKind.isEmpty())
			{
				// Windows and balconies only between two rooms.
				const TriRef b = m_plan->AcrossCellEdge(was.deck, was.cx, was.cy, was.side);
				if(!m_plan->Valid(b) || m_plan->Room(b) < 0 || m_plan->Room(TriRef(was.deck, was.cx, was.cy, was.side)) < 0)
				{
					Notice(m_doorStart, "A window or a balcony between rooms goes on the edge between two rooms; this "
							"door opens to the outside.");
					m_doorHadOne = false;
				}
			}
			if(m_doorHadOne)
			{
				QList<int> rows = ButtonRow(clicked);
				if(m_mirror)
				{
					const TriRef m = m_plan->Mirror(TriRef(was.deck, was.cx, was.cy, was.side));
					const int other = m_plan->DoorAt(m.deck, m.cx, m.cy, m.k);
					if(other >= 0)
						for(int i : ButtonRow(other))
							if(!rows.contains(i))
								rows << i;
				}
				for(int i : rows)
				{
					m_plan->doors[i].kind = m_doorKind;
					if(!m_doorKind.isEmpty())
						m_plan->doors[i].buttons.clear();
				}
				emit PlanChanged(false);
				update();
				m_doorHadOne = false;		// Not taken away below.
			}
			else
				m_doorHadOne = false;
		}
		if(m_doorHadOne && !m_doorMoved)
		{
			const TriRef t = m_doorStart;
			const int existing = m_plan->DoorAt(t.deck, t.cx, t.cy, t.k);
			if(existing >= 0)
				m_plan->doors.removeAt(existing);
			if(m_mirror)
			{
				const TriRef m = m_plan->Mirror(t);
				const int other = m_plan->DoorAt(m.deck, m.cx, m.cy, m.k);
				if(!(m == t) && other >= 0)
					m_plan->doors.removeAt(other);
			}
			emit PlanChanged(false);
			update();
		}
	}
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

void PlanView::NearestCorner(const QPointF& world, int& ix, int& iy) const
{
	const QPointF origin = m_plan->Corner(0, 0);
	ix = qBound(0, qRound((world.x() - origin.x()) / m_plan->cellFeet), m_plan->Width());
	iy = qBound(0, qRound((world.y() - origin.y()) / m_plan->cellFeet), m_plan->Depth());
}

void PlanView::SetHullTris(const AuScTriList& tris)
{
	m_hullTris = tris;
	m_contrailSpots.clear();
	update();
}

QPointF PlanView::ContrailSpot(const PlanEmitter& e) const
{
	const qint64 key = ((qint64(e.deck) * m_plan->Depth() + e.cy) * m_plan->Width() + e.cx) * 4 + e.k;
	QHash<qint64, QPointF>::const_iterator it = m_contrailSpots.constFind(key);
	if(it != m_contrailSpots.constEnd())
		return it.value();
	QPointF spot = m_plan->TriCentre(TriRef(e.deck, e.cx, e.cy, e.k));
	AuPoint at;
	if(ContrailEmitterAt(*m_plan, m_hullTris, e, at))
		spot = QPointF(at.x, at.y);
	m_contrailSpots.insert(key, spot);
	return spot;
}

void PlanView::keyPressEvent(QKeyEvent* event)
{
	if(event->key() == Qt::Key_Escape && m_buttonDoor >= 0)
	{
		StopButtons();
		return;
	}
	if(event->key() == Qt::Key_R && (m_mode == StationMode || m_mode == BerthMode))
	{
		// Shift+R turns the other way.
		m_itemYaw += (event->modifiers() & Qt::ShiftModifier) ? -45 : 45;
		m_itemYaw = fmod(m_itemYaw + 360.0, 360.0);
		if(m_itemYaw > 180)
			m_itemYaw -= 360;
		update();
		return;
	}
	if(event->key() == Qt::Key_R && (m_mode == GroundParkingMode || m_mode == SpaceParkingMode || m_mode == SpacecraftParkingMode))
	{
		m_parkingAcross = !m_parkingAcross;
		update();
		return;
	}
	QWidget::keyPressEvent(event);
}

void PlanView::leaveEvent(QEvent* event)
{
	m_hovering = false;
	update();
	QWidget::leaveEvent(event);
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The Crew step: stations, berths, transporter pads and walk paths by hand.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

QPointF PlanView::ToCells(const QPointF& world) const
{
	return (world - m_plan->Corner(0, 0)) / m_plan->cellFeet;
}

QPolygonF PlanView::ItemOutline(int kind, const QPointF& world, double yawDegrees, const QString& seat) const
{
	// Across and along the facing, in feet: a swivel chair about 1 m wide,
	// 0.6 m behind its middle and 1.1 m in front (the console); an
	// integrated desk 4 ft wide, 3.4 ft deep in front and 1.2 ft behind plus
	// the seat; a berth about 1 by 2 m; a pad 3 ft square.
	double halfAcross = 1.64, behind = 1.97, ahead = 3.61;
	const bool desk = seat == "desk" || (seat.isEmpty() && m_plan->bridgeStyle == "desks");
	if(kind == 0 && desk)
	{
		halfAcross = 2.0;
		behind = 1.2 + 2.0;
		ahead = 3.4;
	}
	else if(kind == 1)
	{
		halfAcross = 1.65;
		behind = ahead = 3.3;
	}
	else if(kind == 2)
		halfAcross = behind = ahead = 1.5;
	const double a = yawDegrees * M_PI / 180.0;
	const QPointF f(cos(a), sin(a)), side(-sin(a), cos(a));
	QPolygonF out;
	out << world - f * behind - side * halfAcross << world + f * ahead - side * halfAcross
		<< world + f * ahead + side * halfAcross << world - f * behind + side * halfAcross;
	return out;
}

int PlanView::OutlineRoom(int deck, const QPolygonF& outline) const
{
	// The corners, the middles of the sides and the middle all in one room.
	QList<QPointF> points;
	QPointF mid;
	for(int i = 0; i < outline.size(); ++i)
	{
		const QPointF a = outline.at(i), b = outline.at((i + 1) % outline.size());
		const QPointF in = (outline.boundingRect().center() - a) * 0.02;		// Just inside, not on a wall.
		points << a + in << (a + b) / 2.0 + ((outline.boundingRect().center() - (a + b) / 2.0) * 0.02);
		mid += a;
	}
	points << mid / outline.size();
	int room = -2;
	for(const QPointF& w : points)
	{
		const QPointF c = ToCells(w);
		const int r = m_plan->RoomAtPoint(deck, c.x(), c.y());
		if(r < 0 || (room != -2 && r != room))
			return -1;
		room = r;
	}
	return room < 0 ? -1 : room;
}

int PlanView::CrewItemAt(const QPointF& world) const
{
	const QPointF c = ToCells(world);
	const double reach = 2.0 / m_plan->cellFeet;	// 2 ft.
	auto closeTo = [&](int deck, double x, double y) {
		return deck == m_deck && std::hypot(x - c.x(), y - c.y()) <= reach;
	};
	if(m_mode == StationMode)
	{
		for(int i = 0; i < m_plan->stations.size(); ++i)
			if(closeTo(m_plan->stations.at(i).deck, m_plan->stations.at(i).x, m_plan->stations.at(i).y))
				return i;
	}
	else if(m_mode == BerthMode)
	{
		for(int i = 0; i < m_plan->berths.size(); ++i)
			if(closeTo(m_plan->berths.at(i).deck, m_plan->berths.at(i).x, m_plan->berths.at(i).y))
				return i;
	}
	else if(m_mode == PadMode)
	{
		for(int i = 0; i < m_plan->pads.size(); ++i)
			if(closeTo(m_plan->pads.at(i).deck, m_plan->pads.at(i).x, m_plan->pads.at(i).y))
				return i;
	}
	return -1;
}

int PlanView::PathAt(const QPointF& world) const
{
	const double reach = qMax(0.8, 6.0 / m_scale);		// Feet, at least a few pixels.
	for(int i = 0; i < m_plan->paths.size(); ++i)
	{
		const PlanPath& path = m_plan->paths.at(i);
		if(path.deck != m_deck)
			continue;
		const QPointF a = m_plan->TriCentre(TriRef(path.deck, path.ax, path.ay, path.ak));
		const QPointF b = m_plan->TriCentre(TriRef(path.deck, path.bx, path.by, path.bk));
		const QPointF ab = b - a;
		const double len2 = QPointF::dotProduct(ab, ab);
		const double t = len2 > 0 ? qBound(0.0, QPointF::dotProduct(world - a, ab) / len2, 1.0) : 0.0;
		const QPointF d = world - (a + ab * t);
		if(std::hypot(d.x(), d.y()) <= reach)
			return i;
	}
	return -1;
}

void PlanView::CrewClick(const QPointF& world, const TriRef& t, bool erase)
{
	if(m_mode == PathMode)
	{
		const int existing = PathAt(world);
		if(existing >= 0 || erase)
		{
			if(existing < 0)
				return;
			emit AboutToChange();
			const PlanPath was = m_plan->paths.at(existing);
			m_plan->paths.removeAt(existing);
			if(m_mirror)
			{
				// Its mirror image, if there is one.
				const TriRef ma = m_plan->Mirror(TriRef(was.deck, was.ax, was.ay, was.ak));
				const TriRef mb = m_plan->Mirror(TriRef(was.deck, was.bx, was.by, was.bk));
				for(int i = 0; i < m_plan->paths.size(); ++i)
				{
					const PlanPath& o = m_plan->paths.at(i);
					const TriRef oa(o.deck, o.ax, o.ay, o.ak), ob(o.deck, o.bx, o.by, o.bk);
					if((oa == ma && ob == mb) || (oa == mb && ob == ma))
					{
						m_plan->paths.removeAt(i);
						break;
					}
				}
			}
			emit PlanChanged(false);
			update();
			return;
		}
		if(m_plan->Room(t) < 0)
		{
			Notice(t, "A walk path runs on the floor of a room: start it in a room.");
			return;
		}
		m_pathDrag = true;
		m_pathStart = t;
		m_pathEnd = world;
		update();
		return;
	}

	const QPointF c = ToCells(world);
	const int existing = CrewItemAt(world);
	if(existing >= 0 || erase)
	{
		if(existing < 0)
			return;
		emit AboutToChange();
		// The item and its mirror image.
		double x = 0, y = 0;
		auto take = [&](double mx, double my) {
			const QPointF w = m_plan->CellPointToWorld(mx, my);
			const int i = CrewItemAt(w);
			if(i < 0)
				return;
			if(m_mode == StationMode)
				m_plan->stations.removeAt(i);
			else if(m_mode == BerthMode)
				m_plan->berths.removeAt(i);
			else
				m_plan->pads.removeAt(i);
		};
		if(m_mode == StationMode)
			x = m_plan->stations.at(existing).x, y = m_plan->stations.at(existing).y;
		else if(m_mode == BerthMode)
			x = m_plan->berths.at(existing).x, y = m_plan->berths.at(existing).y;
		else
			x = m_plan->pads.at(existing).x, y = m_plan->pads.at(existing).y;
		take(x, y);
		if(m_mirror && fabs(m_plan->Depth() - 2 * y) > 0.2)
			take(x, m_plan->Depth() - y);
		emit PlanChanged(false);
		update();
		return;
	}

	// Snap to half a foot.
	const QPointF snapped(qRound(world.x() * 2) / 2.0, qRound(world.y() * 2) / 2.0);
	const int kind = m_mode == StationMode ? 0 : m_mode == BerthMode ? 1 : 2;
	const QPolygonF outline = ItemOutline(kind, snapped, m_itemYaw, m_stationSeat);
	if(OutlineRoom(m_deck, outline) < 0)
	{
		Notice(t, kind == 0 ? "A station and its seat have to stand on the floor of one room. Press R to turn it, or click "
						"further from the walls."
				: kind == 1 ? "A berth has to lie on the floor of one room. Press R to turn it, or click further from the walls."
				: "A transporter pad (3 ft square) has to lie on the floor of one room.");
		return;
	}
	emit AboutToChange();
	const QPointF at = ToCells(snapped);
	auto add = [&](double x, double y, double yaw) {
		if(kind == 0)
		{
			PlanStation st;
			st.deck = m_deck;
			st.x = x;
			st.y = y;
			st.yaw = yaw;
			st.type = m_stationType;
			st.seat = m_stationSeat;
			st.module = m_stationType == "firecontrol" ? m_stationModule : 0;
			m_plan->stations << st;
		}
		else if(kind == 1)
		{
			PlanBerth b;
			b.deck = m_deck;
			b.x = x;
			b.y = y;
			b.yaw = yaw;
			b.kind = m_berthKind;
			m_plan->berths << b;
		}
		else
		{
			PlanPad pad;
			pad.deck = m_deck;
			pad.x = x;
			pad.y = y;
			m_plan->pads << pad;
		}
	};
	add(at.x(), at.y(), m_itemYaw);
	if(m_mirror && fabs(m_plan->Depth() - 2 * at.y()) > 0.2)
	{
		const double my = m_plan->Depth() - at.y();
		const QPointF mw = m_plan->CellPointToWorld(at.x(), my);
		if(CrewItemAt(mw) < 0 && OutlineRoom(m_deck, ItemOutline(kind, mw, -m_itemYaw, m_stationSeat)) >= 0)
			add(at.x(), my, -m_itemYaw);
	}
	(void)c;
	emit PlanChanged(false);
	update();
}

void PlanView::PaintCrew(QPainter& p, double cellPixels)
{
	const double pen = qMax(1.5, cellPixels / 12);
	auto drawOutline = [&](const QPolygonF& outline, double yaw, const QColor& colour, bool skipped, const QString& label) {
		QColor c = skipped ? QColor(120, 120, 120) : colour;
		QPolygonF screen;
		for(const QPointF& w : outline)
			screen << ToScreen(w);
		p.setPen(QPen(c, pen, skipped ? Qt::DashLine : Qt::SolidLine));
		QColor fill = c;
		fill.setAlpha(50);
		p.setBrush(fill);
		p.drawPolygon(screen);
		p.setBrush(Qt::NoBrush);
		// An arrow the way it faces.
		const QPointF centre = (outline.at(0) + outline.at(2)) / 2.0;
		const double a = yaw * M_PI / 180.0;
		const QPointF tip = centre + QPointF(cos(a), sin(a)) * 2.5;
		p.drawLine(ToScreen(centre), ToScreen(tip));
		if(!label.isEmpty() && cellPixels > 18)
		{
			p.setPen(c.lighter(140));
			const QPointF s = ToScreen(centre);
			p.drawText(QRectF(s.x() - 60, s.y() + 2, 120, 16), Qt::AlignHCenter | Qt::AlignTop, label);
		}
	};
	for(const PlanStation& st : m_plan->stations)
		if(st.deck == m_deck)
			drawOutline(ItemOutline(0, m_plan->CellPointToWorld(st.x, st.y), st.yaw, st.seat), st.yaw, QColor(120, 200, 255), st.skip,
					st.type + (st.seat.isEmpty() ? QString() : " (" + st.seat + ")"));
	for(const PlanBerth& b : m_plan->berths)
		if(b.deck == m_deck)
			drawOutline(ItemOutline(1, m_plan->CellPointToWorld(b.x, b.y), b.yaw, QString()), b.yaw, QColor(230, 180, 255), b.skip,
					b.kind + " berth");
	for(const PlanPad& pad : m_plan->pads)
		if(pad.deck == m_deck)
		{
			const QPointF w = m_plan->CellPointToWorld(pad.x, pad.y);
			const QColor c = pad.skip ? QColor(120, 120, 120) : QColor(255, 220, 120);
			const QRectF r = QRectF(ToScreen(w - QPointF(1.5, 1.5)), ToScreen(w + QPointF(1.5, 1.5))).normalized();
			p.setPen(QPen(c, pen, pad.skip ? Qt::DashLine : Qt::SolidLine));
			p.drawRect(r);
			p.drawEllipse(r.adjusted(r.width() * .15, r.height() * .15, -r.width() * .15, -r.height() * .15));
		}
	for(const PlanPath& path : m_plan->paths)
		if(path.deck == m_deck)
		{
			const QColor c = path.skip ? QColor(120, 120, 120) : QColor(90, 230, 120);
			p.setPen(QPen(c, pen, path.skip ? Qt::DotLine : Qt::SolidLine, Qt::RoundCap));
			const QPointF a = ToScreen(m_plan->TriCentre(TriRef(path.deck, path.ax, path.ay, path.ak)));
			const QPointF b = ToScreen(m_plan->TriCentre(TriRef(path.deck, path.bx, path.by, path.bk)));
			p.drawLine(a, b);
			p.setBrush(c);
			p.drawEllipse(a, pen * 1.5, pen * 1.5);
			p.drawEllipse(b, pen * 1.5, pen * 1.5);
			p.setBrush(Qt::NoBrush);
		}

	if(!CrewMode() || !m_hovering)
		return;
	if(m_mode == PathMode)
	{
		if(m_pathDrag)
		{
			TriRef end;
			const bool haveEnd = m_plan->TriAt(m_deck, m_pathEnd, end);
			const bool ok = haveEnd && !(end == m_pathStart) && m_plan->Room(end) >= 0 && m_plan->Room(end) == m_plan->Room(m_pathStart);
			const QPointF a = ToScreen(m_plan->TriCentre(m_pathStart));
			const QPointF b = ToScreen(haveEnd ? m_plan->TriCentre(end) : m_pathEnd);
			p.setPen(QPen(ok ? QColor(90, 230, 120) : QColor(255, 90, 90), pen, Qt::DashLine, Qt::RoundCap));
			p.drawLine(a, b);
			p.drawText(QRectF(b.x() - 200, b.y() + 6, 400, 20), Qt::AlignHCenter | Qt::AlignTop,
					ok ? "let go to put the path in" : "a path stays in one room");
		}
		else
		{
			const QPointF s = ToScreen(m_hoverWorld);
			const bool on = PathAt(m_hoverWorld) >= 0;
			p.setPen(on ? QColor(255, 160, 60) : QColor(200, 205, 215));
			p.drawText(QRectF(s.x() - 200, s.y() + 14, 400, 20), Qt::AlignHCenter | Qt::AlignTop,
					on ? "click takes this path away" : "drag from one triangle to another of the same room");
		}
		return;
	}
	const int kind = m_mode == StationMode ? 0 : m_mode == BerthMode ? 1 : 2;
	const bool taken = CrewItemAt(m_hoverWorld) >= 0;
	const QPointF snapped(qRound(m_hoverWorld.x() * 2) / 2.0, qRound(m_hoverWorld.y() * 2) / 2.0);
	const QPolygonF outline = ItemOutline(kind, snapped, m_itemYaw, m_stationSeat);
	const bool fits = OutlineRoom(m_deck, outline) >= 0;
	const QColor colour = taken ? QColor(255, 160, 60) : fits ? QColor(90, 230, 120) : QColor(255, 90, 90);
	if(!taken)
		drawOutline(outline, m_itemYaw, colour, false, QString());
	const QPointF s = ToScreen(snapped);
	p.setPen(colour);
	p.drawText(QRectF(s.x() - 250, s.y() + cellPixels * .6 + 8, 500, 20), Qt::AlignHCenter | Qt::AlignTop,
			taken ? "click takes this away" : fits ? (kind == 2 ? "click to place" : "click to place, R turns it")
			: "does not fit in one room" + QString(kind == 2 ? "" : ", R turns it"));
}
