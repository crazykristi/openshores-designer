//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - small pictures of the game's library textures.
//
// The images are Qt resources (":data/<name>.png", the file the engine's
// AuTextureLibItem::LibraryTextureFile names) inside the game's own texture
// library (libAuTexture.so.7, AuTexture.dll), which needs only QtCore. It is
// loaded with QLibrary, which registers its resources, from:
//  1. the setting designer/gameTextureLibrary (View menu),
//  2. the environment variable SHIPGEN_GAME_TEXTURES,
//  3. the usual OpenShores install, ~/.local/share/OpenShores/lib.
// Without it the texture boxes show names only, as before.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_TEXTUREPREVIEW_H
#define SHIPGEN_TEXTUREPREVIEW_H

#include <QIcon>
#include <QString>

// A picture of library texture `libTexId`, or a null icon.
QIcon GameTextureIcon(int libTexId);

// The texture library found (and loaded), or empty.
QString GameTextureLibrary();

// Uses this file from now on (and keeps it in the settings). False, with
// the reason, when it holds no textures.
bool SetGameTextureLibrary(const QString& fileName, QString& error);

#endif
