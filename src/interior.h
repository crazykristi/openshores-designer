//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the parts that make an interior walkable: room voids, doors,
// doorway voids, hull voids and walk paths.
//
// How the engine reads them (audata/auscparts.cpp, AuScParts::ListRoomNames,
// VoidContainingLocConstant and VoidContainingLocVariable):
//
//  * A void with no associated part is constant. It is always there, and the
//    constant room voids are what the game lists as the rooms.
//  * A void associated with a part state "comes and goes": it only exists
//    while that door is in that state. ListRoomNames skips it.
//
// So a room's own void must never be associated with a door, or the room is
// only there while the door is open. Each door gets a small doorway void of
// its own across the wall, associated with its open state; that is what the
// report asks for ("Associate a room void with its open state") and it joins
// the two room voids only while the door is open. A hull door gets a doorway
// void from the room to the outside and a hull void that cuts the opening
// through the hull, both associated with its open state.
//
// The hull void is made generous on purpose. The White Star's openings are
// cut tight to the door, and players get stuck in them.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_INTERIOR_H
#define SHIPGEN_INTERIOR_H

#include <QColor>
#include <QtNumeric>
#include <QString>

#include "auscface.h"
#include "aupoint.h"
#include "audef.h"

class AuDesign;
class AuScPart;

// How far a doorway void reaches into the room on each side of the wall.
#define SHIPGEN_DOORWAY_REACH 2.0
// How far below the bottom of the hull the ladder from a hull door reaches,
// as in the working designs (2 to 3 ft): the ground under a landed ship.
#define SHIPGEN_LADDER_BELOW_HULL 2.5
// How far outside the hull surface that ladder stands. It has to be inside
// the doorway's voids, which reach SHIPGEN_DOORWAY_OUTSIDE beyond the hull:
// with the ladder 3 ft out, past their end, the user had to jump to get in.
#define SHIPGEN_LADDER_OUTSIDE 0.75
#define SHIPGEN_DOORWAY_OUTSIDE 1.5
// How far beyond the hull surface the floor of a hull doorway reaches. The
// constant hull void outside the door starts just beyond it, and the ladder
// stands beyond that.
#define SHIPGEN_THRESHOLD_OUTSIDE 0.35
// How much bigger than the door the hull opening is, on every side but the floor.
#define SHIPGEN_HULL_VOID_MARGIN 1.5
// A ramp down from a hull door runs this far out for each foot it falls
// (about 34 degrees; the user's aircraft factory has 45), and is only put
// where the door is no higher than this above the ground; higher doors get
// a ladder.
#define SHIPGEN_RAMP_RUN_PER_RISE 1.5
#define SHIPGEN_RAMP_MAX_RISE 15.0
// The foot of the ramp is this far below the bottom of the hull. The ramp is
// a hull part, so the ship rests on its foot too; level with the bottom.
#define SHIPGEN_RAMP_BELOW_HULL 0.0
// How far past the hull surface the hull opening goes.
#define SHIPGEN_HULL_VOID_OUTSIDE 4.0

// A closed box. Its faces wind the way the game wants voids.
AuScFaceList BoxFaces(const AuPoint& mn, const AuPoint& mx);

// Which way the game wants faces to wind, found by comparing with blueprints
// that work in the game: measured as the right-handed signed volume, hulls
// and voids are negative (visible from outside) and room meshes positive
// (visible from inside). A hull wound the other way is see-through from
// outside.
double SignedVolume(const AuScFaceList& faces);
// Reverses the faces if needed, so that the signed volume is positive or not.
void OrientFaces(AuScFaceList& faces, bool positive);
inline void OrientAsHull(AuScFaceList& faces)	{ OrientFaces(faces, false); }
inline void OrientAsVoid(AuScFaceList& faces)	{ OrientFaces(faces, false); }
inline void OrientAsRoom(AuScFaceList& faces)	{ OrientFaces(faces, true); }

// Cuts an opening the way the game's designer does (AuScParts::JigCutParts):
// a jig of type 'jigType' (Part_DoorJig, Part_WindowJig, ...) made of
// 'jigFaces' is cut through the hull part 'hullId' and, if not 0, the room
// mesh 'roomId'. The hole is cut in both and lined; a door jig turns the
// cut-away pieces into a door, a window jig into a window pane. The jig is
// removed afterwards. 'added' gets the ids of the new parts.
//
// With 'axis' (0 for an opening through a wall at a constant x, 1 for y) the
// lining the engine makes in the hull, which comes out as overlapping
// fragments on a curved hull, is taken out and a clean frame of four thin
// boxes (three without 'frameBottom', for a door) is put round the opening
// as a small hull part, with the look of the hull there. With 'sleeveTo'
// (a coordinate along the axis, usually the room wall) the frame reaches back
// to it, lining the tunnel when the room is some way inside the hull.
bool JigCut(AuDesign& design, int jigType, const AuScFaceList& jigFaces, quint16 hullId, quint16 roomId,
		QList<quint16>* added = 0, int axis = -1, bool frameBottom = true, double sleeveTo = qQNaN());

// The room's constant void and its visible mesh, both a box.
// Buildings stand on a world and use its natural gravity; ships make their own.
// The floor is metal tiles, the walls panels painted 'wallTint' (RoomTint
// of the name when it is not given), the ceiling light plates.
bool AddBoxRoom(AuDesign& design, const QString& groupName, const AuPoint& mn, const AuPoint& mx,
		quint16& voidId, quint16& meshId, bool naturalGravity = false, const QColor& wallTint = QColor());

// The room's constant void and its visible mesh from a closed face list
// (any shape), wound either way: each part is oriented the way the game
// wants it.
bool AddMeshRoom(AuDesign& design, const QString& groupName, const AuScFaceList& faces,
		quint16& voidId, quint16& meshId, bool naturalGravity = false, const QColor& wallTint = QColor());

// A light wall colour for a room, by the words in its name (bridge, engine,
// quarters and so on), so that rooms can be told apart.
QColor RoomTint(const QString& roomName);

// Paints a door so that it stands out from the walls: hull doors yellow,
// interior doors blue, in every state of the door.
void StyleDoor(AuScPart* door, bool hullDoor);

// A door between two rooms. 'slabMin'/'slabMax' is the door slab, standing
// across the wall; 'normalAxis' is 0 when the wall is at a constant x and 1
// at a constant y. A doorway void associated with the door's open state is
// added across the wall, in the room 'groupName'.
// With both room mesh ids, the opening is cut through the two walls the way
// the game's designer does it, and the door is made of the cut-away pieces.
bool AddInteriorDoor(AuDesign& design, const AuPoint& slabMin, const AuPoint& slabMax, int normalAxis,
		const QString& groupName, quint16& doorId, quint16 roomMeshA = 0, quint16 roomMeshB = 0);

// A door out through the hull. The slab is at the hull surface; 'outward' is
// the unit direction out of the ship (along x or y); 'roomWall' is where the
// room's wall is along that axis and 'surface' where the hull surface is.
//
// With 'roomMeshId' set, the opening is cut the way the game's designer cuts
// it (a door jig through the room wall and the hull part at the door, see
// JigCut): the hull gets a real hole and the door is made of the cut-away
// pieces. Without it, or if the cut fails, the door is a box slab.
//
// Besides the doorway void and the hull void tied to the open state, a
// constant hull void clears the outside in front of the door from
// 'clearBelowZ' up to above the door. Working designs have one; the White
// Star does not, and players get stuck at its door. It only has to reach
// down as far as the hull is in the way of the ladder (the caller measures
// that); left out, it covers the door's own height.
bool AddHullDoor(AuDesign& design, const AuPoint& slabMin, const AuPoint& slabMax, const AuPoint& outward,
		double roomWall, double surface, const QString& groupName, quint16& doorId,
		quint16 roomMeshId = 0, double clearBelowZ = 1e30);

// A fixed ramp from a hull door's floor down to 'groundZ': an invisible
// Barrier that is walked on (a level landing over the threshold, then the
// slope), a hull part in the "Frame" group that shows it (a hull part so
// that the engine looks for the barrier out there at all), and a hull void
// over it. 'top' and 'foot' are
// where a walk path down it goes. False, and nothing added, if the door is
// too high (SHIPGEN_RAMP_MAX_RISE) or too low for one.
bool AddDoorRamp(AuDesign& design, const AuPoint& slabMin, const AuPoint& slabMax, const AuPoint& outward,
		double surface, double groundZ, AuPoint& top, AuPoint& foot);

// The hull part nearest to a point (0 if there is none).
quint16 HullPartAt(const AuDesign& design, const AuPoint& p);

// A shaft between two decks: a constant void that joins the room below to
// the room above through the floor. Its group name starts with
// LadderGroupPrefix(), which is how inspect knows it for a connector and
// not a room of its own.
const char* LadderGroupPrefix();
bool AddShaft(AuDesign& design, const QString& groupName, const AuPoint& mn, const AuPoint& mx, quint16& voidId);
// A room void of any closed shape, with no mesh to see.
bool AddVoid(AuDesign& design, const QString& groupName, const AuScFaceList& faces, quint16& voidId);
// Sets the gravity of a room void (G_Zero for a zero-G shaft).
bool SetVoidGravity(AuDesign& design, quint16 voidId, GRAVITY gravity);

// A ladder path from 'bottom' up to 'top'; 'facing' is the way a climber faces.
bool AddLadderPath(AuDesign& design, const AuPoint& bottom, const AuPoint& top, const AuPoint& facing);

// What a ladder between decks looks like, beside its path: the opening is
// cut through the lower room's ceiling and the upper room's floor with the
// engine's cutting jig (the box 'mn'..'mx', usually the shaft), and a ladder
// of two rails and rungs is put in front of the climber, from 'bottom' to
// 3 ft above 'top' (a handhold), facing along 'facing' (X or Y).
bool CutLadderOpening(AuDesign& design, const AuPoint& mn, const AuPoint& mx, quint16 lowerRoomId, quint16 upperRoomId);
// The same through a closed jig of any shape.
bool CutOpening(AuDesign& design, const AuScFaceList& jig, quint16 lowerRoomId, quint16 upperRoomId);
bool AddLadderMesh(AuDesign& design, const AuPoint& bottom, const AuPoint& top, const AuPoint& facing,
		const QString& groupName);

// A flight of stairs, as in the user's aircraft factory: an invisible
// sloped Barrier that is walked on, from 'foot' (the middle of the bottom
// step's front edge, on the lower floor) up 'rise' over 'run' along 'dir'
// (X or Y), 'width' wide, and steps (a mesh) that show it. The opening
// above and the void through it are the caller's (CutLadderOpening,
// AddShaft), as for a ladder.
bool AddStairFlight(AuDesign& design, const AuPoint& foot, const AuPoint& dir, double width, double run, double rise,
		const QString& groupName);

// A rail on a floor from 'from' to 'to' (along X or Y, at the floor's
// height): an invisible Barrier that nobody walks through, and a rail on
// posts to see, 3.5 ft high. Round the opening of a flight of stairs.
bool AddRail(AuDesign& design, const AuPoint& from, const AuPoint& to, const QString& groupName);

// A walk path segment. With a door id it is only walkable while that door is open.
bool AddWalkPath(AuDesign& design, const AuPoint& from, const AuPoint& to, quint16 doorId = 0);

#endif
