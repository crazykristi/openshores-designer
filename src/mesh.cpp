//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - loading models and thinning them out.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "mesh.h"

#include <QDataStream>
#ifdef SHIPGEN_HAVE_ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/config.h>
#endif
#include <QFile>

#include <cstdio>
#include <QHash>
#include <QMap>
#include <QTextStream>

#include <cstring>

#include <algorithm>
#include <queue>

#include "auscparts.h"

bool IsBinaryStl(const QString& fileName)
{
	QFile file(fileName);
	if(!file.open(QFile::ReadOnly) || file.size() < 84)
		return false;
	const QByteArray head = file.read(84);
	const quint32 count = quint32(quint8(head[80])) | quint32(quint8(head[81])) << 8
			| quint32(quint8(head[82])) << 16 | quint32(quint8(head[83])) << 24;
	return file.size() == 84 + qint64(count) * 50;
}

int ReadBinaryStl(const QString& fileName, AuScFaceList& faces)
{
	faces.Clear();
	QFile file(fileName);
	if(!file.open(QFile::ReadOnly))
		return 1;
	QDataStream in(&file);
	in.setByteOrder(QDataStream::LittleEndian);
	in.setFloatingPointPrecision(QDataStream::SinglePrecision);
	in.skipRawData(80);
	quint32 count = 0;
	in >> count;
	while(count-- && !in.atEnd())
	{
		float f[12];
		for(float& x : f)
			in >> x;
		quint16 attribute;
		in >> attribute;		// A colour in some files; never more bytes.
		AuPoint normal(f[0], f[1], f[2]);
		normal.Unit();
		// Counterclockwise round the normal, turned the way the engine's own
		// reader turns them.
		AuScPoint v1(f[3], f[4], f[5], 0, 0, normal), v2(f[6], f[7], f[8], 0, 0, normal), v3(f[9], f[10], f[11], 0, 0, normal);
		AuScFaceTri tri(v1, v3, v2);
		if(normal.IsZero())
			tri.FlattenVertexNormals();
		faces.AddFace(tri);
	}
	return faces.isEmpty() ? 2 : 0;
}

namespace {

// Makes a triangle out of three places, and gives the corners the normal of
// the triangle, which is what the shading wants.
AuScTri MakeTri(const double p[3][3])
{
	double ux = p[1][0] - p[0][0], uy = p[1][1] - p[0][1], uz = p[1][2] - p[0][2];
	double vx = p[2][0] - p[0][0], vy = p[2][1] - p[0][1], vz = p[2][2] - p[0][2];
	double nx = uy * vz - uz * vy;
	double ny = uz * vx - ux * vz;
	double nz = ux * vy - uy * vx;
	const double len = qSqrt(nx * nx + ny * ny + nz * nz);
	if(len > 1e-12)
	{
		nx /= len; ny /= len; nz /= len;
	}
	else
	{
		nx = 0; ny = 0; nz = 1;
	}
	const AuPoint normal(nx, ny, nz);
	AuScPoint v[3];
	for(int i = 0; i < 3; ++i)
		v[i] = AuScPoint(AuPoint(p[i][0], p[i][1], p[i][2]), 0.f, 0.f, normal);
	return AuScTri(v[0], v[1], v[2]);
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Reading obj files.

bool ReadObj(const QString& fileName, AuScTriList& tris, QString& error)
{
	QFile file(fileName);
	if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		error = QString("cannot open %1: %2").arg(fileName).arg(file.errorString());
		return false;
	}
	QTextStream in(&file);

	QVector<double> positions;
	QVector<double> normals;
	QVector<float> texcoords;
	bool haveTexcoords = false;

	int lineNumber = 0;
	while(!in.atEnd())
	{
		++lineNumber;
		const QString line = in.readLine().trimmed();
		if(line.isEmpty() || line.startsWith('#'))
			continue;

		const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
		if(parts.isEmpty())
			continue;
		const QString& key = parts.first();

		if(key == "v")
		{
			if(parts.size() < 4)
			{
				error = QString("line %1: a vertex needs three numbers").arg(lineNumber);
				return false;
			}
			positions << parts.at(1).toDouble() << parts.at(2).toDouble() << parts.at(3).toDouble();
		}
		else if(key == "vn")
		{
			if(parts.size() < 4)
				continue;
			normals << parts.at(1).toDouble() << parts.at(2).toDouble() << parts.at(3).toDouble();
		}
		else if(key == "vt")
		{
			// Texture coordinates. A third number is a depth that the engine
			// does not use, so it is read and ignored.
			if(parts.size() < 3)
				continue;
			texcoords << parts.at(1).toFloat() << parts.at(2).toFloat();
			haveTexcoords = true;
		}
		else if(key == "f")
		{
			// A face is a list of corner indexes. Each one is
			// vertex/texture/normal, and any of them may be left out.
			QVector<int> corners;
			QVector<int> texcorners;
			for(int i = 1; i < parts.size(); ++i)
			{
				const QString& p = parts.at(i);
				const QStringList fields = p.split('/');
				const QString first = fields.first();
				bool ok = false;
				int index = first.toInt(&ok);
				if(!ok)
				{
					error = QString("line %1: '%2' is not a corner").arg(lineNumber).arg(p);
					return false;
				}
				if(index < 0)
					index = int(positions.size() / 3) + index;	// Counting from the end.
				else
					--index;	// The indexes in the file start at one.
				if(index < 0 || index >= positions.size() / 3)
				{
					error = QString("line %1: corner %2 is not in the model").arg(lineNumber).arg(p);
					return false;
				}
				corners << index;

				// The texture coordinate, when there is one.
				int texIndex = -1;
				if(fields.size() >= 2 && !fields.at(1).isEmpty())
				{
					bool texOk = false;
					texIndex = fields.at(1).toInt(&texOk);
					if(texOk)
					{
						if(texIndex < 0)
							texIndex = int(texcoords.size() / 2) + texIndex;
						else
							--texIndex;
						if(texIndex < 0 || texIndex >= texcoords.size() / 2)
							texIndex = -1;
					}
					else
						texIndex = -1;
				}
				texcorners << texIndex;
			}
			// A face with more than three corners is a polygon, which is
			// turned into a fan of triangles.
			for(int i = 2; i < corners.size(); ++i)
			{
				const int indices[3] = { corners.at(0), corners.at(i - 1), corners.at(i) };
				double p[3][3];
				for(int k = 0; k < 3; ++k)
				{
					p[k][0] = positions.at(indices[k] * 3);
					p[k][1] = positions.at(indices[k] * 3 + 1);
					p[k][2] = positions.at(indices[k] * 3 + 2);
				}
				AuScTri tri = MakeTri(p);
				if(haveTexcoords)
				{
					const int textureCorner[3] = { texcorners.at(0), texcorners.at(i - 1), texcorners.at(i) };
					for(int k = 0; k < 3; ++k)
					{
						const int ti = textureCorner[k];
						if(ti < 0)
							continue;
						tri.SetVertexTextureCoord(k, texcoords.at(ti * 2), texcoords.at(ti * 2 + 1));
					}
				}
				tris.append(tri);
			}
		}
		// Everything else, such as the materials, is not used.
	}

	if(tris.isEmpty())
	{
		error = QString("%1 has no faces in it").arg(fileName);
		return false;
	}
	return true;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Writing files.

void WriteBinaryStl(const QString& fileName, const AuScTriList& tris, QString& error)
{
	QFile file(fileName);
	if(!file.open(QIODevice::WriteOnly))
	{
		error = QString("cannot write %1: %2").arg(fileName).arg(file.errorString());
		return;
	}

	// Everything is written raw. Mixing the stream's own writing with raw
	// writing would put the parts of the file in the wrong order, because
	// the stream holds on to what it is given until it is told to write it.
	QDataStream out(&file);
	out.setByteOrder(QDataStream::LittleEndian);
	out.setFloatingPointPrecision(QDataStream::SinglePrecision);

	// The header is 80 bytes of anything, and then the number of triangles.
	const quint32 count = quint32(tris.size());
	char header[80];
	memset(header, 0, sizeof(header));
	memcpy(header, "shipgen mesh", 13);
	out.writeRawData(header, sizeof(header));

	char countBytes[4];
	countBytes[0] = char(count & 0xFF);
	countBytes[1] = char((count >> 8) & 0xFF);
	countBytes[2] = char((count >> 16) & 0xFF);
	countBytes[3] = char((count >> 24) & 0xFF);
	out.writeRawData(countBytes, 4);

	char attribute[2] = { 0, 0 };
	for(const AuScTri& t : tris)
	{
		const AuPoint& n = t.Normal();
		const float normal[3] = { float(n.x), float(n.y), float(n.z) };
		out.writeRawData(reinterpret_cast<const char*>(normal), sizeof(normal));
		const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
		for(int i = 0; i < 3; ++i)
		{
			const float point[3] = { float(v[i]->x), float(v[i]->y), float(v[i]->z) };
			out.writeRawData(reinterpret_cast<const char*>(point), sizeof(point));
		}
		out.writeRawData(attribute, sizeof(attribute));
	}
	file.close();
}

void WriteObj(const QString& fileName, const AuScTriList& tris, QString& error)
{
	QFile file(fileName);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		error = QString("cannot write %1: %2").arg(fileName).arg(file.errorString());
		return;
	}
	QTextStream out(&file);
	out << "# written by shipgen\n";
	int index = 1;
	for(const AuScTri& t : tris)
	{
		const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
		for(int i = 0; i < 3; ++i)
			out << "v " << v[i]->x << " " << v[i]->y << " " << v[i]->z << "\n";
		for(int i = 0; i < 3; ++i)
			out << "vt " << v[i]->m_Tx << " " << v[i]->m_Ty << "\n";
		out << "f " << index << "/" << index << " "
				<< index + 1 << "/" << index + 1 << " "
				<< index + 2 << "/" << index + 2 << "\n";
		index += 3;
	}
	file.close();
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The quadric error metrics.
//
// The idea comes from Garland and Heckbert: every plane of the surface gets a
// matrix that says how much the surface would move if a point were put
// somewhere else, and the matrix of a vertex is the sum of the matrices of the
// planes around it. Moving a vertex to the place where that sum is smallest
// moves it as little as the surface allows.

struct Quadric
{
	double	a2 = 0, ab = 0, ac = 0, ad = 0;
	double	b2 = 0, bc = 0, bd = 0;
	double	c2 = 0, cd = 0;
	double	d2 = 0;

	// Adds a plane. The plane is ax + by + cz + d = 0.
	void AddPlane(double a, double b, double c, double d, double weight)
	{
		a2 += weight * a * a;
		ab += weight * a * b;
		ac += weight * a * c;
		ad += weight * a * d;
		b2 += weight * b * b;
		bc += weight * b * c;
		bd += weight * b * d;
		c2 += weight * c * c;
		cd += weight * c * d;
		d2 += weight * d * d;
	}

	void Add(const Quadric& o)
	{
		a2 += o.a2; ab += o.ab; ac += o.ac; ad += o.ad;
		b2 += o.b2; bc += o.bc; bd += o.bd;
		c2 += o.c2; cd += o.cd; d2 += o.d2;
	}

	// How far the surface moves if a point is put here.
	double Error(double x, double y, double z) const
	{
		return a2 * x * x + 2 * ab * x * y + 2 * ac * x * z + 2 * ad * x
				+ b2 * y * y + 2 * bc * y * z + 2 * bd * y
				+ c2 * z * z + 2 * cd * z + d2;
	}

	// The place where the surface moves the least, if there is one.
	bool Optimal(double& x, double& y, double& z) const
	{
		const double det = a2 * (b2 * c2 - bc * bc) - ab * (ab * c2 - bc * ac) + ac * (ab * bc - b2 * ac);
		if(qAbs(det) < 1e-12)
			return false;
		x = ( ad * (b2 * c2 - bc * bc) - ab * (bd * c2 - bc * cd) + ac * (bd * bc - b2 * cd) ) / det;
		y = ( a2 * (bd * c2 - bc * cd) - ad * (ab * c2 - bc * ac) + ac * (ab * cd - bd * ac) ) / det;
		z = ( a2 * (b2 * cd - bc * bd) - ab * (ab * cd - bd * ac) + ad * (ab * bc - b2 * ac) ) / det;
		return true;
	}
};

// One collapse that is waiting to happen.
struct CollapseEntry
{
	double	cost = 0;
	int		v1 = 0;
	int		v2 = 0;
	double	x = 0, y = 0, z = 0;

	// The most expensive one is at the top of the heap.
	bool operator<(const CollapseEntry& other) const
	{
		return cost > other.cost;
	}
};

// The mesh that is being thinned out.
class Simplifier
{
public:
	Simplifier(const AuScTriList& tris);

	// Thins it out. Returns the triangles that are left.
	bool Run(int targetFaces, AuScTriList& out);

private:
	// The corners of the triangles.
	QVector<double>	m_x, m_y, m_z;
	// The matrix of each corner.
	QVector<Quadric>	m_quadric;
	// Whether a corner has gone.
	QVector<char>	m_gone;
	// Whether a corner is on the edge of the model.
	QVector<char>	m_boundary;
	// The triangles, three corners each, and whether they have gone.
	QVector<int>	m_face;
	QVector<char>	m_faceGone;
	// The triangles that touch each corner.
	QVector<QVector<int> >	m_around;
	// How many triangles are left.
	int				m_live = 0;

	void	BuildQuadrics();
	void	FindBoundaries();
	double	EdgeCost(int v1, int v2, double& x, double& y, double& z) const;
	bool	IsBoundaryEdge(int v1, int v2) const;
	bool	LinkCondition(int v1, int v2) const;
	bool	Collapse(int v1, int v2, double x, double y, double z);
	void	PushEdges(int v);
};

Simplifier::Simplifier(const AuScTriList& tris)
{
	// The corners are put together first, so that a model with three
	// separate corners for every triangle can still be thinned out.
	const double weld = 1e-5;
	QHash<quint64, int> lookup;
	QVector<quint64> keys;

	const int count = tris.size();
	m_face.reserve(count * 3);
	m_faceGone.reserve(count);
	for(const AuScTri& t : tris)
	{
		int corners[3];
		const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
		for(int i = 0; i < 3; ++i)
		{
			// The place is rounded, so that corners that are almost in the
			// same place are treated as being in the same place.
			const long long qx = qRound64(v[i]->x / weld);
			const long long qy = qRound64(v[i]->y / weld);
			const long long qz = qRound64(v[i]->z / weld);
			quint64 key = quint64(qx) * 0x9E3779B97F4A7C15ULL;
			key ^= quint64(qy) * 0xC2B2AE3D27D4EB4FULL;
			key ^= quint64(qz) * 0x165667B19E3779F9ULL;
			int index = lookup.value(key, -1);
			if(index < 0)
			{
				index = m_x.size();
				lookup.insert(key, index);
				keys.append(key);
				m_x.append(v[i]->x);
				m_y.append(v[i]->y);
				m_z.append(v[i]->z);
				m_quadric.append(Quadric());
				m_gone.append(0);
				m_boundary.append(0);
				m_around.append(QVector<int>());
			}
			corners[i] = index;
		}
		if(corners[0] == corners[1] || corners[1] == corners[2] || corners[0] == corners[2])
			continue;	// The triangle is already flat.
		const int faceIndex = m_faceGone.size();
		m_face.append(corners[0]);
		m_face.append(corners[1]);
		m_face.append(corners[2]);
		m_faceGone.append(0);
		for(int i = 0; i < 3; ++i)
			m_around[corners[i]].append(faceIndex);
		++m_live;
	}
}

void Simplifier::BuildQuadrics()
{
	for(int f = 0; f < m_faceGone.size(); ++f)
	{
		if(m_faceGone[f])
			continue;
		const int a = m_face.at(f * 3);
		const int b = m_face.at(f * 3 + 1);
		const int c = m_face.at(f * 3 + 2);

		const double ax = m_x[a], ay = m_y[a], az = m_z[a];
		const double bx = m_x[b], by = m_y[b], bz = m_z[b];
		const double cx = m_x[c], cy = m_y[c], cz = m_z[c];

		// The normal of the triangle, and how big it is.
		const double ux = bx - ax, uy = by - ay, uz = bz - az;
		const double vx = cx - ax, vy = cy - ay, vz = cz - az;
		double nx = uy * vz - uz * vy;
		double ny = uz * vx - ux * vz;
		double nz = ux * vy - uy * vx;
		const double len = qSqrt(nx * nx + ny * ny + nz * nz);
		if(len < 1e-12)
			continue;
		nx /= len; ny /= len; nz /= len;

		// The area of the triangle is the weight of its plane.
		const double area = len * 0.5;
		const double d = -(nx * ax + ny * ay + nz * az);
		for(int i = 0; i < 3; ++i)
		{
			const int v = m_face.at(f * 3 + i);
			m_quadric[v].AddPlane(nx, ny, nz, d, area);
		}
	}
}

void Simplifier::FindBoundaries()
{
	// Every edge of every triangle is written down, and the ones that are
	// used by only one triangle are on the edge of the model.
	QVector<quint64> edges;
	edges.reserve(m_faceGone.size() * 3);
	for(int f = 0; f < m_faceGone.size(); ++f)
	{
		if(m_faceGone[f])
			continue;
		for(int i = 0; i < 3; ++i)
		{
			const int a = m_face.at(f * 3 + i);
			const int b = m_face.at(f * 3 + (i + 1) % 3);
			const quint32 lo = quint32(qMin(a, b));
			const quint32 hi = quint32(qMax(a, b));
			edges.append((quint64(lo) << 32) | quint64(hi));
		}
	}
	std::sort(edges.begin(), edges.end());

	// The edges that come in pairs are in the middle of the model. The ones
	// that come alone are on its edge.
	QVector<quint64> lonely;
	for(int i = 0; i < edges.size(); )
	{
		int j = i + 1;
		while(j < edges.size() && edges.at(j) == edges.at(i))
			++j;
		if(j - i == 1)
			lonely.append(edges.at(i));
		i = j;
	}

	// Each one of them gets a plane that is straight up and down through it,
	// so that the edge of the model stays where it is.
	for(quint64 key : lonely)
	{
		const int a = int(key >> 32);
		const int b = int(key & 0xFFFFFFFF);
		if(a >= m_x.size() || b >= m_x.size())
			continue;
		m_boundary[a] = 1;
		m_boundary[b] = 1;

		// The plane has to be found from the triangle that uses the edge.
		const double ux = m_x[b] - m_x[a], uy = m_y[b] - m_y[a], uz = m_z[b] - m_z[a];
		double nx = 0, ny = 0, nz = 0;
		bool found = false;
		const QVector<int>& around = m_around[a];
		for(int f : around)
		{
			if(m_faceGone[f])
				continue;
			const int v0 = m_face.at(f * 3);
			const int v1 = m_face.at(f * 3 + 1);
			const int v2 = m_face.at(f * 3 + 2);
			const bool hasA = (v0 == a || v1 == a || v2 == a);
			const bool hasB = (v0 == b || v1 == b || v2 == b);
			if(!hasA || !hasB)
				continue;
			const int other = (v0 != a && v0 != b) ? v0 : ((v1 != a && v1 != b) ? v1 : v2);
			const double wx = m_x[other] - m_x[a], wy = m_y[other] - m_y[a], wz = m_z[other] - m_z[a];
			nx = uy * wz - uz * wy;
			ny = uz * wx - ux * wz;
			nz = ux * wy - uy * wx;
			found = true;
			break;
		}
		if(!found)
			continue;
		const double len = qSqrt(nx * nx + ny * ny + nz * nz);
		if(len < 1e-12)
			continue;
		nx /= len; ny /= len; nz /= len;

		// The plane through the edge, standing up from the surface.
		double mx = ny * uz - nz * uy;
		double my = nz * ux - nx * uz;
		double mz = nx * uy - ny * ux;
		const double mlen = qSqrt(mx * mx + my * my + mz * mz);
		if(mlen < 1e-12)
			continue;
		mx /= mlen; my /= mlen; mz /= mlen;
		const double md = -(mx * m_x[a] + my * m_y[a] + mz * m_z[a]);
		const double weight = 1000.0 * qSqrt(ux * ux + uy * uy + uz * uz);
		m_quadric[a].AddPlane(mx, my, mz, md, weight);
		m_quadric[b].AddPlane(mx, my, mz, md, weight);
	}
}

bool Simplifier::IsBoundaryEdge(int v1, int v2) const
{
	int count = 0;
	const QVector<int>& around = m_around[v1];
	for(int f : around)
	{
		if(m_faceGone[f])
			continue;
		const int v0 = m_face.at(f * 3);
		const int v1b = m_face.at(f * 3 + 1);
		const int v2b = m_face.at(f * 3 + 2);
		if((v0 == v1 && (v1b == v2 || v2b == v2)) ||
				(v1b == v1 && (v0 == v2 || v2b == v2)) ||
				(v2b == v1 && (v0 == v2 || v1b == v2)))
			++count;
	}
	return count == 1;
}

double Simplifier::EdgeCost(int v1, int v2, double& ox, double& oy, double& oz) const
{
	const Quadric combined = [&]() {
		Quadric q = m_quadric[v1];
		q.Add(m_quadric[v2]);
		return q;
	}();

	const double ax = m_x[v1], ay = m_y[v1], az = m_z[v1];
	const double bx = m_x[v2], by = m_y[v2], bz = m_z[v2];

	// The best place is either one of the two corners, the middle between
	// them, or the place where the surface moves the least.
	double best = combined.Error(ax, ay, az);
	double x = ax, y = ay, z = az;
	double cost = combined.Error(bx, by, bz);
	if(cost < best)
	{
		best = cost;
		x = bx; y = by; z = bz;
	}
	const double mx = (ax + bx) * 0.5, my = (ay + by) * 0.5, mz = (az + bz) * 0.5;
	const double midCost = combined.Error(mx, my, mz);
	if(midCost < best)
	{
		best = midCost;
		x = mx; y = my; z = mz;
	}

	double ox2, oy2, oz2;
	if(combined.Optimal(ox2, oy2, oz2))
	{
		// The place has to be near the edge, or the model gets folded.
		const double dx = ox2 - mx, dy = oy2 - my, dz = oz2 - mz;
		const double edge = qSqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay) + (bz - az) * (bz - az));
		if(qSqrt(dx * dx + dy * dy + dz * dz) <= edge)
		{
			const double optCost = combined.Error(ox2, oy2, oz2);
			if(optCost < best)
			{
				best = optCost;
				x = ox2; y = oy2; z = oz2;
			}
		}
	}

	ox = x; oy = y; oz = z;
	return best;
}

void Simplifier::PushEdges(int v)
{
	// Every corner that shares a triangle with this one is a neighbour.
	const QVector<int>& around = m_around[v];
	QVector<int> neighbours;
	for(int f : around)
	{
		if(m_faceGone[f])
			continue;
		for(int i = 0; i < 3; ++i)
		{
			const int other = m_face.at(f * 3 + i);
			if(other != v && !m_gone[other] && !neighbours.contains(other))
				neighbours.append(other);
		}
	}
	// The costs are worked out by the caller, so this only gathers them.
	for(int other : neighbours)
	{
		if(other < v)
			continue;	// Every edge is only done once.
		double x, y, z;
		EdgeCost(v, other, x, y, z);
	}
}

bool Simplifier::LinkCondition(int v1, int v2) const
{
	// The neighbours of each end, taken from the triangles that are still
	// there. The two ends are neighbours of each other and are left out.
	QVector<int> n1, n2;
	for(int f : m_around.at(v1))
	{
		if(m_faceGone.at(f))
			continue;
		for(int i = 0; i < 3; ++i)
		{
			const int w = m_face.at(f * 3 + i);
			if(w != v1 && w != v2 && !n1.contains(w))
				n1.append(w);
		}
	}
	for(int f : m_around.at(v2))
	{
		if(m_faceGone.at(f))
			continue;
		for(int i = 0; i < 3; ++i)
		{
			const int w = m_face.at(f * 3 + i);
			if(w != v1 && w != v2 && !n2.contains(w))
				n2.append(w);
		}
	}

	// How many triangles use the edge, and how many neighbours the ends
	// have in common. Every triangle on the edge contributes exactly one
	// common neighbour, the corner opposite the edge. More than that means
	// the collapse would pinch the surface.
	int sharedFaces = 0;
	for(int f : m_around.at(v1))
	{
		if(m_faceGone.at(f))
			continue;
		int a = m_face.at(f * 3), b = m_face.at(f * 3 + 1), c = m_face.at(f * 3 + 2);
		if((a == v1 && (b == v2 || c == v2)) || (b == v1 && (a == v2 || c == v2)) ||
				(c == v1 && (a == v2 || b == v2)))
			++sharedFaces;
	}

	int common = 0;
	for(int w : n1)
		if(n2.contains(w))
			++common;
	return common == sharedFaces;
}

bool Simplifier::Collapse(int v1, int v2, double x, double y, double z)
{
	// The triangles that use the corner that is going away are moved over to
	// the one that stays, and the ones that end up flat are dropped.
	const QVector<int> around = m_around[v2];
	for(int f : around)
	{
		if(m_faceGone[f])
			continue;
		int found = -1;
		bool hasV1 = false;
		for(int i = 0; i < 3; ++i)
		{
			const int v = m_face.at(f * 3 + i);
			if(v == v2)
				found = i;
			if(v == v1)
				hasV1 = true;
		}
		if(found < 0)
			continue;
		if(hasV1)
		{
			// The triangle has both corners in it, so it is flat now.
			m_faceGone[f] = 1;
			--m_live;
			continue;
		}
		m_face[f * 3 + found] = v1;
		m_around[v1].append(f);
	}

	m_x[v1] = x;
	m_y[v1] = y;
	m_z[v1] = z;
	m_quadric[v1].Add(m_quadric[v2]);
	m_boundary[v1] = m_boundary[v1] || m_boundary[v2];
	m_gone[v2] = 1;
	return true;
}

bool Simplifier::Run(int targetFaces, AuScTriList& out)
{
	if(m_live <= 0)
		return false;
	if(targetFaces <= 0 || targetFaces >= m_live)
	{
		// Nothing to do, but the triangles still have to come back.
		for(int f = 0; f < m_faceGone.size(); ++f)
		{
			if(m_faceGone[f])
				continue;
			const int a = m_face.at(f * 3), b = m_face.at(f * 3 + 1), c = m_face.at(f * 3 + 2);
			const double p[3][3] = { { m_x[a], m_y[a], m_z[a] },
					{ m_x[b], m_y[b], m_z[b] }, { m_x[c], m_y[c], m_z[c] } };
			out.append(MakeTri(p));
		}
		return true;
	}

	BuildQuadrics();
	FindBoundaries();

	// The heap of the collapses that are waiting to happen.
	std::priority_queue<CollapseEntry> heap;

	// Every edge of every triangle goes into the heap.
	for(int f = 0; f < m_faceGone.size(); ++f)
	{
		if(m_faceGone[f])
			continue;
		for(int i = 0; i < 3; ++i)
		{
			const int a = m_face.at(f * 3 + i);
			const int b = m_face.at(f * 3 + (i + 1) % 3);
			if(a >= b)
				continue;
			double x, y, z;
			const double cost = EdgeCost(a, b, x, y, z);
			CollapseEntry c;
			c.cost = cost; c.v1 = a; c.v2 = b; c.x = x; c.y = y; c.z = z;
			heap.push(c);
		}
	}

	while(m_live > targetFaces && !heap.empty())
	{
		const CollapseEntry c = heap.top();
		heap.pop();

		if(m_gone[c.v1] || m_gone[c.v2])
			continue;
		// The edge has to still be there.
		bool found = false;
		for(int f : m_around[c.v1])
		{
			if(m_faceGone[f])
				continue;
			const int v0 = m_face.at(f * 3), v1b = m_face.at(f * 3 + 1), v2b = m_face.at(f * 3 + 2);
			if((v0 == c.v1 && (v1b == c.v2 || v2b == c.v2)) ||
					(v1b == c.v1 && (v0 == c.v2 || v2b == c.v2)) ||
					(v2b == c.v1 && (v0 == c.v2 || v1b == c.v2)))
			{
				found = true;
				break;
			}
		}
		if(!found)
			continue;

		// The edge of the model is not allowed to move, so it is not
		// collapsed into a corner that is not on the edge.
		const bool boundaryEdge = IsBoundaryEdge(c.v1, c.v2);
		if(boundaryEdge && !(m_boundary[c.v1] && m_boundary[c.v2]))
			continue;

		// Collapsing an edge whose ends share neighbours outside the two
		// triangles of the edge folds the surface onto itself and leaves
		// edges used by three or more triangles. The mesh is then no longer
		// a solid, and "inside" stops meaning anything. The link condition
		// rules that out, so it is checked before every collapse.
		if(!LinkCondition(c.v1, c.v2))
			continue;

		Collapse(c.v1, c.v2, c.x, c.y, c.z);

		// The edges around the corner that is left have to be worked out
		// again.
		const QVector<int>& around = m_around[c.v1];
		QVector<int> neighbours;
		for(int f : around)
		{
			if(m_faceGone[f])
				continue;
			for(int i = 0; i < 3; ++i)
			{
				const int other = m_face.at(f * 3 + i);
				if(other != c.v1 && !m_gone[other] && !neighbours.contains(other))
					neighbours.append(other);
			}
		}
		for(int other : neighbours)
		{
			const int a = qMin(c.v1, other);
			const int b = qMax(c.v1, other);
			double x, y, z;
			const double cost = EdgeCost(a, b, x, y, z);
			CollapseEntry nc;
			nc.cost = cost; nc.v1 = a; nc.v2 = b; nc.x = x; nc.y = y; nc.z = z;
			heap.push(nc);
		}
	}

	// The triangles that are left.
	for(int f = 0; f < m_faceGone.size(); ++f)
	{
		if(m_faceGone[f])
			continue;
		const int a = m_face.at(f * 3), b = m_face.at(f * 3 + 1), c = m_face.at(f * 3 + 2);
		const double p[3][3] = { { m_x[a], m_y[a], m_z[a] },
				{ m_x[b], m_y[b], m_z[b] }, { m_x[c], m_y[c], m_z[c] } };
		out.append(MakeTri(p));
	}
	return true;
}

} // namespace

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Reading other formats (FBX, glTF, Collada, PLY, ...) with the Assimp
// library, when the program was built with it (build.sh finds it).

#ifdef SHIPGEN_HAVE_ASSIMP
static bool ReadWithAssimp(const QString& fileName, AuScTriList& tris, QString& error)
{
	Assimp::Importer importer;
	// Lines and points are of no use for a hull.
	importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
	// Every mesh is moved to where its node puts it, so a model made of
	// parts (wings, canopy, fuselage) comes in assembled.
	const aiScene* scene = importer.ReadFile(QFile::encodeName(fileName).constData(),
			aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_SortByPType
			| aiProcess_JoinIdenticalVertices | aiProcess_FindDegenerates);
	if(!scene || !scene->mRootNode)
	{
		error = QString("could not read %1: %2").arg(fileName, QString::fromUtf8(importer.GetErrorString()));
		return false;
	}
	for(unsigned m = 0; m < scene->mNumMeshes; ++m)
	{
		const aiMesh* mesh = scene->mMeshes[m];
		for(unsigned f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			if(face.mNumIndices != 3)
				continue;
			double p[3][3];
			for(int i = 0; i < 3; ++i)
			{
				const aiVector3D& v = mesh->mVertices[face.mIndices[i]];
				p[i][0] = v.x;
				p[i][1] = v.y;
				p[i][2] = v.z;
			}
			tris.append(MakeTri(p));
		}
	}
	return true;
}
#endif

bool AssimpAvailable()
{
#ifdef SHIPGEN_HAVE_ASSIMP
	return true;
#else
	return false;
#endif
}

QStringList ModelSuffixes()
{
	QStringList suffixes { "stl", "obj", "3ds" };
#ifdef SHIPGEN_HAVE_ASSIMP
	suffixes << "fbx" << "gltf" << "glb" << "dae" << "ply" << "blend" << "lwo" << "x3d" << "off" << "ac";
#endif
	return suffixes;
}

QString ModelFileFilter()
{
	QStringList patterns;
	for(const QString& s : ModelSuffixes())
		patterns << "*." + s;
	return QString("3D models (%1)").arg(patterns.join(' '));
}

bool LoadMeshFile(const QString& fileName, AuScTriList& tris, QString& error)
{
	const QString suffix = QFileInfo(fileName).suffix().toLower();
	if(suffix == "3ds")
	{
		AuScParts imported;
		imported.Import3DS(fileName, 1.0);
		for(AuScParts::const_iterator it = imported.constBegin(); it != imported.constEnd(); ++it)
		{
			const AuScMeshBase* mesh = dynamic_cast<const AuScMeshBase*>(it.value());
			if(!mesh)
				continue;
			AuScFaceList faces;
			mesh->Faces(faces);
			AuScTriList partTris;
			faces.Triangles(partTris);
			tris.append(partTris);
		}
	}
	else if(suffix == "stl")
	{
		AuScFaceList faces;
		// A binary file is known by its size; the engine's ASCII reader
		// takes a binary one too and reads nonsense from it.
		int result = IsBinaryStl(fileName) ? ReadBinaryStl(fileName, faces) : faces.STLReadASCII(fileName);
		if(result == 2)
			result = faces.STLReadBinary(fileName);	// Not an ASCII file, so try binary.
		if(result != 0)
		{
			error = result == 1 ? QString("could not open %1").arg(fileName)
					: QString("could not read %1").arg(fileName);
			return false;
		}
		faces.Triangles(tris);
	}
	else if(suffix == "obj")
	{
		if(!ReadObj(fileName, tris, error))
			return false;
	}
#ifdef SHIPGEN_HAVE_ASSIMP
	else if(ModelSuffixes().contains(suffix))
	{
		if(!ReadWithAssimp(fileName, tris, error))
			return false;
	}
#endif
	else
	{
		error = QString("unsupported model format '%1' in %2. Use %3.").arg(suffix, fileName, ModelSuffixes().join(", "));
		return false;
	}

	if(tris.isEmpty())
	{
		error = QString("%1 has no usable geometry.").arg(fileName);
		return false;
	}
	return true;
}

bool SaveMeshFile(const QString& fileName, const AuScTriList& tris, QString& error)
{
	const QString suffix = QFileInfo(fileName).suffix().toLower();
	if(suffix == "stl")
		WriteBinaryStl(fileName, tris, error);
	else if(suffix == "obj")
		WriteObj(fileName, tris, error);
	else
	{
		error = QString("cannot write a '%1' file. Use stl or obj.").arg(suffix);
		return false;
	}
	return error.isEmpty();
}

AuScTriList SnapMesh(const AuScTriList& tris, double tolerance)
{
	if(tris.isEmpty() || tolerance <= 0.)
		return tris;

	// The corners are grouped by the place they are in.
	QHash<quint64, int> lookup;
	QVector<double> px, py, pz;
	for(const AuScTri& t : tris)
	{
		const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
		for(int i = 0; i < 3; ++i)
		{
			const long long qx = qRound64(v[i]->x / tolerance);
			const long long qy = qRound64(v[i]->y / tolerance);
			const long long qz = qRound64(v[i]->z / tolerance);
			quint64 key = quint64(qx) * 0x9E3779B97F4A7C15ULL;
			key ^= quint64(qy) * 0xC2B2AE3D27D4EB4FULL;
			key ^= quint64(qz) * 0x165667B19E3779F9ULL;
			if(lookup.contains(key))
				continue;
			lookup.insert(key, px.size());
			px.append(v[i]->x);
			py.append(v[i]->y);
			pz.append(v[i]->z);
		}
	}

	// Every triangle gets the corners of the group it is in. The corners
	// are put onto the grid itself, so that they stay in the same place
	// when the model is written out as a file of single precision numbers.
	AuScTriList out;
	for(const AuScTri& t : tris)
	{
		const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
		AuScPoint corners[3];
		for(int i = 0; i < 3; ++i)
		{
			const double gx = (floor(v[i]->x / tolerance) + .5) * tolerance;
			const double gy = (floor(v[i]->y / tolerance) + .5) * tolerance;
			const double gz = (floor(v[i]->z / tolerance) + .5) * tolerance;
			corners[i] = AuScPoint(AuPoint(gx, gy, gz), v[i]->m_Tx, v[i]->m_Ty, v[i]->m_N);
		}
		out.append(AuScTri(corners[0], corners[1], corners[2]));
	}
	return out;
}

AuScTriList SimplifyMesh(const AuScTriList& tris, int targetFaces, SimplifyMethod method)
{
	if(method == SimplifyMethod::Cluster)
	{
		const double tolerance = ClusterToleranceFor(tris, targetFaces);
		return ClusterMesh(tris, tolerance);
	}

	AuScTriList out;
	Simplifier simplifier(tris);
	if(simplifier.Run(targetFaces, out))
		return out;
	return tris;
}

AuScTriList WeldMesh(const AuScTriList& tris, double tolerance)
{
	AuScTriList out;
	if(tolerance <= 0.)
		return tris;

	QHash<quint64, int> lookup;
	QVector<double> px, py, pz;
	for(const AuScTri& t : tris)
	{
		const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
		AuScPoint corners[3];
		for(int i = 0; i < 3; ++i)
		{
			const long long qx = qRound64(v[i]->x / tolerance);
			const long long qy = qRound64(v[i]->y / tolerance);
			const long long qz = qRound64(v[i]->z / tolerance);
			quint64 key = quint64(qx) * 0x9E3779B97F4A7C15ULL;
			key ^= quint64(qy) * 0xC2B2AE3D27D4EB4FULL;
			key ^= quint64(qz) * 0x165667B19E3779F9ULL;
			int index = lookup.value(key, -1);
			if(index < 0)
			{
				index = px.size();
				lookup.insert(key, index);
				px.append(v[i]->x);
				py.append(v[i]->y);
				pz.append(v[i]->z);
			}
			corners[i] = AuScPoint(AuPoint(px[index], py[index], pz[index]), 0.f, 0.f, AuPoint(0, 0, 1));
		}
		out.append(AuScTri(corners[0], corners[1], corners[2]));
	}
	return out;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The clustering.
//
// The vertices are snapped onto a grid and the triangles that end up flat are
// dropped. The grid cells are found with a hash, and when two cells have the
// same key the clash is probed, so that different cells are never mixed up.

AuScTriList ClusterMesh(const AuScTriList& tris, double tolerance)
{
	AuScTriList out;
	if(tolerance <= 0. || tris.isEmpty())
		return tris;

	QHash<quint64, int> cells;
	QVector<QVector<int> > cellCoords;

	const int mask = 0x1FFFFF;		// 21 bits per axis.
	for(const AuScTri& t : tris)
	{
		int idx[3];
		for(int i = 0; i < 3; ++i)
		{
			const AuScPoint& v = t.Vertex(i);
			const int qx = int(floor(v.x / tolerance));
			const int qy = int(floor(v.y / tolerance));
			const int qz = int(floor(v.z / tolerance));
			quint64 key = (quint64(quint32(qx) & mask) << 42) | (quint64(quint32(qy) & mask) << 21)
					| quint64(quint32(qz) & mask);

			// Find the cell, probing when the key is taken.
			int slot = -1;
			for(int probe = 0; probe < 64; ++probe)
			{
				QHash<quint64, int>::const_iterator it = cells.constFind(key);
				if(it == cells.constEnd())
				{
					slot = cellCoords.size();
					cells.insert(key, slot);
					cellCoords.append(QVector<int>() << qx << qy << qz);
					break;
				}
				const QVector<int>& other = cellCoords[it.value()];
				if(other[0] == qx && other[1] == qy && other[2] == qz)
				{
					slot = it.value();
					break;
				}
				++probe;
				key = key * 6364136223846793005ULL + 1442695040888963407ULL;
			}
			if(slot < 0)
				slot = 0;	// Give up on this corner.
			idx[i] = slot;
		}
		if(idx[0] == idx[1] || idx[1] == idx[2] || idx[0] == idx[2])
			continue;	// The triangle collapsed.

		// The new corners sit in the middles of the grid cells.
		double p[3][3];
		for(int i = 0; i < 3; ++i)
		{
			const QVector<int>& cell = cellCoords[idx[i]];
			p[i][0] = (cell[0] + .5) * tolerance;
			p[i][1] = (cell[1] + .5) * tolerance;
			p[i][2] = (cell[2] + .5) * tolerance;
		}
		out.append(MakeTri(p));
	}
	return out;
}

// The size of the model, so that the grid can be worked out.
static void MeshSize(const AuScTriList& tris, double& area)
{
	area = 0;
	for(const AuScTri& t : tris)
	{
		const AuScPoint& a = t.V0();
		const AuScPoint& b = t.V1();
		const AuScPoint& c = t.V2();
		const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
		const double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
		const double nx = uy * vz - uz * vy;
		const double ny = uz * vx - ux * vz;
		const double nz = ux * vy - uy * vx;
		area += 0.5 * qSqrt(nx * nx + ny * ny + nz * nz);
	}
}

double ClusterToleranceFor(const AuScTriList& tris, int targetFaces)
{
	if(tris.isEmpty() || targetFaces <= 0)
		return 0.;

	double area = 0;
	MeshSize(tris, area);
	if(area <= 0.)
		return 0.;

	// The number of faces goes down with the square of the grid size, so the
	// first guess comes from the area of the model.
	double tolerance = qSqrt(2.0 * area / targetFaces);
	if(tolerance <= 0.)
		return 0.;

	// Then it is worked out from what that guess actually gave.
	for(int i = 0; i < 4; ++i)
	{
		const AuScTriList result = ClusterMesh(tris, tolerance);
		if(result.isEmpty())
			return tolerance;
		if(result.size() <= targetFaces)
			break;
		tolerance *= qSqrt(double(result.size()) / double(targetFaces)) * 1.02;
	}
	return tolerance;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---|---|---|
// Tidying a model.

void MeshExtents(const AuScTriList& tris, AuPoint& min, AuPoint& max);

namespace {
// A corner of a triangle, as a number that can be put in a hash.
quint64 CornerKey(const AuScPoint& p)
{
	const quint64 x = quint64(qRound64(p.x * 4096.));
	const quint64 y = quint64(qRound64(p.y * 4096.));
	const quint64 z = quint64(qRound64(p.z * 4096.));
	return x * 0x9E3779B97F4A7C15ULL ^ y * 0xC2B2AE3D27D4EB4FULL ^ z * 0x165667B19E3779F9ULL;
}

// The model with its corners numbered, so that the edges can be found.
struct IndexedMesh
{
	QVector<double>	x, y, z;
	QVector<int>	face;		// Three corners for each triangle.
	int				vertices = 0;
};

void BuildIndexed(const AuScTriList& tris, IndexedMesh& out)
{
	QHash<quint64, int> lookup;
	for(const AuScTri& t : tris)
	{
		const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
		for(int i = 0; i < 3; ++i)
		{
			const quint64 key = CornerKey(*v[i]);
			int index = lookup.value(key, -1);
			if(index < 0)
			{
				index = out.x.size();
				lookup.insert(key, index);
				out.x.append(v[i]->x);
				out.y.append(v[i]->y);
				out.z.append(v[i]->z);
			}
			out.face.append(index);
		}
	}
	out.vertices = out.x.size();
}


// The area of a triangle.
double TriangleArea(const AuScTri& t)
{
	const AuScPoint& a = t.V0();
	const AuScPoint& b = t.V1();
	const AuScPoint& c = t.V2();
	const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
	const double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
	const double nx = uy * vz - uz * vy;
	const double ny = uz * vx - ux * vz;
	const double nz = ux * vy - uy * vx;
	return 0.5 * qSqrt(nx * nx + ny * ny + nz * nz);
}

// The key of an edge, whichever way round it is.
inline quint64 EdgeKey(int a, int b)
{
	const quint32 lo = quint32(qMin(a, b));
	const quint32 hi = quint32(qMax(a, b));
	return (quint64(lo) << 32) | quint64(hi);
}

} // namespace

MeshReport InspectMesh(const AuScTriList& tris)
{
	MeshReport report;
	report.triangles = tris.size();

	IndexedMesh mesh;
	BuildIndexed(tris, mesh);
	report.vertices = mesh.vertices;

	// How many times each edge is used, and how many times each way round
	// it is used.
	QHash<quint64, int> edgeCount;
	QHash<QPair<int, int>, int> directed;
	for(int f = 0; f < mesh.face.size() / 3; ++f)
	{
		for(int i = 0; i < 3; ++i)
		{
			const int a = mesh.face.at(f * 3 + i);
			const int b = mesh.face.at(f * 3 + (i + 1) % 3);
			if(a == b)
				continue;	// A triangle with no area.
			++edgeCount[EdgeKey(a, b)];
			++directed[qMakePair(a, b)];
		}
	}

	report.edges = edgeCount.size();
	for(auto it = edgeCount.constBegin(); it != edgeCount.constEnd(); ++it)
	{
		if(it.value() == 1)
			++report.loose;
	}

	// Do the triangles all wind the same way? Two triangles that share an
	// edge have to go round it in opposite directions.
	for(auto it = directed.constBegin(); it != directed.constEnd(); ++it)
	{
		if(it.value() > 1)
		{
			report.windingsAgree = false;
			break;
		}
	}

	// Triangles with no area, and the same triangle twice.
	QHash<quint64, int> triangleCount;
	for(int f = 0; f < mesh.face.size() / 3; ++f)
	{
		int keys[3] = { mesh.face.at(f * 3), mesh.face.at(f * 3 + 1), mesh.face.at(f * 3 + 2) };
		qSort(keys, keys + 3);
		const quint64 faceKey = quint64(keys[0]) ^ (quint64(keys[1]) * 31) ^ (quint64(keys[2]) * 7919);
		++triangleCount[faceKey];
	}
	for(auto it = triangleCount.constBegin(); it != triangleCount.constEnd(); ++it)
	{
		if(it.value() > 1)
			report.duplicate += it.value() - 1;
	}
	for(const AuScTri& t : tris)
	{
		if(TriangleArea(t) < 1e-12)
			++report.degenerate;
	}

	return report;
}


AuScTriList CleanMesh(const AuScTriList& tris)
{
	AuScTriList out;
	QHash<quint64, int> seen;
	for(const AuScTri& t : tris)
	{
		if(TriangleArea(t) < 1e-12)
			continue;	// A triangle with no area is of no use.
		quint64 keys[3] = { CornerKey(t.V0()), CornerKey(t.V1()), CornerKey(t.V2()) };
		qSort(keys, keys + 3);
		const quint64 faceKey = keys[0] ^ (keys[1] * 31) ^ (keys[2] * 7919);
		if(seen.contains(faceKey))
			continue;	// The same triangle is already there.
		seen.insert(faceKey, 1);
		out.append(t);
	}
	return out;
}

AuScTriList FixWindings(const AuScTriList& tris)
{
	const int count = tris.size();
	if(count == 0)
		return tris;

	IndexedMesh mesh;
	BuildIndexed(tris, mesh);
	const int faces = mesh.face.size() / 3;

	// Which triangles share an edge, and whether they go round it in the
	// same direction.
	struct Neighbour
	{
		int		other = -1;
		bool	sameWay = false;
	};
	QHash<quint64, QVector<Neighbour> > around;
	QHash<quint64, int> firstDirection;

	for(int f = 0; f < faces; ++f)
	{
		for(int i = 0; i < 3; ++i)
		{
			const int a = mesh.face.at(f * 3 + i);
			const int b = mesh.face.at(f * 3 + (i + 1) % 3);
			const quint64 key = EdgeKey(a, b);
			if(!firstDirection.contains(key))
				firstDirection.insert(key, a < b ? 0 : 1);
			Neighbour n;
			n.other = f;
			n.sameWay = (a < b ? 0 : 1) == firstDirection.value(key);
			around[key].append(n);
		}
	}

	// Now walk over the model, working out which triangles have to be
	// turned the other way round.
	QVector<char> flipped(faces, 0);
	QVector<char> seen(faces, 0);
	QVector<int> stack;
	for(int start = 0; start < faces; ++start)
	{
		if(seen[start])
			continue;
		seen[start] = 1;
		stack.clear();
		stack.append(start);
		while(!stack.isEmpty())
		{
			const int f = stack.takeLast();
			for(int i = 0; i < 3; ++i)
			{
				const int a = mesh.face.at(f * 3 + i);
				const int b = mesh.face.at(f * 3 + (i + 1) % 3);
				const quint64 key = EdgeKey(a, b);
				const bool fDirection = (a < b) ? 0 : 1;
				// The neighbour's direction was recorded against the first
				// triangle that used the edge, not against this one. Turn
				// that round before it decides anything, or a triangle
				// that was itself reached the other way round is flipped
				// wrongly.
				const bool fIsFirst = (fDirection == firstDirection.value(key));
				const QVector<Neighbour>& list = around.value(key);
				for(const Neighbour& n : list)
				{
					if(n.other == f || seen[n.other])
						continue;
					// Two triangles that share an edge have to go round it
					// in opposite ways, so one of them is turned over.
					const bool sameAsThis = (n.sameWay == fIsFirst);
					flipped[n.other] = flipped[f] ^ (sameAsThis ? 1 : 0);
					seen[n.other] = 1;
					stack.append(n.other);
				}
			}
		}
	}

	// Turn the ones that need it over.
	AuScTriList out;
	for(int f = 0; f < faces; ++f)
	{
		AuScTri t = tris.at(f);
		if(flipped[f])
		{
			const AuScPoint v0 = t.V0();
			const AuScPoint v2 = t.V2();
			t = AuScTri(v0, v2, t.V1());
		}
		out.append(t);
	}
	return out;
}



AuScTriList RepairMesh(const AuScTriList& tris, QStringList* done)
{
	AuScTriList out = tris;
	const MeshReport first = InspectMesh(out);
	if(first.degenerate || first.duplicate)
	{
		out = CleanMesh(out);
		if(done)
		{
			QStringList what;
			if(first.degenerate)
				what << QString("%1 triangle(s) with no area").arg(first.degenerate);
			if(first.duplicate)
				what << QString("%1 double(s)").arg(first.duplicate);
			*done << "took away " + what.join(" and ");
		}
	}
	if(!InspectMesh(out).windingsAgree)
	{
		out = FixWindings(out);
		if(done)
			*done << "turned the triangles that wound the wrong way round";
	}
	const int loose = InspectMesh(out).loose;
	if(loose > 0)
	{
		out = FillHoles(out);
		if(done)
			*done << QString("closed holes (%1 open edge(s), %2 left)").arg(loose).arg(InspectMesh(out).loose);
	}
	return out;
}

AuScTriList FillHoles(const AuScTriList& tris)
{
	if(tris.isEmpty())
		return tris;

	// The corners that are in the same place are put onto the same point
	// first, so that the lid that goes over the hole meets the model
	// exactly.
	const AuScTriList snapped = SnapMesh(tris, 1.0 / 4096.);

	IndexedMesh mesh;
	BuildIndexed(snapped, mesh);
	const int faces = mesh.face.size() / 3;

	// How many times each edge is used.
	QHash<quint64, int> edgeCount;
	for(int f = 0; f < faces; ++f)
	{
		for(int i = 0; i < 3; ++i)
		{
			const int a = mesh.face.at(f * 3 + i);
			const int b = mesh.face.at(f * 3 + (i + 1) % 3);
			++edgeCount[EdgeKey(a, b)];
		}
	}

	// The edges that are used by only one triangle are the edges of the
	// holes. The direction matters: the triangle that uses the edge goes
	// from a to b, and the lid has to go from b to a, so that the surface
	// keeps winding the same way.
	QHash<int, QVector<int> > out;
	for(int f = 0; f < faces; ++f)
	{
		for(int i = 0; i < 3; ++i)
		{
			const int a = mesh.face.at(f * 3 + i);
			const int b = mesh.face.at(f * 3 + (i + 1) % 3);
			if(edgeCount.value(EdgeKey(a, b)) != 1)
				continue;
			out[a].append(b);
		}
	}
	if(out.isEmpty())
		return tris;

	// The edges of the holes, in order, so that the same model always gives
	// the same result.
	QVector<QPair<int, int> > edges;
	for(auto it = out.constBegin(); it != out.constEnd(); ++it)
	{
		for(int w : it.value())
			edges.append(qMakePair(it.key(), w));
	}
	std::sort(edges.begin(), edges.end());

	// Walk round the holes. Every corner of a hole has as many ways out as
	// it has ways in, except where the model is pinched and two sheets of
	// the surface meet at one corner. The walk is done in pieces so that
	// every edge of every hole is used exactly once.
	QSet<QPair<int, int> > used;
	QVector<QVector<int> > chains;
	while(true)
	{
		// Find a corner that still has a way out. Where two sheets of the
		// surface meet, the corner has more than one way out, and the walk
		// starts there, so that the hole is not cut in half.
		int start = -1;
		int next = -1;
		for(const QPair<int, int>& edge : edges)
		{
			if(used.contains(edge))
				continue;
			if(out.value(edge.first).size() < 2)
				continue;
			start = edge.first;
			next = edge.second;
			break;
		}
		if(start < 0)
		{
			for(const QPair<int, int>& edge : edges)
			{
				if(used.contains(edge))
					continue;
				start = edge.first;
				next = edge.second;
				break;
			}
		}
		if(start < 0)
			break;

		QVector<int> chain;
		int current = start;
		while(next >= 0)
		{
			used.insert(qMakePair(current, next));
			chain.append(current);
			current = next;

			// The next corner along the hole. The way straight back is
			// never taken, because that would leave the hole and start
			// on another one. Where two sheets of the surface meet at a
			// corner, this is what keeps the walk on the same sheet.
			next = -1;
			const int previous = chain.isEmpty() ? -1 : chain.last();
			const QVector<int>& ways = out[current];
			for(int i = 0; i < ways.size(); ++i)
			{
				const int w = ways.at(i);
				if(used.contains(qMakePair(current, w)))
					continue;
				if(w == previous)
					continue;	// Straight back.
				next = w;
				break;
			}
		}
		if(chain.size() >= 2)
			chains.append(chain);
	}

	if(chains.isEmpty())
		return tris;

	// The pieces that come back to where they started are holes with a
	// clean edge. The rest are holes where the model is pinched, and they
	// have to be joined up to something else.
	QVector<QVector<int> > loops;
	QVector<QVector<int> > pieces;
	for(const QVector<int>& chain : chains)
	{
		const bool isLoop = chain.size() > 2 && used.contains(qMakePair(chain.last(), chain.first()));
		if(isLoop)
			loops.append(chain);
		else
			pieces.append(chain);
	}

	// One middle for all the pieces, so that they can be joined together.
	double cx = 0, cy = 0, cz = 0;
	int corners = 0;
	for(const QVector<int>& chain : pieces)
	{
		for(int index : chain)
		{
			cx += mesh.x.at(index);
			cy += mesh.y.at(index);
			cz += mesh.z.at(index);
			++corners;
		}
	}
	if(corners > 0)
	{
		cx /= corners;
		cy /= corners;
		cz /= corners;
	}

	// Every piece gets a lid of triangles.
	AuScTriList out_tris = snapped;
	const AuScPoint centre = AuScPoint(AuPoint(cx, cy, cz), 0.f, 0.f, AuPoint(0, 0, 1));

	for(const QVector<int>& chain : loops)
	{
		// The middle of the hole, so that the lid is not all thin
		// triangles.
		double lx = 0, ly = 0, lz = 0;
		for(int index : chain)
		{
			lx += mesh.x.at(index);
			ly += mesh.y.at(index);
			lz += mesh.z.at(index);
		}
		lx /= chain.size();
		ly /= chain.size();
		lz /= chain.size();
		const AuScPoint middle = AuScPoint(AuPoint(lx, ly, lz), 0.f, 0.f, AuPoint(0, 0, 1));

		const int last = chain.size() - 1;
		for(int i = 0; i < last; ++i)
		{
			const int a = chain.at(i);
			const int b = chain.at(i + 1);
			const AuScPoint v0 = AuScPoint(AuPoint(mesh.x.at(b), mesh.y.at(b), mesh.z.at(b)), 0.f, 0.f, AuPoint(0, 0, 1));
			const AuScPoint v1 = AuScPoint(AuPoint(mesh.x.at(a), mesh.y.at(a), mesh.z.at(a)), 0.f, 0.f, AuPoint(0, 0, 1));
			out_tris.append(AuScTri(v0, v1, middle));
		}
	}

	for(const QVector<int>& chain : pieces)
	{
		const int last = chain.size() - 1;
		for(int i = 0; i < last; ++i)
		{
			const int a = chain.at(i);
			const int b = chain.at(i + 1);
			const AuScPoint v0 = AuScPoint(AuPoint(mesh.x.at(b), mesh.y.at(b), mesh.z.at(b)), 0.f, 0.f, AuPoint(0, 0, 1));
			const AuScPoint v1 = AuScPoint(AuPoint(mesh.x.at(a), mesh.y.at(a), mesh.z.at(a)), 0.f, 0.f, AuPoint(0, 0, 1));
			out_tris.append(AuScTri(v0, v1, centre));
		}

		// A piece that does not come back to where it started is closed off
		// with one more triangle.
		if(last > 0)
		{
			const int a = chain.first();
			const int b = chain.last();
			const AuScPoint v0 = AuScPoint(AuPoint(mesh.x.at(a), mesh.y.at(a), mesh.z.at(a)), 0.f, 0.f, AuPoint(0, 0, 1));
			const AuScPoint v1 = AuScPoint(AuPoint(mesh.x.at(b), mesh.y.at(b), mesh.z.at(b)), 0.f, 0.f, AuPoint(0, 0, 1));
			out_tris.append(AuScTri(v0, v1, centre));
		}
	}

	// Where two sheets of the surface meet at a corner, the hole cannot be
	// closed cleanly. Joining those corners up would mean putting a thin
	// wall right across the model, which would make the volume wrong in a
	// different way, so they are left open and the tool says how many are
	// left.
	return out_tris;
}

AuScTriList GroundMesh(const AuScTriList& tris)
{
	if(tris.isEmpty())
		return tris;
	AuPoint min, max;
	MeshExtents(tris, min, max);
	const double dx = (min.x + max.x) / 2;
	const double dy = (min.y + max.y) / 2;

	AuScTriList out;
	for(const AuScTri& t : tris)
	{
		AuScPoint v[3];
		const AuScPoint* p[3] = { &t.V0(), &t.V1(), &t.V2() };
		for(int i = 0; i < 3; ++i)
		{
			v[i] = AuScPoint(AuPoint(p[i]->x - dx, p[i]->y - dy, p[i]->z - min.z),
					p[i]->m_Tx, p[i]->m_Ty, p[i]->m_N);
		}
		out.append(AuScTri(v[0], v[1], v[2]));
	}
	return out;
}

AuScTriList MirrorMesh(const AuScTriList& tris, int axis)
{
	AuScTriList out;
	for(const AuScTri& t : tris)
	{
		AuScPoint v[3];
		const AuScPoint* p[3] = { &t.V0(), &t.V1(), &t.V2() };
		for(int i = 0; i < 3; ++i)
		{
			double x = p[i]->x, y = p[i]->y, z = p[i]->z;
			if(axis == 0)
				x = -x;
			else if(axis == 1)
				y = -y;
			else
				z = -z;
			v[i] = AuScPoint(AuPoint(x, y, z), p[i]->m_Tx, p[i]->m_Ty, p[i]->m_N);
		}
		// Turning the model over changes which way round the triangles are,
		// so they are put back the other way round.
		out.append(AuScTri(v[0], v[2], v[1]));
	}
	return out;
}

AuScTriList SmoothMesh(const AuScTriList& tris)
{
	// The corners that are in the same place are gathered together, and each
	// one gets the average of the normals of the triangles around it.
	QHash<quint64, QVector<int> > around;
	QVector<quint64> corner;
	corner.reserve(tris.size() * 3);
	for(const AuScTri& t : tris)
	{
		const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
		for(int i = 0; i < 3; ++i)
		{
			const quint64 key = CornerKey(*v[i]);
			around[key].append(int(corner.size()));
			corner.append(key);
		}
	}

	// The sum of the normals of each corner.
	QHash<quint64, AuPoint> sums;
	for(const AuScTri& t : tris)
	{
		const AuPoint& n = t.Normal();
		const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
		for(int i = 0; i < 3; ++i)
		{
			const quint64 key = CornerKey(*v[i]);
			AuPoint& sum = sums[key];
			sum.x += n.x;
			sum.y += n.y;
			sum.z += n.z;
		}
	}

	AuScTriList out;
	for(const AuScTri& t : tris)
	{
		AuScPoint v[3];
		const AuScPoint* p[3] = { &t.V0(), &t.V1(), &t.V2() };
		for(int i = 0; i < 3; ++i)
		{
			const AuPoint& sum = sums.value(CornerKey(*p[i]));
			AuPoint normal = sum;
			const double len = qSqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
			if(len > 1e-9)
			{
				normal.x /= len;
				normal.y /= len;
				normal.z /= len;
			}
			else
				normal = t.Normal();
			v[i] = AuScPoint(AuPoint(p[i]->x, p[i]->y, p[i]->z), p[i]->m_Tx, p[i]->m_Ty, normal);
		}
		out.append(AuScTri(v[0], v[1], v[2]));
	}
	return out;
}

void MeshExtents(const AuScTriList& tris, AuPoint& min, AuPoint& max)
{
	bool have = false;
	for(const AuScTri& t : tris)
	{
		const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
		for(int i = 0; i < 3; ++i)
		{
			if(!have)
			{
				min = AuPoint(v[i]->x, v[i]->y, v[i]->z);
				max = min;
				have = true;
				continue;
			}
			min.x = qMin(min.x, v[i]->x);
			min.y = qMin(min.y, v[i]->y);
			min.z = qMin(min.z, v[i]->z);
			max.x = qMax(max.x, v[i]->x);
			max.y = qMax(max.y, v[i]->y);
			max.z = qMax(max.z, v[i]->z);
		}
	}
	if(!have)
	{
		min = AuPoint(0, 0, 0);
		max = AuPoint(0, 0, 0);
	}
}
