/*
 * proctex.h
 *
 * Procedural textures: images that tile without a seam, made from a kind,
 * up to three colours, a few numbers and a seed, so the same recipe always
 * gives the same image. A plan keeps only the recipe, written in a paint's
 * texture file as "proc:<kind>;colour=#rrggbb;colour2=...;wear=0.3;seed=7"
 * (LoadPaintImage makes the image from it), so plans stay small and need no
 * image files.
 *
 * With the "facade" wrap, one repeat is one deck high, starting at the deck
 * floors: the kinds made for walls (windows, strip_windows, neon_strip) draw
 * one storey per image, its floor at the bottom.
 */
#ifndef SHIPGEN_PROCTEX_H
#define SHIPGEN_PROCTEX_H

#include <QColor>
#include <QImage>
#include <QString>
#include <QStringList>

struct ProcTex
{
	QString	kind;				// ProcTexKinds().
	QColor	colour;				// The main colour (brick, concrete, the wall between windows).
	QColor	colour2;			// Joints, seams, frames, mortar.
	QColor	colour3;			// Glass, light, the accent.
	double	wear = .2;			// 0 clean to 1 grimy: stains, rain streaks, uneven tone.
	double	density = 1;		// More (above 1) or fewer (below 1) bricks, panels, windows per repeat.
	double	lit = 0;			// windows, strip_windows: the share of windows lit from inside, 0 to 1.
	int		seed = 1;
};

QStringList	ProcTexKinds();
QString		ProcTexKindHelp(const QString& kind);	// One line: what it is and what its colours are.
// A kind with its own colours (such as red brick and pale mortar); false for
// a kind there is not.
bool		DefaultProcTex(const QString& kind, ProcTex& tex);

bool		IsProcTexSpec(const QString& text);		// Starts with "proc:".
bool		ParseProcTex(const QString& spec, ProcTex& tex, QString& error);
QString		ProcTexSpec(const ProcTex& tex);
QString		ProcTexImageName(const ProcTex& tex);	// "brick-7.png": the name the blueprint keeps.

QImage		MakeProcTex(const ProcTex& tex, int size = 256);

#endif
