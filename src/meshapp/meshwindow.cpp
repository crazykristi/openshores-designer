//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the mesh tool.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "meshwindow.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>

#include "mesh.h"
#include "preview.h"

namespace {

// A button that says what it does when the mouse is over it.
QPushButton* MakeButton(const QString& text, const QString& tip, QWidget* parent)
{
	QPushButton* button = new QPushButton(text, parent);
	button->setToolTip(tip);
	button->setStatusTip(tip);
	// The buttons keep their own size, so that they do not squeeze each
	// other when the window is narrow.
	button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	button->setMinimumWidth(72);
	return button;
}

// A row of buttons with a little space between them.
QHBoxLayout* MakeButtonRow(QWidget* parent)
{
	QHBoxLayout* row = new QHBoxLayout;
	row->setSpacing(6);
	row->setContentsMargins(0, 0, 0, 0);
	row->addStretch(1);
	(void)parent;
	return row;
}

} // namespace

MeshWindow::MeshWindow(QWidget* parent) :
	QMainWindow(parent)
{
	setWindowTitle(tr("Shipgen - mesh tool"));

	// The view of the model.
	m_preview = new DesignPreview(this);

	// The files that were loaded.
	m_fileList = new QListWidget(this);
	m_fileList->setToolTip(tr("The models that are loaded. They are all put together and "
			"thinned out as one."));

	// What the target is measured in.
	m_targetUnit = new QComboBox(this);
	m_targetUnit->addItem(tr("Faces"), 0);
	m_targetUnit->addItem(tr("Percent"), 1);
	m_targetUnit->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_targetUnit->setMinimumContentsLength(7);
	m_targetUnit->setToolTip(tr("Whether the target is a number of faces or a share of what "
			"the model has now."));

	m_targetSpin = new QSpinBox(this);
	m_targetSpin->setRange(4, 2000000);
	m_targetSpin->setValue(2000);
	m_targetSpin->setToolTip(tr("How many faces the model may have at most. The game copes with "
			"about 15,000, is comfortable with about 5,000 and is happy with about 2,000."));

	// Which way it is done.
	m_methodCombo = new QComboBox(this);
	m_methodCombo->addItem(tr("Quadric error"), int(SimplifyMethod::Quadric));
	m_methodCombo->addItem(tr("Clustering"), int(SimplifyMethod::Cluster));
	m_methodCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_methodCombo->setMinimumContentsLength(12);
	m_methodCombo->setToolTip(tr("Quadric error collapses the edges one at a time, cheapest first, "
			"and puts each corner where the surface around it wants it to be. It keeps the shape, "
			"but it takes a while and needs a lot of memory for a big model.\n\n"
			"Clustering snaps the corners onto a grid, which is much quicker but makes the surface "
			"blocky and the details flat."));

	// What is shown.
	m_viewCombo = new QComboBox(this);
	m_viewCombo->addItem(tr("Reduced"), 1);
	m_viewCombo->addItem(tr("Original"), 2);
	m_viewCombo->addItem(tr("Both"), 3);
	m_viewCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_viewCombo->setMinimumContentsLength(8);
	m_viewCombo->setToolTip(tr("The original is drawn over the top of the reduced one so that the "
			"difference can be seen."));

	// Which way it is mirrored.
	m_mirrorCombo = new QComboBox(this);
	m_mirrorCombo->addItem(tr("Across"), 0);
	m_mirrorCombo->addItem(tr("Along"), 1);
	m_mirrorCombo->addItem(tr("Up"), 2);
	m_mirrorCombo->setToolTip(tr("Which way the model is turned over."));

	m_smoothCheck = new QCheckBox(tr("Smooth shading"), this);
	m_smoothCheck->setToolTip(tr("Gives every corner the average of the normals of the triangles "
			"around it, so that the model looks smooth rather than faceted. It does not change "
			"the shape, only how it is lit."));

	m_statsLabel = new QLabel(this);
	m_statsLabel->setTextFormat(Qt::RichText);
	m_statsLabel->setWordWrap(true);
	m_infoLabel = new QLabel(this);
	m_infoLabel->setTextFormat(Qt::RichText);
	m_infoLabel->setWordWrap(true);
	m_statusLabel = new QLabel(this);

	// --- the panel on the left, which can be scrolled ----------------------
	QWidget* panel = new QWidget(this);
	QVBoxLayout* panelLayout = new QVBoxLayout(panel);
	panelLayout->setSpacing(8);

	// The models.
	QGroupBox* fileBox = new QGroupBox(tr("Models"), panel);
	QVBoxLayout* fileLayout = new QVBoxLayout(fileBox);
	QPushButton* addButton = MakeButton(tr("Add..."),
			tr("Choose one or more model files to add to the list. 3ds, stl and obj are all read."),
			fileBox);
	connect(addButton, &QPushButton::clicked, this, &MeshWindow::OnOpen);
	QPushButton* removeButton = MakeButton(tr("Remove"),
			tr("Take the model that is picked out of the list, and out of the model."), fileBox);
	connect(removeButton, &QPushButton::clicked, this, [this]() {
		RemoveFile(m_fileList->currentRow());
	});
	QPushButton* clearButton = MakeButton(tr("Clear"),
			tr("Take all the models out of the list."), fileBox);
	connect(clearButton, &QPushButton::clicked, this, [this]() {
		m_fileList->clear();
		m_files.clear();
		m_parts.clear();
		m_original.clear();
		m_reduced.clear();
		UpdateStats();
		RefreshInfo();
		ShowView();
		m_statusLabel->setText(tr("The list is empty."));
	});
	QHBoxLayout* fileButtons = MakeButtonRow(fileBox);
	fileButtons->insertWidget(0, addButton);
	fileButtons->insertWidget(1, removeButton);
	fileButtons->insertWidget(2, clearButton);
	fileLayout->addWidget(m_fileList);
	fileLayout->addLayout(fileButtons);
	panelLayout->addWidget(fileBox);

	// The target and the method.
	QGroupBox* optionBox = new QGroupBox(tr("Thinning out"), panel);
	QFormLayout* optionForm = new QFormLayout(optionBox);
	optionForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

	QHBoxLayout* targetRow = MakeButtonRow(optionBox);
	targetRow->insertWidget(0, m_targetUnit);
	targetRow->insertWidget(1, m_targetSpin);
	optionForm->addRow(tr("Target:"), targetRow);

	// The sizes the game is happy with.
	QHBoxLayout* presetRow = MakeButtonRow(optionBox);
	const int presets[3] = { 15000, 5000, 2000 };
	const QString presetText[3] = { tr("15,000"), tr("5,000"), tr("2,000") };
	const QString presetTip[3] = {
			tr("The most the game is said to cope with."),
			tr("As much as the game is comfortable with."),
			tr("As much as the game is happy with.") };
	for(int i = 0; i < 3; ++i)
	{
		QPushButton* button = MakeButton(presetText[i], presetTip[i], optionBox);
		button->setMinimumWidth(56);
		const int faces = presets[i];
		connect(button, &QPushButton::clicked, this, [this, faces]() {
			m_targetUnit->setCurrentIndex(0);
			m_targetSpin->setValue(faces);
		});
		presetRow->insertWidget(i, button);
	}
	optionForm->addRow(tr("Usual sizes:"), presetRow);

	optionForm->addRow(tr("Method:"), m_methodCombo);

	QPushButton* simplifyButton = MakeButton(tr("Simplify"),
			tr("Takes the model down to the target and shows the result."), optionBox);
	simplifyButton->setDefault(true);
	connect(simplifyButton, &QPushButton::clicked, this, &MeshWindow::OnSimplify);
	QPushButton* resetButton = MakeButton(tr("Reset"),
			tr("Puts the model back the way it was and throws the reduced one away."), optionBox);
	connect(resetButton, &QPushButton::clicked, this, &MeshWindow::OnReset);
	QHBoxLayout* actionRow = MakeButtonRow(optionBox);
	actionRow->insertWidget(0, simplifyButton);
	actionRow->insertWidget(1, resetButton);
	optionForm->addRow(actionRow);
	panelLayout->addWidget(optionBox);

	// The tidying up.
	QGroupBox* toolBox = new QGroupBox(tr("Tidying up"), panel);
	QVBoxLayout* toolLayout = new QVBoxLayout(toolBox);

	QPushButton* cleanButton = MakeButton(tr("Clean up"),
			tr("Takes away the triangles that have no area and the ones that are there twice. "
			"Models from the internet often have them."), toolBox);
	connect(cleanButton, &QPushButton::clicked, this, &MeshWindow::OnClean);
	QPushButton* holeButton = MakeButton(tr("Close the holes"),
			tr("Puts a lid over every hole in the model, so that the model is solid from the outside. "
			"A model with holes in it cannot be measured, and the game cannot tell what is inside it, "
			"so the volume comes out wrong."), toolBox);
	connect(holeButton, &QPushButton::clicked, this, &MeshWindow::OnCloseHoles);
	QPushButton* windingButton = MakeButton(tr("Fix windings"),
			tr("Turns the triangles that wind the wrong way round the other way round, so that the "
			"model is solid from the outside. Models from the internet often have some of them "
			"the wrong way round, which makes parts of the model look inside out."), toolBox);
	connect(windingButton, &QPushButton::clicked, this, &MeshWindow::OnFixWindings);
	QHBoxLayout* cleanRow = MakeButtonRow(toolBox);
	cleanRow->insertWidget(0, cleanButton);
	cleanRow->insertWidget(1, holeButton);
	toolLayout->addLayout(cleanRow);

	QHBoxLayout* windingRow = MakeButtonRow(toolBox);
	windingRow->insertWidget(0, windingButton);
	toolLayout->addLayout(windingRow);

	QPushButton* groundButton = MakeButton(tr("Put on the ground"),
			tr("Puts the bottom of the model on the ground and the middle of it in the middle, "
			"which is where the blueprint generator puts it."), toolBox);
	connect(groundButton, &QPushButton::clicked, this, &MeshWindow::OnGround);
	QHBoxLayout* groundRow = MakeButtonRow(toolBox);
	groundRow->insertWidget(0, groundButton);
	toolLayout->addLayout(groundRow);

	QPushButton* mirrorButton = MakeButton(tr("Turn over"),
			tr("Turns the model over. It is useful when a model has to face the other way."), toolBox);
	connect(mirrorButton, &QPushButton::clicked, this, &MeshWindow::OnMirror);
	QHBoxLayout* mirrorRow = MakeButtonRow(toolBox);
	mirrorRow->insertWidget(0, m_mirrorCombo);
	mirrorRow->insertWidget(1, mirrorButton);
	toolLayout->addLayout(mirrorRow);

	toolLayout->addWidget(m_smoothCheck);
	panelLayout->addWidget(toolBox);

	// What is shown.
	QGroupBox* viewBox = new QGroupBox(tr("Looking at it"), panel);
	QVBoxLayout* viewLayout = new QVBoxLayout(viewBox);
	viewLayout->addWidget(m_viewCombo);
	panelLayout->addWidget(viewBox);

	// The numbers.
	QGroupBox* infoBox = new QGroupBox(tr("How it came out"), panel);
	QVBoxLayout* infoLayout = new QVBoxLayout(infoBox);
	infoLayout->addWidget(m_statsLabel);
	infoLayout->addWidget(m_infoLabel);
	panelLayout->addWidget(infoBox);

	panelLayout->addStretch(1);

	// The panel goes in a scroll area, so that it works on a small screen.
	m_scroll = new QScrollArea(this);
	m_scroll->setWidget(panel);
	m_scroll->setWidgetResizable(true);
	m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_scroll->setFrameShape(QFrame::NoFrame);
	m_scroll->setMinimumWidth(280);
	// The panel has to be able to be narrower than it wants to be, so that
	// the window can be small.
	panel->setMinimumWidth(0);
	m_scroll->setSizeAdjustPolicy(QScrollArea::AdjustIgnored);

	QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
	splitter->addWidget(m_scroll);
	splitter->addWidget(m_preview);
	splitter->setStretchFactor(1, 1);
	splitter->setStretchFactor(0, 0);
	splitter->setChildrenCollapsible(false);
	setCentralWidget(splitter);

	QMenuBar* menu = menuBar();
	QMenu* fileMenu = menu->addMenu(tr("&File"));
	fileMenu->addAction(tr("&Open..."), this, &MeshWindow::OnOpen);
	fileMenu->addAction(tr("&Save as..."), this, &MeshWindow::OnSave);
	fileMenu->addSeparator();
	fileMenu->addAction(tr("&Quit"), this, &QWidget::close);
	QMenu* helpMenu = menu->addMenu(tr("&Help"));
	helpMenu->addAction(tr("&About"), this, [this]() {
		QMessageBox::about(this, tr("Shipgen - mesh tool"),
				tr("Takes a 3ds, stl or obj model down to a given number of faces.\n\n"
				"The game copes with about 15,000 faces in a blueprint, is comfortable with about "
				"5,000 and is happy with about 2,000.\n\n"
				"Quadric error keeps the shape as well as it can be kept. Clustering is much "
				"quicker but makes the surface blocky."));
	});

	// The message goes on the right, so that it does not sit on top of the
	// tip of the button that the mouse is over.
	statusBar()->addPermanentWidget(m_statusLabel);
	statusBar()->showMessage(tr("Open a model to begin."));

	connect(m_viewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshWindow::OnViewChanged);
	connect(m_targetSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MeshWindow::OnTargetChanged);
	connect(m_targetUnit, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshWindow::OnTargetUnitChanged);
	connect(m_smoothCheck, &QCheckBox::toggled, this, &MeshWindow::OnSmoothChanged);

	resize(1150, 760);
	UpdateStats();
	RefreshInfo();
}

int MeshWindow::TargetFaces() const
{
	if(m_targetUnit->currentData().toInt() == 1)
	{
		// A share of what the model has now.
		const int percent = m_targetSpin->value();
		return qMax(4, int(double(m_original.size()) * percent / 100.));
	}
	return m_targetSpin->value();
}

bool MeshWindow::OpenFile(const QString& fileName)
{
	AuScTriList tris;
	QString error;
	if(!LoadMeshFile(fileName, tris, error))
	{
		QMessageBox::warning(this, tr("Shipgen"), error);
		return false;
	}
	m_original = tris;
	m_files.clear();
	m_files << fileName;
	m_parts.clear();
	m_parts.append(tris);
	m_fileList->clear();
	m_fileList->addItem(QFileInfo(fileName).fileName());
	m_name = QFileInfo(fileName).completeBaseName();
	m_reduced.clear();
	UpdateStats();
	RefreshInfo();
	ShowView();
	m_statusLabel->setText(tr("Loaded %1: %2 faces.").arg(QFileInfo(fileName).fileName()).arg(tris.size()));
	return true;
}

void MeshWindow::SetMesh(const AuScTriList& tris, const QString& name)
{
	m_original = tris;
	m_name = name;
	m_files.clear();
	m_files << name;
	m_parts.clear();
	m_parts.append(tris);
	m_fileList->clear();
	m_fileList->addItem(name);
	m_reduced.clear();
	UpdateStats();
	RefreshInfo();
	ShowView();
}

void MeshWindow::SimplifyTo(int faces)
{
	if(m_original.isEmpty())
		return;
	m_targetUnit->setCurrentIndex(0);
	m_targetSpin->setValue(qMax(4, faces));
	OnSimplify();
}

void MeshWindow::SetView(int view)
{
	const int index = m_viewCombo->findData(view);
	if(index >= 0)
		m_viewCombo->setCurrentIndex(index);
	ShowView();
}

void MeshWindow::SetMethod(int method)
{
	const int index = m_methodCombo->findData(method);
	if(index >= 0)
		m_methodCombo->setCurrentIndex(index);
}

void MeshWindow::AddFiles(const QStringList& files)
{
	QStringList added;
	for(const QString& file : files)
	{
		AuScTriList tris;
		QString error;
		if(!LoadMeshFile(file, tris, error))
		{
			// Keep the models that did load rather than throwing the whole
			// lot away because one of them is bad.
			QMessageBox::warning(this, tr("Shipgen"), error);
			continue;
		}
		m_files << file;
		m_parts.append(tris);
		m_fileList->addItem(QFileInfo(file).fileName());
		added << QFileInfo(file).fileName();
	}
	if(added.isEmpty())
		return;

	m_name = m_files.size() == 1 ? QFileInfo(m_files.first()).completeBaseName() : tr("model");
	RebuildOriginal();
	m_reduced.clear();
	UpdateStats();
	RefreshInfo();
	ShowView();
	m_statusLabel->setText(tr("Added %1. The model now has %2 faces.")
			.arg(added.join(tr(", "))).arg(m_original.size()));
}

void MeshWindow::RemoveFile(int row)
{
	if(row < 0 || row >= m_files.size())
		return;
	m_files.removeAt(row);
	m_parts.removeAt(row);
	m_fileList->takeItem(row);
	m_name = m_files.size() == 1 ? QFileInfo(m_files.first()).completeBaseName() : tr("model");
	RebuildOriginal();
	m_reduced.clear();
	UpdateStats();
	RefreshInfo();
	ShowView();
	m_statusLabel->setText(m_original.isEmpty()
			? tr("The list is empty.")
			: tr("Removed it. The model now has %1 faces.").arg(m_original.size()));
}

void MeshWindow::RebuildOriginal()
{
	m_original.clear();
	for(const AuScTriList& part : m_parts)
		m_original.append(part);
}

void MeshWindow::OnOpen()
{
	const QStringList files = QFileDialog::getOpenFileNames(this, tr("Choose a model"), QString(),
			tr("Models (*.stl *.3ds *.obj);;All files (*)"));
	if(files.isEmpty())
		return;
	AddFiles(files);
}

void MeshWindow::OnSave()
{
	if(m_reduced.isEmpty())
	{
		QMessageBox::information(this, tr("Shipgen"), tr("There is nothing to save yet. "
				"Simplify a model first."));
		return;
	}
	const QString fileName = QFileDialog::getSaveFileName(this, tr("Save the model"), m_name + ".stl",
			tr("Binary stl (*.stl);;Obj (*.obj);;All files (*)"));
	if(fileName.isEmpty())
		return;
	QString error;
	if(!SaveMeshFile(fileName, m_reduced, error))
	{
		QMessageBox::warning(this, tr("Shipgen"), error);
		return;
	}
	m_statusLabel->setText(tr("Wrote %1 with %2 faces.").arg(fileName).arg(m_reduced.size()));
}

void MeshWindow::OnSimplify()
{
	if(m_original.isEmpty())
	{
		QMessageBox::information(this, tr("Shipgen"), tr("Open a model first."));
		return;
	}

	const int target = TargetFaces();
	const SimplifyMethod method = SimplifyMethod(m_methodCombo->currentData().toInt());

	QElapsedTimer timer;
	timer.start();
	m_reduced = SimplifyMesh(m_original, target, method);
	const qint64 ms = timer.elapsed();

	UpdateStats();
	RefreshInfo();
	ShowView();
	m_statusLabel->setText(tr("Reduced %1 faces to %2 in %3 ms.")
			.arg(m_original.size()).arg(m_reduced.size()).arg(ms));
}

void MeshWindow::OnReset()
{
	m_reduced.clear();
	UpdateStats();
	RefreshInfo();
	ShowView();
	m_statusLabel->setText(tr("Back to the model as it was."));
}

void MeshWindow::OnViewChanged()
{
	ShowView();
}

void MeshWindow::OnTargetChanged()
{
	if(m_original.isEmpty())
		return;
	// Keep the number that is asked for within what the model can give.
	if(m_targetUnit->currentData().toInt() == 0 && m_targetSpin->value() > m_original.size())
		m_targetSpin->setValue(m_original.size());
}

void MeshWindow::OnTargetUnitChanged()
{
	if(m_targetUnit->currentData().toInt() == 1)
	{
		m_targetSpin->setRange(1, 100);
		m_targetSpin->setValue(10);
		m_targetSpin->setSuffix(tr(" %"));
	}
	else
	{
		m_targetSpin->setRange(4, 2000000);
		m_targetSpin->setValue(2000);
		m_targetSpin->setSuffix(tr(""));
	}
}

void MeshWindow::OnClean()
{
	if(m_original.isEmpty())
		return;
	const MeshReport before = InspectMesh(m_original);
	m_original = CleanMesh(m_original);
	const MeshReport after = InspectMesh(m_original);
	UpdateStats();
	RefreshInfo();
	ShowView();
	m_statusLabel->setText(tr("Cleaned up: %1 triangles with no area and %2 duplicates taken away, "
			"%3 faces left.").arg(before.degenerate).arg(before.duplicate).arg(after.triangles));
}

void MeshWindow::OnCloseHoles()
{
	if(m_original.isEmpty())
		return;
	const MeshReport before = InspectMesh(m_original);
	if(before.loose == 0)
	{
		m_statusLabel->setText(tr("The model is already closed."));
		return;
	}
	m_original = FillHoles(m_original);
	const MeshReport after = InspectMesh(m_original);
	UpdateStats();
	RefreshInfo();
	ShowView();
	m_statusLabel->setText(tr("Closed %1 hole(s); %2 left.").arg(before.loose).arg(after.loose));
}

void MeshWindow::OnFixWindings()
{
	if(m_original.isEmpty())
		return;
	const MeshReport before = InspectMesh(m_original);
	if(before.windingsAgree)
	{
		m_statusLabel->setText(tr("The triangles already all wind the same way round."));
		return;
	}
	m_original = FixWindings(m_original);
	UpdateStats();
	RefreshInfo();
	ShowView();
	m_statusLabel->setText(tr("Turned the triangles that were the wrong way round."));
}

void MeshWindow::OnGround()
{
	if(m_original.isEmpty())
		return;
	m_original = GroundMesh(m_original);
	UpdateStats();
	RefreshInfo();
	ShowView();
	m_statusLabel->setText(tr("Put the model on the ground and in the middle."));
}

void MeshWindow::OnMirror()
{
	if(m_original.isEmpty())
		return;
	m_original = MirrorMesh(m_original, m_mirrorCombo->currentData().toInt());
	UpdateStats();
	RefreshInfo();
	ShowView();
	m_statusLabel->setText(tr("Turned the model over."));
}

void MeshWindow::OnSmoothChanged()
{
	ShowView();
}

void MeshWindow::ShowView()
{
	const int view = m_viewCombo->currentData().toInt();
	const bool smooth = m_smoothCheck->isChecked();

	AuScTriList shown = (view == 2 || m_reduced.isEmpty()) ? m_original : m_reduced;
	AuScTriList overlay;
	if(view == 3 && !m_reduced.isEmpty())
		overlay = m_original;

	if(smooth)
	{
		shown = SmoothMesh(shown);
		if(!overlay.isEmpty())
			overlay = SmoothMesh(overlay);
	}

	m_preview->SetMesh(shown, QColor(150, 200, 175));
	if(overlay.isEmpty())
		m_preview->SetOverlay(AuScTriList(), QColor(230, 130, 90));
	else
		m_preview->SetOverlay(overlay, QColor(230, 130, 90));
}

double MeshWindow::MeshVolume(const AuScTriList& tris) const
{
	// The volume of a closed shape, by looking at how much the triangles
	// turn around.
	double volume = 0;
	for(const AuScTri& t : tris)
	{
		const AuScPoint& a = t.V0();
		const AuScPoint& b = t.V1();
		const AuScPoint& c = t.V2();
		volume += (a.x * (b.y * c.z - b.z * c.y) - a.y * (b.x * c.z - b.z * c.x)
				+ a.z * (b.x * c.y - b.y * c.x)) / 6.;
	}
	return qAbs(volume);
}

double MeshWindow::MeshArea(const AuScTriList& tris) const
{
	double area = 0;
	for(const AuScTri& t : tris)
	{
		const AuScPoint& a = t.V0();
		const AuScPoint& b = t.V1();
		const AuScPoint& c = t.V2();
		const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
		const double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
		const double nx = uy * vz - uz * vy;
		const double ny = uz * vx - ux * vz;
		const double nz = ux * vy - uy * vx;
		area += 0.5 * qSqrt(nx * nx + ny * ny + nz * nz);
	}
	return area;
}

void MeshWindow::UpdateStats()
{
	if(m_original.isEmpty())
	{
		m_statsLabel->setText(tr("<i>No model yet.</i>"));
		return;
	}

	const double volume = MeshVolume(m_original);
	const double area = MeshArea(m_original);
	QString text = tr("<b>Before:</b> %1 faces<br>").arg(m_original.size());
	text += tr("<b>Volume:</b> %1 &nbsp; <b>Surface:</b> %2<br>")
			.arg(QString::number(volume, 'f', 1)).arg(QString::number(area, 'f', 1));

	if(!m_reduced.isEmpty())
	{
		const double reducedVolume = MeshVolume(m_reduced);
		const double reducedArea = MeshArea(m_reduced);
		const double percent = 100.0 * double(m_reduced.size()) / double(m_original.size());
		text += tr("<b>After:</b> %1 faces (%2% of the original)<br>").arg(m_reduced.size())
				.arg(QString::number(percent, 'f', 1));
		text += tr("<b>Volume:</b> %1 (%2% of the original) &nbsp; <b>Surface:</b> %3 (%4%)")
				.arg(QString::number(reducedVolume, 'f', 1))
				.arg(QString::number(volume > 0 ? reducedVolume * 100.0 / volume : 0.0, 'f', 1))
				.arg(QString::number(reducedArea, 'f', 1))
				.arg(QString::number(area > 0 ? reducedArea * 100.0 / area : 0.0, 'f', 1));
	}
	m_statsLabel->setText(text);
}

void MeshWindow::RefreshInfo()
{
	if(m_original.isEmpty())
	{
		m_infoLabel->setText(tr("<i>Nothing to say about it yet.</i>"));
		return;
	}

	const MeshReport report = InspectMesh(m_original);
	AuPoint min, max;
	MeshExtents(m_original, min, max);

	QString text = tr("<b>Size:</b> %1 x %2 x %3<br>")
			.arg(QString::number(max.x - min.x, 'f', 2))
			.arg(QString::number(max.y - min.y, 'f', 2))
			.arg(QString::number(max.z - min.z, 'f', 2));
	text += tr("<b>Corners:</b> %1 &nbsp; <b>Edges:</b> %2<br>")
			.arg(report.vertices).arg(report.edges);
	text += tr("<b>Holes:</b> %1 loose edge(s) &nbsp; <b>Junk:</b> %2 flat, %3 duplicate<br>")
			.arg(report.loose).arg(report.degenerate).arg(report.duplicate);
	text += tr("<b>Windings:</b> %1")
			.arg(report.windingsAgree ? tr("all the same way round")
					: tr("some of them are the wrong way round"));
	m_infoLabel->setText(text);
}
