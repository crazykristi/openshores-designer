//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the main window.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_MAINWINDOW_H
#define SHIPGEN_MAINWINDOW_H

#include <QImage>
#include <QMainWindow>
#include <QPixmap>
#include <QTemporaryDir>
#include <QString>
#include <QStringList>

#include "sohfile.h"
#include "wizard.h"

class AuDesign;
class DesignPreview;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

// One generated blueprint.
struct Candidate
{
	QString	fileName;
	QString	strategy;
	bool		ok = false;
	int			rooms = 0;
	int			stations = 0;
};

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = 0);
	~MainWindow();

public slots:
	// Opens a blueprint.
	void	OpenFile(const QString& fileName);

	// Opens the generation wizard.
	void	OpenWizard();

	// Opens the wizard on a given page. Handy for looking at one page.
	void	OpenWizardPage(int page, int kind = 0);

	// The template the wizard starts with.
	void	SetWizardTemplate(const QString& templateKey) { m_wizardTemplate = templateKey; }

	// A picture of whatever the user is looking at.
	QPixmap	Screenshot();

	// Just the 3D view, which is drawn into its own buffer.
	QImage	PreviewImage();

	// Generates blueprints straight from a brief, without the wizard.
	void	GenerateFromBriefFile(const QString& briefFile, const QString& hullFile, const QStringList& modelFiles);

	// The building equivalent.
	void	GenerateBuildingFromBriefFile(const QString& briefFile, const QStringList& modelFiles, double fit);

private slots:
	void	OnOpen();
	void	OnSave();
	void	OnSaveAs();
	void	OnNewSpacecraft();
	void	OnNewBuilding();
	void	OnAbout();
	void	OnCandidateSelected();
	void	OnShowRooms(bool show);

private:
	void	CreateActions();
	void	CreateDocks();
	void	LoadDesign(const QString& fileName);
	void	RefreshContents();
	void	RefreshValidation();
	void	ShowCandidate(const QString& fileName);
	void	AddFileRow(const QString& label, const QString& fileName);
	void	RefreshFileList();

	// Runs the wizard and generates the blueprints it describes.
	void	RunWizard(BlueprintKind kind);

	// Generates the blueprints a request describes.
	void	RunWizardRequest(const GenerationRequest& request);

	AuDesign*	m_design = 0;
	QString		m_designFile;
	QList<OpaquePart>	m_opaqueParts;	// Parts of the open file this engine cannot read; written back on save.
	QStringList	m_modelFiles;
	QString		m_hullFile;
	QString		m_textureFile;
	QString		m_briefFile;

	DesignPreview*	m_preview = 0;
	// The template the wizard starts with.
	QString	m_wizardTemplate;
	bool			m_hasOpenGL = false;
	// Where the generated blueprints are kept while they are being looked at.
	class QTemporaryDir* m_tempDir = 0;
	class GenerationWizard* m_wizard = 0;
	int					m_wizardKind = 0;
	QListWidget*		m_candidateList = 0;
	QListWidget*		m_fileList = 0;
	QTreeWidget*		m_contentsTree = 0;
	QPlainTextEdit*	m_validationText = 0;
	QLabel*			m_statusLabel = 0;
	QList<Candidate>	m_candidates;
};

#endif // SHIPGEN_MAINWINDOW_H
