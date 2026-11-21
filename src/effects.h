//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the game's animated surfaces (surface effects): when a design can
// run each one, and which frames the game shows for it.
//
// The game animates a surface effect only while what it is named after works
// (DaSc::AnimateEffectTextures for ships, DaBd::AnimateEffectTextures for
// buildings): the shield effect while the shields are up, a weapon's while
// that weapon has fired in the last two seconds, and so on. Each effect has
// six frames of 32 x 32 pixels in the game's texture library, shown at six
// frames a second, in order or at random, and glowing over the surface.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_EFFECTS_H
#define SHIPGEN_EFFECTS_H

#include <QString>
#include <QStringList>

#include "auscface.h"

class AuDesign;
class AuScFace;

// What is going on aboard, for the preview: the game's conditions are about
// systems running, so the designer lets one say which are.
struct EffectSituation
{
	bool	power = true;			// Powered up: power plant, capacitor, life support, sensors; drives on standby.
	bool	shieldsUp = true;
	bool	underWay = false;		// The maneuver drive at full (else holding, low).
	bool	ftlCharged = false;		// The FTL drives charged: hot, and the hull with them.
	bool	weaponsFiring = false;
	bool	transporting = false;	// The transporters sending (else on, low).
};

// How an effect runs.
enum EffectLevel
{
	EffectOff = 0,
	EffectLow = 1,
	EffectHigh = 2,
};

// What a design can do with each effect.
struct DesignEffects
{
	bool	building = false;
	int		faces[EFF_Count];		// How many faces use it.
	bool	possible[EFF_Count];	// Whether the game can ever animate it on this design.
	QString	need[EFF_Count];		// When it runs, or why it never does.
	bool	draft = false;			// A ship with no stations yet (the designer's hull drafts): its
									// effects are taken to run, as the systems are not in yet.
	bool	warp = false;			// The FTL drive is a warp drive (the hull's hot look differs).
	bool	aerialPlatform = false;

	DesignEffects();
	// The effects the design uses that the game never animates on it, one
	// line each: "Weapon 3: ...".
	QStringList	Unused() const;
};

// Looks at the design's systems and stations. Unless 'evenUnused', a design
// with no face that has an effect is not looked at further.
DesignEffects	AnalyseEffects(const AuDesign& design, bool evenUnused = false);

// How the effect runs in that situation on that design.
EffectLevel		EffectLevelFor(const DesignEffects& effects, const EffectSituation& situation, int effect);

// The library file of one of the six frames (0 to 5) the game shows for the
// effect at that level, as a Qt resource path (":/data/...", from the game's
// texture library); empty when it is off.
QString			EffectFrameFile(const DesignEffects& effects, int effect, EffectLevel level, int frame);

// The same as the engine's number for that picture (AuTextureLibItem::
// EFFECT_INDEX); 0 when it is off.
int				EffectFrameIndex(const DesignEffects& effects, int effect, EffectLevel level, int frame);
// How many such numbers there are, and the file of one.
int				EffectFrameCount();
QString			EffectIndexFile(int index);

// Whether the game picks the frames at random (else in order).
bool			EffectFramesAtRandom(int effect);

// Gives the face's effect texture coordinates: the effect image laid flat on
// the face and lined up with the vertical (the engine's own unwrap), one copy
// every 'repeatFeet' feet. A face with no effect is left as it is.
void			ProjectEffect(AuScFace& face, double repeatFeet = 8.0);

#endif
