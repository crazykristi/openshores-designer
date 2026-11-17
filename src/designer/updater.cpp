//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The designer's updates (updater.h).
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "updater.h"
#include "version.h"

#include <QApplication>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QDate>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSslSocket>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryFile>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
const char* kReleasesApi = "https://api.github.com/repos/crazykristi/openshores-designer/releases?per_page=30";
const char* kReleasesPage = "https://github.com/crazykristi/openshores-designer/releases";

QNetworkRequest Request(const QUrl& url)
{
	QNetworkRequest request(url);
	// A plain name for the program: GitHub wants a User-Agent, nothing more.
	request.setHeader(QNetworkRequest::UserAgentHeader, QString("shipgen-designer/" SHIPGEN_VERSION));
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	return request;
}

// "0.3.7-beta-rc3" -> numbers {0, 3, 7}, label parts {"beta", "rc", 3}.
void SplitVersion(const QString& version, QList<int>& numbers, QStringList& label)
{
	QString v = version.trimmed();
	if(v.startsWith('v') || v.startsWith('V'))
		v.remove(0, 1);
	const int dash = v.indexOf('-');
	const QString main = dash < 0 ? v : v.left(dash);
	for(const QString& n : main.split('.'))
		numbers << n.toInt();
	while(numbers.size() < 3)
		numbers << 0;
	if(dash >= 0)
	{
		QRegularExpressionMatchIterator it = QRegularExpression("[A-Za-z]+|[0-9]+").globalMatch(v.mid(dash + 1));
		while(it.hasNext())
			label << it.next().captured(0).toLower();
	}
}

// The version this copy is; SHIPGEN_UPDATE_AS=0.3.6 pretends to be an older
// one, to try the updates out against the releases there are.
QString Current()
{
	const QString as = qEnvironmentVariable("SHIPGEN_UPDATE_AS");
	return as.isEmpty() ? QString(SHIPGEN_VERSION) : as;
}

int LabelRank(const QString& part)
{
	static const QStringList order = { "dev", "alpha", "a", "beta", "b", "pre", "rc" };
	const int at = order.indexOf(part);
	return at < 0 ? 1 : at;		// An unknown word sorts with alpha.
}
}

int CompareVersions(const QString& a, const QString& b)
{
	QList<int> na, nb;
	QStringList la, lb;
	SplitVersion(a, na, la);
	SplitVersion(b, nb, lb);
	for(int i = 0; i < qMax(na.size(), nb.size()); ++i)
	{
		const int x = na.value(i), y = nb.value(i);
		if(x != y)
			return x < y ? -1 : 1;
	}
	// No label is a full release: newer than any alpha, beta or rc.
	if(la.isEmpty() || lb.isEmpty())
		return la.isEmpty() == lb.isEmpty() ? 0 : (la.isEmpty() ? 1 : -1);
	for(int i = 0; i < qMin(la.size(), lb.size()); ++i)
	{
		bool da = false, db = false;
		const int xa = la.at(i).toInt(&da), xb = lb.at(i).toInt(&db);
		int c = 0;
		if(da && db)
			c = xa == xb ? 0 : (xa < xb ? -1 : 1);
		else if(da != db)
			c = da ? -1 : 1;	// A number before a word.
		else
			c = LabelRank(la.at(i)) == LabelRank(lb.at(i)) ? QString::compare(la.at(i), lb.at(i))
					: (LabelRank(la.at(i)) < LabelRank(lb.at(i)) ? -1 : 1);
		if(c)
			return c < 0 ? -1 : 1;
	}
	// "beta" before "beta-rc1".
	return la.size() == lb.size() ? 0 : (la.size() < lb.size() ? -1 : 1);
}

Updater::Updater(QWidget* window, std::function<bool()> saveFirst) :
	QObject(window), m_window(window), m_saveFirst(std::move(saveFirst)), m_net(new QNetworkAccessManager(this))
{
}

bool Updater::CheckAtStart()
{
	return QSettings().value("updates/checkAtStart", true).toBool();
}

void Updater::SetCheckAtStart(bool on)
{
	QSettings().setValue("updates/checkAtStart", on);
}

// The end of the release file's name for the copy that is running, or empty
// where the program cannot update itself (built from source).
QString Updater::AssetSuffix()
{
#if defined(Q_OS_WIN)
	return "-windows-x86_64.zip";
#elif defined(Q_OS_MACOS)
	return QSysInfo::currentCpuArchitecture().startsWith("arm") ? "-macos-arm64.dmg" : "-macos-x86_64.dmg";
#else
	return qEnvironmentVariableIsSet("APPIMAGE") ? "-x86_64.AppImage" : QString();
#endif
}

void Updater::Check(bool quiet)
{
	if(m_busy)
		return;
	QSettings settings;
	if(quiet)
	{
		const QString today = QDate::currentDate().toString(Qt::ISODate);
		if(settings.value("updates/lastCheck").toString() == today)
			return;
		settings.setValue("updates/lastCheck", today);
	}
	if(!QSslSocket::supportsSsl())
	{
		if(!quiet && QMessageBox::question(m_window, "Updates",
				"This copy cannot make secure connections (the OpenSSL libraries were not found), so it cannot ask "
				"GitHub for new versions.\n\nOpen the release page in the browser instead?") == QMessageBox::Yes)
			QDesktopServices::openUrl(QUrl(kReleasesPage));
		return;
	}
	m_busy = true;
	QNetworkRequest request = Request(QUrl(kReleasesApi));
	request.setRawHeader("Accept", "application/vnd.github+json");
	QNetworkReply* reply = m_net->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply, quiet]() {
		reply->deleteLater();
		m_busy = false;
		if(reply->error() != QNetworkReply::NoError)
		{
			if(!quiet)
				QMessageBox::warning(m_window, "Updates", "GitHub could not be asked for new versions: " +
						reply->errorString());
			return;
		}
		const QJsonArray releases = QJsonDocument::fromJson(reply->readAll()).array();
		const QString suffix = AssetSuffix();
		Release best;
		best.version = Current();
		for(const QJsonValue& v : releases)
		{
			const QJsonObject o = v.toObject();
			if(o["draft"].toBool())
				continue;
			QString tag = o["tag_name"].toString();
			if(tag.startsWith('v'))
				tag.remove(0, 1);
			if(tag.isEmpty() || CompareVersions(tag, best.version) <= 0)
				continue;
			Release r;
			r.version = tag;
			r.name = o["name"].toString(tag);
			r.notes = o["body"].toString();
			r.page = o["html_url"].toString(kReleasesPage);
			if(!suffix.isEmpty())
				for(const QJsonValue& a : o["assets"].toArray())
				{
					const QJsonObject asset = a.toObject();
					if(!asset["name"].toString().endsWith(suffix))
						continue;
					r.assetName = asset["name"].toString();
					r.assetUrl = asset["browser_download_url"].toString();
					r.assetSize = qint64(asset["size"].toDouble());
					const QString digest = asset["digest"].toString();
					if(digest.startsWith("sha256:"))
						r.sha256 = digest.mid(7).toLower();
					break;
				}
			best = r;
		}
		if(best.version == Current())
		{
			if(!quiet)
				QMessageBox::information(m_window, "Updates", QString("This is the newest version (%1).")
						.arg(Current()));
			return;
		}
		if(quiet && QSettings().value("updates/skip").toString() == best.version)
			return;
		Offer(best, quiet);
	});
}

void Updater::Offer(const Release& release, bool quiet)
{
	QDialog dialog(m_window);
	dialog.setWindowTitle("A new version");
	QVBoxLayout* layout = new QVBoxLayout(&dialog);
	QLabel* head = new QLabel(QString("<b>%1</b> is out; this is %2.").arg(release.name.toHtmlEscaped(),
			Current()));
	head->setWordWrap(true);
	layout->addWidget(head);
	QTextBrowser* notes = new QTextBrowser;
	notes->setOpenExternalLinks(true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	notes->setMarkdown(release.notes);
#else
	notes->setPlainText(release.notes);
#endif
	layout->addWidget(notes, 1);
	QString how;
#if defined(Q_OS_WIN)
	how = "The designer closes, the new version is unpacked over this folder and started.";
#elif defined(Q_OS_MACOS)
	how = "The disk image is downloaded to your Downloads folder and opened; drag the designer into Applications.";
#else
	how = release.assetUrl.isEmpty() ? QString() : "The new AppImage is put next to this one and started.";
#endif
	if(release.assetUrl.isEmpty())
		how = AssetSuffix().isEmpty() ? "This copy was built from the source, so it cannot update itself: get the "
				"new version from the release page." : "The release has no file for this system yet.";
	QLabel* howLabel = new QLabel(how);
	howLabel->setWordWrap(true);
	layout->addWidget(howLabel);
	QCheckBox* atStart = new QCheckBox("Look for new versions when the designer starts (once a day)");
	atStart->setChecked(CheckAtStart());
	layout->addWidget(atStart);

	QDialogButtonBox* buttons = new QDialogButtonBox;
	QPushButton* install = nullptr;
	if(!release.assetUrl.isEmpty())
		install = buttons->addButton("Download and install", QDialogButtonBox::AcceptRole);
	QPushButton* page = buttons->addButton("Open the release page", QDialogButtonBox::ActionRole);
	QPushButton* skip = buttons->addButton("Skip this version", QDialogButtonBox::RejectRole);
	buttons->addButton("Later", QDialogButtonBox::RejectRole);
	layout->addWidget(buttons);
	QPushButton* clicked = nullptr;
	connect(buttons, &QDialogButtonBox::clicked, &dialog, [&](QAbstractButton* b) {
		clicked = qobject_cast<QPushButton*>(b);
		dialog.accept();
	});
	dialog.resize(560, 460);
	dialog.exec();
	SetCheckAtStart(atStart->isChecked());
	Q_UNUSED(quiet);
	if(clicked == page)
		QDesktopServices::openUrl(QUrl(release.page));
	else if(clicked == skip)
		QSettings().setValue("updates/skip", release.version);
	else if(clicked && clicked == install)
		DownloadAndInstall(release);
}

void Updater::DownloadAndInstall(const Release& release)
{
	// Where the download goes: next to the AppImage, in the temporary folder
	// for the Windows zip, in Downloads for the macOS disk image.
	QString folder;
#if defined(Q_OS_WIN)
	folder = QDir::tempPath();
	{
		QTemporaryFile probe(QCoreApplication::applicationDirPath() + "/shipgen-write-test-XXXXXX");
		if(!probe.open())
		{
			QMessageBox::warning(m_window, "Updates", QString("The designer's folder (%1) cannot be written to, so "
					"the new version cannot be put there. The release page opens instead.")
					.arg(QDir::toNativeSeparators(QCoreApplication::applicationDirPath())));
			QDesktopServices::openUrl(QUrl(release.page));
			return;
		}
	}
#elif defined(Q_OS_MACOS)
	folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
#else
	folder = QFileInfo(qEnvironmentVariable("APPIMAGE")).absolutePath();
	if(!QFileInfo(folder).isWritable())
		folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
#endif
	if(folder.isEmpty())
		folder = QDir::tempPath();
	QDir().mkpath(folder);
	const QString target = QDir(folder).filePath(release.assetName);
	QFile part(target + ".part");
	if(!part.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		QMessageBox::warning(m_window, "Updates", "Could not write " + QDir::toNativeSeparators(part.fileName()));
		return;
	}

	QProgressDialog progress(QString("Downloading %1...").arg(release.assetName), "Cancel", 0, 1000, m_window);
	progress.setWindowTitle("Updates");
	progress.setWindowModality(Qt::WindowModal);
	progress.setMinimumDuration(0);
	progress.setValue(0);
	QCryptographicHash hash(QCryptographicHash::Sha256);
	QNetworkReply* reply = m_net->get(Request(QUrl(release.assetUrl)));
	QEventLoop loop;
	bool writeFailed = false;
	connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
		const QByteArray data = reply->readAll();
		hash.addData(data);
		if(part.write(data) != data.size())
		{
			writeFailed = true;
			reply->abort();
		}
	});
	connect(reply, &QNetworkReply::downloadProgress, &loop, [&](qint64 got, qint64 total) {
		if(total <= 0)
			total = release.assetSize;
		if(total > 0)
			progress.setValue(int(qMin<qint64>(999, got * 1000 / total)));
		progress.setLabelText(QString("Downloading %1... %2 of %3 MB").arg(release.assetName)
				.arg(got / 1048576.0, 0, 'f', 1).arg(total / 1048576.0, 0, 'f', 1));
	});
	connect(&progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	const QByteArray rest = reply->readAll();
	hash.addData(rest);
	part.write(rest);
	part.close();
	progress.reset();
	const bool cancelled = progress.wasCanceled();
	const QNetworkReply::NetworkError error = reply->error();
	const QString errorText = reply->errorString();
	reply->deleteLater();

	QString problem;
	if(cancelled)
		problem = "cancelled";
	else if(writeFailed)
		problem = "the file could not be written (is the disk full?)";
	else if(error != QNetworkReply::NoError)
		problem = errorText;
	else if(release.assetSize > 0 && part.size() != release.assetSize)
		problem = QString("the file is %1 bytes, not %2").arg(part.size()).arg(release.assetSize);
	else if(!release.sha256.isEmpty() && hash.result().toHex() != release.sha256.toLatin1())
		problem = "the file's checksum does not match the one GitHub gives";
	if(!problem.isEmpty())
	{
		part.remove();
		if(!cancelled)
			QMessageBox::warning(m_window, "Updates", "The download did not work: " + problem + ".");
		return;
	}
	if(QFileInfo::exists(target) && !QFile::remove(target))
	{
		QMessageBox::warning(m_window, "Updates", "Could not replace " + QDir::toNativeSeparators(target));
		part.remove();
		return;
	}
	part.rename(target);
	Install(release, target);
}

bool Updater::Install(const Release& release, const QString& file)
{
#if defined(Q_OS_WIN)
	if(m_saveFirst && !m_saveFirst())
	{
		QMessageBox::information(m_window, "Updates", "Not installed. The download stays at " +
				QDir::toNativeSeparators(file));
		return false;
	}
	// Waits for the designer to close, unpacks the zip (its one folder holds
	// the programs) over the designer's folder and starts the new one.
	const QString script = QDir(QDir::tempPath()).filePath("shipgen-update.ps1");
	QFile out(script);
	if(!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
	{
		QMessageBox::warning(m_window, "Updates", "Could not write " + QDir::toNativeSeparators(script));
		return false;
	}
	out.write(R"PS(param([int]$ProcessId, [string]$Zip, [string]$Target)
$ErrorActionPreference = 'Stop'
try {
	Wait-Process -Id $ProcessId -Timeout 120 -ErrorAction SilentlyContinue
	Start-Sleep -Milliseconds 500
	$tmp = Join-Path $env:TEMP ('shipgen-update-' + [guid]::NewGuid())
	Expand-Archive -LiteralPath $Zip -DestinationPath $tmp -Force
	$src = $tmp
	$inner = Get-ChildItem -LiteralPath $tmp -Directory | Select-Object -First 1
	if ($inner -and (Test-Path (Join-Path $inner.FullName 'shipgen-designer.exe'))) { $src = $inner.FullName }
	Copy-Item -Path (Join-Path $src '*') -Destination $Target -Recurse -Force
	Remove-Item -LiteralPath $tmp -Recurse -Force
	Remove-Item -LiteralPath $Zip -Force
	Start-Process -FilePath (Join-Path $Target 'shipgen-designer.exe')
} catch {
	Add-Type -AssemblyName PresentationFramework
	[System.Windows.MessageBox]::Show("The update could not be installed:`n`n" + $_.Exception.Message +
		"`n`nThe download is at $Zip; unpack it by hand. (Is another Shipgen program still open?)",
		'Shipgen update') | Out-Null
}
)PS");
	out.close();
	const QStringList args = { "-NoProfile", "-ExecutionPolicy", "Bypass", "-WindowStyle", "Hidden", "-File",
			QDir::toNativeSeparators(script), "-ProcessId", QString::number(QCoreApplication::applicationPid()),
			"-Zip", QDir::toNativeSeparators(file), "-Target",
			QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) };
	if(!QProcess::startDetached("powershell.exe", args))
	{
		QMessageBox::warning(m_window, "Updates", "PowerShell could not be started to install the update. The "
				"download is at " + QDir::toNativeSeparators(file) + "; unpack it over this folder by hand.");
		return false;
	}
	QApplication::quit();
	return true;
#elif defined(Q_OS_MACOS)
	Q_UNUSED(release);
	QDesktopServices::openUrl(QUrl::fromLocalFile(file));
	QMessageBox::information(m_window, "Updates", "The disk image is open: drag the designer into Applications "
			"(replace the old one), then start it from there.\n\nIt was saved to " + file);
	return true;
#else
	Q_UNUSED(release);
	QFile::setPermissions(file, QFile::permissions(file) | QFileDevice::ExeOwner | QFileDevice::ExeGroup |
			QFileDevice::ExeOther);
	const QString current = QFileInfo(qEnvironmentVariable("APPIMAGE")).absoluteFilePath();
	const bool sameFolder = QFileInfo(file).absolutePath() == QFileInfo(current).absolutePath();
	QMessageBox box(QMessageBox::Question, "Updates", QString("The new version is ready:\n%1\n\nStart it now? "
			"The designer closes first.").arg(file), QMessageBox::NoButton, m_window);
	QPushButton* now = box.addButton("Start the new version", QMessageBox::AcceptRole);
	box.addButton("Later", QMessageBox::RejectRole);
	QCheckBox* removeOld = nullptr;
	if(sameFolder && current != QFileInfo(file).absoluteFilePath())
	{
		removeOld = new QCheckBox("Remove the old version (" + QFileInfo(current).fileName() + ")");
		removeOld->setChecked(true);
		box.setCheckBox(removeOld);
	}
	box.exec();
	// A running AppImage can be removed: it stays mounted until it closes.
	if(removeOld && removeOld->isChecked())
		QFile::remove(current);
	if(box.clickedButton() != now)
		return true;
	if(m_saveFirst && !m_saveFirst())
		return false;
	if(!QProcess::startDetached(file, QStringList()))
	{
		QMessageBox::warning(m_window, "Updates", "The new version could not be started: " + file);
		return false;
	}
	QApplication::quit();
	return true;
#endif
}
