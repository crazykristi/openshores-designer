//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - decals. See decal.h.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "decal.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QPainter>
#include <QSet>
#include <QtMath>

#include "auscparts.h"
#include "interior.h"
#include "paint.h"

namespace
{

const double	kLift = 0.02;			// Feet the decal stands off the hull, so that it never flickers.
const double	kFacing = 0.17;			// cos 80 degrees: faces turned further away get no decal.
const double	kSizeLimit = 14000000.0;	// The game's blueprint size limit, uncompressed.
const int		kPad = 2;				// Transparent pixels round the image; the game repeats textures.

struct HullTri
{
	AuPoint			v[3];
	AuPoint			n[3];				// Vertex normals, pointing out.
	AuPoint			fn;					// Face normal, pointing out.
	const AuScFace*	face = 0;
	double			minS = 0, maxS = 0, minT = 0, maxT = 0;	// Where it lands in the image, for quick tests.
};

AuPoint Unit(AuPoint p)
{
	const double len = p.Length();
	if(len > 1e-12)
		p = p * (1.0 / len);
	return p;
}

// Collects the hull's triangles with normals that point out of the hull.
QVector<HullTri> HullTriangles(const AuDesign& design, AuPoint& mn, AuPoint& mx)
{
	QVector<HullTri> out;
	mn = AuPoint(1e30, 1e30, 1e30);
	mx = AuPoint(-1e30, -1e30, -1e30);
	for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
	{
		const AuScPart* part = it.value();
		if(!part || !part->IsHullMesh() || part->IsOmitted())
			continue;
		AuScFaceList faces;
		part->Faces(faces);
		// Hulls wind with a negative right-handed volume (see interior.h);
		// then the right-handed normal points in.
		const double outSign = SignedVolume(faces) > 0 ? 1.0 : -1.0;
		for(const AuScFace* face : faces)
		{
			AuScTriList tris;
			face->Triangles(tris);
			for(const AuScTri& t : tris)
			{
				HullTri h;
				h.v[0] = t.V0();
				h.v[1] = t.V1();
				h.v[2] = t.V2();
				const AuPoint g = (h.v[1] - h.v[0]).Cross(h.v[2] - h.v[0]);
				if(g.Length() < 1e-9)
					continue;
				h.fn = Unit(g * outSign);
				const AuScPoint* sv[3] = { &t.V0(), &t.V1(), &t.V2() };
				for(int k = 0; k < 3; ++k)
				{
					AuPoint n = sv[k]->m_N;
					if(n.Length() < 1e-9 || n.Dot(h.fn) <= 0)
						n = h.fn;
					h.n[k] = Unit(n);
					mn = mn.Min(h.v[k]);
					mx = mx.Max(h.v[k]);
				}
				h.face = face;
				out.append(h);
			}
		}
	}
	return out;
}

// Moller-Trumbore, both sides. True with the distance along 'dir' when hit.
bool RayHits(const AuPoint& o, const AuPoint& dir, const HullTri& t, double& dist)
{
	const AuPoint e1 = t.v[1] - t.v[0];
	const AuPoint e2 = t.v[2] - t.v[0];
	const AuPoint p = dir.Cross(e2);
	const double det = e1.Dot(p);
	if(qAbs(det) < 1e-12)
		return false;
	const double inv = 1.0 / det;
	const AuPoint s = o - t.v[0];
	const double u = s.Dot(p) * inv;
	if(u < 0 || u > 1)
		return false;
	const AuPoint q = s.Cross(e1);
	const double v = dir.Dot(q) * inv;
	if(v < 0 || u + v > 1)
		return false;
	dist = e2.Dot(q) * inv;
	return dist > 0;
}

// The projection: where the image lies, and how a point maps into it.
struct Frame
{
	bool		wrap = false;
	AuPoint		C;				// The decal's centre, on the hull.
	AuPoint		d;				// The projection, pointing into the hull (flat).
	AuPoint		r, u;			// The image's right and up, at the centre.
	AuPoint		axisPt, A, rho, T;	// Wrap: a point on the axis, the axis, out from it at C, and round it.
	double		R = 0;			// Wrap: the hull's radius at C.
	double		W = 1, H = 1;	// The image's size in feet, padding included.

	// Feet right and up from the centre, measured over the surface for wrap.
	void Local(const AuPoint& p, double& x, double& y, double* angle = 0) const
	{
		AuPoint v;
		if(wrap)
		{
			const AuPoint q = p - axisPt;
			const double qa = q.Dot(A);
			const AuPoint qp = q - A * qa;
			const double th = qAtan2(qp.Dot(T), qp.Dot(rho));
			if(angle)
				*angle = th;
			v = A * qa + T * (th * R);
		}
		else
			v = p - C;
		x = v.Dot(r);
		y = v.Dot(u);
	}

	void UV(const AuPoint& p, double& s, double& t, double* angle = 0) const
	{
		double x, y;
		Local(p, x, y, angle);
		s = x / W + .5;
		t = y / H + .5;		// The game uploads images bottom row first: t = 1 is the top.
	}

	// The way from a point towards the projector.
	AuPoint Toward(const AuPoint& p) const
	{
		if(!wrap)
			return -d;
		const AuPoint q = p - axisPt;
		return Unit(q - A * q.Dot(A));
	}
};

struct PV
{
	AuPoint		p;
	double		s, t;
};

// Clips a polygon to one edge of the image: keep where value(s,t) >= 0.
// Crossings are found on the surface (exactly for flat, by bisection for wrap).
QVector<PV> ClipPolygon(const QVector<PV>& in, const Frame& f, int edge)
{
	auto value = [edge](double s, double t) -> double
	{
		switch(edge)
		{
		case 0: return s;
		case 1: return 1 - s;
		case 2: return t;
		default: return 1 - t;
		}
	};
	QVector<PV> out;
	const int n = in.size();
	for(int i = 0; i < n; ++i)
	{
		const PV& a = in.at(i);
		const PV& b = in.at((i + 1) % n);
		const double va = value(a.s, a.t);
		const double vb = value(b.s, b.t);
		if(va >= 0)
			out.append(a);
		if((va >= 0) != (vb >= 0))
		{
			double lo = 0, hi = 1;	// lo on a's side.
			PV m = a;
			for(int k = 0; k < 24; ++k)
			{
				const double mid = (lo + hi) * .5;
				m.p = a.p + (b.p - a.p) * mid;
				f.UV(m.p, m.s, m.t);
				if((value(m.s, m.t) >= 0) == (va >= 0))
					lo = mid;
				else
					hi = mid;
			}
			m.p = a.p + (b.p - a.p) * ((lo + hi) * .5);
			f.UV(m.p, m.s, m.t);
			out.append(m);
		}
	}
	return out;
}

struct Builder
{
	const Frame&				f;
	const QVector<HullTri>&		hull;
	QVector<int>				occluders;	// Hull triangles that can hide part of the decal.
	AuScFaceList				faces;
	quint8						slot = 0;
	float						opacity = 1;
	float						selfLit = 0;

	Builder(const Frame& frame, const QVector<HullTri>& h) : f(frame), hull(h) {}

	bool Visible(const AuPoint& p, const HullTri& src) const
	{
		const AuPoint dir = f.Toward(p);
		const AuPoint o = p + src.fn * 0.05;
		for(int i : occluders)
		{
			const HullTri& h = hull.at(i);
			if(&h == &src)
				continue;
			double dist;
			if(RayHits(o, dir, h, dist) && dist > 1e-3)
				return false;
		}
		return true;
	}

	void Emit(const PV& a, const PV& b, const PV& c, const HullTri& src)
	{
		const PV* pv[3] = { &a, &b, &c };
		AuScPoint out[3];
		for(int k = 0; k < 3; ++k)
		{
			// The vertex normal, from the source triangle's corners.
			const AuPoint& p = pv[k]->p;
			const AuPoint v0 = src.v[1] - src.v[0], v1 = src.v[2] - src.v[0], v2 = p - src.v[0];
			const double d00 = v0.Dot(v0), d01 = v0.Dot(v1), d11 = v1.Dot(v1), d20 = v2.Dot(v0), d21 = v2.Dot(v1);
			const double den = d00 * d11 - d01 * d01;
			AuPoint n = src.fn;
			if(qAbs(den) > 1e-18)
			{
				const double bv = (d11 * d20 - d01 * d21) / den;
				const double bw = (d00 * d21 - d01 * d20) / den;
				n = Unit(src.n[0] * (1 - bv - bw) + src.n[1] * bv + src.n[2] * bw);
			}
			out[k] = AuScPoint(p + src.fn * kLift, float(qBound(0.0, pv[k]->s, 1.0)), float(qBound(0.0, pv[k]->t, 1.0)), n);
		}
		AuScFaceTri face(out[0], out[1], out[2]);
		face.SetTextureIndex(slot);
		face.SetPaint(QColor(0, 0, 0, 0));
		face.SetOpacity(opacity);
		face.SetSelfIllumination(qMax(src.face->SelfIllumination(), selfLit));
		face.SetShine(src.face->Shine());
		face.SetReflectivity(src.face->Reflectivity());
		faces.AddFace(face);
	}

	// Keeps the parts of a piece that the projector can see.
	void Piece(const PV& a, const PV& b, const PV& c, const HullTri& src, int depth)
	{
		const double area = (b.p - a.p).Cross(c.p - a.p).Length();
		if(area < 1e-8)
			return;
		const AuPoint mid = (a.p + b.p + c.p) * (1.0 / 3);
		const AuPoint samples[4] = { mid, a.p + (mid - a.p) * .1, b.p + (mid - b.p) * .1, c.p + (mid - c.p) * .1 };
		int seen = 0;
		for(const AuPoint& s : samples)
			seen += Visible(s, src) ? 1 : 0;
		if(seen == 4 || (depth >= 3 && Visible(mid, src)))
		{
			Emit(a, b, c, src);
			return;
		}
		if(seen == 0 || depth >= 3)
			return;
		auto half = [this](const PV& x, const PV& y)
		{
			PV m;
			m.p = (x.p + y.p) * .5;
			f.UV(m.p, m.s, m.t);
			return m;
		};
		const PV ab = half(a, b), bc = half(b, c), ca = half(c, a);
		Piece(a, ab, ca, src, depth + 1);
		Piece(ab, b, bc, src, depth + 1);
		Piece(ca, bc, c, src, depth + 1);
		Piece(ab, bc, ca, src, depth + 1);
	}

	void Run()
	{
		// Where every hull triangle lands in the image, to skip far ones fast.
		QVector<HullTri>& h = const_cast<QVector<HullTri>&>(hull);
		for(int i = 0; i < h.size(); ++i)
		{
			HullTri& t = h[i];
			t.minS = t.minT = 1e30;
			t.maxS = t.maxT = -1e30;
			for(int k = 0; k < 3; ++k)
			{
				double s, tt;
				f.UV(t.v[k], s, tt);
				t.minS = qMin(t.minS, s);
				t.maxS = qMax(t.maxS, s);
				t.minT = qMin(t.minT, tt);
				t.maxT = qMax(t.maxT, tt);
			}
			if(t.maxS >= -.05 && t.minS <= 1.05 && t.maxT >= -.05 && t.minT <= 1.05)
				occluders.append(i);
		}

		for(int i : occluders)
		{
			const HullTri& t = hull.at(i);
			if(t.maxS < 0 || t.minS > 1 || t.maxT < 0 || t.minT > 1)
				continue;
			const AuPoint mid = (t.v[0] + t.v[1] + t.v[2]) * (1.0 / 3);
			if(t.fn.Dot(f.Toward(mid)) < kFacing)
				continue;	// Turned away from the projector.
			QVector<PV> poly;
			bool seam = false;
			for(int k = 0; k < 3; ++k)
			{
				PV pv;
				pv.p = t.v[k];
				double angle = 0;
				f.UV(pv.p, pv.s, pv.t, &angle);
				if(f.wrap && qAbs(angle) > M_PI * .9)
					seam = true;	// Round the back of the hull from the decal.
				poly.append(pv);
			}
			if(seam)
				continue;
			for(int e = 0; e < 4 && poly.size() >= 3; ++e)
				poly = ClipPolygon(poly, f, e);
			for(int k = 1; k + 1 < poly.size(); ++k)
				Piece(poly.at(0), poly.at(k), poly.at(k + 1), t, 0);
		}
	}
};

// The image's right and up from the projection direction and rotation.
void ImageAxes(Frame& f, double rotateDeg, bool mirror)
{
	AuPoint up0(0, 0, 1);
	if(qAbs(f.d.z) > .9)
		up0 = AuPoint(0, 1, 0);	// Seen from above or below, the bow is up.
	f.u = Unit(up0 - f.d * f.d.Dot(up0));
	f.r = Unit(f.d.Cross(f.u));
	const double a = qDegreesToRadians(rotateDeg);
	const AuPoint r = f.r * qCos(a) + f.u * qSin(a);
	const AuPoint u = f.u * qCos(a) - f.r * qSin(a);
	f.r = mirror ? -r : r;
	f.u = u;
}

// The first hull point a ray meets.
bool FirstHit(const QVector<HullTri>& hull, const AuPoint& o, const AuPoint& dir, AuPoint& hit)
{
	double best = 1e30;
	for(const HullTri& t : hull)
	{
		double dist;
		if(RayHits(o, dir, t, dist) && dist < best)
			best = dist;
	}
	if(best >= 1e30)
		return false;
	hit = o + dir * best;
	return true;
}

// Makes one decal's faces. False with an error when it cannot be placed.
bool BuildDecal(const QVector<HullTri>& hull, const AuPoint& mn, const AuPoint& mx, const DecalOptions& opts,
		double imageW, double imageH, quint8 slot, float opacity, AuScFaceList& faces, QString& where, QString& error)
{
	AuPoint from(opts.fromX, opts.fromY, opts.fromZ);
	if(from.Length() < 1e-9)
	{
		error = "the decal has no direction";
		return false;
	}
	from = Unit(from);
	const AuPoint aim = opts.haveAim ? AuPoint(opts.aimX, opts.aimY, opts.aimZ) : (mn + mx) * .5;
	const double reach = (mx - mn).Length() * 2 + 100;

	// imageW and imageH are the padded image's sides per foot of content
	// width (see AddDecal), so the content comes out opts.width feet across.
	Frame f;
	f.W = opts.width * imageW;
	f.H = opts.width * imageH;

	if(opts.projection == DecalProjection::Wrap)
	{
		// The axis runs along the hull's longer level side.
		f.A = (mx.y - mn.y) >= (mx.x - mn.x) ? AuPoint(0, 1, 0) : AuPoint(1, 0, 0);
		const AuPoint fp = from - f.A * from.Dot(f.A);
		if(fp.Length() < .2)
		{
			error = "a wrapped decal cannot be aimed along the hull's long axis; use flat";
			return false;
		}
		const AuPoint d0 = -Unit(fp);
		if(!FirstHit(hull, aim - d0 * reach, d0, f.C))
		{
			error = "the decal's projection does not meet the hull";
			return false;
		}
		// The axis goes through the middle of the hull's cross-section there.
		const double slab = qMax(2.0, f.W * .5);
		AuPoint smn(1e30, 1e30, 1e30), smx(-1e30, -1e30, -1e30);
		for(const HullTri& t : hull)
			for(int k = 0; k < 3; ++k)
				if(qAbs((t.v[k] - f.C).Dot(f.A)) <= slab)
				{
					smn = smn.Min(t.v[k]);
					smx = smx.Max(t.v[k]);
				}
		AuPoint centre = (smn + smx) * .5;
		centre = centre + f.A * (f.C - centre).Dot(f.A);
		AuPoint out = f.C - centre;
		out = out - f.A * out.Dot(f.A);
		f.R = out.Length();
		if(f.R < .5)
		{
			error = "the decal's centre is on the hull's axis; aim it at the side of the hull";
			return false;
		}
		f.wrap = true;
		f.axisPt = centre;
		f.rho = Unit(out);
		f.d = -f.rho;
		f.T = f.A.Cross(f.rho);
	}
	else
	{
		f.d = -from;
		if(!FirstHit(hull, aim - f.d * reach, f.d, f.C))
		{
			error = "the decal's projection does not meet the hull";
			return false;
		}
	}
	ImageAxes(f, opts.rotate, opts.mirror);
	if(f.wrap)
	{
		// Keep the image's axes in the surface: round the hull and along it.
		f.r = Unit(f.r - f.rho * f.r.Dot(f.rho));
		f.u = Unit(f.u - f.rho * f.u.Dot(f.rho));
	}

	Builder b(f, hull);
	b.slot = slot;
	b.opacity = opacity;
	b.selfLit = float(qBound(0.0, opts.selfLit, 1.0));
	b.Run();
	if(b.faces.isEmpty())
	{
		error = "no part of the hull faces the decal's projection there";
		return false;
	}
	faces = b.faces;
	where = QString("centre %1, %2, %3 ft").arg(f.C.x, 0, 'f', 1).arg(f.C.y, 0, 'f', 1).arg(f.C.z, 0, 'f', 1);
	return true;
}

} // namespace

QString DecalProjectionName(DecalProjection projection)
{
	return projection == DecalProjection::Wrap ? "wrap" : "flat";
}

bool DecalProjectionFromName(const QString& name, DecalProjection& projection)
{
	const QString n = name.trimmed().toLower();
	if(n == "flat")
		projection = DecalProjection::Flat;
	else if(n == "wrap")
		projection = DecalProjection::Wrap;
	else
		return false;
	return true;
}

QString DecalSidesName(DecalSides sides)
{
	switch(sides)
	{
	case DecalSides::Both: return "both";
	case DecalSides::BothMirrored: return "both-mirrored";
	default: return "one";
	}
}

bool DecalSidesFromName(const QString& name, DecalSides& sides)
{
	const QString n = name.trimmed().toLower();
	if(n == "one")
		sides = DecalSides::One;
	else if(n == "both")
		sides = DecalSides::Both;
	else if(n == "both-mirrored" || n == "mirrored")
		sides = DecalSides::BothMirrored;
	else
		return false;
	return true;
}

bool DecalFromDirection(const QString& text, double& x, double& y, double& z)
{
	const QString t = text.trimmed().toLower();
	struct Named { const char* name; double x, y, z; };
	static const Named names[] = {
		{ "+x", 1, 0, 0 }, { "x", 1, 0, 0 }, { "starboard", 1, 0, 0 }, { "right", 1, 0, 0 },
		{ "-x", -1, 0, 0 }, { "port", -1, 0, 0 }, { "left", -1, 0, 0 },
		{ "+y", 0, 1, 0 }, { "y", 0, 1, 0 }, { "front", 0, 1, 0 }, { "bow", 0, 1, 0 },
		{ "-y", 0, -1, 0 }, { "back", 0, -1, 0 }, { "stern", 0, -1, 0 },
		{ "+z", 0, 0, 1 }, { "z", 0, 0, 1 }, { "top", 0, 0, 1 }, { "up", 0, 0, 1 },
		{ "-z", 0, 0, -1 }, { "bottom", 0, 0, -1 }, { "down", 0, 0, -1 },
	};
	for(const Named& n : names)
		if(t == n.name)
		{
			x = n.x;
			y = n.y;
			z = n.z;
			return true;
		}
	const QStringList parts = t.split(',');
	if(parts.size() != 3)
		return false;
	bool ok[3];
	x = parts.at(0).toDouble(&ok[0]);
	y = parts.at(1).toDouble(&ok[1]);
	z = parts.at(2).toDouble(&ok[2]);
	return ok[0] && ok[1] && ok[2] && (x != 0 || y != 0 || z != 0);
}

QJsonObject DecalOptions::ToJson() const
{
	QJsonObject o;
	o["image"] = image;
	o["projection"] = DecalProjectionName(projection);
	o["from"] = QJsonArray{ fromX, fromY, fromZ };
	if(haveAim)
		o["aim"] = QJsonArray{ aimX, aimY, aimZ };
	o["width"] = width;
	if(rotate != 0)
		o["rotate"] = rotate;
	if(mirror)
		o["mirror"] = true;
	if(sides != DecalSides::One)
		o["sides"] = DecalSidesName(sides);
	if(selfLit != 0)
		o["selfLit"] = selfLit;
	if(opacity != 1)
		o["opacity"] = opacity;
	if(skip)
		o["skip"] = true;
	return o;
}

DecalOptions DecalOptions::FromJson(const QJsonObject& o)
{
	DecalOptions d;
	d.image = o.value("image").toString();
	DecalProjectionFromName(o.value("projection").toString("flat"), d.projection);
	const QJsonArray from = o.value("from").toArray();
	if(from.size() == 3)
	{
		d.fromX = from.at(0).toDouble();
		d.fromY = from.at(1).toDouble();
		d.fromZ = from.at(2).toDouble();
	}
	const QJsonArray aim = o.value("aim").toArray();
	if(aim.size() == 3)
	{
		d.haveAim = true;
		d.aimX = aim.at(0).toDouble();
		d.aimY = aim.at(1).toDouble();
		d.aimZ = aim.at(2).toDouble();
	}
	d.width = o.value("width").toDouble(20);
	d.rotate = o.value("rotate").toDouble(0);
	d.mirror = o.value("mirror").toBool(false);
	DecalSidesFromName(o.value("sides").toString("one"), d.sides);
	d.selfLit = o.value("selfLit").toDouble(0);
	d.opacity = o.value("opacity").toDouble(1);
	d.skip = o.value("skip").toBool(false);
	return d;
}

int DesignFaceCount(const AuDesign& design)
{
	int n = 0;
	for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
	{
		if(!it.value())
			continue;
		AuScFaceList faces;
		it.value()->Faces(faces);
		n += faces.size();
	}
	return n;
}

double DesignSizePercent(const AuDesign& design)
{
	return double(design.MemoryUsage()) * 100.0 / kSizeLimit;
}

bool AddDecal(AuDesign& design, const DecalOptions& opts, DecalResult& result, QString& error)
{
	if(opts.image.trimmed().startsWith('#'))
	{
		error = "a decal needs an image file, not a colour";
		return false;
	}
	if(opts.width <= 0)
	{
		error = "the decal needs a width above 0";
		return false;
	}

	// The image, at most 256 pixels with a transparent rim.
	QImage loaded;
	if(!LoadPaintImage(opts.image, loaded, error))
		return false;
	const int maxContent = AuDesignTex::MaxImageDimension() - 2 * kPad;
	QImage content = loaded;
	if(content.width() > maxContent || content.height() > maxContent)
		content = content.scaled(maxContent, maxContent, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	content = content.convertToFormat(QImage::Format_ARGB32);
	QImage padded(content.width() + 2 * kPad, content.height() + 2 * kPad, QImage::Format_ARGB32);
	padded.fill(Qt::transparent);
	{
		QPainter painter(&padded);
		painter.setCompositionMode(QPainter::CompositionMode_Source);
		painter.drawImage(kPad, kPad, content);
	}

	// Soft edges need the game's blended pass, which only takes faces that
	// are not fully opaque.
	bool soft = false;
	for(int y = 0; y < content.height() && !soft; ++y)
		for(int x = 0; x < content.width(); ++x)
		{
			const int a = qAlpha(content.pixel(x, y));
			if(a > 8 && a < 247)
			{
				soft = true;
				break;
			}
		}
	float opacity = float(qBound(0.05, opts.opacity, 1.0));
	if(soft && opacity >= 1)
		opacity = .99f;

	// A palette slot: the same image already there, else one no face uses.
	AuDesignTex palette[SCTEXI_Count];
	design.TexturePalette(palette);
	QSet<int> used;
	for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
	{
		if(!it.value())
			continue;
		AuScFaceList faces;
		it.value()->Faces(faces);
		for(const AuScFace* face : faces)
			used.insert(face->TextureIndex());
	}
	int slot = -1;
	for(int i = 0; i < SCTEXI_Count && slot < 0; ++i)
		if(!palette[i].Image().isNull() && palette[i].Image() == padded)
			slot = i;
	for(int i = SCTEXI_Count - 1; i >= 0 && slot < 0; --i)
		if(i != SCTEXI_Material && !used.contains(i))
			slot = i;
	if(slot < 0)
	{
		error = "every texture slot of the design is in use, so there is no room for the decal's image";
		return false;
	}

	AuPoint mn, mx;
	QVector<HullTri> hull = HullTriangles(design, mn, mx);
	if(hull.isEmpty())
	{
		error = "the design has no hull to put a decal on";
		return false;
	}

	// The rect in feet per foot of content width.
	const double ratioW = double(padded.width()) / content.width();
	const double ratioH = double(padded.height()) / content.width();

	QList<DecalOptions> sides;
	sides << opts;
	if(opts.sides != DecalSides::One)
	{
		// The other side, mirrored across the hull's middle (port and starboard).
		const double cx = (mn.x + mx.x) * .5;
		DecalOptions other = opts;
		other.fromX = -opts.fromX;
		if(opts.haveAim)
			other.aimX = 2 * cx - opts.aimX;
		if(opts.sides == DecalSides::BothMirrored)
		{
			other.mirror = !opts.mirror;
			other.rotate = -opts.rotate;
		}
		sides << other;
	}

	QList<AuScFaceList> made;
	QStringList wheres;
	for(int i = 0; i < sides.size(); ++i)
	{
		AuScFaceList faces;
		QString where, sideError;
		if(!BuildDecal(hull, mn, mx, sides.at(i), ratioW, ratioH, quint8(slot), opacity, faces, where, sideError))
		{
			if(i == 0)
			{
				error = sideError;
				return false;
			}
			result.notes << QString("the copy on the other side was left out: %1.").arg(sideError);
			continue;
		}
		made << faces;
		wheres << where;
	}

	// Everything worked out: change the design.
	palette[slot].SetImage(QFileInfo(opts.image).fileName(), padded);
	design.SetTexturePalette(palette);
	const QString name = QString("Decal %1").arg(QFileInfo(opts.image).completeBaseName());
	for(int i = 0; i < made.size(); ++i)
	{
		AuScDecal part;
		part.AddFaces(made.at(i));
		part.SetGroupName(name);
		if(!part.IsValid() || !design.Parts().AddPart(part))
		{
			result.notes << "the game did not take one of the decal parts.";
			continue;
		}
		result.faces += made.at(i).size();
		result.parts++;
	}
	result.slot = slot;
	result.notes.prepend(QString("decal %1: %2 face(s) in %3 part(s), %4, %5 x %6 ft, palette slot %7%8; %9.")
			.arg(QFileInfo(opts.image).fileName())
			.arg(result.faces).arg(result.parts)
			.arg(DecalProjectionName(opts.projection))
			.arg(opts.width, 0, 'f', 1).arg(opts.width * content.height() / content.width(), 0, 'f', 1)
			.arg(slot)
			.arg(opacity < 1 ? (soft && opts.opacity >= 1 ? ", soft edges" : ", see-through") : "")
			.arg(wheres.join("; ")));
	return result.parts > 0;
}

DecalsApplied ApplyDecals(AuDesign& design, const QList<DecalOptions>& decals)
{
	DecalsApplied out;
	for(const DecalOptions& d : decals)
	{
		if(d.skip || !d.IsSet())
			continue;
		DecalResult r;
		QString error;
		if(AddDecal(design, d, r, error))
		{
			out.parts += r.parts;
			out.faces += r.faces;
			out.notes << r.notes;
		}
		else
			out.warnings << QString("Decal %1 was left out: %2.").arg(QFileInfo(d.image).fileName(), error);
	}
	return out;
}

int RemoveDecals(AuDesign& design)
{
	QList<quint16> ids;
	for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
		if(it.value() && it.value()->IsDecal() && it.value()->GroupName().startsWith("Decal "))
			ids << it.key();
	for(quint16 id : ids)
	{
		AuScParts deleted;
		design.Parts().DeletePart(id, deleted);
	}
	return ids.size();
}
