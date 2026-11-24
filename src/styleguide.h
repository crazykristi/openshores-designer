/*
 * styleguide.h
 *
 * Building shapes and visual styles for the AI's style_guide tool: what a
 * kind of building looks like in numbers the plan tools take (decks, cells),
 * the game's template closest to it, how to draw it with the tools, and a
 * palette per style as set_paint arguments.
 */
#ifndef SHIPGEN_STYLEGUIDE_H
#define SHIPGEN_STYLEGUIDE_H

#include <QString>
#include <QStringList>

QStringList	StyleGuideShapes();		// The shape keys, such as "Office_tower".
QStringList	StyleGuideStyles();		// The style keys, such as "Cozy_neon".

// The guide: with neither, the list of shapes and styles; with a shape, its
// numbers, template and recipe; with a style, its palette and how to lay it
// on. Empty 'error' when it worked.
QString		StyleGuideText(const QString& shape, const QString& style, QString& error);

#endif
