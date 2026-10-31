//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - a list of files.
//
// A model is often in more than one file, so the picker has to be able to
// hold several of them, and they have to be able to be taken away again.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_FILELIST_H
#define SHIPGEN_FILELIST_H

#include <QStringList>
#include <QWidget>

class QListWidget;
class QPushButton;

// A list of files, with buttons to add, remove and clear them.
class FileList : public QWidget
{
	Q_OBJECT

public:
	explicit FileList(QWidget* parent = 0);

	// The files in the list.
	QStringList	Files() const;

	// Puts files into the list, without making a fuss.
	void		SetFiles(const QStringList& files);

	// Takes the files that are picked out of the list.
	void		RemoveSelected();

	// Takes everything out of the list.
	void		Clear();

	// What the file dialog says.
	void		SetDialog(const QString& title, const QString& filter);

	// The tip for the list itself.
	void		SetListTip(const QString& tip);

	// Puts a file into the list without asking for it.
	void		AddFile(const QString& fileName);

signals:
	// The list has changed.
	void	Changed();

	// The list has become empty, or has stopped being empty.
	void	EmptyChanged(bool empty);

private slots:
	void	OnAdd();
	void	OnRemove();
	void	OnClear();

private:
	QListWidget*	m_list = 0;
	QPushButton*	m_removeButton = 0;
	QString			m_dialogTitle;
	QString			m_filter;
};

#endif // SHIPGEN_FILELIST_H
