//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - a primitive base shape.
//
// A building does not always begin with a model. It can begin with a simple
// solid, and the models are then put on it, one after another. The same works
// for a ship that is put together out of several models.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_BASE_H
#define SHIPGEN_BASE_H

#include <QString>
#include <QStringList>

#include "auscface.h"

// The outline of the primitive base.
enum class BaseShapeKind
{
	None,		// No base; the first model is the anchor.
	Rectangle,
	Circle,
	Hexagon
};

// A solid base, standing on the ground with its middle at the origin.
struct BaseShape
{
	BaseShapeKind	kind = BaseShapeKind::None;
	double			width = 140.;		// Feet, across X.
	double			depth = 100.;		// Feet, across Y.
	double			height = 4.;		// Feet.
	// Put every model on the base as its own object, in a grid. When this is
	// off the files are treated as parts of one model and are put on the
	// base together, the way they were before.
	bool			separateModels = true;

	bool	IsSet() const { return kind != BaseShapeKind::None; }

	// The name the command line and the brief files use.
	QString	Key() const;
	QString	Label() const;
};

// The keys and labels that are on offer.
QStringList	BaseShapeKeys();
QStringList	BaseShapeLabels();

// Finds a shape by its key. An empty key or "none" gives the no-base shape.
bool	FindBaseShape(const QString& key, BaseShapeKind& out);

// Makes a closed mesh of the base. Returns an empty list when there is no
// base. The mesh has its bottom at z = 0 and is centred on the origin.
AuScTriList	MakeBaseMesh(const BaseShape& base);

#endif // SHIPGEN_BASE_H
