//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - reading and writing .SoH files written by newer game builds.
//
// The engine source we build against knows part types up to Part_BuildZone
// (89). The live game writes at least one more (type 90, a per-deck plan of
// the rooms; see docs/SOH_FORMAT.md). A part record has no length prefix, so
// when AuScParts::operator<< meets a type it does not know it stops reading,
// and every part after it in the stream is lost without an error: the
// White Star loads with no hull at all.
//
// LoadDesign walks the part list itself. An unknown part is skipped by
// finding the next record that parses, its bytes are kept, and a hidden
// placeholder part holds its id so that nothing new is given the same id.
// SaveDesign puts the original bytes back in place of the placeholder, so a
// round trip keeps the part even though we cannot read it.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_SOHFILE_H
#define SHIPGEN_SOHFILE_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

class AuDesign;

// A part the engine cannot read, kept byte for byte.
struct OpaquePart
{
	quint16		id = 0;
	quint8		type = 0;
	QByteArray	record;		// The whole record: id, type and the part's own data.
};

struct DesignLoadReport
{
	bool				recovered = false;		// Unknown parts were skipped.
	bool				truncated = false;		// The part list could not be read to its end.
	int					partsInFile = 0;		// The count the file states.
	QList<OpaquePart>	opaque;
	QStringList			notes;					// What happened, for the user.
};

// The group name of the placeholder that holds an opaque part's id.
const char* OpaquePlaceholderGroup();

// Loads a .SoH design. Never returns a design that silently lost parts:
// either the parts are all there (some of them as placeholders), or
// report->truncated says that they are not.
AuDesign* LoadDesign(const QString& fileName, DesignLoadReport* report = 0);

// Saves a design. Placeholders are replaced by the opaque parts they stand
// for; an opaque part whose placeholder has gone (deleted) is left out.
bool SaveDesign(const AuDesign& design, const QString& fileName, const QList<OpaquePart>& opaque = QList<OpaquePart>());

// The decrypted, uncompressed design bytes of a .SoH file, as
// AuDesign::Pack() writes them. Empty when the file cannot be read.
QByteArray ReadDesignBytes(const QString& fileName, quint8* fileType = 0);

#endif
