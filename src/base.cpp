//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - a primitive base shape.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "base.h"

#include <QObject>
#include <QPointF>
#include <QVector>
#include <cmath>

namespace {

const double kPi = 3.14159265358979323846;

// A corner of the base, with a simple texture coordinate worked out from
// where it is.
AuScPoint Corner(const AuPoint& p, double width, double depth, double height)
{
	const double w = width > 0 ? width : 1.;
	const double d = depth > 0 ? depth : 1.;
	const double h = height > 0 ? height : 1.;
	const float tx = float((p.x + w / 2) / w);
	const float ty = float((p.y + d / 2) / d + p.z / h * 0.25);
	return AuScPoint(p, tx, ty, AuPoint(0, 0, 1));
}

void AddQuad(AuScTriList& out, const AuScPoint& a, const AuScPoint& b,
		const AuScPoint& c, const AuScPoint& d)
{
	out.append(AuScTri(a, b, c));
	out.append(AuScTri(a, c, d));
}

// A closed prism: a flat bottom, a flat top and a wall between them.
AuScTriList MakePrism(const QVector<QPointF>& outline, double height, double width, double depth)
{
	AuScTriList out;
	if(outline.size() < 3 || height <= 0)
		return out;

	const AuPoint bottomCentre(0, 0, 0);
	const AuPoint topCentre(0, 0, height);

	// The walls.
	for(int i = 0; i < outline.size(); ++i)
	{
		const QPointF& a = outline.at(i);
		const QPointF& b = outline.at((i + 1) % outline.size());
		const AuPoint b0(a.x(), a.y(), 0);
		const AuPoint b1(b.x(), b.y(), 0);
		const AuPoint t0(a.x(), a.y(), height);
		const AuPoint t1(b.x(), b.y(), height);
		AddQuad(out, Corner(b0, width, depth, height), Corner(b1, width, depth, height),
				Corner(t1, width, depth, height), Corner(t0, width, depth, height));
	}

	// The bottom. It winds the other way round, because it faces downwards.
	const AuScPoint bottom = Corner(bottomCentre, width, depth, height);
	for(int i = 0; i < outline.size(); ++i)
	{
		const QPointF& a = outline.at(i);
		const QPointF& b = outline.at((i + 1) % outline.size());
		out.append(AuScTri(bottom, Corner(AuPoint(b.x(), b.y(), 0), width, depth, height),
				Corner(AuPoint(a.x(), a.y(), 0), width, depth, height)));
	}

	// The top.
	const AuScPoint top = Corner(topCentre, width, depth, height);
	for(int i = 0; i < outline.size(); ++i)
	{
		const QPointF& a = outline.at(i);
		const QPointF& b = outline.at((i + 1) % outline.size());
		out.append(AuScTri(top, Corner(AuPoint(a.x(), a.y(), height), width, depth, height),
				Corner(AuPoint(b.x(), b.y(), height), width, depth, height)));
	}
	return out;
}

} // namespace

QString BaseShape::Key() const
{
	switch(kind)
	{
	case BaseShapeKind::Rectangle:	return "rectangle";
	case BaseShapeKind::Circle:		return "circle";
	case BaseShapeKind::Hexagon:	return "hexagon";
	case BaseShapeKind::None:
	default:						return "none";
	}
}

QString BaseShape::Label() const
{
	switch(kind)
	{
	case BaseShapeKind::Rectangle:	return QObject::tr("Rectangle");
	case BaseShapeKind::Circle:		return QObject::tr("Circle");
	case BaseShapeKind::Hexagon:	return QObject::tr("Hexagon");
	case BaseShapeKind::None:
	default:						return QObject::tr("None");
	}
}

QStringList BaseShapeKeys()
{
	return QStringList() << "none" << "rectangle" << "circle" << "hexagon";
}

QStringList BaseShapeLabels()
{
	return QStringList() << QObject::tr("None") << QObject::tr("Rectangle")
			<< QObject::tr("Circle") << QObject::tr("Hexagon");
}

bool FindBaseShape(const QString& key, BaseShapeKind& out)
{
	const QString k = key.trimmed().toLower();
	if(k.isEmpty() || k == "none" || k == "no" || k == "off")
	{
		out = BaseShapeKind::None;
		return true;
	}
	if(k == "rectangle" || k == "box" || k == "square")
	{
		out = BaseShapeKind::Rectangle;
		return true;
	}
	if(k == "circle" || k == "cylinder" || k == "round")
	{
		out = BaseShapeKind::Circle;
		return true;
	}
	if(k == "hexagon" || k == "hex")
	{
		out = BaseShapeKind::Hexagon;
		return true;
	}
	return false;
}

AuScTriList MakeBaseMesh(const BaseShape& base)
{
	if(!base.IsSet())
		return AuScTriList();

	const double w = qMax(0.01, base.width);
	const double d = qMax(0.01, base.depth);
	const double h = qMax(0.01, base.height);
	const double rx = w / 2;
	const double ry = d / 2;

	QVector<QPointF> outline;
	switch(base.kind)
	{
	case BaseShapeKind::Rectangle:
		outline << QPointF(-rx, -ry) << QPointF(rx, -ry)
				<< QPointF(rx, ry) << QPointF(-rx, ry);
		break;
	case BaseShapeKind::Circle:
	{
		const int segments = 32;
		for(int i = 0; i < segments; ++i)
		{
			const double angle = 2 * kPi * double(i) / double(segments);
			outline << QPointF(cos(angle) * rx, sin(angle) * ry);
		}
		break;
	}
	case BaseShapeKind::Hexagon:
		for(int i = 0; i < 6; ++i)
		{
			const double angle = 2 * kPi * double(i) / 6.;
			outline << QPointF(cos(angle) * rx, sin(angle) * ry);
		}
		break;
	case BaseShapeKind::None:
	default:
		return AuScTriList();
	}

	return MakePrism(outline, h, w, d);
}
