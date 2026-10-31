//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the mesh tool.
//
// A small tool that takes a model down to a given number of faces and shows
// what is left, so that the shape can be judged before it is used.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_MESHWINDOW_H
#define SHIPGEN_MESHWINDOW_H

#include <QColor>
#include <QMainWindow>
#include <QStringList>
#include <QVector>

#include "auscface.h"

class DesignPreview;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QScrollArea;
class QSpinBox;

// The window of the mesh tool.
class MeshWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MeshWindow(QWidget* parent = 0);

	// Opens a model. Returns false when it could not be read.
	bool	OpenFile(const QString& fileName);

	// Puts the model into the window, without reading it from a file.
	void	SetMesh(const AuScTriList& tris, const QString& name);

	// Takes the model down to the number of faces asked for.
	void	SimplifyTo(int faces);

	// Which view to show: 1 reduced, 2 original, 3 both.
	void	SetView(int view);

	// Which method to use: 0 quadric error, 1 clustering.
	void	SetMethod(int method);

private slots:
	void	OnOpen();
	void	OnSave();
	void	OnSimplify();
	void	OnReset();
	void	OnViewChanged();
	void	OnTargetChanged();
	void	OnTargetUnitChanged();
	void	OnClean();
	void	OnCloseHoles();
	void	OnFixWindings();
	void	OnGround();
	void	OnMirror();
	void	OnSmoothChanged();

private:
	void	UpdateStats();
	void	ShowView();
	void	RefreshInfo();
	void	AddFiles(const QStringList& files);
	void	RemoveFile(int row);
	void	RebuildOriginal();
	int		TargetFaces() const;
	double	MeshVolume(const AuScTriList& tris) const;
	double	MeshArea(const AuScTriList& tris) const;

	DesignPreview*	m_preview = 0;
	QScrollArea*	m_scroll = 0;
	QListWidget*	m_fileList = 0;
	QSpinBox*		m_targetSpin = 0;
	QComboBox*		m_targetUnit = 0;
	QComboBox*		m_methodCombo = 0;
	QComboBox*		m_viewCombo = 0;
	QComboBox*		m_mirrorCombo = 0;
	QCheckBox*		m_smoothCheck = 0;
	QLabel*		m_statsLabel = 0;
	QLabel*		m_infoLabel = 0;
	QLabel*		m_statusLabel = 0;

	// The model that was loaded, and the models that were put together to
	// make it. The two lists have a row for every file.
	AuScTriList	m_original;
	QStringList	m_files;
	QVector<AuScTriList>	m_parts;
	// The model that was made from it.
	AuScTriList	m_reduced;
	QString		m_name;
};

#endif // SHIPGEN_MESHWINDOW_H
