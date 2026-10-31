//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - changing the shape of a model.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "shape.h"

#include <QMap>
#include <QObject>

#include "aupoint.h"

namespace {

// A small hash based noise. It does not have to be pretty, it only has to be
// the same every time.
inline double Hash3(int x, int y, int z, int seed)
{
	quint32 h = quint32(x) * 374761393u + quint32(y) * 668265263u + quint32(z) * 2147483647u
			+ quint32(seed) * 1274126177u;
	h = (h ^ (h >> 13)) * 1274126177u;
	h = h ^ (h >> 16);
	return double(h & 0xFFFFFF) / double(0xFFFFFF) * 2.0 - 1.0;
}

inline double Smooth(double t)
{
	return t * t * (3.0 - 2.0 * t);
}

// Noise in the range -1 to 1.
double ValueNoise(const AuPoint& p, double scale, int seed)
{
	const double x = p.x / scale;
	const double y = p.y / scale;
	const double z = p.z / scale;
	const int ix = int(floor(x));
	const int iy = int(floor(y));
	const int iz = int(floor(z));
	const double fx = Smooth(x - ix);
	const double fy = Smooth(y - iy);
	const double fz = Smooth(z - iz);

	double n = 0;
	for(int dz = 0; dz < 2; ++dz)
	{
		for(int dy = 0; dy < 2; ++dy)
		{
			for(int dx = 0; dx < 2; ++dx)
			{
				const double weight = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy) * (dz ? fz : 1 - fz);
				n += weight * Hash3(ix + dx, iy + dy, iz + dz, seed);
			}
		}
	}
	return n;
}

} // namespace

QString ModelShape::Description() const
{
	QStringList bits;
	if(taperBottom != 1.0 || taperTop != 1.0)
		bits << QString("taper %1 to %2").arg(taperBottom).arg(taperTop);
	if(twist != 0.0)
		bits << QString("twist %1 degrees").arg(twist);
	if(repeats > 1)
		bits << QString("%1 copies around a circle").arg(repeats);
	if(stack > 1)
		bits << QString("%1 copies stacked up").arg(stack);
	if(noise != 0.0)
		bits << QString("noise %1").arg(noise);
	if(stretch != 1.0)
		bits << QString("stretched %1").arg(stretch);
	if(bits.isEmpty())
		return "the model as it is";
	return bits.join(", ");
}

QList<AuScTriList> ApplyModelShape(const QList<AuScTriList>& parts, const ModelShape& shape)
{
	if(parts.isEmpty() || !shape.AnyChange())
		return parts;

	// The size and the height of the whole model.
	AuPoint min, max;
	bool have = false;
	for(const AuScTriList& tris : parts)
	{
		for(const AuScTri& t : tris)
		{
			for(int i = 0; i < 3; ++i)
			{
				const AuScPoint& v = t.Vertex(i);
				if(!have)
				{
					min = AuPoint(v.x, v.y, v.z);
					max = min;
					have = true;
					continue;
				}
				min.x = qMin(min.x, v.x);
				min.y = qMin(min.y, v.y);
				min.z = qMin(min.z, v.z);
				max.x = qMax(max.x, v.x);
				max.y = qMax(max.y, v.y);
				max.z = qMax(max.z, v.z);
			}
		}
	}
	if(!have)
		return parts;

	const double height = max.z - min.z;
	const double width = qMax(max.x - min.x, max.y - min.y);
	const double centreX = (min.x + max.x) / 2;
	const double centreY = (min.y + max.y) / 2;

	// Move a single vertex.
	auto move = [&](const AuPoint& v, AuPoint& out) -> void
	{
		double x = v.x - centreX;
		double y = v.y - centreY;
		double z = v.z;

		// The taper changes the cross section with the height.
		if(shape.taperBottom != 1.0 || shape.taperTop != 1.0)
		{
			const double t = (height > 0) ? (z - min.z) / height : 0;
			const double scale = shape.taperBottom + (shape.taperTop - shape.taperBottom) * t;
			x *= scale;
			y *= scale;
		}

		// The twist turns the model about its axis as it goes up.
		if(shape.twist != 0.0)
		{
			const double t = (height > 0) ? (z - min.z) / height : 0;
			const double angle = shape.twist * PI / 180.0 * t;
			const double c = cos(angle);
			const double s = sin(angle);
			const double nx = x * c - y * s;
			const double ny = x * s + y * c;
			x = nx;
			y = ny;
		}

		// The noise pushes the surface out and in.
		if(shape.noise != 0.0)
		{
			AuPoint p(x + centreX, y + centreY, z);
			const double n = ValueNoise(p, qMax(0.01, width * shape.noiseScale), shape.seed)
					+ 0.5 * ValueNoise(p, qMax(0.01, width * shape.noiseScale * 0.4), shape.seed + 1);
			const double amount = shape.noise * width;
			const double len = qSqrt(x * x + y * y);
			if(len > 0.0001)
			{
				x += x / len * n * amount;
				y += y / len * n * amount;
			}
			z += n * amount * 0.3;
		}

		out = AuPoint(x + centreX, y + centreY, z);
	};

	// Move every vertex of every part.
	QList<AuScTriList> moved;
	for(const AuScTriList& tris : parts)
	{
		AuScTriList out;
		for(const AuScTri& t : tris)
		{
			AuScPoint v[3];
			for(int i = 0; i < 3; ++i)
			{
				AuPoint p;
				move(t.Vertex(i), p);
				v[i] = AuScPoint(p.x, p.y, p.z, t.Vertex(i).m_Tx, t.Vertex(i).m_Ty, t.Vertex(i).m_N);
			}
			out.append(AuScTri(v[0], v[1], v[2]));
		}
		moved << out;
	}

	// The stretch is on the whole model, so it is easier to do it on the
	// triangles that are already in place.
	if(shape.stretch != 1.0)
	{
		for(AuScTriList& tris : moved)
		{
			for(AuScTri& t : tris)
			{
				AuScPoint v[3];
				for(int i = 0; i < 3; ++i)
				{
					const AuScPoint& p = t.Vertex(i);
					v[i] = AuScPoint(p.x, p.y, (p.z - min.z) * shape.stretch + min.z, p.m_Tx, p.m_Ty, p.m_N);
				}
				t = AuScTri(v[0], v[1], v[2]);
			}
		}
	}

	// Copies around a circle and copies stacked up.
	//
	// The copies are put in the order: for each copy up the tower, for each
	// copy around the circle, for each part of the model. That way the parts
	// of one copy sit next to each other in the list.
	QList<AuScTriList> result;
	const int around = qMax(1, shape.repeats);
	const int up = qMax(1, shape.stack);
	// When the copies are not told how far apart to sit, they are spread out
	// enough that they do not sit inside each other.
	const double radius = (shape.repeatRadius > 0 ? shape.repeatRadius : 0.75) * width;

	for(int s = 0; s < up; ++s)
	{
		const double stackScale = pow(shape.stackShrink, s);
		const double stackRise = s * height * stackScale;
		for(int r = 0; r < around; ++r)
		{
			const double angle = (around > 1) ? (2.0 * PI * r / around) : 0.0;
			const double c = cos(angle);
			const double sn = sin(angle);
			const double dx = (around > 1) ? cos(angle) * radius : 0.0;
			const double dy = (around > 1) ? sin(angle) * radius : 0.0;
			for(const AuScTriList& tris : moved)
			{
				AuScTriList out;
				for(const AuScTri& t : tris)
				{
					AuScPoint v[3];
					for(int i = 0; i < 3; ++i)
					{
						const AuScPoint& p = t.Vertex(i);
						// Smaller, turned, and moved out to where this copy
						// belongs.
						const double x = (p.x - centreX) * stackScale;
						const double y = (p.y - centreY) * stackScale;
						const double z = (p.z - min.z) * stackScale + min.z + stackRise;
						const double nx = x * c - y * sn;
						const double ny = x * sn + y * c;
						v[i] = AuScPoint(nx + centreX + dx, ny + centreY + dy, z, p.m_Tx, p.m_Ty, p.m_N);
					}
					out.append(AuScTri(v[0], v[1], v[2]));
				}
				result << out;
			}
		}
	}

	return result;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---|---+---+---+---+---+---+---+---+---+---|
// The looks that are on offer.

QList<ShapePreset> ShapePresets()
{
	QList<ShapePreset> list;

	ShapePreset p;
	p.key = "as-is";
	p.label = QObject::tr("As it is");
	list << p;

	p.key = "spire";
	p.label = QObject::tr("Spire");
	p.shape = ModelShape();
	p.shape.taperTop = 0.35;
	list << p;

	p.key = "obelisk";
	p.label = QObject::tr("Obelisk");
	p.shape = ModelShape();
	p.shape.taperTop = 0.25;
	p.shape.taperBottom = 0.75;
	list << p;

	p.key = "column";
	p.label = QObject::tr("Column");
	p.shape = ModelShape();
	p.shape.taperTop = 0.85;
	list << p;

	p.key = "flared";
	p.label = QObject::tr("Flared");
	p.shape = ModelShape();
	p.shape.taperTop = 1.5;
	p.shape.taperBottom = 0.55;
	list << p;

	p.key = "spiral";
	p.label = QObject::tr("Spiral");
	p.shape = ModelShape();
	p.shape.twist = 200.;
	list << p;

	p.key = "spiral-cluster";
	p.label = QObject::tr("Spiral cluster");
	p.shape = ModelShape();
	p.shape.twist = 120.;
	p.shape.repeats = 3;
	list << p;

	p.key = "cathedral";
	p.label = QObject::tr("Cathedral");
	p.shape = ModelShape();
	p.shape.repeats = 4;
	list << p;

	p.key = "ring";
	p.label = QObject::tr("Ring");
	p.shape = ModelShape();
	p.shape.repeats = 6;
	p.shape.repeatRadius = 1.1;
	list << p;

	p.key = "tower";
	p.label = QObject::tr("Tower");
	p.shape = ModelShape();
	p.shape.stack = 3;
	p.shape.stackShrink = 0.7;
	list << p;

	p.key = "rough";
	p.label = QObject::tr("Rough crystal");
	p.shape = ModelShape();
	p.shape.noise = 0.04;
	p.shape.noiseScale = 0.2;
	list << p;

	p.key = "wide";
	p.label = QObject::tr("Wide hall");
	p.shape = ModelShape();
	p.shape.stretch = 0.55;
	p.shape.taperTop = 1.25;
	list << p;

	return list;
}

bool FindShapePreset(const QString& key, ShapePreset& out)
{
	const QList<ShapePreset> list = ShapePresets();
	for(const ShapePreset& p : list)
	{
		if(p.key == key)
		{
			out = p;
			return true;
		}
	}
	return false;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---|---+---+---+---+---+---+---+---+---+---|
// Making a set of variations.

// A little random number maker that always gives the same numbers.
namespace {

class Rng
{
public:
	explicit Rng(quint32 seed) : m_state(seed ? seed : 1)
	{
	}

	// A number between 0 and 1.
	double Next()
	{
		m_state = m_state * 1664525u + 1013904223u;
		return double(m_state >> 8) / double(0xFFFFFF);
	}

	// A number between -amount and amount.
	double Jitter(double amount)
	{
		return (Next() * 2. - 1.) * amount;
	}

private:
	quint32	m_state;
};

} // namespace

ModelShape VaryShape(const ModelShape& base, int index)
{
	ModelShape out = base;
	if(index <= 0)
		return out;

	Rng rng(quint32(base.seed) * 1000003u + quint32(index) * 7919u);

	// The proportions move a little.
	if(out.taperTop != 1.0)
		out.taperTop = qBound(0.02, out.taperTop * (1. + rng.Jitter(0.18)), 4.);
	if(out.taperBottom != 1.0)
		out.taperBottom = qBound(0.02, out.taperBottom * (1. + rng.Jitter(0.18)), 4.);
	if(out.stretch != 1.0)
		out.stretch = qBound(0.1, out.stretch * (1. + rng.Jitter(0.12)), 4.);

	// The turn and the distance between the copies move a little.
	if(out.twist != 0.0)
		out.twist += rng.Jitter(40.);
	if(out.repeats > 1)
	{
		// A distance of zero means the copies sit on top of each other, so
		// the usual distance is used as the base to move away from.
		const double base = out.repeatRadius > 0 ? out.repeatRadius : 0.75;
		out.repeatRadius = qMax(0.05, base * (1. + rng.Jitter(0.25)));
	}
	if(out.stack > 1)
		out.stackShrink = qBound(0.3, out.stackShrink + rng.Jitter(0.08), 1.);

	// The lumps are always different.
	out.seed = base.seed + index * 7919;

	return out;
}

