//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - exact containment checks against the hull triangles.
//
// Room placement used to work on the engine's rasterised silhouettes, which
// are quick but approximate: a point can be inside all three projections and
// still be in the open air of a concave or split hull. The solid mesh here
// uses the triangles themselves, with a bounding volume hierarchy so that the
// test is fast enough to run while a room is being fitted.
//
// A mesh can only answer "inside" when it is closed: every edge used exactly
// twice, once in each direction. AuditSolidity says whether that is so.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_AUDIT_H
#define SHIPGEN_AUDIT_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

#include "auscface.h"

class AuScPart;
class AuScParts;

// Whether a triangle mesh encloses a volume.
struct MeshSolidity
{
	int		triangles = 0;
	int		vertices = 0;
	int		openEdges = 0;			// Edges used by one triangle.
	int		thickEdges = 0;			// Edges used by three or more triangles.
	int		foldedEdges = 0;		// Two triangles that go round the same way.
	int		nonManifoldEdges = 0;	// Thick or folded.
	// Thick edges where the surface only pinches: an even number of
	// triangles, half going round the edge each way, as where two solids
	// touch along an edge. The inside is still well defined there.
	int		pinchedEdges = 0;
	// Doors and windows in the design. Each leaves a ring of open edges, more
	// on a hull of a few big triangles, so they raise the allowance below.
	int		openings = 0;

	bool	Closed() const
	{
		return triangles > 0 && openEdges == 0 && foldedEdges == 0 && thickEdges == pinchedEdges;
	}

	// Closed but for a few openings: the holes the game's designer (and
	// ours) cuts for doors and windows, and the cracks its slicing leaves.
	// At most 8% of the edges are open, or 40 per opening when that is more,
	// and none folded. The inside is
	// still clear away from the holes, so rooms can be tested by a vote of
	// rays (SolidMesh::Contains).
	bool	ClosedButForOpenings() const
	{
		return triangles > 0 && !Closed() && foldedEdges == 0 &&
				openEdges <= qMax(qMax(64, openings * 40), triangles * 3 / 2 * 8 / 100) && thickEdges - pinchedEdges <= openEdges;
	}
};

// Measures the solidity of a mesh.
MeshSolidity AuditSolidity(const AuScTriList& triangles);

// A closed triangle mesh with a bounding volume hierarchy, for exact
// containment tests. Building one costs a sort; each test after that is
// logarithmic in the number of triangles.
class SolidMesh
{
public:
	SolidMesh() {}
	explicit SolidMesh(const AuScTriList& triangles);

	bool	IsValid() const { return !m_triangles.isEmpty(); }
	bool	IsClosed() const { return m_solidity.Closed(); }
	// Closed, or closed but for door and window openings: an inside can be told.
	bool	HasInside() const { return m_solidity.Closed() || m_solidity.ClosedButForOpenings(); }
	const MeshSolidity& Solidity() const { return m_solidity; }
	void	SetOpenings(int openings) { m_solidity.openings = openings; }

	// Exact point-in-mesh test. A ray is cast and the crossings counted;
	// `onSurface` reports a point that lies on the surface, where the answer
	// means nothing.
	bool	Contains(const AuPoint& point, bool* onSurface = 0) const;

	// True when the whole box is inside. The 27 sample points have to be
	// inside and no triangle may pass through the box.
	bool	BoxInside(const AuPoint& mn, const AuPoint& mx) const;

	// The test itself, without the allowance for openings.
	bool	BoxInsideSampled(const AuPoint& mn, const AuPoint& mx) const;

	// Counts how many triangles the mesh has.
	int		TriangleCount() const { return m_triangles.size(); }

private:
	struct Triangle
	{
		AuPoint	a;
		AuPoint	b;
		AuPoint	c;
		AuPoint	centroid;
	};

	struct Node
	{
		AuPoint	mn;
		AuPoint	mx;
		int		left = -1;
		int		right = -1;
		int		start = 0;
		int		count = 0;
	};

	int		BuildNode(int start, int count);
	void	CollectTriangles(int node, const AuPoint& mn, const AuPoint& mx, QVector<int>& out) const;
	int		CountCrossings(const AuPoint& origin, const AuPoint& direction, bool& surface) const;
	bool	BoxHitsSurface(const AuPoint& mn, const AuPoint& mx) const;

	QVector<Triangle>	m_triangles;
	QVector<Node>		m_nodes;
	MeshSolidity		m_solidity;
};

// One hull part and how solid it is.
struct HullSolidity
{
	quint16			id = 0;
	MeshSolidity	mesh;
};

// One room part and where it ended up.
struct RoomContainment
{
	quint16		partId = 0;
	QString		type;
	quint16		hullId = 0;			// The hull part that holds it, or 0.
	bool		checked = false;	// False when no hull is closed enough to tell.
	bool		inside = false;
	bool		throughOpenings = false;	// Decided in a hull that has door and window openings.
	QString		note;
};

// Audits every hull part and every room part of a design. This does not use
// the engine's obstruction model or its silhouettes; it is independent of
// the placement code.
void	AuditDesign(const AuScParts& parts, QList<HullSolidity>& hulls,
		QList<RoomContainment>& rooms, QStringList& notes);

// A part that reaches out of the hull: how many of its corners are outside,
// how far the worst one is, and where they are.
struct StickOut
{
	quint16		partId = 0;
	QString		type;
	QString		group;
	int			corners = 0;
	int			outside = 0;
	double		worst = 0;			// Feet past the hull, roughly (along the way to the part's middle).
	AuPoint		mn;					// The box of the corners outside.
	AuPoint		mx;
};

// Every part that should be inside the hull (rooms, their voids, frames and
// linings, furniture, stations, berths, lifts) and has corners outside it.
// Hull doors, doorways, windows, paths and things on the hull's surface
// (emitters, flags, decals) reach out on purpose and are not looked at.
// 'tolerance' is how far past the surface a corner may be before it counts.
QList<StickOut> AuditStickOut(const AuScParts& parts, double tolerance = .15);

// The hull as one solid: every hull part but the frames, with the window
// glass, which is the hull's own surface taken out of it.
SolidMesh	HullSolidOf(const AuScParts& parts);

// A part that is meant to be outside the hull: the threshold and the ramp of
// a hull door (frames at or below the door's floor, next to it).
bool		OutsideOnPurpose(const AuScParts& parts, const AuScPart* part);

// Moves 'p' into the hull along the shortest of 'directions' (unit
// vectors), and 'margin' further where that is still inside. False, with
// 'p' as it was, when none gets there within 'reach'.
bool		PullPointInside(const SolidMesh& hull, AuPoint& p, const QList<AuPoint>& directions,
				double reach, double margin);

// Moves every corner of the frames and linings (hull parts in the group
// "Frame") that is outside the hull back into it, the shortest way. Returns
// how many corners were moved; 'stuck' counts those that could not be.
int			PullFramesInside(AuScParts& parts, double margin = .05, int* stuck = 0);

#endif // SHIPGEN_AUDIT_H
