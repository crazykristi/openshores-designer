//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - reading and writing .SoH files written by newer game builds.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "sohfile.h"

#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QHash>

#include "aubddesign.h"
#include "aubdmodules.h"
#include "aucrypt.h"
#include "audesign.h"
#include "aufilever.h"
#include "auscdesign.h"
#include "auscmodules.h"
#include "auscparts.h"

// audesign.cpp keeps these private.
static const quint8 kFileVerUncompressed = 0x01;
static const quint8 kFileVer = 0x02;
static const quint8 kPartSerializeVer = 0x11;

const char* OpaquePlaceholderGroup()
{
	return "shipgen:opaque-part";
}

namespace
{

struct PartRecord
{
	int		start = 0;
	int		end = 0;
	quint16	id = 0;
	quint8	type = 0;
	bool	known = false;
	QString	group;
};

struct PartList
{
	int					countPos = -1;	// Where the quint16 part count is.
	int					endPos = -1;	// The first byte after the last part.
	quint16				count = 0;
	QList<PartRecord>	records;
	bool				complete = false;
};

// Reads one part record at `pos`. A record of an unknown type, or one that
// runs off the end, is not known.
PartRecord ReadPart(const QByteArray& ba, int pos)
{
	PartRecord r;
	r.start = pos;
	if(pos + 5 > ba.size())
		return r;

	QDataStream in(ba);
	in.setVersion(QDataStream::Qt_4_1);
	in.device()->seek(pos);
	in >> r.id;
	in >> r.type;
	if(r.type == AuScPart::Part_Invalid || r.type >= AuScPart::PART_Count)
		return r;
	AuScPart* part = AuScPart::Create((AuScPart::PART_TYPE)r.type);
	if(!part)
		return r;
	in >> *part;
	if(in.status() == QDataStream::Ok)
	{
		r.known = true;
		r.end = (int)in.device()->pos();
		r.group = part->GroupName();
	}
	delete part;
	return r;
}

// Whether a record that parses could start at `pos`: a known type, the
// bits byte that carries a version byte after it, and a version we know.
bool LooksLikePart(const QByteArray& ba, int pos)
{
	if(pos + 5 > ba.size())
		return false;
	const quint8 type = (quint8)ba[pos + 2];
	const quint8 bits = (quint8)ba[pos + 3];
	const quint8 ver = (quint8)ba[pos + 4];
	return type > AuScPart::Part_Invalid && type < AuScPart::PART_Count
		&& (bits >> 4) == 0x0f && ver >= 0x10 && ver <= kPartSerializeVer;
}

// Finds where the part list starts, by reading what comes before it the way
// AuScDesign::operator<< and AuBdDesign::operator<< do.
int FindPartsStart(const QByteArray& ba)
{
	QDataStream in(ba);
	in.setVersion(QDataStream::Qt_4_1);
	quint8 designType;
	in >> designType;
	AuDesign* design = AuDesign::Create((AuDesign::BLUEPRINT_TYPE)designType);
	if(!design)
		return -1;
	design->AuDesign::operator<<(in);
	delete design;

	quint8 ver;
	in >> ver;
	if(designType == AuDesign::BLUEPRINT_Spacecraft)
	{
		ver = ver >> 4;
		if(ver == 0)
		{
			quint16 cryoBerths;
			in >> cryoBerths;
		}
		else
		{
			quint32 hullVolume;
			in >> hullVolume;
			if(ver >= 0x06)
			{
				quint8 landing;
				in >> landing;
			}
			AuScModules modules;
			in >> modules;
		}
	}
	else if(designType == AuDesign::BLUEPRINT_Building)
	{
		quint32 hullVolume;
		in >> hullVolume;
		AuBdModules modules;
		in >> modules;
	}
	else
		return -1;

	if(in.status() != QDataStream::Ok)
		return -1;
	return (int)in.device()->pos();
}

// Walks the part list, stepping over parts of unknown types by finding the
// next record that parses and is followed by another one (or by the end of
// the list).
PartList WalkParts(const QByteArray& ba)
{
	PartList list;
	list.countPos = FindPartsStart(ba);
	if(list.countPos < 0 || list.countPos + 2 > ba.size())
		return list;
	list.count = ((quint8)ba[list.countPos] << 8) | (quint8)ba[list.countPos + 1];

	int pos = list.countPos + 2;
	int left = list.count;
	while(left > 0)
	{
		PartRecord r = ReadPart(ba, pos);
		if(!r.known)
		{
			// Unknown: look for the next record that holds up.
			int next = -1;
			for(int q = pos + 5; q + 5 <= ba.size(); q++)
			{
				if(!LooksLikePart(ba, q))
					continue;
				PartRecord candidate = ReadPart(ba, q);
				if(!candidate.known)
					continue;
				if(left - 1 == 1 || LooksLikePart(ba, candidate.end))
				{
					next = q;
					break;
				}
			}
			if(next < 0 || left == 1)
				return list;	// The last part, or nothing after it parses: give up.
			r.end = next;
		}
		list.records.append(r);
		pos = r.end;
		left--;
	}
	list.endPos = pos;
	list.complete = true;
	return list;
}

// Rebuilds the part list section of `ba` from records, each either copied
// from `ba` or replaced.
QByteArray Splice(const QByteArray& ba, const PartList& list, const QList<QByteArray>& recordBytes)
{
	QByteArray outBytes = ba.left(list.countPos);
	const quint16 count = (quint16)recordBytes.size();
	outBytes.append((char)(count >> 8));
	outBytes.append((char)(count & 0xff));
	for(const QByteArray& r : recordBytes)
		outBytes.append(r);
	outBytes.append(ba.mid(list.endPos));
	return outBytes;
}

AuDesign* UnpackChecked(const QByteArray& ba, bool* clean)
{
	QDataStream in(ba);
	in.setVersion(QDataStream::Qt_4_1);
	quint8 designType;
	in >> designType;
	AuDesign* design = AuDesign::Create((AuDesign::BLUEPRINT_TYPE)designType);
	if(!design)
		return 0;
	in >> *design;
	*clean = in.status() == QDataStream::Ok && in.atEnd();
	return design;
}

}	// namespace

QByteArray ReadDesignBytes(const QString& fileName, quint8* fileTypeOut)
{
	QFile file(fileName);
	if(!file.open(QFile::ReadOnly))
		return QByteArray();
	QDataStream ds(&file);
	ds.setVersion(QDataStream::Qt_5_0);
	quint8 fileType, fileVer;
	ds >> fileType;
	ds >> fileVer;
	if(fileTypeOut)
		*fileTypeOut = fileType;
	if(fileVer != kFileVer && fileVer != kFileVerUncompressed)
		return QByteArray();

	AuCrypt crypt;
	ds >> crypt;
	crypt.Crypt((fileType << 8) | fileVer);
	QByteArray inner;
	QDataStream dsc(&crypt, QIODevice::ReadOnly);
	dsc.setVersion(QDataStream::Qt_5_0);
	dsc >> inner;
	if(fileVer == kFileVerUncompressed)
		return inner;
	return qUncompress(inner);
}

AuDesign* LoadDesign(const QString& fileName, DesignLoadReport* report)
{
	DesignLoadReport local;
	DesignLoadReport& rep = report ? *report : local;
	rep = DesignLoadReport();

	quint8 fileType = 0;
	const QByteArray ba = ReadDesignBytes(fileName, &fileType);
	if(ba.isEmpty() || fileType != SOH_FILE_TYPE)
		return AuDesign::LoadFromFile(fileName, SOH_FILE_TYPE);	// Let the engine have a go.

	bool clean = false;
	AuDesign* design = UnpackChecked(ba, &clean);
	if(!design || clean)
		return design;

	// The engine stopped early. Find out where and step over what it could not read.
	PartList list = WalkParts(ba);
	rep.partsInFile = list.count;
	if(!list.complete)
	{
		rep.truncated = true;
		rep.notes << QString("The part list could not be read to the end: the file states %1 parts and "
							 "only %2 could be read. The design is incomplete.")
						 .arg(list.count).arg(design->Parts().size());
		return design;
	}

	QList<QByteArray> keep;
	for(const PartRecord& r : list.records)
	{
		if(r.known)
		{
			keep.append(ba.mid(r.start, r.end - r.start));
			continue;
		}
		OpaquePart op;
		op.id = r.id;
		op.type = r.type;
		op.record = ba.mid(r.start, r.end - r.start);
		rep.opaque.append(op);
		rep.notes << QString("Part %1 has type %2, which this engine build does not know (it knows up to %3). "
							 "Its %4 bytes are kept as they are and written back on save.")
						 .arg(r.id).arg(r.type).arg(AuScPart::PART_Count - 1).arg(op.record.size());
	}
	delete design;

	design = UnpackChecked(Splice(ba, list, keep), &clean);
	if(!design)
		return 0;
	if(!clean)
	{
		rep.truncated = true;
		rep.notes << "The design still did not read cleanly after the unknown parts were stepped over.";
	}
	rep.recovered = true;

	// Hold each opaque part's id so that nothing new gets it.
	for(const OpaquePart& op : rep.opaque)
	{
		AuScPart* placeholder = AuScPart::Create(AuScPart::Part_Sketch);
		placeholder->SetGroupName(OpaquePlaceholderGroup());
		placeholder->SetOmitted(true);
		design->Parts().insert(op.id, placeholder);
	}
	return design;
}

bool SaveDesign(const AuDesign& design, const QString& fileName, const QList<OpaquePart>& opaque)
{
	QByteArray ba = design.Pack();

	if(!opaque.isEmpty())
	{
		QHash<quint16, const OpaquePart*> byId;
		for(const OpaquePart& op : opaque)
			byId.insert(op.id, &op);

		PartList list = WalkParts(ba);
		if(!list.complete)
			return false;
		QList<QByteArray> records;
		for(const PartRecord& r : list.records)
		{
			if(r.known && r.type == AuScPart::Part_Sketch && r.group == OpaquePlaceholderGroup() && byId.contains(r.id))
				records.append(byId.value(r.id)->record);
			else
				records.append(ba.mid(r.start, r.end - r.start));
		}
		ba = Splice(ba, list, records);
	}

	QFile file(fileName);
	if(!file.open(QFile::WriteOnly))
		return false;

	// The same layout as AuDesign::SaveToFile.
	AuCrypt crypt;
	QDataStream dsc(&crypt, QIODevice::WriteOnly);
	dsc.setVersion(QDataStream::Qt_5_0);
	dsc << qCompress(ba);
	crypt.Crypt((SOH_FILE_TYPE << 8) | kFileVer);

	QDataStream ds(&file);
	ds.setVersion(QDataStream::Qt_5_0);
	ds << (quint8)SOH_FILE_TYPE;
	ds << (quint8)kFileVer;
	ds << crypt;
	file.close();
	return ds.status() == QDataStream::Ok;
}
