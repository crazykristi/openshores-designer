//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - changing the shape of a model.
//
// A model that comes from a 3D print site is one shape. The same model can be
// made into a different shape by moving its vertices around, which keeps the
// material and the look of the original while changing the silhouette.
//
// Every operation here works on the triangles of the model, before the model
// is thinned out.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_SHAPE_H
#define SHIPGEN_SHAPE_H

#include <QList>
#include <QString>
#include <QStringList>

#include "auscface.h"

// What to do to a model.
struct ModelShape
{
	// The cross section is scaled with the height. The bottom is the scale at
	// the bottom of the model and the top is the scale at the top.
	double	taperBottom = 1.0;
	double	taperTop = 1.0;

	// The model is turned about its vertical axis as it goes up, in degrees
	// over the whole height.
	double	twist = 0.0;

	// Copies of the model placed around a circle. The distance is a fraction
	// of the width of the model.
	int		repeats = 1;
	double	repeatRadius = 0.0;

	// Copies of the model stacked on top of each other, each one smaller.
	int		stack = 1;
	double	stackShrink = 0.8;

	// The surface is pushed out and in by noise. The amount is a fraction of
	// the size of the model and the scale is how big the lumps are.
	double	noise = 0.0;
	double	noiseScale = 0.15;
	int		seed = 1;

	// The model is stretched on the vertical axis.
	double	stretch = 1.0;

	// Is there anything to do at all?
	bool	AnyChange() const
	{
		return taperBottom != 1.0 || taperTop != 1.0 || twist != 0.0 ||
				repeats > 1 || stack > 1 || noise != 0.0 || stretch != 1.0;
	}

	// A short description, for the tool to print.
	QString	Description() const;
};

// Applies the shape to a model that is in several parts. The parts keep their
// own identity, so a model that is in three files still gives three hull parts.
QList<AuScTriList> ApplyModelShape(const QList<AuScTriList>& parts, const ModelShape& shape);

// A named look that the shape can be put into.
struct ShapePreset
{
	QString		key;		// What the command line calls it.
	QString		label;		// What the user sees.
	ModelShape	shape;
};

// The looks that are on offer.
QList<ShapePreset>	ShapePresets();

// Finds a preset by its key. Returns false when there is no such preset.
bool	FindShapePreset(const QString& key, ShapePreset& out);

// A shape that is a bit different from the one given, so that a set of
// blueprints can be made that are all the same kind of thing but not quite the
// same thing. The same index always gives the same variation.
ModelShape	VaryShape(const ModelShape& base, int index);

#endif // SHIPGEN_SHAPE_H
