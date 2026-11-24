/*
 * proctex.cpp
 *
 * See proctex.h. Every pattern repeats exactly across the image (whole
 * numbers of bricks, panels and windows, noise on a lattice that wraps), so
 * the image tiles without a seam.
 */
#include "proctex.h"

#include <QPainter>
#include <QtMath>
#include <cmath>

namespace {

struct Kind
{
	const char* key;
	const char* help;
	const char* c1;
	const char* c2;
	const char* c3;
};

const Kind kKinds[] = {
	{ "brick", "Bricks in running bond; colour the bricks, colour2 the mortar.", "#8a4b38", "#cfc6b8", "#000000" },
	{ "concrete", "Board-formed concrete with form-tie holes; colour the concrete, colour2 the ties.", "#9a9893", "#5a5956", "#000000" },
	{ "stone", "Stone blocks in courses of different lengths; colour the stone, colour2 the joints.", "#8b857a", "#5d5850", "#000000" },
	{ "plaster", "Smooth plaster or render with a fine grain; colour the plaster.", "#e3ddd0", "#b9b2a4", "#000000" },
	{ "panels", "Metal panels with seams and rivets; colour the panels, colour2 the seams.", "#7c838a", "#3e4246", "#000000" },
	{ "corrugated", "Corrugated metal sheet, ribs up and down; colour the sheet.", "#8e9499", "#50555a", "#000000" },
	{ "planks", "Wooden planks across with grain; colour the wood, colour2 the gaps.", "#7a5a3c", "#3b2a1c", "#000000" },
	{ "tiles", "Square tiles; colour the tiles, colour2 the grout.", "#d8d6d0", "#8f8c86", "#000000" },
	{ "windows", "One storey of a facade: a band of wall at the floor and a row of windows; colour the wall, colour2 "
				 "the frames, colour3 the glass. With the facade wrap, one image is one deck.", "#6d6f73", "#2b2d30", "#3c5a72" },
	{ "strip_windows", "One storey with a long ribbon window across it; colour the wall, colour2 the frame, colour3 "
					   "the glass.", "#d9d6cf", "#3a3c3f", "#40627c" },
	{ "neon_strip", "A dark wall with one bright line across it (use a self-lit paint); colour the line, colour2 "
					"the wall.", "#ff4fb0", "#17161c", "#000000" },
	{ "hazard", "Diagonal warning stripes; colour and colour2 the stripes.", "#e2b41c", "#1d1d1d", "#000000" },
};

const Kind* FindKind(const QString& key)
{
	for(const Kind& k : kKinds)
		if(key == QLatin1String(k.key))
			return &k;
	return 0;
}

// A small, fast hash, the same everywhere.
quint32 Hash(quint32 x, quint32 y, quint32 seed)
{
	quint32 h = x * 374761393u + y * 668265263u + seed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

double Rand(quint32 x, quint32 y, quint32 seed)
{
	return (Hash(x, y, seed) & 0xffffff) / double(0xffffff);
}

// Value noise on a lattice of px by py cells that wraps, so it tiles.
double Noise(double u, double v, int px, int py, quint32 seed)
{
	const double x = u * px, y = v * py;
	const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
	const double fx = x - x0, fy = y - y0;
	const double sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
	auto at = [&](int i, int j) {
		return Rand(quint32(((i % px) + px) % px), quint32(((j % py) + py) % py), seed);
	};
	const double a = at(x0, y0), b = at(x0 + 1, y0), c = at(x0, y0 + 1), d = at(x0 + 1, y0 + 1);
	return a + (b - a) * sx + (c - a) * sy + (a - b - c + d) * sx * sy;
}

// Fractal noise, 0 to 1 about 0.5; px by py cells at the coarsest.
double Fbm2(double u, double v, int px, int py, int octaves, quint32 seed)
{
	double sum = 0, amp = .5, norm = 0;
	for(int o = 0; o < octaves; ++o)
	{
		sum += Noise(u, v, px, py, seed + quint32(o) * 7919u) * amp;
		norm += amp;
		amp *= .5;
		px *= 2;
		py *= 2;
	}
	return sum / norm;
}

double Fbm(double u, double v, int period, int octaves, quint32 seed)
{
	return Fbm2(u, v, period, period, octaves, seed);
}

QColor Mix(const QColor& a, const QColor& b, double t)
{
	t = qBound(0.0, t, 1.0);
	return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
						    a.blueF() + (b.blueF() - a.blueF()) * t);
}

QColor Shade(const QColor& c, double f)		// f below 1 darker, above lighter.
{
	return f < 1 ? Mix(Qt::black, c, f) : Mix(c, Qt::white, f - 1);
}

int Count(double base, double density)
{
	return qMax(1, int(std::round(base * qBound(.25, density, 4.0))));
}

// Frames and joints: true within 'half' (0..0.5 of a cell) of the cell's edges.
bool NearEdge(double f, double half)
{
	return f < half || f > 1 - half;
}

} // namespace

QStringList ProcTexKinds()
{
	QStringList keys;
	for(const Kind& k : kKinds)
		keys << k.key;
	return keys;
}

QString ProcTexKindHelp(const QString& kind)
{
	const Kind* k = FindKind(kind);
	return k ? QString::fromLatin1(k->help) : QString();
}

bool DefaultProcTex(const QString& kind, ProcTex& tex)
{
	const Kind* k = FindKind(kind);
	if(!k)
		return false;
	tex = ProcTex();
	tex.kind = kind;
	tex.colour = QColor(k->c1);
	tex.colour2 = QColor(k->c2);
	tex.colour3 = QColor(k->c3);
	return true;
}

bool IsProcTexSpec(const QString& text)
{
	return text.trimmed().startsWith("proc:");
}

bool ParseProcTex(const QString& spec, ProcTex& tex, QString& error)
{
	const QString text = spec.trimmed();
	if(!text.startsWith("proc:"))
	{
		error = "a procedural texture begins with proc:";
		return false;
	}
	const QStringList parts = text.mid(5).split(';', QString::SkipEmptyParts);
	if(parts.isEmpty() || !DefaultProcTex(parts.first().trimmed(), tex))
	{
		error = QString("there is no procedural texture '%1' (there are %2)")
				.arg(parts.isEmpty() ? QString() : parts.first(), ProcTexKinds().join(", "));
		return false;
	}
	for(int i = 1; i < parts.size(); ++i)
	{
		const QString key = parts.at(i).section('=', 0, 0).trimmed();
		const QString value = parts.at(i).section('=', 1).trimmed();
		bool ok = true;
		if(key == "colour" || key == "colour2" || key == "colour3")
		{
			const QColor c(value);
			ok = c.isValid();
			(key == "colour" ? tex.colour : key == "colour2" ? tex.colour2 : tex.colour3) = c;
		}
		else if(key == "wear")
			tex.wear = qBound(0.0, value.toDouble(&ok), 1.0);
		else if(key == "density")
			tex.density = qBound(.25, value.toDouble(&ok), 4.0);
		else if(key == "lit")
			tex.lit = qBound(0.0, value.toDouble(&ok), 1.0);
		else if(key == "seed")
			tex.seed = value.toInt(&ok);
		else
			ok = false;
		if(!ok)
		{
			error = QString("'%1' is not understood in a procedural texture").arg(parts.at(i));
			return false;
		}
	}
	return true;
}

QString ProcTexSpec(const ProcTex& tex)
{
	QStringList parts;
	parts << "proc:" + tex.kind
		  << "colour=" + tex.colour.name()
		  << "colour2=" + tex.colour2.name()
		  << "colour3=" + tex.colour3.name()
		  << "wear=" + QString::number(tex.wear, 'g', 3)
		  << "density=" + QString::number(tex.density, 'g', 3)
		  << "lit=" + QString::number(tex.lit, 'g', 3)
		  << "seed=" + QString::number(tex.seed);
	return parts.join(';');
}

QString ProcTexImageName(const ProcTex& tex)
{
	return QString("%1-%2.png").arg(tex.kind).arg(tex.seed);
}

QImage MakeProcTex(const ProcTex& tex, int size)
{
	size = qBound(16, size, 1024);
	QImage image(size, size, QImage::Format_RGB32);
	const quint32 seed = quint32(tex.seed) * 2654435761u + 17u;
	const double wear = tex.wear;
	const QString& k = tex.kind;

	for(int py = 0; py < size; ++py)
	{
		// v grows upwards: the bottom row of the image is the floor.
		const double v = 1.0 - (py + .5) / size;
		QRgb* row = reinterpret_cast<QRgb*>(image.scanLine(py));
		for(int px = 0; px < size; ++px)
		{
			const double u = (px + .5) / size;
			QColor c;
			const double grain = Fbm(u, v, 16, 3, seed) - .5;		// About -0.5 to 0.5.
			if(k == "brick")
			{
				const int rows = Count(16, tex.density), cols = Count(4, tex.density);
				const double y = v * rows;
				const int r = int(y);
				const double x = u * cols + (r % 2 ? .5 : 0);
				const int b = int(std::floor(x));
				const double fx = x - b, fy = y - r;
				if(NearEdge(fy, .07) || NearEdge(fx, .025))
					c = Shade(tex.colour2, 1 + grain * .2);
				else
				{
					const double tone = .82 + Rand(quint32((b % cols + cols) % cols), quint32(r), seed) * .3;
					c = Shade(tex.colour, tone + grain * .25);
				}
			}
			else if(k == "concrete")
			{
				const int boards = Count(6, tex.density);
				const double fy = v * boards - std::floor(v * boards);
				double tone = 1 + (Fbm(u, v, 8, 4, seed) - .5) * .35 + grain * .1;
				if(NearEdge(fy, .015))
					tone *= .86;		// The lines between the boards of the form.
				c = Shade(tex.colour, tone);
				// The tie holes: 4 across, 2 up.
				const double tx = u * 4 - std::floor(u * 4) - .5, ty = v * 2 - std::floor(v * 2) - .5;
				if(tx * tx + ty * ty < .0035)
					c = tex.colour2;
			}
			else if(k == "stone")
			{
				const int rows = Count(4, tex.density);
				const double y = v * rows;
				const int r = int(y);
				const double fy = y - r;
				// Joints at places that differ from course to course.
				const int blocks = 3;
				double fx = 1;
				for(int j = 0; j < blocks; ++j)
				{
					const double at = (j + .25 + Rand(quint32(j), quint32(r), seed) * .5) / blocks;
					fx = qMin(fx, std::fabs(u - at) * blocks);
				}
				if(NearEdge(fy, .05) || fx < .03)
					c = Shade(tex.colour2, 1 + grain * .2);
				else
				{
					const int block = int(u * blocks);
					c = Shade(tex.colour, .85 + Rand(quint32(block), quint32(r + 99), seed) * .25 + grain * .35
							+ (Fbm(u, v, 32, 2, seed + 5) - .5) * .2);
				}
			}
			else if(k == "plaster")
				c = Shade(tex.colour, 1 + (Fbm(u, v, 32, 3, seed) - .5) * .12 + (Fbm(u, v, 4, 2, seed + 3) - .5) * .1);
			else if(k == "panels")
			{
				const int n = Count(2, tex.density);
				const double x = u * n, y = v * n;
				const double fx = x - std::floor(x), fy = y - std::floor(y);
				if(NearEdge(fx, .012) || NearEdge(fy, .012))
					c = tex.colour2;
				else
				{
					const int pi = int(x), pj = int(y);
					double tone = .9 + Rand(quint32(pi), quint32(pj), seed) * .15 + grain * .12;
					// Rivets along each edge.
					const int rivets = 8;
					const double ru = fx * rivets - std::floor(fx * rivets) - .5, rv = fy * rivets - std::floor(fy * rivets) - .5;
					const bool edgeBand = fx < .06 || fx > .94 || fy < .06 || fy > .94;
					const double ex = (fx < .06 || fx > .94) ? (fx < .5 ? fx : 1 - fx) - .03 : ru / rivets;
					const double ey = (fy < .06 || fy > .94) ? (fy < .5 ? fy : 1 - fy) - .03 : rv / rivets;
					if(edgeBand && ex * ex + ey * ey < .00012)
						tone *= 1.25;
					c = Shade(tex.colour, tone);
				}
			}
			else if(k == "corrugated")
			{
				const int ribs = Count(24, tex.density);
				const double s = std::sin(u * ribs * 2 * M_PI);
				c = Shade(tex.colour, 1 + s * .18 + grain * .1);
			}
			else if(k == "planks")
			{
				const int n = Count(8, tex.density);
				const double y = v * n;
				const int r = int(y);
				const double fy = y - r;
				// One butt joint in each plank, at a different place.
				const double joint = Rand(quint32(r), 3, seed);
				const double dx = std::fabs(u - joint);
				if(NearEdge(fy, .04) || qMin(dx, 1 - dx) < .006)
					c = tex.colour2;
				else
				{
					const double g = std::sin((u * 40 + Fbm(u, v, 8, 3, seed + quint32(r)) * 6) * M_PI) * .5 + .5;
					c = Shade(tex.colour, .85 + Rand(quint32(r), 7, seed) * .2 + g * .12 + grain * .1);
				}
			}
			else if(k == "tiles")
			{
				const int n = Count(8, tex.density);
				const double fx = u * n - std::floor(u * n), fy = v * n - std::floor(v * n);
				if(NearEdge(fx, .04) || NearEdge(fy, .04))
					c = tex.colour2;
				else
					c = Shade(tex.colour, .95 + Rand(quint32(u * n), quint32(v * n), seed) * .08 + grain * .06);
			}
			else if(k == "windows" || k == "strip_windows")
			{
				// One storey: the wall band at the floor (and a little at the
				// ceiling), the windows between.
				const bool strip = k == "strip_windows";
				const double sill = strip ? .38 : .3, head = strip ? .78 : .86;
				const int cols = strip ? 1 : Count(2, tex.density);
				const double x = u * cols;
				const int w = int(x);
				const double fx = x - w;
				if(v < sill || v > head)
					c = Shade(tex.colour, 1 + grain * .15);
				else
				{
					const double fy = (v - sill) / (head - sill);
					const int panes = strip ? Count(4, tex.density) : 1;
					const double pf = fx * panes - std::floor(fx * panes);
					const bool frame = strip ? (NearEdge(pf, .02) || NearEdge(fy, .05))
											 : (NearEdge(fx, .07) || NearEdge(fy, .05));
					if(frame)
						c = tex.colour2;
					else
					{
						const int pane = strip ? int(fx * panes) : w;
						const bool lit = Rand(quint32(pane), 11, seed) < tex.lit;
						if(lit)
							c = Mix(QColor("#ffd59a"), QColor("#fff1d6"), fy);		// Warm light inside.
						else
						{
							// Sky reflected: lighter at the top, a diagonal sheen.
							const double sheen = std::fabs(std::fmod(fx * 1.3 + fy * .7 + Rand(quint32(pane), 5, seed), 1.0) - .5);
							c = Shade(tex.colour3, .75 + fy * .35 + (sheen < .06 ? .25 : 0));
						}
					}
				}
			}
			else if(k == "neon_strip")
			{
				const double d = std::fabs(v - .5);
				const double core = .02, halo = .12;
				if(d < core)
					c = Mix(tex.colour, Qt::white, .35);
				else if(d < halo)
					c = Mix(tex.colour2, tex.colour, std::pow(1 - (d - core) / (halo - core), 2.2));
				else
					c = Shade(tex.colour2, 1 + grain * .2);
			}
			else		// hazard
			{
				const int n = Count(4, tex.density);
				const double s = (u + v) * n;
				c = (int(std::floor(s)) % 2 == 0) ? Shade(tex.colour, 1 + grain * .15) : Shade(tex.colour2, 1 + grain * .15);
			}

			// Wear: stains, and rain streaks running down from above.
			if(wear > 0)
			{
				const double stain = Fbm(u, v, 4, 3, seed + 101) - .5;
				const double streak = Fbm2(u, v, 64, 4, 2, seed + 202);		// Long and thin up and down.
				double dark = 1 - wear * (qMax(0.0, stain) * .5 + qMax(0.0, streak - .55) * 1.2);
				c = Shade(c, qBound(.3, dark, 1.0));
			}
			row[px] = c.rgb();
		}
	}
	return image;
}
