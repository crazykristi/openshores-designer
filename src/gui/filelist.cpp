//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - a list of files.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "filelist.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

FileList::FileList(QWidget* parent) :
	QWidget(parent)
{
	m_dialogTitle = tr("Choose a file");
	m_filter = tr("All files (*)");

	m_list = new QListWidget(this);
	m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_list->setToolTip(tr("The files that the model is made from. A model that is in parts "
			"has more than one."));
	m_list->setMinimumHeight(80);

	QPushButton* addButton = new QPushButton(tr("Add..."), this);
	addButton->setToolTip(tr("Choose one or more files and put them in the list."));
	connect(addButton, &QPushButton::clicked, this, &FileList::OnAdd);

	m_removeButton = new QPushButton(tr("Remove"), this);
	m_removeButton->setToolTip(tr("Take the files that are picked out of the list."));
	connect(m_removeButton, &QPushButton::clicked, this, &FileList::OnRemove);

	QPushButton* clearButton = new QPushButton(tr("Clear"), this);
	clearButton->setToolTip(tr("Take every file out of the list."));
	connect(clearButton, &QPushButton::clicked, this, &FileList::OnClear);

	QHBoxLayout* buttons = new QHBoxLayout;
	buttons->setSpacing(6);
	buttons->addWidget(addButton);
	buttons->addWidget(m_removeButton);
	buttons->addWidget(clearButton);
	buttons->addStretch(1);

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);
	layout->addWidget(m_list);
	layout->addLayout(buttons);
}

QStringList FileList::Files() const
{
	QStringList files;
	for(int i = 0; i < m_list->count(); ++i)
		files << m_list->item(i)->data(Qt::UserRole).toString();
	return files;
}

void FileList::SetFiles(const QStringList& files)
{
	m_list->clear();
	for(const QString& file : files)
		AddFile(file);
}

void FileList::AddFile(const QString& fileName)
{
	if(fileName.isEmpty())
		return;
	// The same file is not put in twice.
	for(int i = 0; i < m_list->count(); ++i)
	{
		if(m_list->item(i)->data(Qt::UserRole).toString() == fileName)
			return;
	}
	QListWidgetItem* item = new QListWidgetItem(QFileInfo(fileName).fileName(), m_list);
	item->setData(Qt::UserRole, fileName);
	item->setToolTip(fileName);
	emit Changed();
	emit EmptyChanged(m_list->count() == 0);
}

void FileList::RemoveSelected()
{
	const QList<QListWidgetItem*> items = m_list->selectedItems();
	for(QListWidgetItem* item : items)
		delete m_list->takeItem(m_list->row(item));
	emit Changed();
	emit EmptyChanged(m_list->count() == 0);
}

void FileList::Clear()
{
	if(m_list->count() == 0)
		return;
	m_list->clear();
	emit Changed();
	emit EmptyChanged(true);
}

void FileList::SetDialog(const QString& title, const QString& filter)
{
	m_dialogTitle = title;
	m_filter = filter;
}

void FileList::SetListTip(const QString& tip)
{
	m_list->setToolTip(tip);
}

void FileList::OnAdd()
{
	const QStringList files = QFileDialog::getOpenFileNames(this, m_dialogTitle, QString(), m_filter);
	if(files.isEmpty())
		return;
	for(const QString& file : files)
		AddFile(file);
}

void FileList::OnRemove()
{
	RemoveSelected();
}

void FileList::OnClear()
{
	Clear();
}
