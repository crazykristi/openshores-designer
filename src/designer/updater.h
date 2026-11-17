//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The designer's updates: asks GitHub for the project's releases, says when
// one is newer than this copy, and downloads and installs it where that can
// be done by the program itself:
//   Linux AppImage: the new AppImage is put next to this one (the old one
//                   removed if wanted) and started;
//   Windows zip:    a small PowerShell script waits for the designer to
//                   close, unpacks the new zip over this folder and starts it;
//   macOS:          the disk image is downloaded and opened (drag the app
//                   into Applications);
//   built from source: the release page is opened.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_UPDATER_H
#define SHIPGEN_UPDATER_H

#include <QObject>
#include <QString>
#include <functional>

class QNetworkAccessManager;
class QWidget;

// -1, 0 or 1 as version a is older than, the same as or newer than b
// ("0.3.7-beta-rc3": the numbers, then the label; alpha < beta < rc, and a
// version without a label is newer than any with one).
int CompareVersions(const QString& a, const QString& b);

class Updater : public QObject
{
	Q_OBJECT
public:
	// 'saveFirst' is asked before the program closes to install; false stops it.
	Updater(QWidget* window, std::function<bool()> saveFirst);

	// 'quiet': at start; says nothing unless there is a newer version (and
	// not one the user said to skip), and at most once a day.
	void	Check(bool quiet);

	static bool	CheckAtStart();
	static void	SetCheckAtStart(bool on);

private:
	struct Release
	{
		QString	version;		// The tag without its "v".
		QString	name;
		QString	notes;			// Markdown.
		QString	page;
		QString	assetName;
		QString	assetUrl;
		qint64	assetSize = 0;
		QString	sha256;			// From GitHub's digest of the asset, when it gives one.
	};

	void	Offer(const Release& release, bool quiet);
	void	DownloadAndInstall(const Release& release);
	bool	Install(const Release& release, const QString& file);
	static QString	AssetSuffix();

	QWidget*				m_window;
	std::function<bool()>	m_saveFirst;
	QNetworkAccessManager*	m_net;
	bool					m_busy = false;
};

#endif
