//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the parts that make an interior walkable.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "interior.h"
#include "austation.h"

#include <QMap>
#include <QSet>
#include <cmath>

#include "audesign.h"
#include "auscparts.h"

AuScFaceList BoxFaces(const AuPoint& mn, const AuPoint& mx)
{
	AuPointArray bnd;
	bnd.append(AuPoint(mn.x, mn.y, mn.z));
	bnd.append(AuPoint(mx.x, mn.y, mn.z));
	bnd.append(AuPoint(mx.x, mx.y, mn.z));
	bnd.append(AuPoint(mn.x, mx.y, mn.z));

	AuScFaceList faces;
	faces.MakeExtrusion(bnd, AuPoint(0, 0, mx.z - mn.z), AuMatrix4x4::IdentityMatrix(), 1, true);
	return faces;
}

double SignedVolume(const AuScFaceList& faces)
{
	AuScTriList tris;
	faces.Triangles(tris);
	double volume = 0;
	for(const AuScTri& t : tris)
		volume += AuPoint(t.V0()).Dot(AuPoint(t.V1()).Cross(AuPoint(t.V2()))) / 6.0;
	return volume;
}

void OrientFaces(AuScFaceList& faces, bool positive)
{
	const double volume = SignedVolume(faces);
	if(volume == 0 || (volume > 0) == positive)
		return;
	faces.ReverseTriangleFacing();
	faces.ReverseVertexNormals();
}

// Splits a part's faces into those near a box and the rest.
static void SplitFacesNear(const AuScPart* part, const AuPoint& mn, const AuPoint& mx, AuScFaceList& nearFaces, AuScFaceList& otherFaces)
{
	AuScFaceList faces;
	part->Faces(faces);
	for(const AuScFace* f : faces)
	{
		AuPoint fmn(1e30, 1e30, 1e30), fmx(-1e30, -1e30, -1e30);
		for(int v = 0; v < f->VertexCount(); ++v)
		{
			fmn = fmn.Min(f->Vertex(v));
			fmx = fmx.Max(f->Vertex(v));
		}
		const bool touches = fmn.x <= mx.x && fmx.x >= mn.x && fmn.y <= mx.y && fmx.y >= mn.y && fmn.z <= mx.z && fmx.z >= mn.z;
		(touches ? nearFaces : otherFaces).AddFace(*f);
	}
}

// Drops faces that are exact copies of another (the same corners in the same
// order), which the engine's lining can produce.
static AuScFaceList WithoutDuplicates(const AuScFaceList& faces)
{
	auto key = [](const AuPoint& p) {
		return QString("%1,%2,%3").arg(p.x, 0, 'f', 3).arg(p.y, 0, 'f', 3).arg(p.z, 0, 'f', 3);
	};
	QSet<QString> seen;
	AuScFaceList out;
	for(const AuScFace* f : faces)
	{
		const int n = f->VertexCount();
		if(n < 3)
			continue;
		// Start the loop at its smallest corner so that a copy gives the same key.
		QStringList corners;
		for(int v = 0; v < n; ++v)
			corners << key(f->Vertex(v));
		int first = 0;
		for(int v = 1; v < n; ++v)
			if(corners[v] < corners[first])
				first = v;
		QString k;
		for(int v = 0; v < n; ++v)
			k += corners[(first + v) % n] + "|";
		if(seen.contains(k))
			continue;
		seen.insert(k);
		out.AddFace(*f);
	}
	return out;
}

// Four thin boxes round an opening (three for a door, which has no sill): 'a'
// along its axis, 'c' across it. A hull part, not a mesh: the game wants every
// mesh part of a ship inside its hull, and the frame reaches its surface.
static quint16 AddOpeningFrame(AuDesign& design, int axis, double a0, double a1, double c0, double c1, double z0, double z1,
		bool frameBottom, const AuScFace* hullLook, const AuMatrix4x4& place)
{
	const double t = .3;
	auto box = [&](double b0, double b1, double zb, double zt) {
		return axis == 0 ? BoxFaces(AuPoint(a0, b0, zb), AuPoint(a1, b1, zt))
				: BoxFaces(AuPoint(b0, a0, zb), AuPoint(b1, a1, zt));
	};
	AuScFaceList frameFaces;
	frameFaces.AddFaces(box(c0 - t, c0, z0 - (frameBottom ? t : 0), z1 + t));
	frameFaces.AddFaces(box(c1, c1 + t, z0 - (frameBottom ? t : 0), z1 + t));
	frameFaces.AddFaces(box(c0, c1, z1, z1 + t));
	if(frameBottom)
		frameFaces.AddFaces(box(c0, c1, z0 - t, z0));
	frameFaces.Transform(place);
	OrientAsHull(frameFaces);
	AuScSelectInfo all;
	for(int i = 0; i < frameFaces.size(); ++i)
		all.m_Faces.insert(i);
	const quint8 slot = hullLook ? hullLook->TextureIndex() : 0;
	frameFaces.TextureUnwrap(all, slot, 4.0, AuScFaceList::UNWRAP_ByFace, 45.f, true, 0, AuMatrix4x4::IdentityMatrix());
	if(hullLook)
		for(AuScFace* f : frameFaces)
		{
			f->SetPaint(hullLook->Paint());
			if(hullLook->ColorTheme() != AuScFace::THEME_Count)
				f->SetPaint(hullLook->ColorTheme(), quint8(hullLook->Paint().alpha()));
			f->SetShine(hullLook->Shine());
			f->SetReflectivity(hullLook->Reflectivity());
		}
	AuScHull framePart;
	framePart.AddFaces(frameFaces);
	framePart.SetGroupName("Frame");
	return framePart.IsValid() ? design.Parts().AddPart(framePart) : 0;
}

bool JigCut(AuDesign& design, int jigType, const AuScFaceList& jigFaces, quint16 hullId, quint16 roomId,
		QList<quint16>* added, int axis, bool frameBottom, double sleeveTo)
{
	AuScPart* jig = AuScPart::Create(AuScPart::PART_TYPE(jigType));
	if(!jig)
		return false;
	jig->AddFaces(jigFaces);
	const quint16 jigId = jig->IsValid() ? design.Parts().AddPart(*jig) : 0;
	delete jig;
	if(!jigId)
		return false;

	// The engine slices every face of the parts it cuts along the planes of
	// the jig, across the whole part. On a flat hull it joins the pieces
	// again; on a curved one they stay, thousands of slivers. So only the
	// faces near the jig are handed to it, and the rest are put back after.
	AuPoint jmn(1e30, 1e30, 1e30), jmx(-1e30, -1e30, -1e30);
	for(const AuScFace* f : jigFaces)
		for(int v = 0; v < f->VertexCount(); ++v)
		{
			jmn = jmn.Min(f->Vertex(v));
			jmx = jmx.Max(f->Vertex(v));
		}
	jmn -= AuPoint(.5, .5, .5);
	jmx += AuPoint(.5, .5, .5);
	QMap<quint16, AuScFaceList> farFaces;
	for(quint16 id : { hullId, roomId })
	{
		AuScPart* part = id ? design.Parts().value(id) : 0;
		if(!part)
			continue;
		AuScFaceList nearFaces, otherFaces;
		SplitFacesNear(part, jmn, jmx, nearFaces, otherFaces);
		if(nearFaces.isEmpty())
			continue;
		part->SetFaces(nearFaces);
		farFaces.insert(id, otherFaces);
	}

	AuScSelection sel;
	sel.insert(jigId, AuScSelectInfo());
	if(hullId)
		sel.insert(hullId, AuScSelectInfo());
	if(roomId)
		sel.insert(roomId, AuScSelectInfo());
	AuScParts partsAdded, partsChanged, partsDeleted;
	const bool ok = design.Parts().JigCutParts(AuId(0), sel, partsAdded, partsChanged, partsDeleted);

	// On a curved hull the engine's lining of the hole comes out as many
	// overlapping fragments. With the axis of the opening known, the lining
	// is taken out of the hull and a clean frame is put round the opening.
	AuPoint cmn = jmn + AuPoint(.5, .5, .5), cmx = jmx - AuPoint(.5, .5, .5);	// The jig itself.
	const bool frame = ok && hullId && axis >= 0 && axis <= 1;
	AuPoint liningMin(1e30, 1e30, 1e30), liningMax(-1e30, -1e30, -1e30);
	AuScFace* hullLook = 0;
	if(frame)
		if(AuScPart* hull = design.Parts().value(hullId))
		{
			AuScFaceList faces, kept;
			hull->Faces(faces);
			const double e = .01;
			for(const AuScFace* f : faces)
			{
				bool inJig = true;
				for(int v = 0; v < f->VertexCount() && inJig; ++v)
				{
					const AuPoint& p = f->Vertex(v);
					inJig = p.x >= cmn.x - e && p.x <= cmx.x + e && p.y >= cmn.y - e && p.y <= cmx.y + e &&
							p.z >= cmn.z - e && p.z <= cmx.z + e;
				}
				if(!inJig)
				{
					kept.AddFace(*f);
					continue;
				}
				for(int v = 0; v < f->VertexCount(); ++v)
				{
					liningMin = liningMin.Min(f->Vertex(v));
					liningMax = liningMax.Max(f->Vertex(v));
				}
			}
			hull->SetFaces(kept);
			// The frame takes the look of a painted hull face by the opening.
			// (Linings of earlier cuts have no paint, and a frame without any
			// shows bare, in the palette's first texture.)
			const AuPoint middle = (cmn + cmx) / 2.0;
			double best = 1e30;
			for(AuScFace* f : faces)
			{
				if(f->Paint().alpha() == 0 && f->ColorTheme() == AuScFace::THEME_Count)
					continue;
				const double d = (AuPoint(f->Vertex(0)) - middle).Length();
				if(d < best)
				{
					best = d;
					delete hullLook;
					hullLook = f->Clone();
				}
			}
		}

	for(QMap<quint16, AuScFaceList>::const_iterator it = farFaces.constBegin(); it != farFaces.constEnd(); ++it)
		if(AuScPart* part = design.Parts().value(it.key()))
		{
			AuScFaceList faces;
			part->Faces(faces);
			faces = WithoutDuplicates(faces);
			faces.AddFaces(it.value());
			part->SetFaces(faces);
		}

	if(frame && liningMin.x < 1e29)
	{
		// Four thin boxes round the opening (three for a door, which has no
		// sill), from where the lining began to where it ended: flush with the
		// hull surface, proud only by enough not to flicker against it.
		const double lip = .03;
		double a0 = (axis == 0 ? liningMin.x : liningMin.y) - lip;
		double a1 = (axis == 0 ? liningMax.x : liningMax.y) + lip;
		if(!qIsNaN(sleeveTo))
		{
			a0 = qMin(a0, sleeveTo - lip);
			a1 = qMax(a1, sleeveTo + lip);
		}
		const double c0 = axis == 0 ? cmn.y : cmn.x, c1 = axis == 0 ? cmx.y : cmx.x;
		const quint16 frameId = AddOpeningFrame(design, axis, a0, a1, c0, c1, cmn.z, cmx.z, frameBottom, hullLook, AuMatrix4x4());
		if(added && frameId)
			*added << frameId;
	}
	delete hullLook;

	AuScParts jigDeleted;
	design.Parts().DeletePart(jigId, jigDeleted);
	if(added)
		for(AuScParts::const_iterator it = partsAdded.constBegin(); it != partsAdded.constEnd(); ++it)
			*added << it.key();
	return ok;
}

// Whether the vertex normals of a closed mesh point inwards: against the
// way its faces are wound outwards (the winding is outward when the signed
// volume is positive).
static bool NormalsPointIn(const AuScFaceList& faces)
{
	AuScTriList tris;
	faces.Triangles(tris);
	double along = 0;
	for(const AuScTri& t : tris)
	{
		const AuPoint n = (AuPoint(t.V1()) - AuPoint(t.V0())).Cross(AuPoint(t.V2()) - AuPoint(t.V0()));
		for(int i = 0; i < 3; ++i)
			along += n.Dot(t.Vertex(i).m_N);
	}
	return (along < 0) == (SignedVolume(faces) > 0);
}

// The default texture palette's slots used inside (the engine's own
// defaults, which shipgen keeps): 0 Wall Metal, 1 Plate Metal Box, 2 Wall
// Cargo, 5 Tile Metal.
enum { kSlotWall = 0, kSlotCeiling = 1, kSlotDoor = 2, kSlotFloor = 5 };

QColor RoomTint(const QString& roomName)
{
	const QString n = roomName.toLower();
	struct Tint { const char* word; QColor colour; };
	static const Tint tints[] = {
		{ "bridge", QColor(170, 192, 220) }, { "command", QColor(170, 192, 220) },
		{ "engine", QColor(222, 190, 150) }, { "reactor", QColor(222, 175, 140) }, { "power", QColor(222, 190, 150) },
		{ "quarters", QColor(182, 210, 180) }, { "barracks", QColor(182, 210, 180) }, { "berth", QColor(182, 210, 180) },
		{ "sick", QColor(215, 238, 232) }, { "medical", QColor(215, 238, 232) }, { "hospital", QColor(215, 238, 232) },
		{ "galley", QColor(225, 205, 170) }, { "lounge", QColor(225, 205, 170) }, { "cantina", QColor(225, 205, 170) },
		{ "weapon", QColor(212, 170, 165) }, { "armory", QColor(212, 170, 165) },
		{ "store", QColor(210, 204, 165) }, { "hold", QColor(210, 204, 165) }, { "cargo", QColor(210, 204, 165) },
		{ "sensor", QColor(190, 175, 215) }, { "shield", QColor(165, 205, 222) }, { "transporter", QColor(200, 180, 222) },
		{ "hall", QColor(205, 205, 200) }, { "corridor", QColor(205, 205, 200) },
	};
	for(const Tint& t : tints)
		if(n.contains(t.word))
			return t.colour;
	return QColor(205, 208, 212);
}

// Floors, walls and ceilings told apart (the user found the rooms all one
// texture, and disorienting): metal tiles on the floor, wall panels
// tinted for the room, plates overhead painted light. The faces' vertex
// normals point into the room.
static void StyleRoomFaces(AuScFaceList& faces, const QColor& wallTint)
{
	AuScSelectInfo floors, walls, ceilings;
	for(int i = 0; i < faces.size(); ++i)
	{
		const AuScFace* f = faces.at(i);
		AuPoint n(0, 0, 0);
		for(int v = 0; v < f->VertexCount(); ++v)
			n += f->Vertex(v).m_N;
		if(n.Length() > 1e-9)
			n.Unit();
		(n.z > .7 ? floors : n.z < -.7 ? ceilings : walls).m_Faces.insert(i);
	}
	struct Part { AuScSelectInfo* sel; int slot; double feet; QColor paint; };
	const Part parts[] = {
		{ &floors, kSlotFloor, 6.0, QColor(150, 152, 158) },
		{ &walls, kSlotWall, 8.0, wallTint },
		{ &ceilings, kSlotCeiling, 8.0, QColor(225, 228, 232) },
	};
	for(const Part& part : parts)
	{
		if(part.sel->m_Faces.isEmpty())
			continue;
		faces.TextureUnwrap(*part.sel, quint8(part.slot), part.feet, AuScFaceList::UNWRAP_ByFace, 45.f, true, 0,
				AuMatrix4x4::IdentityMatrix());
		for(quint32 i : part.sel->m_Faces)
		{
			faces[i]->SetTextureIndex(quint8(part.slot));
			faces[i]->SetPaint(part.paint);
		}
	}
}

void RestyleRoom(AuDesign& design, quint16 meshId, const RoomLook& look)
{
	AuScPart* part = design.Parts().value(meshId);
	if(!part)
		return;
	AuScFaceList faces;
	part->Faces(faces);
	AuScSelectInfo floors, walls, ceilings;
	for(int i = 0; i < faces.size(); ++i)
	{
		const AuScFace* f = faces.at(i);
		const double nz = f->FaceNormal().z;
		const int kind = nz > .7 ? 0 : nz < -.7 ? 2 : 1;
		const int wanted = kind == 0 ? look.floorSlot : kind == 2 ? look.ceilingSlot : look.wallSlot;
		if(wanted < 0 && f->Paint().alpha() != 0)
			continue;
		(kind == 0 ? floors : kind == 2 ? ceilings : walls).m_Faces.insert(i);
	}
	struct Part { AuScSelectInfo* sel; int slot, usual; double feet; QColor paint; };
	const Part kinds[] = {
		{ &floors, look.floorSlot, kSlotFloor, 6.0, QColor(150, 152, 158) },
		{ &walls, look.wallSlot, kSlotWall, 8.0, look.wallTint },
		{ &ceilings, look.ceilingSlot, kSlotCeiling, 8.0, QColor(225, 228, 232) },
	};
	bool changed = false;
	for(const Part& kind : kinds)
	{
		if(kind.sel->m_Faces.isEmpty())
			continue;
		const int slot = kind.slot >= 0 ? kind.slot : kind.usual;
		faces.TextureUnwrap(*kind.sel, quint8(slot), kind.feet, AuScFaceList::UNWRAP_ByFace, 45.f, true, 0,
				AuMatrix4x4::IdentityMatrix());
		for(quint32 i : kind.sel->m_Faces)
		{
			faces[i]->SetTextureIndex(quint8(slot));
			faces[i]->SetPaint(kind.slot >= 0 ? QColor(0, 0, 0, 0) : kind.paint);
		}
		changed = true;
	}
	if(changed)
		part->SetFaces(faces);
}

void RestyleDoor(AuScPart* part, int slot)
{
	AuScStatePart* door = dynamic_cast<AuScStatePart*>(part);
	if(!door)
		return;
	const quint8 was = door->State();
	for(int state = 0; state < 256; ++state)
	{
		if(!door->HasState(quint8(state)))
			continue;
		door->SetState(quint8(state));
		AuScFaceList faces;
		door->Faces(faces);
		AuScSelectInfo all;
		for(int i = 0; i < faces.size(); ++i)
			all.m_Faces.insert(i);
		door->TextureUnwrap(all, SC_TEXTURE_INDEX(slot), 6.0, AuScFaceList::UNWRAP_ByFace, 45.f, true, 0,
				AuMatrix4x4::IdentityMatrix());
		door->PaintFaces(all, QColor(0, 0, 0, 0));
	}
	door->SetState(was);
}

void ClearWindowGlass(AuDesign& design)
{
	for(AuScParts::iterator it = design.Parts().begin(); it != design.Parts().end(); ++it)
	{
		AuScPart* part = it.value();
		if(!part || !part->IsWindow())
			continue;
		AuScFaceList faces;
		part->Faces(faces);
		for(AuScFace* f : faces)
		{
			f->SetPaint(QColor(0, 0, 0, 0));
			f->SetTextureIndex(0);
			f->SetShine(0);
			f->SetReflectivity(0);
		}
		part->SetFaces(faces);
	}
}

void SetFlatPane(AuScPart* window, double x, double width, double z0, double z1)
{
	const AuPoint a(x, -width / 2, z0), b(x, width / 2, z0), c(x, width / 2, z1), d(x, -width / 2, z1);
	const AuPoint a2 = a + AuPoint(.02, 0, 0), b2 = b + AuPoint(.02, 0, 0), c2 = c + AuPoint(.02, 0, 0), d2 = d + AuPoint(.02, 0, 0);
	auto tri = [](const AuPoint& p, const AuPoint& q, const AuPoint& r, const AuPoint& n) {
		return AuScFaceTri(AuScPoint(p, 0.f, 0.f, n), AuScPoint(q, 0.f, 0.f, n), AuScPoint(r, 0.f, 0.f, n));
	};
	const AuPoint outward(1, 0, 0), inward(-1, 0, 0);
	AuScFaceList faces;
	faces.AddFace(tri(a2, b2, c2, outward));
	faces.AddFace(tri(a2, c2, d2, outward));
	faces.AddFace(tri(a, c, b, inward));
	faces.AddFace(tri(a, d, c, inward));
	window->SetFaces(faces);
}

bool CutFlatWindow(AuDesign& design, const AuMatrix4x4& toWall, const AuMatrix4x4& back, double depth, double width,
		double sill, double top, const QString& groupName)
{
	const double y0 = -width / 2, y1 = width / 2, x0 = -.5;
	// Planes to slice along, far bigger than the ship.
	auto cutter = [&back](const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& d) {
		const AuPoint n(0, 0, 1);
		return AuScFaceQuad(AuScPoint(back.Project(a), 0.f, 0.f, n), AuScPoint(back.Project(b), 0.f, 0.f, n),
				AuScPoint(back.Project(c), 0.f, 0.f, n), AuScPoint(back.Project(d), 0.f, 0.f, n));
	};
	const double far_ = 5000;
	QList<AuScFaceQuad> cutters;
	for(double y : { y0, y1 })
		cutters << cutter(AuPoint(-far_, y, -far_), AuPoint(far_, y, -far_), AuPoint(far_, y, far_), AuPoint(-far_, y, far_));
	for(double z : { sill, top })
		cutters << cutter(AuPoint(-far_, -far_, z), AuPoint(far_, -far_, z), AuPoint(far_, far_, z), AuPoint(-far_, far_, z));
	// The hole ends a little past the pane (the hull can have a thin layer just
	// beyond it, which would hide the pane from outside); a ledge further out stays.
	const double x1 = depth + 2.0;
	cutters << cutter(AuPoint(x1, -far_, -far_), AuPoint(x1, far_, -far_), AuPoint(x1, far_, far_), AuPoint(x1, -far_, far_));

	const AuScFace* look = 0;
	AuScFace* lookCopy = 0;
	double best = 1e30;
	const AuPoint middle = back.Project(AuPoint(depth, 0, (sill + top) / 2));
	bool cutAny = false;
	for(AuScParts::iterator it = design.Parts().begin(); it != design.Parts().end(); ++it)
	{
		AuScPart* part = it.value();
		if(!part || !part->IsHullMesh() || part->GroupName() == "Frame")
			continue;
		AuScFaceList faces, kept;
		part->Faces(faces);
		bool changed = false;
		for(const AuScFace* f : faces)
		{
			AuPoint mn(1e30, 1e30, 1e30), mx(-1e30, -1e30, -1e30);
			for(int v = 0; v < f->VertexCount(); ++v)
			{
				const AuPoint p = toWall.Project(AuPoint(f->Vertex(v)));
				mn = mn.Min(p);
				mx = mx.Max(p);
			}
			if(mx.x < x0 || mn.x >= x1 || mx.y <= y0 || mn.y >= y1 || mx.z <= sill || mn.z >= top)
			{
				kept.AddFace(*f);
				continue;
			}
			if(f->Paint().alpha() != 0 || f->ColorTheme() != AuScFace::THEME_Count)
			{
				const double d = (AuPoint(f->Vertex(0)) - middle).Length();
				if(d < best)
				{
					best = d;
					delete lookCopy;
					lookCopy = f->Clone();
				}
			}
			AuScFaceList pieces;
			pieces.AddFace(*f);
			for(const AuScFaceQuad& plane : cutters)
			{
				AuScFaceList next;
				for(const AuScFace* piece : pieces)
				{
					AuScFaceList sliced;
					if(piece->Slice(plane, sliced) && !sliced.isEmpty())
						next.AddFaces(sliced);
					else
						next.AddFace(*piece);
				}
				pieces = next;
			}
			for(const AuScFace* piece : pieces)
			{
				AuPoint c(0, 0, 0);
				for(int v = 0; v < piece->VertexCount(); ++v)
					c += toWall.Project(AuPoint(piece->Vertex(v)));
				c = c / double(piece->VertexCount());
				if(c.x >= x0 && c.x < x1 && c.y > y0 && c.y < y1 && c.z > sill && c.z < top)
				{
					changed = true;
					continue;
				}
				kept.AddFace(*piece);
			}
		}
		if(changed)
		{
			part->SetFaces(kept);
			cutAny = true;
		}
	}
	look = lookCopy;
	if(!cutAny)
	{
		delete lookCopy;
		return false;
	}
	AddOpeningFrame(design, 0, -.03, depth + .03, y0, y1, sill, top, true, look, back);
	delete lookCopy;

	AuScPart* window = AuScPart::Create(AuScPart::Part_Window);
	if(!window)
		return false;
	SetFlatPane(window, depth, width, sill, top);
	window->SetOpacity(.25f);
	window->Transform(back);
	window->SetGroupName(groupName);
	const bool ok = window->IsValid() && design.Parts().AddPart(*window) != 0;
	delete window;
	return ok;
}

void StyleDoor(AuScPart* part, bool hullDoor)
{
	AuScStatePart* door = dynamic_cast<AuScStatePart*>(part);
	if(!door)
		return;
	// Every state has its own mesh, and painting touches the current one.
	const QColor paint = hullDoor ? QColor(230, 180, 40) : QColor(70, 120, 175);
	const quint8 was = door->State();
	for(int state = 0; state < 256; ++state)
	{
		if(!door->HasState(quint8(state)))
			continue;
		door->SetState(quint8(state));
		AuScFaceList faces;
		door->Faces(faces);
		AuScSelectInfo all;
		for(int i = 0; i < faces.size(); ++i)
			all.m_Faces.insert(i);
		door->TextureUnwrap(all, SC_TEXTURE_INDEX(kSlotDoor), 6.0, AuScFaceList::UNWRAP_ByFace, 45.f, true, 0,
				AuMatrix4x4::IdentityMatrix());
		door->PaintFaces(all, paint);
	}
	door->SetState(was);
}

static void SetUpRoomVoid(AuScRoomVoid& roomVoid, const QString& groupName, bool naturalGravity)
{
	roomVoid.SetGroupName(groupName);
	roomVoid.SetGravity(naturalGravity ? G_Natural : G_Artificial, AuPoint(0, 0, 1));
	roomVoid.SetHoldAccess(HOLD_Hold);
	roomVoid.SetPersonnelAccess(PERSONNEL_Everyone);
}

bool AddBoxRoom(AuDesign& design, const QString& groupName, const AuPoint& mn, const AuPoint& mx,
		quint16& voidId, quint16& meshId, bool naturalGravity, const QColor& wallTint)
{
	return AddMeshRoom(design, groupName, BoxFaces(mn, mx), voidId, meshId, naturalGravity, wallTint);
}

bool AddMeshRoom(AuDesign& design, const QString& groupName, const AuScFaceList& faces,
		quint16& voidId, quint16& meshId, bool naturalGravity, const QColor& wallTint)
{
	voidId = meshId = 0;

	// The void is the air of the room. It is not associated with anything,
	// so it is always there.
	AuScFaceList voidFaces = faces;
	OrientAsVoid(voidFaces);
	AuScRoomVoid roomVoid;
	roomVoid.AddFaces(voidFaces);
	SetUpRoomVoid(roomVoid, groupName, naturalGravity);
	if(!roomVoid.IsValid())
		return false;
	voidId = design.Parts().AddPart(roomVoid);

	// The mesh is the visible interior surface.
	AuScFaceList roomFaces = faces;
	OrientAsRoom(roomFaces);
	// Every vertex normal square to its face and pointing into the room, as
	// in the user's scout. Ours pointed every way (most of them straight up,
	// along the walls), and the rooms came out very dark in the game.
	roomFaces.FlattenVertexNormals();
	if(!NormalsPointIn(roomFaces))
		roomFaces.ReverseVertexNormals();
	StyleRoomFaces(roomFaces, wallTint.isValid() ? wallTint : RoomTint(groupName));
	AuScRoom room;
	room.AddFaces(roomFaces);
	room.SetGroupName(groupName);
	// Sunlit and bright, the engine's default and what the rooms of the
	// working designs have. Rooms that were not sunlit came out very dark in
	// the game (the user's test of the Wedge, 2026-10-31).
	room.SetInteriorLighting(true, INTLIGHT_Bright);
	if(!room.IsValid())
		return false;
	meshId = design.Parts().AddPart(room);
	return voidId && meshId;
}

static bool AddDoorPart(AuDesign& design, const AuPoint& mn, const AuPoint& mx, bool hullDoor, quint16& doorId)
{
	AuScDoor door;
	door.AddFaces(BoxFaces(mn, mx));
	door.AddState(AuScDoor::Open);
	door.SetGroupName(hullDoor ? "Hull Door" : "Door");
	if(hullDoor)
		door.SetHullDoor(true);
	if(!door.IsValid())
		return false;
	doorId = design.Parts().AddPart(door);
	return doorId != 0;
}

// A void that only exists while the door is open.
template<class VoidType>
static bool AddDoorVoid(AuDesign& design, const AuPoint& mn, const AuPoint& mx, const QString& groupName, quint16 doorId)
{
	VoidType v;
	v.AddFaces(BoxFaces(mn, mx));
	v.SetGroupName(groupName);
	v.SetAssociatedPart(doorId, AuScDoor::Open);
	// The rooms' gravity. Left to its default a doorway has natural gravity: near
	// a planet that pulls towards the planet, whichever way the ship lies, which
	// turned people round in the doorway and could trap them there. (A void for
	// the hull has no gravity of its own to set.)
	v.SetGravity(G_Artificial, AuPoint(0, 0, 1));
	if(!v.IsValid())
		return false;
	return design.Parts().AddPart(v) != 0;
}

bool AddInteriorDoor(AuDesign& design, const AuPoint& slabMin, const AuPoint& slabMax, int normalAxis,
		const QString& groupName, quint16& doorId, quint16 roomMeshA, quint16 roomMeshB)
{
	doorId = 0;
	if(roomMeshA && roomMeshB)
	{
		// Cut the opening through both walls; the cut-away pieces become the door.
		AuPoint mn = slabMin, mx = slabMax;
		mn.z += .01;		// Not flush with the floor, so the cut has no coplanar faces.
		AuScFaceList jig = BoxFaces(mn, mx);
		OrientAsHull(jig);
		QList<quint16> added;
		if(JigCut(design, AuScPart::Part_DoorJig, jig, roomMeshA, roomMeshB, &added))
			for(quint16 id : added)
				if(AuScDoor* door = dynamic_cast<AuScDoor*>(design.Parts().value(id)))
				{
					door->SetGroupName("Door");
					doorId = id;
				}
	}
	if(!doorId && !AddDoorPart(design, slabMin, slabMax, false, doorId))
		return false;
	// So that it can be told from the wall it is cut out of.
	StyleDoor(design.Parts().value(doorId), false);

	// The doorway reaches into the room on both sides of the wall.
	AuPoint mn = slabMin, mx = slabMax;
	if(normalAxis == 0)
	{
		const double cx = (slabMin.x + slabMax.x) / 2;
		mn.x = cx - SHIPGEN_DOORWAY_REACH;
		mx.x = cx + SHIPGEN_DOORWAY_REACH;
	}
	else
	{
		const double cy = (slabMin.y + slabMax.y) / 2;
		mn.y = cy - SHIPGEN_DOORWAY_REACH;
		mx.y = cy + SHIPGEN_DOORWAY_REACH;
	}
	return AddDoorVoid<AuScRoomVoid>(design, mn, mx, groupName, doorId);
}

// The point of triangle abc closest to p (Ericson, Real-Time Collision
// Detection, 5.1.5).
static AuPoint ClosestOnTriangle(const AuPoint& p, const AuPoint& a, const AuPoint& b, const AuPoint& c)
{
	const AuPoint ab = b - a, ac = c - a, ap = p - a;
	const double d1 = ab.Dot(ap), d2 = ac.Dot(ap);
	if(d1 <= 0 && d2 <= 0)
		return a;
	const AuPoint bp = p - b;
	const double d3 = ab.Dot(bp), d4 = ac.Dot(bp);
	if(d3 >= 0 && d4 <= d3)
		return b;
	const double vc = d1 * d4 - d3 * d2;
	if(vc <= 0 && d1 >= 0 && d3 <= 0)
		return a + ab * (d1 / (d1 - d3));
	const AuPoint cp = p - c;
	const double d5 = ab.Dot(cp), d6 = ac.Dot(cp);
	if(d6 >= 0 && d5 <= d6)
		return c;
	const double vb = d5 * d2 - d1 * d6;
	if(vb <= 0 && d2 >= 0 && d6 <= 0)
		return a + ac * (d2 / (d2 - d6));
	const double va = d3 * d6 - d5 * d4;
	if(va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
		return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
	const double denom = 1.0 / (va + vb + vc);
	return a + ab * (vb * denom) + ac * (vc * denom);
}

quint16 HullPartAt(const AuDesign& design, const AuPoint& p)
{
	quint16 best = 0;
	double bestDistance = 1e30;
	const AuScParts& parts = design.Parts();
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* part = it.value();
		// The frames round openings are hull parts too, but never the hull
		// to cut.
		if(!part || part->IsOmitted() || !part->IsHullMesh() || part->GroupName() == "Frame")
			continue;
		AuScFaceList faces;
		part->Faces(faces);
		AuScTriList tris;
		faces.Triangles(tris);
		for(const AuScTri& t : tris)
		{
			const double d = (ClosestOnTriangle(p, AuPoint(t.V0()), AuPoint(t.V1()), AuPoint(t.V2())) - p).Length();
			if(d < bestDistance)
			{
				bestDistance = d;
				best = it.key();
			}
		}
	}
	return best;
}

bool AddHullDoor(AuDesign& design, const AuPoint& slabMin, const AuPoint& slabMax, const AuPoint& outward,
		double roomWall, double surface, const QString& groupName, quint16& doorId,
		quint16 roomMeshId, double clearBelowZ)
{
	doorId = 0;
	const bool alongX = fabs(outward.x) > fabs(outward.y);
	const double sgn = alongX ? (outward.x > 0 ? 1. : -1.) : (outward.y > 0 ? 1. : -1.);
	auto span = [&](AuPoint& mn, AuPoint& mx, double from, double to) {
		if(alongX)
		{
			mn.x = qMin(from, to);
			mx.x = qMax(from, to);
		}
		else
		{
			mn.y = qMin(from, to);
			mx.y = qMax(from, to);
		}
	};
	auto widen = [&](AuPoint& mn, AuPoint& mx, double by) {
		if(alongX)
		{
			mn.y -= by;
			mx.y += by;
		}
		else
		{
			mn.x -= by;
			mx.x += by;
		}
	};

	// The door: cut through the room wall and the hull, like the game's
	// designer does it, or a plain slab.
	if(roomMeshId)
	{
		AuPoint mn = slabMin, mx = slabMax;
		mn.z += .01;		// Not flush with the floor, so the cut has no coplanar faces.
		span(mn, mx, roomWall - sgn * .05, surface + sgn * 3.0);
		AuPoint middle = (slabMin + slabMax) / 2.0;
		if(alongX)
			middle.x = surface;
		else
			middle.y = surface;
		const quint16 hullId = HullPartAt(design, middle);
		AuScFaceList jig = BoxFaces(mn, mx);
		OrientAsHull(jig);
		QList<quint16> added;
		if(hullId && JigCut(design, AuScPart::Part_DoorJig, jig, hullId, roomMeshId, &added, alongX ? 0 : 1, false))
			for(quint16 id : added)
				if(AuScDoor* door = dynamic_cast<AuScDoor*>(design.Parts().value(id)))
				{
					door->SetHullDoor(true);
					door->SetGroupName("Hull Door");
					doorId = id;
				}
	}
	if(!doorId && !AddDoorPart(design, slabMin, slabMax, true, doorId))
		return false;

	// Where the door is opened and closed: just in front of it inside the
	// room, and just outside the hull, 4 ft up. Left unset, the game takes
	// the middle of the closed door, which for a door made of the room
	// wall and the hull far apart (a bay door some way in from the hull)
	// is inside the hull, out of reach: the user could not find where to
	// open the Wedge's bay door.
	if(AuScDoor* door = dynamic_cast<AuScDoor*>(design.Parts().value(doorId)))
	{
		AuPoint in = (slabMin + slabMax) / 2.0, out = in;
		in.z = out.z = slabMin.z + 4.0;
		if(alongX)
		{
			in.x = roomWall - sgn * 1.0;
			out.x = surface + sgn * 1.0;
		}
		else
		{
			in.y = roomWall - sgn * 1.0;
			out.y = surface + sgn * 1.0;
		}
		door->SetDoorOpLocs(in, out);
	}
	StyleDoor(design.Parts().value(doorId), true);

	// The doorway, from inside the room to just outside the hull.
	AuPoint mn = slabMin, mx = slabMax;
	span(mn, mx, roomWall - sgn * SHIPGEN_DOORWAY_REACH, surface + sgn * SHIPGEN_DOORWAY_OUTSIDE);
	if(!AddDoorVoid<AuScRoomVoid>(design, mn, mx, groupName, doorId))
		return false;

	// The opening through the hull, while the door is open. It is bigger than
	// the door all round except at the floor, so that nobody gets caught on
	// its edge.
	AuPoint hmn = slabMin, hmx = slabMax;
	span(hmn, hmx, roomWall - sgn * .5, surface + sgn * SHIPGEN_DOORWAY_OUTSIDE);
	widen(hmn, hmx, SHIPGEN_HULL_VOID_MARGIN);
	hmx.z += SHIPGEN_HULL_VOID_MARGIN;
	if(!AddDoorVoid<AuScHullVoid>(design, hmn, hmx, "Hull Door", doorId))
		return false;

	// A floor through the doorway, flush with the room's floor: cutting the
	// opening takes away whatever was under it, and without a floor there
	// the user had to jump to get in. It reaches a little past the hull
	// surface, towards the ladder.
	{
		AuPoint tmn = slabMin, tmx = slabMax;
		span(tmn, tmx, roomWall - sgn * .2, surface + sgn * SHIPGEN_THRESHOLD_OUTSIDE);
		tmx.z = slabMin.z;
		tmn.z = slabMin.z - .4;
		AuScFaceList faces = BoxFaces(tmn, tmx);
		OrientAsHull(faces);
		AuScHull threshold;
		threshold.AddFaces(faces);
		threshold.SetGroupName("Frame");
		if(threshold.IsValid())
			design.Parts().AddPart(threshold);
	}

	// The way up to the door from outside, always clear: from just beyond
	// the threshold outwards (as in the scout, where it starts 0.9 ft out, so
	// that it does not take the floor of the doorway away), and from where
	// the hull stops being in the way of the ladder to above the door.
	AuPoint omn = slabMin, omx = slabMax;
	span(omn, omx, surface + sgn * (SHIPGEN_THRESHOLD_OUTSIDE + .05), surface + sgn * SHIPGEN_HULL_VOID_OUTSIDE);
	widen(omn, omx, SHIPGEN_HULL_VOID_MARGIN);
	omx.z += SHIPGEN_HULL_VOID_MARGIN;
	omn.z = qMin(slabMin.z, clearBelowZ);
	AuScHullVoid outside;
	outside.AddFaces(BoxFaces(omn, omx));
	outside.SetGroupName("Hull Door");
	return outside.IsValid() && design.Parts().AddPart(outside) != 0;
}

// A side view ('profile': points (u, h, 0), u along 'dir', h up) made solid
// across 'dir', 'halfWidth' to either side, with (0, 0) at 'origin'. 'dir'
// is along x or y.
static AuScFaceList ProfilePrism(const AuPointArray& profile, const AuPoint& dir, const AuPoint& origin, double halfWidth)
{
	const bool alongX = fabs(dir.x) > fabs(dir.y);
	const double sgn = alongX ? (dir.x > 0 ? 1. : -1.) : (dir.y > 0 ? 1. : -1.);
	AuScFaceList faces;
	faces.MakeExtrusion(profile, AuPoint(0, 0, 2 * halfWidth), AuMatrix4x4::IdentityMatrix(), 1, true);
	// From (u, h, w) to the design: u along 'dir', h up, w across.
	AuMatrix4x4 t;
	t.LoadIdentity();
	t.Translate(origin);
	if(!alongX)
		t.RotateAxis(M_PI/2, 0, 0, 1);
	if(sgn < 0)
		t.RotateAxis(M_PI, 0, 0, 1);
	t.RotateAxis(M_PI/2, 1, 0, 0);
	t.Translate(AuPoint(0, 0, -halfWidth));
	faces.Transform(t);
	OrientAsHull(faces);
	return faces;
}

bool AddDoorRamp(AuDesign& design, const AuPoint& slabMin, const AuPoint& slabMax, const AuPoint& outward,
		double surface, double groundZ, AuPoint& top, AuPoint& foot)
{
	const bool alongX = fabs(outward.x) > fabs(outward.y);
	const double sgn = alongX ? (outward.x > 0 ? 1. : -1.) : (outward.y > 0 ? 1. : -1.);
	const double z0 = slabMin.z;
	const double rise = z0 - groundZ;
	if(rise < .5 || rise > SHIPGEN_RAMP_MAX_RISE)
		return false;
	const double width = alongX ? slabMax.y - slabMin.y : slabMax.x - slabMin.x;
	const double across = alongX ? (slabMin.y + slabMax.y)/2 : (slabMin.x + slabMax.x)/2;

	// The side view, out from the hull surface ('u') and up from the floor
	// ('h'): a landing level with the floor across the threshold, then the
	// slope down to the ground, as at the door of the user's aircraft
	// factory (a level barrier, then a sloped one).
	const double u0 = -.2, uLanding = SHIPGEN_THRESHOLD_OUTSIDE + 1.0;
	const double u1 = uLanding + rise * SHIPGEN_RAMP_RUN_PER_RISE;
	const double thick = 1.0;
	AuPointArray profile;
	profile.append(AuPoint(u0, -thick, 0));
	profile.append(AuPoint(u1, -rise - thick, 0));
	profile.append(AuPoint(u1, -rise, 0));
	profile.append(AuPoint(uLanding, 0, 0));
	profile.append(AuPoint(u0, 0, 0));
	const AuPoint origin = alongX ? AuPoint(surface, across, z0) : AuPoint(across, surface, z0);
	auto ramp = [&](double halfWidth) { return ProfilePrism(profile, outward, origin, halfWidth); };

	// What is walked on (invisible), and what is seen, a little narrower.
	AuScBarrier barrier;
	barrier.AddFaces(ramp(width/2));
	barrier.SetGroupName("Hull Door");
	// The visible ramp is a hull part, not a mesh: the engine only looks for
	// barriers inside the extents of the hull parts (AuScParts::
	// IsPathBlocked), so with a mesh the part of the ramp beyond the hull
	// side was a ghost in the user's test. The hull void over it keeps it
	// from blocking anyone itself.
	AuScHull mesh;
	mesh.AddFaces(ramp(width/2 - .1));
	mesh.SetGroupName("Frame");
	if(!barrier.IsValid() || !mesh.IsValid())
		return false;
	if(!design.Parts().AddPart(barrier) || !design.Parts().AddPart(mesh))
		return false;

	// The hull does not stop anyone anywhere over the ramp.
	AuPoint vmn = slabMin, vmx = slabMax;
	const double a = surface + sgn * (SHIPGEN_THRESHOLD_OUTSIDE + .05), b = surface + sgn * (u1 + .5);
	if(alongX)
	{
		vmn.x = qMin(a, b);
		vmx.x = qMax(a, b);
		vmn.y -= SHIPGEN_HULL_VOID_MARGIN;
		vmx.y += SHIPGEN_HULL_VOID_MARGIN;
	}
	else
	{
		vmn.y = qMin(a, b);
		vmx.y = qMax(a, b);
		vmn.x -= SHIPGEN_HULL_VOID_MARGIN;
		vmx.x += SHIPGEN_HULL_VOID_MARGIN;
	}
	vmn.z = groundZ - 1.0;
	vmx.z += SHIPGEN_HULL_VOID_MARGIN;
	AuScHullVoid clear;
	clear.AddFaces(BoxFaces(vmn, vmx));
	clear.SetGroupName("Hull Door");
	if(!clear.IsValid() || !design.Parts().AddPart(clear))
		return false;

	top = alongX ? AuPoint(surface + sgn * uLanding, across, z0) : AuPoint(across, surface + sgn * uLanding, z0);
	foot = alongX ? AuPoint(surface + sgn * u1, across, groundZ) : AuPoint(across, surface + sgn * u1, groundZ);
	return true;
}

const char* LadderGroupPrefix()
{
	return "Ladder";
}

bool AddShaft(AuDesign& design, const QString& groupName, const AuPoint& mn, const AuPoint& mx, quint16& voidId)
{
	voidId = 0;
	AuScRoomVoid shaft;
	shaft.AddFaces(BoxFaces(mn, mx));
	SetUpRoomVoid(shaft, groupName, false);
	if(!shaft.IsValid())
		return false;
	voidId = design.Parts().AddPart(shaft);
	return voidId != 0;
}

bool AddVoid(AuDesign& design, const QString& groupName, const AuScFaceList& faces, quint16& voidId)
{
	voidId = 0;
	AuScFaceList voidFaces = faces;
	OrientAsVoid(voidFaces);
	AuScRoomVoid v;
	v.AddFaces(voidFaces);
	SetUpRoomVoid(v, groupName, false);
	if(!v.IsValid())
		return false;
	voidId = design.Parts().AddPart(v);
	return voidId != 0;
}

bool SetVoidGravity(AuDesign& design, quint16 voidId, GRAVITY gravity)
{
	AuScPart* part = voidId ? design.Parts().value(voidId) : 0;
	if(!part)
		return false;
	part->SetGravity(gravity, AuPoint(0, 0, 1));
	return part->GravityType() == gravity;
}

bool CutLadderOpening(AuDesign& design, const AuPoint& mn, const AuPoint& mx, quint16 lowerRoomId, quint16 upperRoomId)
{
	return CutOpening(design, BoxFaces(mn, mx), lowerRoomId, upperRoomId);
}

bool CutOpening(AuDesign& design, const AuScFaceList& jigFaces, quint16 lowerRoomId, quint16 upperRoomId)
{
	if(!lowerRoomId || !upperRoomId)
		return false;
	AuScFaceList jig = jigFaces;
	OrientAsHull(jig);
	QList<quint16> added;
	if(!JigCut(design, AuScPart::Part_CuttingJig, jig, lowerRoomId, upperRoomId, &added))
		return false;
	// The cutting jig keeps the cut-away plug as a part of its own; the
	// opening is what is wanted.
	AuScParts deleted;
	for(quint16 id : added)
		design.Parts().DeletePart(id, deleted);
	return true;
}

bool AddLadderMesh(AuDesign& design, const AuPoint& bottom, const AuPoint& top, const AuPoint& facing,
		const QString& groupName)
{
	const bool alongX = fabs(facing.x) > fabs(facing.y);
	const double sgn = alongX ? (facing.x > 0 ? 1. : -1.) : (facing.y > 0 ? 1. : -1.);
	const double ahead = .9, rail = .15, halfWidth = .75;
	const double z0 = bottom.z, z1 = top.z + 3.0;
	auto box = [&](double a0, double a1, double b0, double b1, double c0, double c1) {
		// 'a' is ahead of the climber, 'b' across, 'c' up.
		const double ax = alongX ? bottom.x : bottom.y, bx = alongX ? bottom.y : bottom.x;
		const AuPoint p0 = alongX ? AuPoint(ax + sgn * a0, bx + b0, c0) : AuPoint(bx + b0, ax + sgn * a0, c0);
		const AuPoint p1 = alongX ? AuPoint(ax + sgn * a1, bx + b1, c1) : AuPoint(bx + b1, ax + sgn * a1, c1);
		return BoxFaces(p0.Min(p1), p0.Max(p1));
	};
	AuScFaceList faces;
	faces.AddFaces(box(ahead, ahead + rail, -halfWidth - rail, -halfWidth, z0, z1));
	faces.AddFaces(box(ahead, ahead + rail, halfWidth, halfWidth + rail, z0, z1));
	for(double z = z0 + 1.0; z < z1 - .2; z += 1.0)
		faces.AddFaces(box(ahead, ahead + rail, -halfWidth, halfWidth, z, z + rail * .7));
	OrientAsHull(faces);
	AuScMesh ladder;
	ladder.AddFaces(faces);
	ladder.SetGroupName(groupName);
	if(!ladder.IsValid())
		return false;
	return design.Parts().AddPart(ladder) != 0;
}

bool AddStairFlight(AuDesign& design, const AuPoint& foot, const AuPoint& dir, double width, double run, double rise,
		const QString& groupName)
{
	if(run < 1 || rise < .5 || width < 1)
		return false;
	const bool alongX = fabs(dir.x) > fabs(dir.y);
	const double sgn = alongX ? (dir.x > 0 ? 1. : -1.) : (dir.y > 0 ? 1. : -1.);

	// What is walked on: an invisible slope from the foot to the top, and a
	// little way on over the floor above, as in the user's aircraft factory.
	const double thick = 1.0, onto = .5;
	AuPointArray slope;
	slope.append(AuPoint(0, -thick, 0));
	slope.append(AuPoint(run + onto, rise - thick, 0));
	slope.append(AuPoint(run + onto, rise, 0));
	slope.append(AuPoint(run, rise, 0));
	slope.append(AuPoint(0, 0, 0));
	AuScBarrier barrier;
	barrier.AddFaces(ProfilePrism(slope, dir, foot, width / 2));
	barrier.SetGroupName(groupName);

	// What is seen: the steps, each a block from the floor up, about 0.65 ft
	// high, its top half a step above the slope where it starts and half a
	// step below where it ends, so that the slope runs through the middle
	// of the steps.
	const int steps = qMax(2, int(ceil(rise / .65)));
	const double going = run / steps, step = rise / steps;
	const double halfWidth = width / 2 - .1;
	AuScFaceList faces;
	for(int i = 1; i <= steps; ++i)
	{
		const double u0 = (i - 1) * going, u1 = i * going, h = (i - .5) * step;
		AuPoint a = foot, b = foot;
		if(alongX)
		{
			a.x += sgn * u0;
			b.x += sgn * u1;
			a.y -= halfWidth;
			b.y += halfWidth;
		}
		else
		{
			a.y += sgn * u0;
			b.y += sgn * u1;
			a.x -= halfWidth;
			b.x += halfWidth;
		}
		b.z += h;
		faces.AddFaces(BoxFaces(a.Min(b), a.Max(b)));
	}
	OrientAsHull(faces);
	AuScMesh mesh;
	mesh.AddFaces(faces);
	mesh.SetGroupName(groupName);
	if(!barrier.IsValid() || !mesh.IsValid())
		return false;
	return design.Parts().AddPart(barrier) != 0 && design.Parts().AddPart(mesh) != 0;
}

const double kDeskWidth = 4.0, kDeskReach = 3.4, kDeskBehind = 1.2;

bool AddStationDesk(AuDesign& design, const AuPoint& floorLoc, double yaw, const QString& groupName,
		AuMatrix4x4& plane, AuPoint& corner1, AuPoint& corner2, const QColor& deskPaint, const QColor& stoolPaint,
		double topFeet, bool withStool)
{
	// From the side: a sloping panel on a post, out of the way of the knees. In
	// proportion to the stations of the Atlantis: its screen 1.4 ft to 4 ft ahead
	// of the seat, 5.25 ft wide, half as high as wide (this one is narrower, to
	// fit a bridge's rows). That one lies almost flat,
	// and from a seat is seen at a slant; this one leans back to face the seated
	// eye, and stays below it.
	const double gap = 1.7, depth = 1.5, rise = 1.4, low = topFeet - rise, thick = .12, knee = .7;
	AuPointArray profile;
	profile.append(AuPoint(gap + knee, 0, 0));
	profile.append(AuPoint(gap + depth, 0, 0));
	profile.append(AuPoint(gap + depth, low - thick * .6 + rise * (depth - thick) / depth, 0));
	profile.append(AuPoint(gap + depth + thick, low - thick * .6 + rise, 0));
	profile.append(AuPoint(gap + depth, low + rise, 0));
	profile.append(AuPoint(gap, low, 0));
	profile.append(AuPoint(gap + thick, low - thick * .6, 0));
	profile.append(AuPoint(gap + knee, low - thick * .6 + rise * (knee - thick) / depth, 0));
	AuScFaceList desk;
	desk.MakeExtrusion(profile, AuPoint(0, 0, kDeskWidth), AuMatrix4x4::IdentityMatrix(), 1, true);
	AuMatrix4x4 t;
	t.LoadIdentity();
	t.Translate(floorLoc);
	t.RotateAxis(yaw, 0, 0, 1);
	AuMatrix4x4 standUp(t);
	standUp.RotateAxis(M_PI/2, 1, 0, 0);
	standUp.Translate(AuPoint(0, 0, -kDeskWidth / 2));
	desk.Transform(standUp);
	AuScFaceList stool = BoxFaces(AuPoint(-.3, -.3, 0), AuPoint(.3, .3, AuStation::SeatHeight() - .1));
	stool.Transform(t);
	OrientAsHull(desk);
	OrientAsHull(stool);
	auto paint = [](AuScFaceList& faces, const QColor& colour) {
		AuScSelectInfo all;
		for(int i = 0; i < faces.size(); ++i)
			all.m_Faces.insert(i);
		faces.TextureUnwrap(all, quint8(kSlotWall), 6.0, AuScFaceList::UNWRAP_ByFace, 45.f, true, 0, AuMatrix4x4::IdentityMatrix());
		for(AuScFace* f : faces)
		{
			f->SetTextureIndex(quint8(kSlotWall));
			f->SetPaint(colour);
		}
	};
	paint(desk, deskPaint);
	paint(stool, stoolPaint);
	AuScFaceList faces;
	faces.AddFaces(desk);
	if(withStool)
		faces.AddFaces(stool);
	AuScMesh mesh;
	mesh.AddFaces(faces);
	mesh.SetGroupName(groupName);
	if(!mesh.IsValid() || !design.Parts().AddPart(mesh))
		return false;

	// The screen lies on the slope, a little clear of it. The plane's x runs to
	// the operator's right and its y up the slope: MakeScreen puts the image's
	// lower left at the lowest x and y, so that way it reads the right way round.
	// (Every station of the Atlantis has its lower left corner on the operator's
	// left. This had it on the right, and showed the image mirrored.)
	const AuPoint f(cos(yaw), sin(yaw), 0);
	const double slope = atan2(rise, depth), along = sqrt(rise * rise + depth * depth);
	const AuPoint right(f.y, -f.x, 0);
	const AuPoint up = f * cos(slope) + AuPoint(0, 0, 1) * sin(slope);
	const AuPoint middle = floorLoc + f * (gap + depth / 2) + AuPoint(0, 0, low + rise / 2)
			+ right.Cross(up) * .08;
	plane = AuMatrix4x4(middle, right, up);
	corner1 = plane.Project(AuPoint(-1.7, -(along / 2 - .08), 0));
	corner2 = plane.Project(AuPoint(1.7, along / 2 - .08, 0));
	return true;
}

bool AddRail(AuDesign& design, const AuPoint& from, const AuPoint& to, const QString& groupName)
{
	const double height = 3.5, bar = .15;
	const AuPoint along = to - from;
	const double length = along.Length();
	if(length < .5)
		return false;
	const bool alongX = fabs(along.x) > fabs(along.y);
	auto box = [&](const AuPoint& a, const AuPoint& b, double half, double z0, double z1) {
		AuPoint mn = a.Min(b), mx = a.Max(b);
		if(alongX)
		{
			mn.y -= half;
			mx.y += half;
		}
		else
		{
			mn.x -= half;
			mx.x += half;
		}
		mn.z = from.z + z0;
		mx.z = from.z + z1;
		return BoxFaces(mn, mx);
	};

	// It stops anyone walking through it,
	AuScFaceList wall = box(from, to, .15, 0, height);
	OrientAsHull(wall);
	AuScBarrier barrier;
	barrier.AddFaces(wall);
	barrier.SetGroupName(groupName);

	// and looks like a rail on posts.
	AuScFaceList faces = box(from, to, bar / 2, height - bar, height);
	const int posts = qMax(2, int(ceil(length / 4.0)) + 1);
	const AuPoint unit = along / length;
	for(int i = 0; i < posts; ++i)
	{
		const AuPoint at = from + unit * (bar / 2 + (length - bar) * i / (posts - 1));
		faces.AddFaces(box(at - unit * (bar / 2), at + unit * (bar / 2), bar / 2, 0, height - bar));
	}
	OrientAsHull(faces);
	AuScMesh mesh;
	mesh.AddFaces(faces);
	mesh.SetGroupName(groupName);
	if(!barrier.IsValid() || !mesh.IsValid())
		return false;
	return design.Parts().AddPart(barrier) != 0 && design.Parts().AddPart(mesh) != 0;
}

bool AddLadderPath(AuDesign& design, const AuPoint& bottom, const AuPoint& top, const AuPoint& facing)
{
	AuScPathLadder ladder;
	ladder.MakePath(bottom, top);
	ladder.SetFacing(facing);
	if(!ladder.IsValid())
		return false;
	return design.Parts().AddPart(ladder) != 0;
}

bool AddWalkPath(AuDesign& design, const AuPoint& from, const AuPoint& to, quint16 doorId)
{
	if((to - from).Length() < 0.5)
		return false;
	AuScPathWalk path;
	path.MakePath(from, to);
	if(doorId)
		path.SetAssociatedPart(doorId, AuScDoor::Open);
	if(!path.IsValid())
		return false;
	return design.Parts().AddPart(path) != 0;
}
