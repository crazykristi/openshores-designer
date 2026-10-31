//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - a ship drawn with triangles.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "triplan.h"

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
		for(PlanParking& spot : parking)
			++spot.deck;
		for(PlanStair& stair : stairs)
			++stair.deck;
	}
	else
	{
		m_hull.append(hull);
		m_paint.append(paint);
		m_room.append(room);
	}
	++m_decks;
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

QPointF TriPlan::ParkingSize(const PlanParking& p) const
{
	// AuScGroundVehicleParkingSpot / AuScSpaceVehicleParkingSpot::Icon*.
	const double feet = 3.2808399;
	const QPointF size = p.space ? QPointF(17 * feet, 12 * feet) : QPointF(12 * feet, 6 * feet);
	return p.across ? QPointF(size.y(), size.x()) : size;
}

bool TriPlan::ParkingFits(const PlanParking& p) const
{
	const QPointF centre = Corner(p.cx, p.cy), half = ParkingSize(p) / 2.0;
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

	QList<PlanParking> keptParking;
	for(const PlanParking& spot : parking)
		if(ParkingFits(spot))
			keptParking << spot;
	parking = keptParking;

	QList<PlanStair> keptStairs;
	for(const PlanStair& stair : stairs)
		if(StairFits(stair))
			keptStairs << stair;
	stairs = keptStairs;
}

bool TriPlan::Save(const QString& fileName, QString& error) const
{
	QJsonObject root;
	root["format"] = "shipgen-triplan";
	root["version"] = 1;
	root["name"] = name;
	root["template"] = templateKey;
	root["ftl"] = ftlDrive;
	root["leaveOut"] = QJsonArray::fromStringList(leaveOut);
	root["cellFeet"] = cellFeet;
	root["deckFeet"] = deckFeet;
	root["floorFeet"] = floorFeet;
	root["skinFeet"] = skinFeet;
	root["smoothing"] = smoothing;
	root["maxFaces"] = maxFaces;
	root["windowSill"] = windowSillFeet;
	root["windowTop"] = windowTopFeet;
	root["joinWindows"] = joinWindows;
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
		ladderArray.append(o);
	}
	root["ladders"] = ladderArray;

	QJsonArray parkingArray;
	for(const PlanParking& p : parking)
	{
		QJsonObject o;
		o["deck"] = p.deck;
		o["x"] = p.cx;
		o["y"] = p.cy;
		o["kind"] = p.space ? "space" : "ground";
		o["across"] = p.across;
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
		stairArray.append(o);
	}
	root["stairs"] = stairArray;

	QJsonArray paintArray;
	for(const PlanPaint& p : palette)
	{
		QJsonObject o;
		o["colour"] = ColourName(p.colour);
		o["theme"] = p.theme;
		o["name"] = p.name;
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
	plan.leaveOut.clear();
	for(const QJsonValue& v : root["leaveOut"].toArray())
		plan.leaveOut << v.toString();
	plan.cellFeet = root["cellFeet"].toDouble(plan.cellFeet);
	plan.deckFeet = root["deckFeet"].toDouble(plan.deckFeet);
	plan.floorFeet = root["floorFeet"].toDouble(plan.floorFeet);
	plan.skinFeet = root["skinFeet"].toDouble(plan.skinFeet);
	plan.smoothing = root["smoothing"].toInt(plan.smoothing);
	plan.maxFaces = root["maxFaces"].toInt(plan.maxFaces);
	plan.windowSillFeet = root["windowSill"].toDouble(plan.windowSillFeet);
	plan.windowTopFeet = root["windowTop"].toDouble(plan.windowTopFeet);
	plan.joinWindows = root["joinWindows"].toBool(plan.joinWindows);
	plan.m_width = qBound(2, root["width"].toInt(), 400);
	plan.m_depth = qBound(2, root["depth"].toInt(), 400);
	plan.m_decks = qBound(1, root["decks"].toInt(), 64);

	plan.rooms.clear();
	for(const QJsonValue& v : root["rooms"].toArray())
	{
		const QJsonObject o = v.toObject();
		PlanRoom r;
		r.type = o["type"].toString();
		r.label = o["label"].toString();
		r.colour = QColor(o["colour"].toString("#808080"));
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
		plan.ladders << l;
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
		p.across = o["across"].toBool();
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
		plan.stairs << st;
	}
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
