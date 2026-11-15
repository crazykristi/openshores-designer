//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - small pictures of the game's library textures.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "texturepreview.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QLibrary>
#include <QPixmap>
#include <QSettings>

#include "augleffects.h"

namespace
{
QString	s_loaded;			// The library loaded, once one has been.
bool	s_tried = false;
QHash<int, QIcon>	s_icons;	// Made so far (null ones too, until a library is set).

// The resource path of a library texture. The engine writes ":data/x.png";
// Qt's resource system wants ":/data/x.png".
QString TextureFile(int libTexId)
{
	QString file = AuTextureLibItem::LibraryTextureFile((quint16)libTexId);
	if(file.startsWith(":") && !file.startsWith(":/"))
		file.insert(1, '/');
	return file;
}

// Whether the library's resources are there: Wall Metal (96), an interior
// texture (the terrain ones are in another library).
bool HaveTextures()
{
	return QFile::exists(TextureFile(96));
}

bool LoadLibrary(const QString& fileName)
{
	if(fileName.isEmpty() || !QFile::exists(fileName))
		return false;
	QLibrary* lib = new QLibrary(fileName);		// Kept loaded for the resources.
	if(!lib->load())
	{
		delete lib;
		return false;
	}
	if(!HaveTextures())
	{
		lib->unload();
		delete lib;
		return false;
	}
	s_loaded = fileName;
	return true;
}

void FindLibrary()
{
	if(s_tried)
		return;
	s_tried = true;
	if(HaveTextures())		// Linked in some other way.
	{
		s_loaded = "(built in)";
		return;
	}
	QStringList candidates;
	candidates << QSettings().value("designer/gameTextureLibrary").toString();
	candidates << QString::fromLocal8Bit(qgetenv("SHIPGEN_GAME_TEXTURES"));
	const QString home = QDir::homePath();
	candidates << home + "/.local/share/OpenShores/lib/libAuTexture.so.7"
			   << home + "/.local/share/OpenShores/lib/libAuTexture.so";
	for(const QString& c : candidates)
		if(LoadLibrary(c))
			return;
}
}

QIcon GameTextureIcon(int libTexId)
{
	if(s_icons.contains(libTexId))
		return s_icons.value(libTexId);
	FindLibrary();
	QIcon icon;
	if(!s_loaded.isEmpty() && libTexId >= 0)
	{
		QImage image(TextureFile(libTexId));
		if(!image.isNull())
			icon = QIcon(QPixmap::fromImage(image.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
	}
	s_icons.insert(libTexId, icon);
	return icon;
}

QString GameTextureLibrary()
{
	FindLibrary();
	return s_loaded;
}

bool SetGameTextureLibrary(const QString& fileName, QString& error)
{
	FindLibrary();
	if(!s_loaded.isEmpty())
	{
		error = "The game's textures are already loaded from " + s_loaded + "; restart the designer to use another file.";
		return false;
	}
	if(!LoadLibrary(fileName))
	{
		error = fileName + " could not be loaded, or holds none of the game's textures.";
		return false;
	}
	QSettings().setValue("designer/gameTextureLibrary", fileName);
	s_icons.clear();
	return true;
}
