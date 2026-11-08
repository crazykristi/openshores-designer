//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - loading models and thinning them out.
//
// The engine can read 3ds and stl files but not obj, so there is a reader for
// obj here. On top of that there are two ways of taking a model down to a
// given number of faces:
//
//   - Quadric error metrics. The edges are collapsed one at a time, cheapest
//     first, and each vertex is put where the surface it belongs to wants it
//     to be. This keeps the shape as well as it can be kept.
//
//   - Clustering. The vertices are snapped onto a grid and the triangles that
//     collapse are dropped. This is quick, but the surface goes blocky and the
//     details go flat.
//
// Both work on the triangles of a model, before it becomes part of a design.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_MESH_H
#define SHIPGEN_MESH_H

#include <QList>
#include <QString>
#include <QStringList>

#include "auscface.h"

// The ways a model can be thinned out.
enum class SimplifyMethod
{
	Quadric,		// Keeps the shape. Slow.
	Cluster			// Quick, but the surface goes blocky.
};

// Reads a model from a 3ds, stl or obj file, and, when the program was built
// with the Assimp library, FBX, glTF, Collada (dae), PLY and the other
// formats in ModelSuffixes().
bool	AssimpAvailable();		// Built with Assimp (FBX, glTF, ...).
QStringList	ModelSuffixes();	// The model files LoadMeshFile reads, lower case.
QString	ModelFileFilter();	// For a file dialog: "3D models (*.stl *.obj ...)".
bool	LoadMeshFile(const QString& fileName, AuScTriList& tris, QString& error);

// Writes a model out as a binary stl or an obj file. The format follows the
// name of the file.
bool	SaveMeshFile(const QString& fileName, const AuScTriList& tris, QString& error);

// Thins a model out to the number of faces asked for. The method says how.
AuScTriList	SimplifyMesh(const AuScTriList& tris, int targetFaces, SimplifyMethod method);

// The clustering method on its own, because it needs a grid size rather than
// a number of faces.
AuScTriList	ClusterMesh(const AuScTriList& tris, double tolerance);

// The grid size that gives about the number of faces asked for.
double	ClusterToleranceFor(const AuScTriList& tris, int targetFaces);

// Puts the vertices that are in the same place together, so that a model
// which has three separate vertices for every triangle can still be thinned
// out. The triangles come back with their own corners again.
AuScTriList	WeldMesh(const AuScTriList& tris, double tolerance);

// Puts the corners that are in the same place onto the same point.
//
// This is different from welding, which throws the corners away. This one
// keeps every triangle but moves its corners, so that a model which is
// written out and read again comes back exactly the same. Without it, a
// lid that is put over a hole does not quite meet the model it is on, and
// the model comes back with holes in it.
AuScTriList	SnapMesh(const AuScTriList& tris, double tolerance);

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---|---|
// Tidying a model.
//
// Models that come from the internet are often in a mess: they have
// triangles with no area, the same triangle twice, and faces that wind the
// wrong way round.

// What is wrong with a model.
struct MeshReport
{
	int		degenerate = 0;		// Triangles with no area.
	int		duplicate = 0;		// The same triangle more than once.
	int		loose = 0;			// Edges that are used by only one triangle.
	int		edges = 0;			// How many edges there are.
	bool	windingsAgree = true;	// Whether the triangles all wind the same way.
	int		triangles = 0;
	int		vertices = 0;
};

// Looks at a model and says what is wrong with it.
MeshReport	InspectMesh(const AuScTriList& tris);

// Takes away the triangles that have no area and the ones that are there
// twice.
AuScTriList	CleanMesh(const AuScTriList& tris);

// Turns the triangles that wind the wrong way round the other way round, so
// that the model is solid from the outside.
AuScTriList	FixWindings(const AuScTriList& tris);

// Closes the holes in a model.
//
// A model that has holes in it cannot be measured, and the game cannot tell
// what is inside it, so the volume comes out wrong. Every hole is closed
// with a lid of triangles.
//
// Where two sheets of the surface meet at one corner, the hole cannot be
// closed cleanly. Those corners are joined up as well, so the model comes
// out as closed as it can be.
AuScTriList	FillHoles(const AuScTriList& tris);

// Everything the mesh tool's "Tidying up" does, in its order: takes away
// the triangles with no area and the doubles, turns the ones that wind the
// wrong way round, and closes the holes. What it did is added to 'done' (one
// line per step that changed anything), for telling the user.
AuScTriList	RepairMesh(const AuScTriList& tris, QStringList* done = 0);

// Puts the model in the place it belongs: the bottom on the ground and the
// middle in the middle.
AuScTriList	GroundMesh(const AuScTriList& tris);

// Mirrors the model. The axis is 0 for x, 1 for y and 2 for z.
AuScTriList	MirrorMesh(const AuScTriList& tris, int axis);

// Gives every corner the average of the normals of the triangles around it,
// which makes the model look smooth rather than faceted.
AuScTriList	SmoothMesh(const AuScTriList& tris);

// The size of a model.
void	MeshExtents(const AuScTriList& tris, AuPoint& min, AuPoint& max);

// Whether an STL file is binary: its size is 84 bytes plus 50 for each
// triangle its header counts. (The engine's ASCII reader does not refuse a
// binary file: a header such as "STLB ATF ... COLOR=" read as a few bad
// triangles.)
bool	IsBinaryStl(const QString& fileName);
// Reads a binary STL file, every triangle 50 bytes. The engine's own reader
// (AuScFaceList::STLReadBinary) skips as many bytes as a triangle's
// attribute says, which in files that keep a colour there loses its place.
// 0 when read, 1 when it cannot be opened, 2 when it has no triangles.
int		ReadBinaryStl(const QString& fileName, AuScFaceList& faces);

#endif // SHIPGEN_MESH_H
