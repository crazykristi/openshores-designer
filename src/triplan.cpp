//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - a ship drawn with triangles.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "triplan.h"
#include "names.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <cmath>

namespace {

// Run length coding for a layer: "count*value,count*value,...".
template<class T>
QString Encode(const QVector<T>& v)
{
	QStringList runs;
	int i = 0;
	while(i < v.size())
	{
		int j = i;
		while(j < v.size() && v.at(j) == v.at(i))
			++j;
		runs << QString("%1*%2").arg(j - i).arg(int(v.at(i)));
		i = j;
	}
	return runs.join(',');
}

template<class T>
bool Decode(const QString& text, int size, QVector<T>& out)
{
	out.clear();
	out.reserve(size);
	if(!text.isEmpty())
	{
		for(const QString& run : text.split(','))
		{
			const int star = run.indexOf('*');
			if(star < 0)
				return false;
			const int count = run.left(star).toInt();
			const int value = run.mid(star + 1).toInt();
			for(int i = 0; i < count; ++i)
				out.append(T(value));
		}
	}
	return out.size() == size;
}

QString ColourName(const QColor& c)
{
	return c.name(QColor::HexRgb);
}

}	// namespace

TriPlan::TriPlan()
{
	PlanPaint base;
	base.name = "Hull";
	palette << base;
	Resize(m_width, m_depth);
}

void TriPlan::Resize(int width, int depth)
{
	width = qBound(2, width, 400);
	depth = qBound(2, depth, 400);
	const int ox = (width - m_width) / 2;
	const int oy = (depth - m_depth) / 2;
	const bool empty = m_hull.isEmpty();

	QVector<QVector<quint8> > hull(m_decks), paint(m_decks);
	QVector<QVector<qint16> > room(m_decks);
	for(int d = 0; d < m_decks; ++d)
	{
		hull[d].fill(0, width * depth * 4);
		paint[d].fill(0, width * depth * 4);
		room[d].fill(-1, width * depth * 4);
		if(empty)
			continue;
		for(int cy = 0; cy < m_depth; ++cy)
			for(int cx = 0; cx < m_width; ++cx)
			{
				const int nx = cx + ox, ny = cy + oy;
				if(nx < 0 || ny < 0 || nx >= width || ny >= depth)
					continue;
				for(int k = 0; k < 4; ++k)
				{
					const int from = Index(cx, cy, k);
					const int to = (ny * width + nx) * 4 + k;
					hull[d][to] = m_hull[d][from];
					paint[d][to] = m_paint[d][from];
					room[d][to] = m_room[d][from];
				}
			}
	}
	m_width = width;
	m_depth = depth;
	m_hull = hull;
	m_paint = paint;
	m_room = room;
	for(PlanDoor& door : doors)
	{
		door.cx += ox;
		door.cy += oy;
		for(QPointF& b : door.buttons)
			b += QPointF(8 * ox, 8 * oy);
	}
	for(PlanDoor& window : windows)
	{
		window.cx += ox;
		window.cy += oy;
	}
	for(PlanLadder& ladder : ladders)
	{
		ladder.cx += ox;
		ladder.cy += oy;
	}
	for(PlanSkylight& window : skylights)
	{
		window.cx += ox;
		window.cy += oy;
	}
	for(PlanParking& spot : parking)
	{
		spot.cx += ox;
		spot.cy += oy;
	}
	for(PlanStair& stair : stairs)
	{
		stair.cx += ox;
		stair.cy += oy;
	}
	for(PlanEmitter& e : emitters)
	{
		e.cx += ox;
		e.cy += oy;
	}
	for(PlanStation& st : stations)
	{
		st.x += ox;
		st.y += oy;
	}
	for(PlanBerth& b : berths)
	{
		b.x += ox;
		b.y += oy;
	}
	for(PlanPad& pad : pads)
	{
		pad.x += ox;
		pad.y += oy;
	}
	for(PlanPath& path : paths)
	{
		path.ax += ox;
		path.ay += oy;
		path.bx += ox;
		path.by += oy;
	}
	Tidy();
}

void TriPlan::AddDeck(bool below)
{
	const int n = m_width * m_depth * 4;
	QVector<quint8> hull(n, 0), paint(n, 0);
	QVector<qint16> room(n, -1);
	if(below)
	{
		m_hull.prepend(hull);
		m_paint.prepend(paint);
		m_room.prepend(room);
		for(PlanDoor& door : doors)
			++door.deck;
		for(PlanDoor& window : windows)
			++window.deck;
		for(PlanLadder& ladder : ladders)
			++ladder.deck;
		for(PlanSkylight& window : skylights)
			++window.deck;
		for(PlanParking& spot : parking)
			++spot.deck;
		for(PlanStair& stair : stairs)
			++stair.deck;
		for(PlanEmitter& e : emitters)
			++e.deck;
		for(PlanStation& st : stations)
			++st.deck;
		for(PlanBerth& b : berths)
			++b.deck;
		for(PlanPad& pad : pads)
			++pad.deck;
		for(PlanPath& path : paths)
			++path.deck;
	}
	else
	{
		m_hull.append(hull);
		m_paint.append(paint);
		m_room.append(room);
	}
	++m_decks;
}

// Takes the items on a deck that goes away out of a list, and moves those
// above it down one.
template<typename T> static void DropDeck(QList<T>& list, int deck)
{
	for(int k = list.size() - 1; k >= 0; --k)
	{
		if(list.at(k).deck == deck)
			list.removeAt(k);
		else if(list.at(k).deck > deck)
			--list[k].deck;
	}
}

void TriPlan::RemoveDeck(int deck)
{
	if(m_decks <= 1 || deck < 0 || deck >= m_decks)
		return;
	m_hull.remove(deck);
	m_paint.remove(deck);
	m_room.remove(deck);
	--m_decks;
	QList<PlanDoor> kept;
	for(PlanDoor door : doors)
	{
		if(door.deck == deck)
			continue;
		if(door.deck > deck)
			--door.deck;
		kept << door;
	}
	doors = kept;
	QList<PlanDoor> keptWindows;
	for(PlanDoor window : windows)
	{
		if(window.deck == deck)
			continue;
		if(window.deck > deck)
			--window.deck;
		keptWindows << window;
	}
	windows = keptWindows;

	QList<PlanLadder> keptLadders;
	for(PlanLadder ladder : ladders)
	{
		if(ladder.deck == deck || ladder.deck + 1 == deck)
			continue;
		if(ladder.deck > deck)
			--ladder.deck;
		keptLadders << ladder;
	}
	ladders = keptLadders;

	QList<PlanSkylight> keptSkylights;
	for(PlanSkylight window : skylights)
	{
		if(window.deck == deck)
			continue;
		if(window.deck > deck)
			--window.deck;
		keptSkylights << window;
	}
	skylights = keptSkylights;

	QList<PlanParking> keptParking;
	for(PlanParking spot : parking)
	{
		if(spot.deck == deck)
			continue;
		if(spot.deck > deck)
			--spot.deck;
		keptParking << spot;
	}
	parking = keptParking;

	QList<PlanStair> keptStairs;
	for(PlanStair stair : stairs)
	{
		if(stair.deck == deck || stair.deck + 1 == deck)
			continue;
		if(stair.deck > deck)
			--stair.deck;
		keptStairs << stair;
	}
	stairs = keptStairs;

	QList<PlanEmitter> keptEmitters;
	for(PlanEmitter e : emitters)
	{
		if(e.deck == deck)
			continue;
		if(e.deck > deck)
			--e.deck;
		keptEmitters << e;
	}
	emitters = keptEmitters;
	// The rest placed by hand, in one go.
	DropDeck(stations, deck);
	DropDeck(berths, deck);
	DropDeck(pads, deck);
	DropDeck(paths, deck);
}

bool TriPlan::Valid(const TriRef& t) const
{
	return t.deck >= 0 && t.deck < m_decks && t.cx >= 0 && t.cy >= 0 && t.cx < m_width && t.cy < m_depth
		&& t.k >= 0 && t.k < 4;
}

bool TriPlan::Hull(const TriRef& t) const
{
	return Valid(t) && m_hull[t.deck][Index(t.cx, t.cy, t.k)];
}

int TriPlan::Room(const TriRef& t) const
{
	return Valid(t) ? m_room[t.deck][Index(t.cx, t.cy, t.k)] : -1;
}

int TriPlan::Paint(const TriRef& t) const
{
	return Valid(t) ? m_paint[t.deck][Index(t.cx, t.cy, t.k)] : 0;
}

void TriPlan::SetHull(const TriRef& t, bool on)
{
	if(!Valid(t))
		return;
	m_hull[t.deck][Index(t.cx, t.cy, t.k)] = on ? 1 : 0;
	if(!on)
		m_room[t.deck][Index(t.cx, t.cy, t.k)] = -1;
}

void TriPlan::SetRoom(const TriRef& t, int room)
{
	if(!Valid(t))
		return;
	if(room >= 0 && !Hull(t))
		return;			// A room has to be inside the hull.
	m_room[t.deck][Index(t.cx, t.cy, t.k)] = qint16(room);
}

void TriPlan::SetPaint(const TriRef& t, int paint)
{
	if(!Valid(t))
		return;
	m_paint[t.deck][Index(t.cx, t.cy, t.k)] = quint8(qBound(0, paint, 255));
}

QPointF TriPlan::Corner(int ix, int iy) const
{
	return QPointF((ix - m_width / 2.0) * cellFeet, (iy - m_depth / 2.0) * cellFeet);
}

QPointF TriPlan::EighthPoint(const QPointF& q) const
{
	return QPointF((q.x() / 8.0 - m_width / 2.0) * cellFeet, (q.y() / 8.0 - m_depth / 2.0) * cellFeet);
}

QPointF TriPlan::ToEighths(const QPointF& w) const
{
	return QPointF((w.x() / cellFeet + m_width / 2.0) * 8.0, (w.y() / cellFeet + m_depth / 2.0) * 8.0);
}

bool TriPlan::ButtonWallSpot(const TriRef& t, const QPointF& p, double margin, QPointF& spot, QPointF& inward,
		QPointF* wallA, QPointF* wallB) const
{
	if(!Valid(t))
		return false;
	double best = 1e30;
	for(int e = 0; e < 3; ++e)
	{
		const TriRef across = Across(t, e);
		if(Valid(across) && Room(across) == Room(t))
			continue;		// Not a wall.
		if(e == 0 && DoorAt(t.deck, t.cx, t.cy, t.k) >= 0)
			continue;		// A doorway.
		QPointF a, b;
		EdgeEnds(t, e, a, b);
		const QPointF ab = b - a;
		const double len = sqrt(QPointF::dotProduct(ab, ab));
		if(len < 1e-9)
			continue;
		const QPointF dir = ab / len;
		const double m = qMin(margin, len / 2);
		const double along = qBound(m, QPointF::dotProduct(p - a, dir), len - m);
		const QPointF on = a + dir * along, off = p - on;
		const double d = QPointF::dotProduct(off, off);
		if(d < best)
		{
			best = d;
			spot = on;
			QPointF in(-dir.y(), dir.x());
			if(QPointF::dotProduct(in, TriCentre(t) - on) < 0)
				in = -in;
			inward = in;
			if(wallA)
				*wallA = a;
			if(wallB)
				*wallB = b;
		}
	}
	return best < 1e29;
}

bool TriPlan::TriAt(int deck, const QPointF& w, TriRef& out) const
{
	const double gx = w.x() / cellFeet + m_width / 2.0, gy = w.y() / cellFeet + m_depth / 2.0;
	const int cx = int(floor(gx)), cy = int(floor(gy));
	if(deck < 0 || deck >= m_decks || cx < 0 || cy < 0 || cx >= m_width || cy >= m_depth)
		return false;
	// Which of the four triangles: compare with the two diagonals.
	const double u = gx - cx, v = gy - cy;
	const double d1 = v - u, d2 = v + u - 1;
	const int k = (d1 < 0 && d2 < 0) ? TRI_S : (d1 < 0 ? TRI_E : (d2 >= 0 ? TRI_N : TRI_W));
	out = TriRef(deck, cx, cy, k);
	return true;
}

QPointF TriPlan::CellCentre(int cx, int cy) const
{
	return QPointF((cx + 0.5 - m_width / 2.0) * cellFeet, (cy + 0.5 - m_depth / 2.0) * cellFeet);
}

void TriPlan::TriCorners(const TriRef& t, QPointF out[3]) const
{
	const QPointF corners[4] = { Corner(t.cx, t.cy), Corner(t.cx + 1, t.cy),
			Corner(t.cx + 1, t.cy + 1), Corner(t.cx, t.cy + 1) };
	out[0] = corners[t.k];
	out[1] = corners[(t.k + 1) % 4];
	out[2] = CellCentre(t.cx, t.cy);
}

QPointF TriPlan::TriCentre(const TriRef& t) const
{
	QPointF c[3];
	TriCorners(t, c);
	return (c[0] + c[1] + c[2]) / 3.0;
}

TriRef TriPlan::AcrossCellEdge(int deck, int cx, int cy, int side) const
{
	static const int dx[4] = { 0, 1, 0, -1 };
	static const int dy[4] = { -1, 0, 1, 0 };
	return TriRef(deck, cx + dx[side], cy + dy[side], (side + 2) % 4);
}

TriRef TriPlan::Across(const TriRef& t, int edge) const
{
	if(edge == 0)
		return AcrossCellEdge(t.deck, t.cx, t.cy, t.k);
	if(edge == 1)
		return TriRef(t.deck, t.cx, t.cy, (t.k + 1) % 4);
	return TriRef(t.deck, t.cx, t.cy, (t.k + 3) % 4);
}

TriRef TriPlan::Mirror(const TriRef& t) const
{
	int k = t.k;
	if(k == TRI_S)
		k = TRI_N;
	else if(k == TRI_N)
		k = TRI_S;
	return TriRef(t.deck, t.cx, m_depth - 1 - t.cy, k);
}

bool TriPlan::DoorsJoin(const PlanDoor& a, const PlanDoor& b) const
{
	if(a.deck != b.deck || a.row != b.row || a.kind != b.kind)
		return false;
	const bool xWall = a.side == TRI_E || a.side == TRI_W;
	if(xWall != (b.side == TRI_E || b.side == TRI_W))
		return false;
	// The line of the wall, the same whichever cell names the door.
	auto line = [](const PlanDoor& d) {
		return d.side == TRI_E ? d.cx + 1 : d.side == TRI_W ? d.cx : d.side == TRI_N ? d.cy + 1 : d.cy;
	};
	if(line(a) != line(b) || abs(xWall ? a.cy - b.cy : a.cx - b.cx) != 1)
		return false;
	auto rooms = [this](const PlanDoor& d) {
		const int p = Room(TriRef(d.deck, d.cx, d.cy, d.side));
		const int q = Room(AcrossCellEdge(d.deck, d.cx, d.cy, d.side));
		return qMakePair(qMin(p, q), qMax(p, q));
	};
	return rooms(a) == rooms(b);
}

int TriPlan::NewDoorRow() const
{
	int most = 0;
	for(const PlanDoor& d : doors)
		most = qMax(most, d.row);
	return most + 1;
}

int TriPlan::NewWindowRow() const
{
	int most = 0;
	for(const PlanDoor& w : windows)
		most = qMax(most, w.row);
	return most + 1;
}

int TriPlan::DoorAt(int deck, int cx, int cy, int side) const
{
	// A door can be named from either cell.
	const TriRef other = AcrossCellEdge(deck, cx, cy, side);
	for(int i = 0; i < doors.size(); ++i)
	{
		const PlanDoor& d = doors.at(i);
		if(d.deck != deck)
			continue;
		if(d.cx == cx && d.cy == cy && d.side == side)
			return i;
		if(d.cx == other.cx && d.cy == other.cy && d.side == other.k)
			return i;
	}
	return -1;
}

int TriPlan::WindowAt(int deck, int cx, int cy, int side, int edge) const
{
	// A window can be named from the triangle on either side of its edge.
	TriRef other = edge == 0 ? AcrossCellEdge(deck, cx, cy, side) : Across(TriRef(deck, cx, cy, side), edge);
	const int otherEdge = edge == 0 ? 0 : 3 - edge;
	for(int i = 0; i < windows.size(); ++i)
	{
		const PlanDoor& w = windows.at(i);
		if(w.deck != deck)
			continue;
		if(w.cx == cx && w.cy == cy && w.side == side && w.edge == edge)
			return i;
		if(w.cx == other.cx && w.cy == other.cy && w.side == other.k && w.edge == otherEdge)
			return i;
	}
	return -1;
}

void TriPlan::EdgeEnds(const TriRef& t, int edge, QPointF& a, QPointF& b) const
{
	QPointF c[3];
	TriCorners(t, c);
	a = c[edge % 3];
	b = c[(edge + 1) % 3];
}

int TriPlan::ParkingAt(int deck, int cx, int cy) const
{
	for(int i = 0; i < parking.size(); ++i)
		if(parking.at(i).deck == deck && parking.at(i).cx == cx && parking.at(i).cy == cy)
			return i;
	return -1;
}

int TriPlan::EmitterAt(const TriRef& t, bool weapon, int marking) const
{
	for(int i = 0; i < emitters.size(); ++i)
	{
		const PlanEmitter& e = emitters.at(i);
		if(e.weapon == weapon && e.marking == marking && e.deck == t.deck && e.cx == t.cx && e.cy == t.cy && e.k == t.k)
			return i;
	}
	return -1;
}

QPointF TriPlan::ParkingSize(const PlanParking& p) const
{
	// AuScGroundVehicleParkingSpot / AuScSpaceVehicleParkingSpot::Icon*.
	const double feet = 3.2808399;
	if(p.spacecraft)
		return QPointF(238., 238.);	// AuScSpacecraftParkingSpot: GroundSpawnSizeLimit each way.
	const QPointF size = p.space ? QPointF(17 * feet, 12 * feet) : QPointF(12 * feet, 6 * feet);
	return p.across ? QPointF(size.y(), size.x()) : size;
}

bool TriPlan::ParkingFits(const PlanParking& p) const
{
	const QPointF centre = Corner(p.cx, p.cy), half = ParkingSize(p) / 2.0;
	if(p.spacecraft)
	{
		// Anywhere on the grid, as long as it keeps clear of the other
		// landing spots on its deck.
		if(p.deck < 0 || p.deck >= Decks() || p.cx < 0 || p.cx > m_width || p.cy < 0 || p.cy > m_depth)
			return false;
		for(const PlanParking& o : parking)
		{
			if(!o.spacecraft || o.deck != p.deck || (o.cx == p.cx && o.cy == p.cy))
				continue;
			const QPointF oc = Corner(o.cx, o.cy), oh = ParkingSize(o) / 2.0;
			if(qAbs(oc.x() - centre.x()) < oh.x() + half.x() - .01 && qAbs(oc.y() - centre.y()) < oh.y() + half.y() - .01)
				return false;
		}
		return true;
	}
	int room = -2;
	// Every cell the spot covers (less a little slack at its edges) has to be
	// a whole cell of the same room.
	for(int cy = 0; cy < m_depth; ++cy)
		for(int cx = 0; cx < m_width; ++cx)
		{
			const QPointF lo = Corner(cx, cy), hi = Corner(cx + 1, cy + 1);
			const double slack = cellFeet * .25;
			if(hi.x() <= centre.x() - half.x() + slack || lo.x() >= centre.x() + half.x() - slack ||
					hi.y() <= centre.y() - half.y() + slack || lo.y() >= centre.y() + half.y() - slack)
				continue;
			const int r = CellRoom(p.deck, cx, cy);
			if(r < 0 || (room != -2 && r != room))
				return false;
			room = r;
		}
	return room >= 0;
}

int TriPlan::LadderAt(int deck, int cx, int cy) const
{
	for(int i = 0; i < ladders.size(); ++i)
		if(ladders.at(i).deck == deck && ladders.at(i).cx == cx && ladders.at(i).cy == cy)
			return i;
	return -1;
}

int TriPlan::SkylightAt(int deck, int cx, int cy) const
{
	for(int i = 0; i < skylights.size(); ++i)
		if(skylights.at(i).deck == deck && skylights.at(i).cx == cx && skylights.at(i).cy == cy)
			return i;
	return -1;
}

int TriPlan::SkylightOn(const TriRef& t) const
{
	for(int i = 0; i < skylights.size(); ++i)
	{
		const PlanSkylight& w = skylights.at(i);
		if(w.deck == t.deck && w.cx == t.cx && w.cy == t.cy && (w.k < 0 || w.k == t.k))
			return i;
	}
	return -1;
}

bool TriPlan::SkylightFits(const PlanSkylight& window) const
{
	// A canopy may also reach over hull next to its room (a fighter's canopy
	// runs on over the nose); the build gives it the room it joins.
	const bool canopyOverHull = window.shape == "canopy" && !window.floor &&
			(window.k >= 0 ? Hull(TriRef(window.deck, window.cx, window.cy, window.k))
				: Hull(TriRef(window.deck, window.cx, window.cy, 0)) && Hull(TriRef(window.deck, window.cx, window.cy, 1)) &&
				  Hull(TriRef(window.deck, window.cx, window.cy, 2)) && Hull(TriRef(window.deck, window.cx, window.cy, 3)));
	if(!canopyOverHull && (window.k >= 0 ? Room(TriRef(window.deck, window.cx, window.cy, window.k)) < 0
			: CellRoom(window.deck, window.cx, window.cy) < 0))
		return false;
	const int step = window.floor ? -1 : 1;
	for(int d = window.deck + step; d >= 0 && d < m_decks; d += step)
		for(int k = 0; k < 4; ++k)
			if((window.k < 0 || k == window.k) && Room(TriRef(d, window.cx, window.cy, k)) >= 0)
				return false;
	return true;
}

bool TriPlan::SkylightIsHull(const PlanSkylight& window) const
{
	return window.shape == "hull" || window.shape == "canopy" || (window.shape.isEmpty() && windowStyle == "hull");
}

int TriPlan::Flip(bool foreAft, bool portStarboard, bool upsideDown, int deck)
{
	if(deck >= 0)
		upsideDown = false;
	if(!foreAft && !portStarboard && !upsideDown)
		return 0;
	const int before = ladders.size() + skylights.size() + stairs.size() + parking.size();
	// Upside down is a half turn about x: y is mirrored as well as z.
	const bool my = portStarboard != upsideDown;
	auto flipped = [&](int d) { return deck < 0 || d == deck; };
	auto newDeck = [&](int d) { return upsideDown ? m_decks - 1 - d : d; };
	auto side = [&](int k) {
		if(foreAft && (k == TRI_E || k == TRI_W))
			return k == TRI_E ? int(TRI_W) : int(TRI_E);
		if(my && (k == TRI_N || k == TRI_S))
			return k == TRI_N ? int(TRI_S) : int(TRI_N);
		return k;
	};
	auto cellX = [&](int cx) { return foreAft ? m_width - 1 - cx : cx; };
	auto cellY = [&](int cy) { return my ? m_depth - 1 - cy : cy; };
	// One mirror turns the corners of a triangle the other way round: its two
	// diagonals (edges 1 and 2) swap.
	const bool reversed = (foreAft ? 1 : 0) + (my ? 1 : 0) == 1;

	QVector<QVector<quint8> > hull = m_hull, paint = m_paint;
	QVector<QVector<qint16> > room = m_room;
	for(int d = 0; d < m_decks; ++d)
	{
		if(!flipped(d))
			continue;
		const int nd = newDeck(d);
		for(int cy = 0; cy < m_depth; ++cy)
			for(int cx = 0; cx < m_width; ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const int from = Index(cx, cy, k), to = Index(cellX(cx), cellY(cy), side(k));
					hull[nd][to] = m_hull[d][from];
					paint[nd][to] = m_paint[d][from];
					room[nd][to] = m_room[d][from];
				}
	}
	m_hull = hull;
	m_paint = paint;
	m_room = room;
	for(QList<PlanDoor>* list : { &doors, &windows })
		for(PlanDoor& door : *list)
			if(flipped(door.deck))
			{
				door.deck = newDeck(door.deck);
				door.cx = cellX(door.cx);
				door.cy = cellY(door.cy);
				door.side = side(door.side);
				if(reversed && door.edge != 0)
					door.edge = 3 - door.edge;
				for(QPointF& b : door.buttons)
					b = QPointF(foreAft ? 8 * m_width - b.x() : b.x(), my ? 8 * m_depth - b.y() : b.y());
			}
	for(PlanEmitter& e : emitters)
		if(flipped(e.deck))
		{
			e.deck = newDeck(e.deck);
			e.cx = cellX(e.cx);
			e.cy = cellY(e.cy);
			e.k = side(e.k);
		}
	// Placed by hand: points in cells, and the way they face.
	auto flipYaw = [&](double yaw) {
		if(foreAft)
			yaw = 180 - yaw;
		if(my)
			yaw = -yaw;
		return fmod(yaw + 720, 360);
	};
	for(PlanStation& st : stations)
		if(flipped(st.deck))
		{
			st.deck = newDeck(st.deck);
			st.x = foreAft ? m_width - st.x : st.x;
			st.y = my ? m_depth - st.y : st.y;
			st.yaw = flipYaw(st.yaw);
		}
	for(PlanBerth& b : berths)
		if(flipped(b.deck))
		{
			b.deck = newDeck(b.deck);
			b.x = foreAft ? m_width - b.x : b.x;
			b.y = my ? m_depth - b.y : b.y;
			b.yaw = flipYaw(b.yaw);
		}
	for(PlanPad& pad : pads)
		if(flipped(pad.deck))
		{
			pad.deck = newDeck(pad.deck);
			pad.x = foreAft ? m_width - pad.x : pad.x;
			pad.y = my ? m_depth - pad.y : pad.y;
		}
	for(PlanPath& path : paths)
		if(flipped(path.deck))
		{
			path.deck = newDeck(path.deck);
			path.ax = cellX(path.ax);
			path.ay = cellY(path.ay);
			path.ak = side(path.ak);
			path.bx = cellX(path.bx);
			path.by = cellY(path.by);
			path.bk = side(path.bk);
		}
	for(PlanParking& spot : parking)
		if(flipped(spot.deck))
		{
			// Centred on the corner at the cell's low x and y.
			spot.deck = newDeck(spot.deck);
			if(foreAft)
				spot.cx = m_width - spot.cx;
			if(my)
				spot.cy = m_depth - spot.cy;
		}
	// A ladder or a flight joins a deck and the one above it: flipped with a
	// whole plan; with one deck it no longer meets the other, and goes unless
	// it happens to fit (Tidy).
	for(PlanSkylight& window : skylights)
	{
		if(deck >= 0 && window.deck != deck)
			continue;
		if(deck < 0 && upsideDown)
			window.deck = m_decks - 1 - window.deck;
		if(upsideDown)
			window.floor = !window.floor;
		window.cx = cellX(window.cx);
		window.cy = cellY(window.cy);
		if(window.k >= 0)
			window.k = side(window.k);
	}
	if(deck < 0)
	{
		for(PlanLadder& ladder : ladders)
		{
			ladder.deck = upsideDown ? m_decks - 2 - ladder.deck : ladder.deck;
			ladder.cx = cellX(ladder.cx);
			ladder.cy = cellY(ladder.cy);
		}
		if(upsideDown)
			stairs.clear();		// They would climb down; drawn again by hand.
		for(PlanStair& stair : stairs)
		{
			stair.cx = cellX(stair.cx);
			stair.cy = cellY(stair.cy);
			stair.side = side(stair.side);
		}
		if(model.IsSet())
		{
			// After an odd number of quarter turns, the plan's x is the
			// model's y (the mirrors come before the turns).
			const bool odd = model.quarterTurns % 2 == 1;
			if(foreAft)
				(odd ? model.mirrorY : model.mirrorX) = !(odd ? model.mirrorY : model.mirrorX);
			if(my)
				(odd ? model.mirrorX : model.mirrorY) = !(odd ? model.mirrorX : model.mirrorY);
			if(upsideDown)
			{
				model.mirrorZ = !model.mirrorZ;
				model.height = m_decks * deckFeet;
			}
		}
	}
	Tidy();
	return before - (ladders.size() + skylights.size() + stairs.size() + parking.size());
}

int TriPlan::Rotate(bool clockwise, int deck, bool* grew)
{
	if(grew)
		*grew = false;
	auto drawn = [this](int d, int cx, int cy) {
		for(int k = 0; k < 4; ++k)
		{
			const int i = Index(cx, cy, k);
			if(m_hull[d][i] || m_room[d][i] >= 0)
				return true;
		}
		return false;
	};
	if(deck >= 0)
	{
		if(deck >= m_decks)
			return 0;
		// One deck turns in the grid it has: about the middle, which needs
		// the width and depth both even or both odd (or the deck would end
		// up half a cell off the others), and room for it turned.
		auto fits = [&]() {
			if((m_width - m_depth) % 2 != 0)
				return false;
			for(int cy = 0; cy < m_depth; ++cy)
				for(int cx = 0; cx < m_width; ++cx)
				{
					if(!drawn(deck, cx, cy))
						continue;
					const int nx = clockwise ? cy + (m_width - m_depth) / 2 : (m_width + m_depth) / 2 - 1 - cy;
					const int ny = clockwise ? (m_depth + m_width) / 2 - 1 - cx : cx + (m_depth - m_width) / 2;
					if(nx < 0 || ny < 0 || nx >= m_width || ny >= m_depth)
						return false;
				}
			return true;
		};
		if(!fits())
		{
			int size = qMax(m_width, m_depth);
			Resize(size + (size - m_width) % 2, size + (size - m_depth) % 2);
			if(grew)
				*grew = true;
		}
	}
	const int before = ladders.size() + skylights.size() + stairs.size() + parking.size();
	const int w = m_width, h = m_depth;
	const int nw = deck >= 0 ? w : h, nh = deck >= 0 ? h : w;
	auto turned = [&](int d) { return deck < 0 || d == deck; };
	// Cells, and points in cells (corners, eighths over 8).
	auto cellX = [&](int cx, int cy) { return clockwise ? cy + (nw - h) / 2 : (nw + h) / 2 - 1 - cy; };
	auto cellY = [&](int cx, int cy) { return clockwise ? (nh + w) / 2 - 1 - cx : cx + (nh - w) / 2; };
	auto point = [&](double x, double y) {
		return clockwise ? QPointF(y + (nw - h) / 2.0, (nh + w) / 2.0 - x) : QPointF((nw + h) / 2.0 - y, x + (nh - w) / 2.0);
	};
	// Seen from above, clockwise takes +x (E) to -y (S), S to W, W to N, N to E.
	auto side = [&](int k) { return clockwise ? (k + 3) % 4 : (k + 1) % 4; };
	auto inside = [&](int cx, int cy) { return cx >= 0 && cy >= 0 && cx < nw && cy < nh; };

	QVector<QVector<quint8> > hull(m_decks), paint(m_decks);
	QVector<QVector<qint16> > room(m_decks);
	for(int d = 0; d < m_decks; ++d)
	{
		if(!turned(d))
		{
			hull[d] = m_hull[d];
			paint[d] = m_paint[d];
			room[d] = m_room[d];
			continue;
		}
		hull[d].fill(0, nw * nh * 4);
		paint[d].fill(0, nw * nh * 4);
		room[d].fill(-1, nw * nh * 4);
		for(int cy = 0; cy < h; ++cy)
			for(int cx = 0; cx < w; ++cx)
			{
				const int nx = cellX(cx, cy), ny = cellY(cx, cy);
				if(!inside(nx, ny))
					continue;
				for(int k = 0; k < 4; ++k)
				{
					const int from = Index(cx, cy, k), to = (ny * nw + nx) * 4 + side(k);
					hull[d][to] = m_hull[d][from];
					paint[d][to] = m_paint[d][from];
					room[d][to] = m_room[d][from];
				}
			}
	}
	m_width = nw;
	m_depth = nh;
	m_hull = hull;
	m_paint = paint;
	m_room = room;
	// A turn keeps a triangle's corners in the same order round it, so the
	// diagonals (a window's 'edge') stay as they are.
	for(QList<PlanDoor>* list : { &doors, &windows })
		for(PlanDoor& door : *list)
			if(turned(door.deck))
			{
				const int cx = door.cx, cy = door.cy;
				door.cx = cellX(cx, cy);
				door.cy = cellY(cx, cy);
				door.side = side(door.side);
				for(QPointF& b : door.buttons)
					b = point(b.x() / 8.0, b.y() / 8.0) * 8.0;
			}
	for(PlanEmitter& e : emitters)
		if(turned(e.deck))
		{
			const int cx = e.cx, cy = e.cy;
			e.cx = cellX(cx, cy);
			e.cy = cellY(cx, cy);
			e.k = side(e.k);
		}
	// Placed by hand: points in cells; clockwise seen from above takes a
	// heading 90 degrees round from port towards fore.
	const double turnYaw = clockwise ? -90 : 90;
	for(PlanStation& st : stations)
		if(turned(st.deck))
		{
			const QPointF q = point(st.x, st.y);
			st.x = q.x();
			st.y = q.y();
			st.yaw = fmod(st.yaw + turnYaw + 720, 360);
		}
	for(PlanBerth& b : berths)
		if(turned(b.deck))
		{
			const QPointF q = point(b.x, b.y);
			b.x = q.x();
			b.y = q.y();
			b.yaw = fmod(b.yaw + turnYaw + 720, 360);
		}
	for(PlanPad& pad : pads)
		if(turned(pad.deck))
		{
			const QPointF q = point(pad.x, pad.y);
			pad.x = q.x();
			pad.y = q.y();
		}
	for(PlanPath& path : paths)
		if(turned(path.deck))
		{
			const int ax = path.ax, ay = path.ay, bx = path.bx, by = path.by;
			path.ax = cellX(ax, ay);
			path.ay = cellY(ax, ay);
			path.ak = side(path.ak);
			path.bx = cellX(bx, by);
			path.by = cellY(bx, by);
			path.bk = side(path.bk);
		}
	for(PlanSkylight& window : skylights)
		if(turned(window.deck))
		{
			const int cx = window.cx, cy = window.cy;
			window.cx = cellX(cx, cy);
			window.cy = cellY(cx, cy);
			if(window.k >= 0)
				window.k = side(window.k);
		}
	for(PlanParking& spot : parking)
		if(turned(spot.deck))
		{
			// Centred on the corner at the cell's low x and y; its long side
			// turns from along x to across, or back.
			const QPointF q = point(spot.cx, spot.cy);
			spot.cx = qRound(q.x());
			spot.cy = qRound(q.y());
			if(!spot.spacecraft)
				spot.across = !spot.across;
		}
	// Ladders and stairs join two decks: turned with the whole plan; with one
	// deck they go unless they still fit (Tidy), as with Flip.
	if(deck < 0)
	{
		for(PlanLadder& ladder : ladders)
		{
			const int cx = ladder.cx, cy = ladder.cy;
			ladder.cx = cellX(cx, cy);
			ladder.cy = cellY(cx, cy);
		}
		for(PlanStair& stair : stairs)
		{
			const int cx = stair.cx, cy = stair.cy;
			stair.cx = cellX(cx, cy);
			stair.cy = cellY(cx, cy);
			stair.side = side(stair.side);
		}
		if(model.IsSet())
			model.quarterTurns = (model.quarterTurns + (clockwise ? 1 : 3)) % 4;
	}
	Tidy();
	return before - (ladders.size() + skylights.size() + stairs.size() + parking.size());
}

int TriPlan::RoomAtPoint(int deck, double x, double y) const
{
	TriRef t;
	if(deck < 0 || deck >= m_decks || !TriAt(deck, CellPointToWorld(x, y), t))
		return -1;
	return Room(t);
}

QList<QPair<QString, QString> > TriPlan::BuildKinds()
{
	return {
		{ "doors", "Doors" }, { "doorButtons", "Door buttons" }, { "innerWindows", "Windows between rooms" },
		{ "balconies", "Balconies" }, { "windows", "Windows" }, { "skylights", "Ceiling windows" },
		{ "liveries", "Livery cuts (liveries are plain paint without)" }, { "ladders", "Ladders" }, { "stairs", "Stairs" },
		{ "ramps", "Ramps and ladders down from hull doors" }, { "parking", "Vehicle parking and landing spots" },
		{ "contrails", "Contrail emitters" }, { "weapons", "Weapon emitters" }, { "markings", "Empire flags and names" },
		{ "stations", "Stations" }, { "berths", "Berths" }, { "pads", "Transporter pads" },
		{ "systems", "System volumes and service panels" }, { "paths", "Walk paths" } };
}

template<typename T> static void DropSkipped(QList<T>& list)
{
	for(int k = list.size() - 1; k >= 0; --k)
		if(list.at(k).skip)
			list.removeAt(k);
}

const double TriPlan::kFighterCellFeet = 2.5;
const double TriPlan::kFighterDeckFeet = 4.5;

void TriPlan::MakeFighter(bool sizes)
{
	mode = "fighter";
	templateKey = "fighter";
	ftlDrive = "none";
	windowStyle = "hull";
	autoBerths = false;		// No room for a bed in a cockpit; one can be placed by hand (Crew step).
	if(sizes)
	{
		cellFeet = kFighterCellFeet;
		deckFeet = kFighterDeckFeet;
		floorFeet = .5;
		skinFeet = .5;
		smoothing = 0;
		windowSillFeet = 1.0;
		windowTopFeet = 3.5;
	}
	bool cockpit = false;
	for(const PlanRoom& r : rooms)
		cockpit = cockpit || r.type == "Bridge";
	if(!cockpit)
	{
		PlanRoom r;
		r.type = "Bridge";
		r.label = "Cockpit";
		r.colour = QColor(90, 140, 220);
		rooms << r;
	}
}

TriPlan TriPlan::ForBuild() const
{
	TriPlan plan = *this;
	const QStringList& out = buildLeaveOut;
	if(plan.IsFighter())
		plan.ftlDrive = "none";		// The FTL drive's least volume is more than a fighter's hull.
	DropSkipped(plan.doors);
	DropSkipped(plan.windows);
	DropSkipped(plan.ladders);
	DropSkipped(plan.skylights);
	DropSkipped(plan.stairs);
	DropSkipped(plan.parking);
	DropSkipped(plan.emitters);
	DropSkipped(plan.stations);
	DropSkipped(plan.berths);
	DropSkipped(plan.pads);
	DropSkipped(plan.paths);
	for(int k = plan.doors.size() - 1; k >= 0; --k)
	{
		const QString& kind = plan.doors.at(k).kind;
		if((kind.isEmpty() && out.contains("doors")) || (kind == "window" && out.contains("innerWindows"))
				|| (kind == "balcony" && out.contains("balconies")))
			plan.doors.removeAt(k);
		else if(out.contains("doorButtons"))
			plan.doors[k].buttons.clear();
	}
	if(out.contains("windows"))
		plan.windows.clear();
	if(out.contains("skylights"))
		plan.skylights.clear();
	if(out.contains("liveries"))
		for(PlanPaint& p : plan.palette)
			p.livery = false;
	if(out.contains("ladders"))
		plan.ladders.clear();
	if(out.contains("stairs"))
		plan.stairs.clear();
	if(out.contains("ramps"))
	{
		plan.hullDoorExit = "none";
		for(PlanDoor& d : plan.doors)
			d.exit = "none";
	}
	if(out.contains("parking"))
		plan.parking.clear();
	for(int k = plan.emitters.size() - 1; k >= 0; --k)
	{
		const PlanEmitter& e = plan.emitters.at(k);
		if((e.marking && out.contains("markings")) || (!e.marking && e.weapon && out.contains("weapons"))
				|| (!e.marking && !e.weapon && out.contains("contrails")))
			plan.emitters.removeAt(k);
	}
	if(out.contains("stations"))
	{
		plan.stations.clear();
		plan.autoStations = false;
	}
	if(out.contains("berths"))
	{
		plan.berths.clear();
		plan.autoBerths = false;
	}
	if(out.contains("pads"))
	{
		plan.pads.clear();
		plan.autoPads = false;
	}
	if(out.contains("paths"))
	{
		plan.paths.clear();
		plan.autoPaths = false;
	}
	return plan;
}

int TriPlan::StairCells() const
{
	return qMax(2, int(ceil(deckFeet * 1.4 / cellFeet - 1e-6)));
}

void TriPlan::StairCell(const PlanStair& stair, int i, int& cx, int& cy) const
{
	cx = stair.cx + (stair.side == TRI_E ? i : stair.side == TRI_W ? -i : 0);
	cy = stair.cy + (stair.side == TRI_N ? i : stair.side == TRI_S ? -i : 0);
}

int TriPlan::StairAt(int deck, int cx, int cy) const
{
	for(int i = 0; i < stairs.size(); ++i)
	{
		if(stairs.at(i).deck != deck)
			continue;
		for(int j = 0; j < StairCells(); ++j)
		{
			int x, y;
			StairCell(stairs.at(i), j, x, y);
			if(x == cx && y == cy)
				return i;
		}
	}
	return -1;
}

bool TriPlan::StairFits(const PlanStair& stair, QString* why) const
{
	auto no = [why](const QString& text) {
		if(why)
			*why = text;
		return false;
	};
	const int n = StairCells();
	if(stair.deck < 0 || stair.deck + 1 >= m_decks)
		return no("Stairs go up to the deck above; there is none.");
	int x, y;
	StairCell(stair, 0, x, y);
	const int lower = CellRoom(stair.deck, x, y);
	StairCell(stair, n, x, y);
	const int upper = CellRoom(stair.deck + 1, x, y);
	if(upper < 0)
		return no(QString("The stairs climb %1 cells and come out on the next cell, on the deck above; that cell "
				"has to be a whole cell of room.").arg(n));
	for(int i = 0; i < n; ++i)
	{
		StairCell(stair, i, x, y);
		if(lower < 0 || CellRoom(stair.deck, x, y) != lower)
			return no(QString("The %1 cells the stairs climb over have to be whole cells of one room.").arg(n));
		if(CellRoom(stair.deck + 1, x, y) != upper)
			return no(QString("On the deck above, the %1 cells over the stairs have to be in the room the stairs "
					"come out in (the opening is cut through its floor).").arg(n));
		if(LadderAt(stair.deck, x, y) >= 0 || LadderAt(stair.deck + 1, x, y) >= 0)
			return no("The stairs would run into a ladder.");
		for(int j = 0; j < stairs.size(); ++j)
		{
			const PlanStair& other = stairs.at(j);
			if(other.deck == stair.deck && other.cx == stair.cx && other.cy == stair.cy && other.side == stair.side)
				continue;
			for(int d = -1; d <= 1; ++d)
				if(StairAt(stair.deck + d, x, y) == j)
					return no("The stairs would run into other stairs.");
		}
	}
	return true;
}

int TriPlan::CellRoom(int deck, int cx, int cy) const
{
	const int room = Room(TriRef(deck, cx, cy, 0));
	for(int k = 1; k < 4; ++k)
		if(Room(TriRef(deck, cx, cy, k)) != room)
			return -1;
	return room;
}

void TriPlan::Tidy()
{
	for(int d = 0; d < m_decks; ++d)
		for(int i = 0; i < m_hull[d].size(); ++i)
		{
			if(!m_hull[d][i])
				m_room[d][i] = -1;
			if(m_room[d][i] >= rooms.size())
				m_room[d][i] = -1;
			if(m_paint[d][i] >= palette.size())
				m_paint[d][i] = 0;
		}

	QList<PlanDoor> kept;
	for(const PlanDoor& door : doors)
	{
		const TriRef a(door.deck, door.cx, door.cy, door.side);
		const TriRef b = AcrossCellEdge(door.deck, door.cx, door.cy, door.side);
		if(!Valid(a) && !Valid(b))
			continue;
		if(Room(a) < 0 && Room(b) < 0)
			continue;			// A door has to lead out of a room.
		if(Room(a) == Room(b))
			continue;			// Not across a wall.
		if(!door.kind.isEmpty() && (Room(a) < 0 || Room(b) < 0))
			continue;			// A window or a balcony between two rooms only.
		kept << door;
	}
	doors = kept;

	// A window looks out of a room: a room on one side, no room on the other.
	QList<PlanDoor> keptWindows;
	for(const PlanDoor& window : windows)
	{
		const TriRef a(window.deck, window.cx, window.cy, window.side);
		const TriRef b = Across(a, window.edge);
		if((Room(a) < 0) == (Room(b) < 0))
			continue;
		if(window.edge == 0 && DoorAt(window.deck, window.cx, window.cy, window.side) >= 0)
			continue;
		keptWindows << window;
	}
	windows = keptWindows;

	QList<PlanLadder> keptLadders;
	for(const PlanLadder& ladder : ladders)
		if(CellRoom(ladder.deck, ladder.cx, ladder.cy) >= 0 && CellRoom(ladder.deck + 1, ladder.cx, ladder.cy) >= 0)
			keptLadders << ladder;
	ladders = keptLadders;

	// One at a time, so of two that cover the same triangle the first stays.
	const QList<PlanSkylight> allSkylights = skylights;
	skylights.clear();
	for(const PlanSkylight& window : allSkylights)
	{
		bool covered = false;
		for(int k = 0; k < 4 && !covered; ++k)
			covered = (window.k < 0 || k == window.k) && SkylightOn(TriRef(window.deck, window.cx, window.cy, k)) >= 0;
		if(!covered && SkylightFits(window))
			skylights << window;
	}

	// One at a time, so of two landing spots that overlap the first stays.
	const QList<PlanParking> allParking = parking;
	parking.clear();
	for(const PlanParking& spot : allParking)
		if(ParkingFits(spot))
			parking << spot;

	QList<PlanStair> keptStairs;
	for(const PlanStair& stair : stairs)
		if(StairFits(stair))
			keptStairs << stair;
	stairs = keptStairs;

	QList<PlanEmitter> keptEmitters;
	for(const PlanEmitter& e : emitters)
		if(Hull(TriRef(e.deck, e.cx, e.cy, e.k)))
			keptEmitters << e;
	emitters = keptEmitters;

	// Placed by hand: on the floor of a room, a path in one room.
	for(int k = stations.size() - 1; k >= 0; --k)
		if(RoomAtPoint(stations.at(k).deck, stations.at(k).x, stations.at(k).y) < 0)
			stations.removeAt(k);
	for(int k = berths.size() - 1; k >= 0; --k)
		if(RoomAtPoint(berths.at(k).deck, berths.at(k).x, berths.at(k).y) < 0)
			berths.removeAt(k);
	for(int k = pads.size() - 1; k >= 0; --k)
		if(RoomAtPoint(pads.at(k).deck, pads.at(k).x, pads.at(k).y) < 0)
			pads.removeAt(k);
	for(int k = paths.size() - 1; k >= 0; --k)
	{
		const PlanPath& path = paths.at(k);
		const TriRef a(path.deck, path.ax, path.ay, path.ak), b(path.deck, path.bx, path.by, path.bk);
		if(!Valid(a) || !Valid(b) || Room(a) < 0 || Room(a) != Room(b))
			paths.removeAt(k);
	}
}

bool TriPlan::Save(const QString& fileName, QString& error) const
{
	QJsonObject root;
	root["format"] = "shipgen-triplan";
	root["version"] = 1;
	root["name"] = name;
	root["template"] = templateKey;
	root["ftl"] = ftlDrive;
	if(!mode.isEmpty())
		root["mode"] = mode;
	root["leaveOut"] = QJsonArray::fromStringList(leaveOut);
	if(fireControlModule != 252)
		root["fireControlModule"] = fireControlModule;
	root["cellFeet"] = cellFeet;
	root["deckFeet"] = deckFeet;
	root["floorFeet"] = floorFeet;
	root["skinFeet"] = skinFeet;
	root["smoothing"] = smoothing;
	root["panels"] = panels;
	if(model.IsSet())
	{
		QJsonObject m;
		m["file"] = model.file;
		m["yUp"] = model.yUp;
		m["turn"] = model.turn;
		m["flip"] = model.flip;
		m["centre"] = QJsonArray({ model.centre[0], model.centre[1], model.centre[2] });
		m["scale"] = model.scale;
		m["seal"] = model.sealFeet;
		m["asHull"] = model.asHull;
		if(model.sharp)
			m["sharp"] = true;
		if(model.quarterTurns)
			m["quarterTurns"] = model.quarterTurns;
		if(model.mirrorX || model.mirrorY || model.mirrorZ)
		{
			m["mirror"] = QJsonArray({ model.mirrorX, model.mirrorY, model.mirrorZ });
			m["height"] = model.height;
		}
		root["model"] = m;
	}
	root["maxFaces"] = maxFaces;
	root["windowSill"] = windowSillFeet;
	root["windowTop"] = windowTopFeet;
	root["joinWindows"] = joinWindows;
	if(wallTex >= 0)
		root["wallTexture"] = wallTex;
	if(floorTex >= 0)
		root["floorTexture"] = floorTex;
	if(ceilingTex >= 0)
		root["ceilingTexture"] = ceilingTex;
	if(doorTex >= 0)
		root["doorTexture"] = doorTex;
	if(!flatWindows)
		root["flatWindows"] = false;
	if(!windowStyle.isEmpty())
		root["windowStyle"] = windowStyle;
	if(bridgeStyle != "hazeron")
		root["bridgeStyle"] = bridgeStyle;
	if(deskStanding)
		root["deskStanding"] = true;
	if(deskHeightFeet > 0)
		root["deskHeight"] = deskHeightFeet;
	if(hullDoorExit != "auto")
		root["hullDoorExit"] = hullDoorExit;
	if(hullStyle != "smooth")
		root["hullStyle"] = hullStyle;
	root["slopedDecks"] = slopedDecks;
	root["width"] = m_width;
	root["depth"] = m_depth;
	root["decks"] = m_decks;

	QJsonArray roomArray;
	for(const PlanRoom& r : rooms)
	{
		QJsonObject o;
		o["type"] = r.type;
		o["label"] = r.label;
		o["colour"] = ColourName(r.colour);
		if(r.wallTex >= 0)
			o["wallTexture"] = r.wallTex;
		if(r.floorTex >= 0)
			o["floorTexture"] = r.floorTex;
		if(r.ceilingTex >= 0)
			o["ceilingTexture"] = r.ceilingTex;
		if(r.carFeet > 0)
			o["carHeight"] = r.carFeet;
		if((r.access & 15) != 15)
		{
			QJsonArray who;
			const char* names[4] = { "officers", "crew", "passengers", "troops" };
			for(int i = 0; i < 4; ++i)
				if(r.access & (1 << i))
					who.append(QString(names[i]));
			o["access"] = who;
		}
		roomArray.append(o);
	}
	root["rooms"] = roomArray;

	QJsonArray doorArray;
	for(const PlanDoor& d : doors)
	{
		QJsonObject o;
		o["deck"] = d.deck;
		o["x"] = d.cx;
		o["y"] = d.cy;
		o["side"] = d.side;
		if(d.row)
			o["row"] = d.row;
		if(d.tex >= 0)
			o["texture"] = d.tex;
		if(!d.exit.isEmpty())
			o["exit"] = d.exit;
		if(d.rampRun > 0)
			o["rampRun"] = d.rampRun;
		if(!d.kind.isEmpty())
			o["kind"] = d.kind;
		if(!d.buttons.isEmpty())
		{
			QJsonArray buttons;
			// Two decimals of an eighth are under a hundredth of a foot.
			for(const QPointF& b : d.buttons)
				buttons.append(QJsonArray{ std::round(b.x() * 100) / 100, std::round(b.y() * 100) / 100 });
			o["buttons"] = buttons;
		}
		if(d.skip)
			o["skip"] = true;
		doorArray.append(o);
	}
	root["doors"] = doorArray;

	QJsonArray windowArray;
	for(const PlanDoor& w : windows)
	{
		QJsonObject o;
		o["deck"] = w.deck;
		o["x"] = w.cx;
		o["y"] = w.cy;
		o["side"] = w.side;
		if(w.edge)
			o["edge"] = w.edge;
		if(w.row)
			o["row"] = w.row;
		if(!w.style.isEmpty())
			o["style"] = w.style;
		if(w.skip)
			o["skip"] = true;
		windowArray.append(o);
	}
	root["windows"] = windowArray;

	QJsonArray ladderArray;
	for(const PlanLadder& l : ladders)
	{
		QJsonObject o;
		o["deck"] = l.deck;
		o["x"] = l.cx;
		o["y"] = l.cy;
		if(l.skip)
			o["skip"] = true;
		ladderArray.append(o);
	}
	root["ladders"] = ladderArray;

	QJsonArray skylightArray;
	for(const PlanSkylight& window : skylights)
	{
		QJsonObject o;
		o["deck"] = window.deck;
		o["x"] = window.cx;
		o["y"] = window.cy;
		if(window.k >= 0)
			o["k"] = window.k;
		if(window.floor)
			o["floor"] = true;
		if(!window.shape.isEmpty())
			o["shape"] = window.shape;
		if(window.skip)
			o["skip"] = true;
		skylightArray.append(o);
	}
	if(!skylightArray.isEmpty())
		root["skylights"] = skylightArray;

	QJsonArray parkingArray;
	for(const PlanParking& p : parking)
	{
		QJsonObject o;
		o["deck"] = p.deck;
		o["x"] = p.cx;
		o["y"] = p.cy;
		o["kind"] = p.spacecraft ? "spacecraft" : p.space ? "space" : "ground";
		o["across"] = p.across;
		if(p.skip)
			o["skip"] = true;
		parkingArray.append(o);
	}
	root["parking"] = parkingArray;

	QJsonArray stairArray;
	for(const PlanStair& st : stairs)
	{
		QJsonObject o;
		o["deck"] = st.deck;
		o["x"] = st.cx;
		o["y"] = st.cy;
		o["side"] = st.side;
		if(st.skip)
			o["skip"] = true;
		stairArray.append(o);
	}
	root["stairs"] = stairArray;

	QJsonArray emitterArray;
	for(const PlanEmitter& e : emitters)
	{
		QJsonObject o;
		o["deck"] = e.deck;
		o["x"] = e.cx;
		o["y"] = e.cy;
		o["side"] = e.k;
		o["kind"] = e.marking == 1 ? "flag" : e.marking == 2 ? "name" : e.weapon ? "weapon" : "contrail";
		o["size"] = e.sizeFeet;
		if(e.weapon && e.module > 0)
			o["module"] = e.module;
		if(e.skip)
			o["skip"] = true;
		emitterArray.append(o);
	}
	if(!emitterArray.isEmpty())
		root["emitters"] = emitterArray;

	// Placed by hand on the Crew page.
	auto xy = [](QJsonObject& o, double x, double y) {
		o["x"] = qRound(x * 1000) / 1000.0;
		o["y"] = qRound(y * 1000) / 1000.0;
	};
	QJsonArray stationArray;
	for(const PlanStation& st : stations)
	{
		QJsonObject o;
		o["deck"] = st.deck;
		xy(o, st.x, st.y);
		o["yaw"] = st.yaw;
		o["type"] = st.type;
		if(!st.seat.isEmpty())
			o["seat"] = st.seat;
		if(st.module > 0)
			o["module"] = st.module;
		if(st.skip)
			o["skip"] = true;
		stationArray.append(o);
	}
	if(!stationArray.isEmpty())
		root["stations"] = stationArray;
	QJsonArray berthArray;
	for(const PlanBerth& b : berths)
	{
		QJsonObject o;
		o["deck"] = b.deck;
		xy(o, b.x, b.y);
		o["yaw"] = b.yaw;
		o["kind"] = b.kind;
		if(b.skip)
			o["skip"] = true;
		berthArray.append(o);
	}
	if(!berthArray.isEmpty())
		root["berths"] = berthArray;
	QJsonArray padArray;
	for(const PlanPad& pad : pads)
	{
		QJsonObject o;
		o["deck"] = pad.deck;
		xy(o, pad.x, pad.y);
		if(pad.skip)
			o["skip"] = true;
		padArray.append(o);
	}
	if(!padArray.isEmpty())
		root["pads"] = padArray;
	QJsonArray pathArray;
	for(const PlanPath& path : paths)
	{
		QJsonObject o;
		o["deck"] = path.deck;
		o["from"] = QJsonArray{ path.ax, path.ay, path.ak };
		o["to"] = QJsonArray{ path.bx, path.by, path.bk };
		if(path.skip)
			o["skip"] = true;
		pathArray.append(o);
	}
	if(!pathArray.isEmpty())
		root["paths"] = pathArray;
	if(!autoStations)
		root["autoStations"] = false;
	if(!autoBerths)
		root["autoBerths"] = false;
	if(!autoPads)
		root["autoPads"] = false;
	if(!autoPaths)
		root["autoPaths"] = false;
	if(!buildLeaveOut.isEmpty())
		root["buildLeaveOut"] = QJsonArray::fromStringList(buildLeaveOut);

	// Three decimals: floats would otherwise be written as 0.6000000238.
	auto Rounded = [](float v) { return std::round(double(v) * 1000.0) / 1000.0; };
	QJsonArray paintArray;
	for(const PlanPaint& p : palette)
	{
		QJsonObject o;
		o["colour"] = ColourName(p.colour);
		o["theme"] = p.theme;
		o["name"] = p.name;
		if(p.livery)
			o["livery"] = true;
		// The finish is written only where it is not the plain default, so
		// older plans and plain colours stay as they were.
		if(p.opacity < 1.f)
			o["opacity"] = Rounded(p.opacity);
		if(p.selfLit > 0.f)
			o["selfLit"] = Rounded(p.selfLit);
		if(p.shine > 0.f)
			o["shine"] = Rounded(p.shine);
		if(p.reflect > 0.f)
			o["reflect"] = Rounded(p.reflect);
		if(p.HasGlow())
			o["glow"] = ColourName(p.glow);
		if(p.effect != 0)
			o["effect"] = p.effect;
		paintArray.append(o);
	}
	root["palette"] = paintArray;

	QJsonArray layers;
	for(int d = 0; d < m_decks; ++d)
	{
		QJsonObject o;
		o["hull"] = Encode(m_hull[d]);
		o["room"] = Encode(m_room[d]);
		o["paint"] = Encode(m_paint[d]);
		layers.append(o);
	}
	root["layers"] = layers;

	QFile file(fileName);
	if(!file.open(QFile::WriteOnly | QFile::Truncate))
	{
		error = QString("could not write %1").arg(fileName);
		return false;
	}
	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	return true;
}

bool TriPlan::Load(const QString& fileName, QString& error)
{
	QFile file(fileName);
	if(!file.open(QFile::ReadOnly))
	{
		error = QString("could not read %1").arg(fileName);
		return false;
	}
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
	if(doc.isNull())
	{
		error = QString("%1 is not JSON: %2").arg(fileName, parseError.errorString());
		return false;
	}
	const QJsonObject root = doc.object();
	if(root["format"].toString() != "shipgen-triplan")
	{
		error = QString("%1 is not a triangle plan").arg(fileName);
		return false;
	}

	TriPlan plan;
	plan.name = root["name"].toString(plan.name);
	plan.templateKey = root["template"].toString(plan.templateKey);
	plan.ftlDrive = root["ftl"].toString(plan.ftlDrive);
	plan.mode = root["mode"].toString() == "fighter" ? QString("fighter") : QString();
	plan.fireControlModule = root["fireControlModule"].toInt(252);
	plan.leaveOut.clear();
	for(const QJsonValue& v : root["leaveOut"].toArray())
		plan.leaveOut << v.toString();
	plan.cellFeet = root["cellFeet"].toDouble(plan.cellFeet);
	plan.deckFeet = root["deckFeet"].toDouble(plan.deckFeet);
	plan.floorFeet = root["floorFeet"].toDouble(plan.floorFeet);
	plan.skinFeet = root["skinFeet"].toDouble(plan.skinFeet);
	plan.smoothing = root["smoothing"].toInt(plan.smoothing);
	plan.panels = root["panels"].toInt(plan.panels);
	if(root["model"].isObject())
	{
		const QJsonObject m = root["model"].toObject();
		plan.model.file = m["file"].toString();
		plan.model.yUp = m["yUp"].toBool();
		plan.model.turn = m["turn"].toBool();
		plan.model.flip = m["flip"].toBool();
		const QJsonArray c = m["centre"].toArray();
		for(int i = 0; i < 3 && i < c.size(); ++i)
			plan.model.centre[i] = c.at(i).toDouble();
		plan.model.scale = m["scale"].toDouble(1);
		plan.model.sealFeet = m["seal"].toDouble(1);
		plan.model.asHull = m["asHull"].toBool();
		plan.model.sharp = m["sharp"].toBool();
		const QJsonArray mirror = m["mirror"].toArray();
		plan.model.mirrorX = mirror.size() > 0 && mirror.at(0).toBool();
		plan.model.mirrorY = mirror.size() > 1 && mirror.at(1).toBool();
		plan.model.mirrorZ = mirror.size() > 2 && mirror.at(2).toBool();
		plan.model.height = m["height"].toDouble(0);
		plan.model.quarterTurns = ((m["quarterTurns"].toInt(0) % 4) + 4) % 4;
	}
	plan.maxFaces = root["maxFaces"].toInt(plan.maxFaces);
	plan.windowSillFeet = root["windowSill"].toDouble(plan.windowSillFeet);
	plan.windowTopFeet = root["windowTop"].toDouble(plan.windowTopFeet);
	plan.joinWindows = root["joinWindows"].toBool(plan.joinWindows);
	plan.wallTex = root["wallTexture"].toInt(-1);
	plan.floorTex = root["floorTexture"].toInt(-1);
	plan.ceilingTex = root["ceilingTexture"].toInt(-1);
	plan.doorTex = root["doorTexture"].toInt(-1);
	plan.flatWindows = root["flatWindows"].toBool(true);
	plan.windowStyle = root["windowStyle"].toString() == "hull" ? QString("hull") : QString();
	plan.bridgeStyle = root["bridgeStyle"].toString("hazeron");
	plan.deskStanding = root["deskStanding"].toBool(false);
	plan.deskHeightFeet = root["deskHeight"].toDouble(0);
	plan.hullDoorExit = root["hullDoorExit"].toString("auto");
	if(plan.hullDoorExit != "ladder" && plan.hullDoorExit != "none")
		plan.hullDoorExit = "auto";
	plan.hullStyle = root["hullStyle"].toString("smooth");
	if(plan.hullStyle != "angular" && plan.hullStyle != "classic")
		plan.hullStyle = "smooth";
	plan.slopedDecks = root["slopedDecks"].toBool(plan.slopedDecks);
	plan.m_width = qBound(2, root["width"].toInt(), 400);
	plan.m_depth = qBound(2, root["depth"].toInt(), 400);
	plan.m_decks = qBound(1, root["decks"].toInt(), 64);

	plan.rooms.clear();
	for(const QJsonValue& v : root["rooms"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanRoom r;
		r.type = o["type"].toString();
		if(IsZeroGRoomType(r.type))
			r.type = ZeroGRoomType();		// Plans from before 0.3.5-alpha say "Gravity Well".
		r.label = o["label"].toString();
		r.colour = QColor(o["colour"].toString("#808080"));
		r.wallTex = o["wallTexture"].toInt(-1);
		r.floorTex = o["floorTexture"].toInt(-1);
		r.ceilingTex = o["ceilingTexture"].toInt(-1);
		r.carFeet = qMax(0.0, o["carHeight"].toDouble(0));
		if(o.contains("access"))
		{
			r.access = 0;
			const char* names[4] = { "officers", "crew", "passengers", "troops" };
			for(const QJsonValue& who : o["access"].toArray())
				for(int i = 0; i < 4; ++i)
					if(who.toString().compare(names[i], Qt::CaseInsensitive) == 0)
						r.access |= 1 << i;
			if(!r.access)
				r.access = 15;		// Nobody at all is not a choice the game has.
		}
		plan.rooms << r;
	}
	plan.doors.clear();
	for(const QJsonValue& v : root["doors"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanDoor d;
		d.deck = o["deck"].toInt();
		d.cx = o["x"].toInt();
		d.cy = o["y"].toInt();
		d.side = o["side"].toInt();
		d.row = o["row"].toInt();
		d.tex = o["texture"].toInt(-1);
		d.exit = o["exit"].toString();
		if(d.exit != "auto" && d.exit != "ladder" && d.exit != "ramp" && d.exit != "none")
			d.exit.clear();
		d.rampRun = o["rampRun"].toDouble();
		d.rampRun = d.rampRun > 0 ? qBound(1.0, d.rampRun, 8.0) : 0.0;
		d.kind = o["kind"].toString();
		if(d.kind != "window" && d.kind != "balcony")
			d.kind.clear();
		for(const QJsonValue& b : o["buttons"].toArray())
			if(b.toArray().size() == 2 && d.buttons.size() < 2)
				d.buttons << QPointF(b.toArray().at(0).toDouble(), b.toArray().at(1).toDouble());
		d.skip = o["skip"].toBool();
		plan.doors << d;
	}
	plan.windows.clear();
	for(const QJsonValue& v : root["windows"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanDoor w;
		w.deck = o["deck"].toInt();
		w.cx = o["x"].toInt();
		w.cy = o["y"].toInt();
		w.side = o["side"].toInt();
		w.edge = qBound(0, o["edge"].toInt(), 2);
		w.row = qMax(0, o["row"].toInt());
		w.style = o["style"].toString();
		if(w.style != "hull" && w.style != "frame")
			w.style.clear();
		w.skip = o["skip"].toBool();
		plan.windows << w;
	}
	plan.ladders.clear();
	for(const QJsonValue& v : root["ladders"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanLadder l;
		l.deck = o["deck"].toInt();
		l.cx = o["x"].toInt();
		l.cy = o["y"].toInt();
		l.skip = o["skip"].toBool();
		plan.ladders << l;
	}
	plan.skylights.clear();
	for(const QJsonValue& v : root["skylights"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanSkylight window;
		window.deck = o["deck"].toInt();
		window.cx = o["x"].toInt();
		window.cy = o["y"].toInt();
		window.floor = o["floor"].toBool();
		window.k = o["k"].toInt(-1);
		if(window.k < -1 || window.k > 3)
			window.k = -1;
		window.shape = o["shape"].toString();
		if(window.shape != "hull" && window.shape != "flat" && window.shape != "canopy")
			window.shape.clear();
		window.skip = o["skip"].toBool();
		plan.skylights << window;
	}
	plan.parking.clear();
	for(const QJsonValue& v : root["parking"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanParking p;
		p.deck = o["deck"].toInt();
		p.cx = o["x"].toInt();
		p.cy = o["y"].toInt();
		p.space = o["kind"].toString() == "space";
		p.spacecraft = o["kind"].toString() == "spacecraft";
		p.across = o["across"].toBool();
		p.skip = o["skip"].toBool();
		plan.parking << p;
	}
	plan.stairs.clear();
	for(const QJsonValue& v : root["stairs"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanStair st;
		st.deck = o["deck"].toInt();
		st.cx = o["x"].toInt();
		st.cy = o["y"].toInt();
		st.side = qBound(0, o["side"].toInt(), 3);
		st.skip = o["skip"].toBool();
		plan.stairs << st;
	}
	plan.emitters.clear();
	for(const QJsonValue& v : root["emitters"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanEmitter e;
		e.deck = o["deck"].toInt();
		e.cx = o["x"].toInt();
		e.cy = o["y"].toInt();
		e.k = qBound(0, o["side"].toInt(), 3);
		e.weapon = o["kind"].toString() == "weapon";
		e.marking = o["kind"].toString() == "flag" ? 1 : o["kind"].toString() == "name" ? 2 : 0;
		e.sizeFeet = o["size"].toDouble(e.sizeFeet);
		e.module = e.weapon ? o["module"].toInt(0) : 0;
		e.skip = o["skip"].toBool();
		plan.emitters << e;
	}
	for(const QJsonValue& v : root["stations"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanStation st;
		st.deck = o["deck"].toInt();
		st.x = o["x"].toDouble();
		st.y = o["y"].toDouble();
		st.yaw = o["yaw"].toDouble();
		st.type = o["type"].toString();
		st.seat = o["seat"].toString();
		st.module = o["module"].toInt();
		st.skip = o["skip"].toBool();
		plan.stations << st;
	}
	for(const QJsonValue& v : root["berths"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanBerth b;
		b.deck = o["deck"].toInt();
		b.x = o["x"].toDouble();
		b.y = o["y"].toDouble();
		b.yaw = o["yaw"].toDouble();
		b.kind = o["kind"].toString("crew");
		b.skip = o["skip"].toBool();
		plan.berths << b;
	}
	for(const QJsonValue& v : root["pads"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanPad pad;
		pad.deck = o["deck"].toInt();
		pad.x = o["x"].toDouble();
		pad.y = o["y"].toDouble();
		pad.skip = o["skip"].toBool();
		plan.pads << pad;
	}
	for(const QJsonValue& v : root["paths"].toArray())
	{
		const QJsonObject o = v.toObject();
		const QJsonArray a = o["from"].toArray(), b = o["to"].toArray();
		if(a.size() < 3 || b.size() < 3)
			continue;
		PlanPath path;
		path.deck = o["deck"].toInt();
		path.ax = a.at(0).toInt();
		path.ay = a.at(1).toInt();
		path.ak = a.at(2).toInt() & 3;
		path.bx = b.at(0).toInt();
		path.by = b.at(1).toInt();
		path.bk = b.at(2).toInt() & 3;
		path.skip = o["skip"].toBool();
		plan.paths << path;
	}
	plan.autoStations = root["autoStations"].toBool(true);
	plan.autoBerths = root["autoBerths"].toBool(true);
	plan.autoPads = root["autoPads"].toBool(true);
	plan.autoPaths = root["autoPaths"].toBool(true);
	for(const QJsonValue& v : root["buildLeaveOut"].toArray())
		plan.buildLeaveOut << v.toString();

	const QJsonArray paints = root["palette"].toArray();
	if(!paints.isEmpty())
	{
		plan.palette.clear();
		for(const QJsonValue& v : paints)
		{
			const QJsonObject o = v.toObject();
			PlanPaint p;
			p.colour = QColor(o["colour"].toString("#9aa0a8"));
			p.theme = o["theme"].toInt(-1);
			p.name = o["name"].toString();
			p.livery = o["livery"].toBool();
			p.opacity = float(qBound(0.0, o["opacity"].toDouble(1.0), 1.0));
			p.selfLit = float(qBound(0.0, o["selfLit"].toDouble(0.0), 1.0));
			p.shine = float(qMax(0.0, o["shine"].toDouble(0.0)));
			p.reflect = float(qBound(0.0, o["reflect"].toDouble(0.0), 1.0));
			if(o.contains("glow"))
				p.glow = QColor(o["glow"].toString());
			p.effect = qMax(0, o["effect"].toInt(0));		// Checked against the engine when built.
			plan.palette << p;
		}
	}

	const QJsonArray layers = root["layers"].toArray();
	if(layers.size() != plan.m_decks)
	{
		error = QString("%1 has %2 decks but %3 layers").arg(fileName).arg(plan.m_decks).arg(layers.size());
		return false;
	}
	const int n = plan.m_width * plan.m_depth * 4;
	plan.m_hull.resize(plan.m_decks);
	plan.m_room.resize(plan.m_decks);
	plan.m_paint.resize(plan.m_decks);
	for(int d = 0; d < plan.m_decks; ++d)
	{
		const QJsonObject o = layers.at(d).toObject();
		if(!Decode(o["hull"].toString(), n, plan.m_hull[d]) || !Decode(o["room"].toString(), n, plan.m_room[d]) ||
				!Decode(o["paint"].toString(), n, plan.m_paint[d]))
		{
			error = QString("deck %1 of %2 is damaged").arg(d + 1).arg(fileName);
			return false;
		}
	}
	plan.Tidy();
	*this = plan;
	return true;
}

TriPlan TriPlan::Example()
{
	TriPlan plan;
	plan.name = "Wedge";
	// The user tests the Wedge without these (2026-10-31).
	plan.leaveOut << "shield" << "firecontrol" << "transporter";
	plan.Resize(36, 16);

	// A wedge: full width aft, with the stern corners cut off, narrowing to
	// a point at the bow. The slopes are 45 degrees, which the diagonals of
	// the cells draw exactly: a triangle is hull when all its corners are
	// inside the outline.
	const int W = plan.Width(), D = plan.Depth();
	const double s = plan.cellFeet;
	const double xMin = plan.Corner(0, 0).x();
	for(int deck = 0; deck < 2; ++deck)
	{
		const double inset = deck == 1 ? 2 * s : 0.0;	// The upper deck is narrower all round.
		const double full = D / 2.0 * s - inset;
		const double bowTip = plan.Corner(W, 0).x() - inset;
		const double sternStart = xMin + inset;
		auto halfWidth = [&](double x) {
			if(x < sternStart - 1e-6)
				return -1.0;
			return qMin(full, qMin(bowTip - x, full - 2 * s + (x - sternStart)));
		};
		for(int cy = 0; cy < D; ++cy)
			for(int cx = 0; cx < W; ++cx)
				for(int k = 0; k < 4; ++k)
				{
					const TriRef t(deck, cx, cy, k);
					QPointF c[3];
					plan.TriCorners(t, c);
					bool inside = true;
					for(const QPointF& p : c)
						if(fabs(p.y()) > halfWidth(p.x()) + 1e-6)
							inside = false;
					if(inside)
						plan.SetHull(t, true);
				}
	}

	PlanRoom bridge;
	bridge.type = "Bridge";
	bridge.label = "Bridge 1";
	bridge.colour = QColor(90, 140, 220);
	PlanRoom engine;
	engine.type = "Engine Room";
	engine.label = "Engine Room 1";
	engine.colour = QColor(220, 130, 70);
	PlanRoom hall;
	hall.type = "Hall";
	hall.label = "Hall 1";
	hall.colour = QColor(160, 160, 160);
	PlanRoom quarters;
	quarters.type = "Crew Quarters";
	quarters.label = "Crew Quarters 1";
	quarters.colour = QColor(120, 190, 110);
	plan.rooms << bridge << engine << hall << quarters;

	auto fill = [&plan](int deck, int x0, int x1, int y0, int y1, int room) {
		for(int cx = x0; cx <= x1; ++cx)
			for(int cy = y0; cy <= y1; ++cy)
				for(int k = 0; k < 4; ++k)
					plan.SetRoom(TriRef(deck, cx, cy, k), room);
	};
	// Deck 1: the engine room aft, a hall down the middle, quarters forward.
	fill(0, 1, 7, 2, 13, 1);
	fill(0, 8, 21, 6, 9, 2);
	fill(0, 10, 11, 10, D - 1, 2);		// A side passage out to the port hull.
	fill(0, 12, 19, 2, 5, 3);
	// Deck 2: the bridge, the full width of the deck, reaching forward into
	// the taper of the bow so that it can look ahead (the user, 2026-10-31:
	// the bridge had no front windows).
	// In the taper the bridge keeps a triangle of hull all round it: the
	// rounded bow cuts into the outermost triangles.
	for(int cx = 18; cx < W; ++cx)
		for(int cy = 2; cy <= D - 3; ++cy)
			for(int k = 0; k < 4; ++k)
			{
				const TriRef t(1, cx, cy, k);
				bool ok = plan.Hull(t);
				if(ok && cx >= 28)
					for(int e = 0; e < 3; ++e)
					{
						const TriRef b = plan.Across(t, e);
						if(!plan.Valid(b) || !plan.Hull(b))
							ok = false;
					}
				if(ok)
					plan.SetRoom(t, 0);
			}

	auto door = [&plan](int deck, int cx, int cy, int side) {
		PlanDoor d;
		d.deck = deck;
		d.cx = cx;
		d.cy = cy;
		d.side = side;
		plan.doors << d;
	};
	door(0, 7, 7, TRI_E);		// Engine room to the hall.
	door(0, 15, 5, TRI_N);		// Quarters to the hall.
	door(0, 10, D - 1, TRI_N);	// From the side passage out through the port hull: a hull door.

	auto window = [&plan](int deck, int cx, int cy, int side) {
		PlanDoor w;
		w.deck = deck;
		w.cx = cx;
		w.cy = cy;
		w.side = side;
		plan.windows << w;
	};
	for(int cx = 20; cx <= 23; ++cx)
	{
		window(1, cx, D - 3, TRI_N);	// Port windows of the bridge.
		window(1, cx, 2, TRI_S);		// Starboard windows.
	}
	// The front windows: every edge of the bridge in the taper of the bow
	// that has no room beyond it, the slanted walls and the tip.
	for(int cx = 28; cx < W; ++cx)
		for(int cy = 0; cy < D; ++cy)
			for(int k = 0; k < 4; ++k)
			{
				const TriRef t(1, cx, cy, k);
				if(plan.Room(t) != 0)
					continue;
				for(int e = 0; e < 3; ++e)
				{
					const TriRef b = plan.Across(t, e);
					if(plan.Valid(b) && plan.Room(b) >= 0)
						continue;
					PlanDoor w;
					w.deck = 1;
					w.cx = cx;
					w.cy = cy;
					w.side = k;
					w.edge = e;
					plan.windows << w;
				}
			}
	window(0, 11, D - 1, TRI_N);		// Beside the hull door.

	PlanLadder ladder;			// Up from the hall into the bridge.
	ladder.deck = 0;
	ladder.cx = 21;
	ladder.cy = 7;
	plan.ladders << ladder;

	PlanPaint stripe;
	stripe.colour = QColor(170, 40, 40);
	stripe.name = "Stripe";
	PlanPaint empire;
	empire.theme = 0;
	empire.colour = QColor(60, 90, 160);
	empire.name = "Empire colour";
	plan.palette << stripe << empire;
	for(int cx = 22; cx < 26; ++cx)
		for(int cy = 0; cy < D; ++cy)
			for(int k = 0; k < 4; ++k)
				plan.SetPaint(TriRef(0, cx, cy, k), 1);
	return plan;
}
