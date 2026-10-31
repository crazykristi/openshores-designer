//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - painting a model.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "paint.h"

#include <QFileInfo>
#include <QPainter>
#include <QPen>

#include "auglsl.h"
#include "auscface.h"
#include "auscparts.h"

namespace {

// The user texture slots, in the order the tool fills them. SCTEXI_Material
// sits in the middle of the enum and is not ours to use, so it is skipped.
quint8 PaintSlot(int index)
{
	if(index <= 0)
		return SCTEXI_User0;
	int slot = int(SCTEXI_User1) + (index - 1);
	if(slot >= int(SCTEXI_Count))
		slot = int(SCTEXI_Count) - 1;
	return quint8(slot);
}

// Builds a small tile from a plain colour. The lines make it read as a
// painted panel rather than a flat wash of colour.
QImage MakeColourTile(const QColor& colour)
{
	const int size = 64;
	QImage image(size, size, QImage::Format_ARGB32);
	image.fill(colour);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing, false);
	painter.setPen(QPen(colour.darker(130), 1));
	painter.drawRect(0, 0, size - 1, size - 1);
	painter.drawLine(0, size / 2, size, size / 2);
	painter.drawLine(size / 2, 0, size / 2, size);

	// A thin highlight along the top edge, which catches the light in the
	// preview and in the game.
	painter.fillRect(0, 0, size, 2, colour.lighter(118));
	painter.end();
	return image;
}

// The number of times a texture should go round the model, so that it keeps
// roughly the same size as it does in the flat projection.
quint16 CircumferenceRepeats(double diameter, double texSize)
{
	if(texSize <= 0.)
		return 1;
	const double repeats = 3.141592653589793 * qMax(1.0, diameter) / texSize;
	return quint16(qBound(1., repeats, 32.));
}

} // namespace

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

QStringList PaintProjectionNames()
{
	return QStringList() << "by-face" << "planar" << "cylindrical" << "spherical" << "keep";
}

QString PaintProjectionName(PaintProjection projection)
{
	switch(projection)
	{
	case PaintProjection::Planar:		return "planar";
	case PaintProjection::Cylindrical:	return "cylindrical";
	case PaintProjection::Spherical:	return "spherical";
	case PaintProjection::Keep:			return "keep";
	case PaintProjection::ByFace:
	default:							return "by-face";
	}
}

bool PaintProjectionFromName(const QString& name, PaintProjection& projection)
{
	const QString key = name.trimmed().toLower();
	if(key == "by-face" || key == "byface" || key == "unwrap" || key == "auto" || key == "face")
		projection = PaintProjection::ByFace;
	else if(key == "planar" || key == "flat" || key == "project")
		projection = PaintProjection::Planar;
	else if(key == "cylindrical" || key == "cylinder" || key == "wrap")
		projection = PaintProjection::Cylindrical;
	else if(key == "spherical" || key == "sphere" || key == "ball")
		projection = PaintProjection::Spherical;
	else if(key == "keep" || key == "none" || key == "as-is")
		projection = PaintProjection::Keep;
	else
		return false;
	return true;
}

bool LoadPaintImage(const QString& spec, QImage& image, QString& error)
{
	QString text = spec.trimmed();
	if(text.isEmpty())
	{
		error = "there is an empty texture name";
		return false;
	}

	// A colour looks like #rrggbb.
	if(text.startsWith('#'))
	{
		const QColor colour(text);
		if(!colour.isValid())
		{
			error = QString("'%1' is not a colour").arg(text);
			return false;
		}
		image = MakeColourTile(colour);
		return true;
	}

	QImage loaded;
	if(!loaded.load(text))
	{
		error = QString("could not load the texture %1").arg(text);
		return false;
	}
	if(loaded.width() > AuDesignTex::MaxImageDimension() || loaded.height() > AuDesignTex::MaxImageDimension())
		loaded = loaded.scaled(AuDesignTex::MaxImageDimension(), AuDesignTex::MaxImageDimension(),
				Qt::KeepAspectRatio, Qt::SmoothTransformation);
	image = loaded.convertToFormat(QImage::Format_ARGB32);
	return true;
}

QColor PaintAverageColour(const QImage& image)
{
	if(image.isNull())
		return QColor(170, 170, 170);
	const QImage rgba = image.convertToFormat(QImage::Format_ARGB32);
	const int stepX = qMax(1, rgba.width() / 16);
	const int stepY = qMax(1, rgba.height() / 16);
	quint64 r = 0, g = 0, b = 0, n = 0;
	for(int y = 0; y < rgba.height(); y += stepY)
	{
		for(int x = 0; x < rgba.width(); x += stepX)
		{
			const QRgb pixel = rgba.pixel(x, y);
			const int alpha = qAlpha(pixel);
			if(alpha == 0)
				continue;
			r += quint64(qRed(pixel)) * alpha;
			g += quint64(qGreen(pixel)) * alpha;
			b += quint64(qBlue(pixel)) * alpha;
			n += quint64(alpha);
		}
	}
	if(!n)
		return QColor(170, 170, 170);
	return QColor(int(r / n), int(g / n), int(b / n));
}

bool PaintDesign(AuDesign& design, const PaintOptions& opts, QStringList* notes, QString& error)
{
	if(!opts.IsSet())
		return true;

	// There are 14 user slots. SCTEXI_Material sits in the middle and is the
	// engine's, so only that one is left out.
	const int maxTextures = int(SCTEXI_Count) - 1;
	QStringList specs = opts.textures;
	int dropped = 0;
	while(specs.size() > maxTextures)
	{
		specs.removeLast();
		++dropped;
	}
	if(dropped && notes)
	{
		notes->append(QString("the engine can hold %1 textures, so %2 were left out.")
				.arg(maxTextures).arg(dropped));
	}

	QList<QImage> images;
	QStringList names;
	for(const QString& spec : specs)
	{
		QImage image;
		QString loadError;
		if(!LoadPaintImage(spec, image, loadError))
		{
			error = loadError;
			return false;
		}
		const QString name = spec.startsWith('#') ? spec
				: QFileInfo(spec).fileName();
		images << image;
		names << name;
	}

	// Put the images in the design's palette.
	AuDesignTex palette[SCTEXI_Count];
	for(int i = 0; i < images.size(); ++i)
		palette[PaintSlot(i)].SetImage(names.at(i), images.at(i));
	design.SetTexturePalette(palette);

	// Work out which way the model is longest. The face unwrap is aligned to
	// that axis so that the texture runs along the ship or building rather
	// than across it.
	int alignAxis = 0;
	{
		AuPoint min, max;
		bool have = false;
		for(AuScParts::const_iterator it = design.Parts().constBegin(); it != design.Parts().constEnd(); ++it)
		{
			const AuScPart* part = it.value();
			if(!part || !part->IsHullMesh())
				continue;
			AuPoint pmin, pmax;
			part->ModelExtents(pmin, pmax);
			if(!have)
			{
				min = pmin;
				max = pmax;
				have = true;
			}
			else
			{
				min = min.Min(pmin);
				max = max.Max(pmax);
			}
		}
		if(have)
		{
			const double dx = max.x - min.x;
			const double dy = max.y - min.y;
			if(dy > dx)
				alignAxis = 1;
		}
	}

	const double texSize = qMax(.01, opts.size);
	const AuMatrix4x4 plane = AuMatrix4x4::IdentityMatrix();

	int partIndex = 0;
	int painted = 0;
	for(AuScParts::iterator it = design.Parts().begin(); it != design.Parts().end(); ++it)
	{
		AuScPart* part = it.value();
		if(!part || !part->IsHullMesh() || part->IsOmitted())
			continue;

		AuScFaceList faces;
		part->Faces(faces);
		if(faces.isEmpty())
			continue;

		const int slot = opts.perPart ? (partIndex % images.size()) : 0;
		const quint8 texIndex = PaintSlot(slot);

		// The engine's texture calls take a selection. An empty face set
		// means "nothing" to them, not "everything", so every face has to be
		// listed.
		AuScSelectInfo selection;
		for(int i = 0; i < faces.size(); ++i)
			selection.m_Faces.insert(i);

		switch(opts.projection)
		{
		case PaintProjection::Planar:
			faces.TextureProject(selection, texIndex, texSize, plane);
			break;

		case PaintProjection::Cylindrical:
			{
				AuPoint min, max;
				part->ModelExtents(min, max);
				const double width = qMax(max.x - min.x, max.y - min.y);
				faces.TextureCylindrical(selection, texIndex, CircumferenceRepeats(width, texSize),
						texSize, plane);
			}
			break;

		case PaintProjection::Spherical:
			{
				AuPoint min, max;
				part->ModelExtents(min, max);
				const double width = qMax(max.x - min.x, qMax(max.y - min.y, max.z - min.z));
				faces.TextureSpherical(selection, texIndex, CircumferenceRepeats(width, texSize), plane);
			}
			break;

		case PaintProjection::Keep:
			faces.SetTextureIndex(selection, texIndex);
			break;

		case PaintProjection::ByFace:
		default:
			faces.TextureUnwrap(selection, texIndex, texSize, AuScFaceList::UNWRAP_ByFace, 45.f,
					true, alignAxis, plane);
			break;
		}

		part->SetFaces(faces);
		++painted;
		++partIndex;
	}

	if(notes)
	{
		notes->append(QString("painted %1 hull part(s) with %2 texture(s), %3")
				.arg(painted).arg(images.size())
				.arg(opts.projection == PaintProjection::Keep
						? QString("using the coordinates that were already there.")
						: QString("%1, %2 feet per repeat.").arg(PaintProjectionName(opts.projection)).arg(texSize)));
		// With one texture per part, a model that is one piece cannot use
		// more than one. Saying so stops the user wondering where the rest
		// of the images went.
		if(opts.perPart && painted > 0 && painted < images.size())
		{
			notes->append(QString("the hull has %1 part(s), so only %2 of the %3 texture(s) were used. "
					"Each hull part takes one texture.")
					.arg(painted).arg(painted).arg(images.size()));
		}
	}
	return true;
}
