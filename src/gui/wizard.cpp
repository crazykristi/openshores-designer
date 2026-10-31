//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the generation wizard.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "wizard.h"

#include "basepanel.h"
#include "filelist.h"
#include "shapepanel.h"

#include <QComboBox>
#include <QRegularExpression>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QRegularExpression>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// A scroll area that starts at the top when its page is shown. Without this,
// the first field takes the focus and Qt scrolls just far enough to show it,
// which cuts the heading off the top of the page.
class TopScrollArea : public QScrollArea
{
public:
	explicit TopScrollArea(QWidget* parent = 0) : QScrollArea(parent) {}

protected:
	void showEvent(QShowEvent* event) override
	{
		QScrollArea::showEvent(event);
		QTimer::singleShot(0, this, [this]() { verticalScrollBar()->setValue(0); });
	}
};

// Puts the contents of a wizard page into a scroll area, so that the page
// works on a screen that is not very tall. The page keeps its title and its
// buttons, only the contents scroll.
void MakePageScrollable(QWizardPage* page, QWidget* content)
{
	TopScrollArea* scroll = new TopScrollArea(page);
	scroll->setWidget(content);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setFocusPolicy(Qt::NoFocus);

	QVBoxLayout* outer = new QVBoxLayout(page);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->setSpacing(0);
	outer->addWidget(scroll);
}

const int kRoomColumns = 6;		// type, label, count, length, beam, height.
const int kStationColumns = 3;	// type, count, room.

QTableWidgetItem* MakeItem(const QString& text)
{
	QTableWidgetItem* item = new QTableWidgetItem(text);
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	return item;
}

// A cell that offers a fixed list of choices. The game does not let the
// player invent names, so neither does this.
void SetComboCell(QTableWidget* table, int row, int column, const QStringList& choices, const QString& current)
{
	QComboBox* combo = new QComboBox();
	combo->addItems(choices);
	const int index = combo->findText(current);
	if(index >= 0)
		combo->setCurrentIndex(index);
	else if(!choices.isEmpty())
		combo->setCurrentIndex(0);
	else
		combo->addItem(current);
	table->setCellWidget(row, column, combo);
}

// Reads the value of a cell, whether it is plain text or a drop down.
QString CellValue(QTableWidget* table, int row, int column)
{
	if(QComboBox* combo = qobject_cast<QComboBox*>(table->cellWidget(row, column)))
		return combo->currentText();
	QTableWidgetItem* item = table->item(row, column);
	return item ? item->text() : QString();
}

} // namespace

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

GenerationWizard::GenerationWizard(BlueprintKind kind, QWidget* parent) :
	QWizard(parent)
{
	m_request.kind = kind;
	m_roomTypes = (kind == BlueprintKind::Spacecraft) ? SpacecraftRoomTypes() : BuildingRoomTypes();
	m_stationTypes = StationTypes();

	setWizardStyle(QWizard::ModernStyle);
	setWindowTitle(kind == BlueprintKind::Spacecraft
			? tr("New spacecraft blueprint")
			: tr("New building blueprint"));
	setOption(QWizard::NoBackButtonOnStartPage, true);

	if(kind == BlueprintKind::Spacecraft)
		BuildShipPages();
	else
		BuildBuildingPages();

	// Everything the pages hold is copied into the request when the wizard
	// is accepted. Without this the request would still be the empty one it
	// started as.
	connect(this, &QWizard::accepted, this, &GenerationWizard::SyncRequest);
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The paint panel. Both the ship page and the building page use it. They are
// never built at the same time, so one set of widgets is enough.

QWidget* GenerationWizard::MakePaintPanel(QWidget* parent)
{
	QGroupBox* box = new QGroupBox(tr("Paint"), parent);
	box->setToolTip(tr("Put textures on the hull. The tool works out the texture coordinates, "
			"so there is nothing to paint face by face."));
	QVBoxLayout* layout = new QVBoxLayout(box);

	m_textureList = new FileList(box);
	m_textureList->SetDialog(tr("Choose a texture"), tr("Images (*.png *.jpg *.jpeg *.bmp *.tga);;All files (*)"));
	m_textureList->SetListTip(tr("Images to put on the hull, or colours. A model that is split into "
			"parts gets the next texture for each part, so a hull, a ring and a stand can look "
			"different. The engine holds 14 of them."));
	layout->addWidget(m_textureList);

	QPushButton* colourButton = new QPushButton(tr("Add a colour..."), box);
	colourButton->setToolTip(tr("Make a painted panel of a single colour, without needing an image."));
	connect(colourButton, &QPushButton::clicked, this, [this]() {
		const QColor colour = QColorDialog::getColor(QColor(150, 160, 175), this, tr("Paint colour"));
		if(colour.isValid() && m_textureList)
			m_textureList->AddFile(colour.name());
	});
	QHBoxLayout* colourRow = new QHBoxLayout();
	colourRow->addWidget(colourButton);
	colourRow->addStretch(1);
	layout->addLayout(colourRow);

	QFormLayout* form = new QFormLayout();
	m_paintCombo = new QComboBox(box);
	const QString labels[] = {
		tr("By face - no stretching"),
		tr("Planar - flat, from above"),
		tr("Cylindrical - around the long axis"),
		tr("Spherical - round hulls"),
		tr("Keep the coordinates that are there")
	};
	const QStringList names = PaintProjectionNames();
	for(int i = 0; i < names.size() && i < 5; ++i)
		m_paintCombo->addItem(labels[i], names.at(i));
	m_paintCombo->setCurrentIndex(0);
	m_paintCombo->setToolTip(tr("How the image is wrapped around the shape. By face keeps the "
			"texture the same size on every face; the others are best for flat, long or round hulls."));
	form->addRow(tr("Wrapping:"), m_paintCombo);

	m_paintSizeSpin = new QDoubleSpinBox(box);
	m_paintSizeSpin->setRange(.1, 1000.);
	m_paintSizeSpin->setDecimals(1);
	m_paintSizeSpin->setValue(8.);
	m_paintSizeSpin->setSuffix(tr(" ft"));
	m_paintSizeSpin->setToolTip(tr("How many feet one repeat of the texture covers. A smaller "
			"number makes the pattern finer."));
	form->addRow(tr("Texture size:"), m_paintSizeSpin);

	m_paintPerPartCheck = new QCheckBox(tr("Give each model its own texture"), box);
	m_paintPerPartCheck->setChecked(true);
	m_paintPerPartCheck->setToolTip(tr("Use the next texture in the list for every hull part. "
			"Clear this to put the first texture on all of them."));
	form->addRow(QString(), m_paintPerPartCheck);
	layout->addLayout(form);

	return box;
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The spacecraft pages.

void GenerationWizard::BuildShipPages()
{
	// Page 1: the source of the hull.
	{
		m_sourcePage = new QWizardPage(this);
		QWidget* content = new QWidget(m_sourcePage);
		QFormLayout* form = new QFormLayout(content);

		// The base comes first: a simple solid can be the whole hull, or
		// something for the models to stand on.
		m_basePanel = new BaseShapePanel(tr("Primitive base shape"), m_sourcePage);
		form->addRow(m_basePanel);

		QGroupBox* modelBox = new QGroupBox(tr("Ship model"), m_sourcePage);
		QFormLayout* modelForm = new QFormLayout(modelBox);

		m_modelList = new FileList(modelBox);
		m_modelList->SetDialog(tr("Choose a ship model"), tr("Ship models (*.3ds *.stl);;All files (*)"));
		m_modelList->SetListTip(tr("The files that the hull is made from. A model that is in "
				"parts has more than one, and they are put together as one model."));
		connect(m_modelList, &FileList::Changed, this, [this]() {
			// A model and an existing hull cannot both be used.
			if(m_hullEdit && !m_modelList->Files().isEmpty())
				m_hullEdit->clear();
		});
		modelForm->addRow(tr("Model files:"), m_modelList);

		m_hullEdit = new QLineEdit(modelBox);
		m_hullEdit->setPlaceholderText(tr("or an existing blueprint to take the hull from"));
		QPushButton* hullButton = new QPushButton(tr("Choose..."), modelBox);
		hullButton->setToolTip(tr("Pick a blueprint that has already been made and take its hull "
				"from it, instead of using a model."));
		connect(hullButton, &QPushButton::clicked, this, [this]() {
			const QString file = QFileDialog::getOpenFileName(this, tr("Choose a blueprint"), QString(),
					tr("Blueprints (*.SoH);;All files (*)"));
			if(!file.isEmpty())
			{
				m_hullEdit->setText(file);
				m_modelList->Clear();
			}
		});
		QHBoxLayout* hullRow = new QHBoxLayout();
		hullRow->addWidget(m_hullEdit);
		hullRow->addWidget(hullButton);
		modelForm->addRow(tr("Existing hull:"), hullRow);

		m_scaleSpin = new QDoubleSpinBox(modelBox);
		m_scaleSpin->setRange(0.000001, 1000.);
		m_scaleSpin->setDecimals(6);
		m_scaleSpin->setValue(1.);
		m_scaleSpin->setToolTip(tr("Feet per model unit for a 3ds file, a plain multiplier for an stl file."));
		modelForm->addRow(tr("Scale:"), m_scaleSpin);

		m_fitSpin = new QDoubleSpinBox(modelBox);
		m_fitSpin->setRange(0., 100000.);
		m_fitSpin->setDecimals(1);
		m_fitSpin->setSpecialValueText(tr("as it is"));
		m_fitSpin->setSuffix(tr(" ft"));
		m_fitSpin->setToolTip(tr("Scale the model so that its largest side is this many feet. "
				"Handy for models that are in millimetres."));
		modelForm->addRow(tr("Fit to:"), m_fitSpin);

		m_decimateSpin = new QDoubleSpinBox(modelBox);
		m_decimateSpin->setRange(0., 100.);
		m_decimateSpin->setDecimals(2);
		m_decimateSpin->setSpecialValueText(tr("automatic"));
		m_decimateSpin->setToolTip(tr("Thin the model out by snapping its vertices onto a grid of this size. "
				"The engine refuses designs that are too big."));
		modelForm->addRow(tr("Simplify:"), m_decimateSpin);

		form->addRow(modelBox);
		form->addRow(MakePaintPanel(m_sourcePage));

		m_shapePanel = new ShapePanel(tr("Shape of the model"), m_sourcePage);
		m_shapePanel->setToolTip(tr("The same model can be made into different shapes. "
				"This changes the shape without changing the look of the model."));
		form->addRow(m_shapePanel);
		MakePageScrollable(m_sourcePage, content);
		addPage(m_sourcePage);
		m_sourcePage->setTitle(tr("Hull"));
	}

	// Page 2: what the ship is for.
	{
		m_purposePage = new QWizardPage(this);
		QWidget* content = new QWidget(m_purposePage);
		QFormLayout* form = new QFormLayout(content);

		m_templateCombo = new QComboBox(m_purposePage);
		const QList<ShipTemplate> templates = AllTemplates();
		m_templateCombo->addItem(tr("(start from scratch)"), QString());
		for(const ShipTemplate& t : templates)
			m_templateCombo->addItem(t.label, t.key);
		connect(m_templateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { OnTemplateChanged(); });
		form->addRow(tr("Type:"), m_templateCombo);

		m_nameEdit = new QLineEdit(m_purposePage);
		form->addRow(tr("Name:"), m_nameEdit);

		m_crewSpin = new QSpinBox(m_purposePage);
		m_crewSpin->setRange(0, 10000);
		form->addRow(tr("Crew:"), m_crewSpin);

		m_descriptionEdit = new QPlainTextEdit(m_purposePage);
		m_descriptionEdit->setMaximumHeight(90);
		form->addRow(tr("Description:"), m_descriptionEdit);

		MakePageScrollable(m_purposePage, content);
		addPage(m_purposePage);
		m_purposePage->setTitle(tr("Purpose"));
	}

	// Page 3: the rooms.
	{
		m_roomsPage = new QWizardPage(this);
		QWidget* content = new QWidget(m_roomsPage);
		QVBoxLayout* layout = new QVBoxLayout(content);
		layout->addWidget(new QLabel(tr("The rooms the ship has to have. The length, beam and height are in feet. "
				"Rooms are dropped in order of importance when the hull is too small."), m_roomsPage));
		m_roomTable = new QTableWidget(0, kRoomColumns, m_roomsPage);
		m_roomTable->setHorizontalHeaderLabels(QStringList() << tr("Type") << tr("Name") << tr("Count")
				<< tr("Length") << tr("Beam") << tr("Height"));
		m_roomTable->horizontalHeader()->setStretchLastSection(true);
		connect(m_roomTable, &QTableWidget::cellChanged, this, [this](int row, int col) { OnRoomCellChanged(row, col); });
		layout->addWidget(m_roomTable);

		QHBoxLayout* buttons = new QHBoxLayout();
		QPushButton* addButton = new QPushButton(tr("Add room"), m_roomsPage);
		QPushButton* removeButton = new QPushButton(tr("Remove room"), m_roomsPage);
		connect(addButton, &QPushButton::clicked, this, &GenerationWizard::AddRoom);
		connect(removeButton, &QPushButton::clicked, this, &GenerationWizard::RemoveRoom);
		buttons->addWidget(addButton);
		buttons->addWidget(removeButton);
		buttons->addStretch();
		layout->addLayout(buttons);
		MakePageScrollable(m_roomsPage, content);
		addPage(m_roomsPage);
		m_roomsPage->setTitle(tr("Rooms"));
	}

	// Page 4: the stations.
	{
		m_stationsPage = new QWizardPage(this);
		QWidget* content = new QWidget(m_stationsPage);
		QVBoxLayout* layout = new QVBoxLayout(content);
		layout->addWidget(new QLabel(tr("The stations that have to be staffed. The room column says which kind of "
				"room the station belongs in."), m_stationsPage));
		m_stationTable = new QTableWidget(0, kStationColumns, m_stationsPage);
		m_stationTable->setHorizontalHeaderLabels(QStringList() << tr("Type") << tr("Count") << tr("Room"));
		m_stationTable->horizontalHeader()->setStretchLastSection(true);
		connect(m_stationTable, &QTableWidget::cellChanged, this, [this](int row, int col) { OnStationCellChanged(row, col); });
		layout->addWidget(m_stationTable);

		QHBoxLayout* buttons = new QHBoxLayout();
		QPushButton* addButton = new QPushButton(tr("Add station"), m_stationsPage);
		QPushButton* removeButton = new QPushButton(tr("Remove station"), m_stationsPage);
		connect(addButton, &QPushButton::clicked, this, &GenerationWizard::AddStation);
		connect(removeButton, &QPushButton::clicked, this, &GenerationWizard::RemoveStation);
		buttons->addWidget(addButton);
		buttons->addWidget(removeButton);
		buttons->addStretch();
		layout->addLayout(buttons);
		MakePageScrollable(m_stationsPage, content);
		addPage(m_stationsPage);
		m_stationsPage->setTitle(tr("Stations"));
	}

	// Page 5: the berths and the systems.
	{
		m_berthsPage = new QWizardPage(this);
		QWidget* content = new QWidget(m_berthsPage);
		QVBoxLayout* layout = new QVBoxLayout(content);

		QGroupBox* berthBox = new QGroupBox(tr("Crew accommodation"), m_berthsPage);
		QFormLayout* berthForm = new QFormLayout(berthBox);
		m_captSpin = new QSpinBox(berthBox);
		m_officerSpin = new QSpinBox(berthBox);
		m_crewBerthSpin = new QSpinBox(berthBox);
		m_passengerSpin = new QSpinBox(berthBox);
		m_troopSpin = new QSpinBox(berthBox);
		// The list has to be a named variable. A temporary would be gone
		// before the loop body runs.
		const QList<QSpinBox*> berthSpins = QList<QSpinBox*>() << m_captSpin << m_officerSpin
				<< m_crewBerthSpin << m_passengerSpin << m_troopSpin;
		for(QSpinBox* spin : berthSpins)
			spin->setRange(0, 10000);
		berthForm->addRow(tr("Captain:"), m_captSpin);
		berthForm->addRow(tr("Officer:"), m_officerSpin);
		berthForm->addRow(tr("Crew:"), m_crewBerthSpin);
		berthForm->addRow(tr("Passenger:"), m_passengerSpin);
		berthForm->addRow(tr("Troop:"), m_troopSpin);
		layout->addWidget(berthBox);

		QGroupBox* moduleBox = new QGroupBox(tr("Systems (cubic meters of hull)"), m_berthsPage);
		QVBoxLayout* moduleLayout = new QVBoxLayout(moduleBox);
		m_moduleTable = new QTableWidget(0, 2, moduleBox);
		m_moduleTable->setHorizontalHeaderLabels(QStringList() << tr("System") << tr("Cubic meters"));
		m_moduleTable->horizontalHeader()->setStretchLastSection(true);
		const QStringList modules = QStringList() << "powerPlant" << "maneuverDrive" << "capacitor" << "fuel"
				<< "lifeSupport" << "armor" << "skeleton" << "hold" << "sensors" << "shields";
		for(const QString& m : modules)
		{
			const int row = m_moduleTable->rowCount();
			m_moduleTable->insertRow(row);
			m_moduleTable->setItem(row, 0, new QTableWidgetItem(m));
			m_moduleTable->setItem(row, 1, MakeItem("0"));
		}
		moduleLayout->addWidget(m_moduleTable);
		layout->addWidget(moduleBox);
		MakePageScrollable(m_berthsPage, content);
		addPage(m_berthsPage);
		m_berthsPage->setTitle(tr("Crew and systems"));
	}

	// Page 6: the layout options.
	{
		m_optionsPage = new QWizardPage(this);
		QWidget* content = new QWidget(m_optionsPage);
		QFormLayout* form = new QFormLayout(content);

		m_strategyList = new QListWidget(m_optionsPage);
		m_strategyList->setSelectionMode(QAbstractItemView::NoSelection);
		const QStringList strategies = LayoutStrategies();
		for(const QString& s : strategies)
		{
			QListWidgetItem* item = new QListWidgetItem(s, m_strategyList);
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			item->setCheckState(Qt::Checked);
		}
		form->addRow(tr("Layouts:"), m_strategyList);

		m_wallSpin = new QDoubleSpinBox(m_optionsPage);
		m_wallSpin->setRange(0., 100.);
		m_wallSpin->setValue(8.);
		m_wallSpin->setSuffix(tr(" ft"));
		form->addRow(tr("Wall thickness:"), m_wallSpin);

		m_deckSpin = new QDoubleSpinBox(m_optionsPage);
		m_deckSpin->setRange(0., 100.);
		m_deckSpin->setValue(4.);
		m_deckSpin->setSuffix(tr(" ft"));
		form->addRow(tr("Deck thickness:"), m_deckSpin);

		m_variationsSpin = new QSpinBox(m_optionsPage);
		m_variationsSpin->setRange(1, 12);
		m_variationsSpin->setToolTip(tr("How many blueprints to make. Every one is a little different "
				"from the others, but they all have the same look."));
		form->addRow(tr("Variations:"), m_variationsSpin);

		m_repairCheck = new QCheckBox(tr("Put the model right"), m_optionsPage);
		m_repairCheck->setChecked(true);
		m_repairCheck->setToolTip(tr("Turns the triangles that wind the wrong way round the other way "
				"round, and puts a lid over the holes in the model. A model that is not closed cannot "
				"be measured, so the game cannot tell what is inside it and the volume comes out "
				"wrong."));
		form->addRow(m_repairCheck);

		MakePageScrollable(m_optionsPage, content);
		addPage(m_optionsPage);
		m_optionsPage->setTitle(tr("Layout"));
	}
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The building pages.

void GenerationWizard::BuildBuildingPages()
{
	// Page 1: what the building is.
	{
		m_buildingPage = new QWizardPage(this);
		QWidget* content = new QWidget(m_buildingPage);
		QFormLayout* form = new QFormLayout(content);

		m_templateCombo = new QComboBox(m_buildingPage);
		m_templateCombo->addItem(tr("(start from scratch)"), QString());
		const QList<BuildingTemplate> templates = AllBuildingTemplates();
		for(const BuildingTemplate& t : templates)
			m_templateCombo->addItem(t.label, t.key);
		connect(m_templateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { OnTemplateChanged(); });
		form->addRow(tr("Type:"), m_templateCombo);

		m_nameEdit = new QLineEdit(m_buildingPage);
		form->addRow(tr("Name:"), m_nameEdit);

		m_descriptionEdit = new QPlainTextEdit(m_buildingPage);
		m_descriptionEdit->setMaximumHeight(70);
		form->addRow(tr("Description:"), m_descriptionEdit);

		// A building can begin with a simple solid instead of a model: a
		// block, a round tower or a hexagon. The models are then put on it.
		m_basePanel = new BaseShapePanel(tr("Primitive base shape"), m_buildingPage);
		form->addRow(m_basePanel);

		m_buildingModelList = new FileList(m_buildingPage);
		m_buildingModelList->SetDialog(tr("Choose a model"), tr("Models (*.3ds *.stl *.obj);;All files (*)"));
		m_buildingModelList->SetListTip(tr("The files that the shell of the building is made from. "
				"A model that is in parts has more than one, and they are put together as one model."));
		form->addRow(tr("Model files:"), m_buildingModelList);
		form->addRow(MakePaintPanel(m_buildingPage));

		m_buildingFitSpin = new QDoubleSpinBox(m_buildingPage);
		m_buildingFitSpin->setRange(0., 100000.);
		m_buildingFitSpin->setDecimals(1);
		m_buildingFitSpin->setSpecialValueText(tr("as it is"));
		m_buildingFitSpin->setSuffix(tr(" ft"));
		m_buildingFitSpin->setToolTip(tr("Scale the model so that its largest side is this many feet. "
				"Handy for models that are in millimetres."));
		form->addRow(tr("Fit to:"), m_buildingFitSpin);

		m_footprintXSpin = new QDoubleSpinBox(m_buildingPage);
		m_footprintXSpin->setRange(20., 1000.);
		m_footprintXSpin->setValue(140.);
		m_footprintXSpin->setSuffix(tr(" ft"));
		form->addRow(tr("Footprint:"), m_footprintXSpin);

		m_footprintYSpin = new QDoubleSpinBox(m_buildingPage);
		m_footprintYSpin->setRange(20., 1000.);
		m_footprintYSpin->setValue(100.);
		m_footprintYSpin->setSuffix(tr(" ft"));
		form->addRow(QString(), m_footprintYSpin);

		m_floorsSpin = new QSpinBox(m_buildingPage);
		m_floorsSpin->setRange(1, 20);
		form->addRow(tr("Floors:"), m_floorsSpin);

		m_floorHeightSpin = new QDoubleSpinBox(m_buildingPage);
		m_floorHeightSpin->setRange(6., 60.);
		m_floorHeightSpin->setValue(14.);
		m_floorHeightSpin->setSuffix(tr(" ft"));
		form->addRow(tr("Floor height:"), m_floorHeightSpin);

		m_buildingShapePanel = new ShapePanel(tr("Shape of the model"), m_buildingPage);
		form->addRow(m_buildingShapePanel);

		// A set of buildings from the same model, each for something else.
		QGroupBox* setBox = new QGroupBox(tr("A set of buildings from the same model"), m_buildingPage);
		QVBoxLayout* setLayout = new QVBoxLayout(setBox);
		setLayout->addWidget(new QLabel(tr("Each row is another building, made from the same model but "
				"with its own shape. Leave the table empty to make just the one above."), setBox));
		m_setTable = new QTableWidget(0, 4, setBox);
		m_setTable->setHorizontalHeaderLabels(QStringList() << tr("Building") << tr("Look") << tr("Name")
				<< tr("Variations"));
		m_setTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
		m_setTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
		m_setTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
		m_setTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
		setLayout->addWidget(m_setTable);
		QHBoxLayout* setButtons = new QHBoxLayout();
		QPushButton* addSetButton = new QPushButton(tr("Add"), setBox);
		connect(addSetButton, &QPushButton::clicked, this, [this]() { AddSetRow(); });
		QPushButton* removeSetButton = new QPushButton(tr("Remove"), setBox);
		connect(removeSetButton, &QPushButton::clicked, this, [this]() { RemoveSetRow(); });
		setButtons->addWidget(addSetButton);
		setButtons->addWidget(removeSetButton);
		setButtons->addStretch(1);
		setLayout->addLayout(setButtons);
		form->addRow(setBox);

		MakePageScrollable(m_buildingPage, content);
		addPage(m_buildingPage);
		m_buildingPage->setTitle(tr("Building"));
	}

	// Page 2: the rooms.
	{
		m_roomsPage = new QWizardPage(this);
		QWidget* content = new QWidget(m_roomsPage);
		QVBoxLayout* layout = new QVBoxLayout(content);
		layout->addWidget(new QLabel(tr("The rooms the building has to have. The length, beam and height are in feet."),
				m_roomsPage));
		m_buildingRoomTable = new QTableWidget(0, kRoomColumns, m_roomsPage);
		m_buildingRoomTable->setHorizontalHeaderLabels(QStringList() << tr("Type") << tr("Name") << tr("Count")
				<< tr("Length") << tr("Beam") << tr("Height"));
		m_buildingRoomTable->horizontalHeader()->setStretchLastSection(true);
		connect(m_buildingRoomTable, &QTableWidget::cellChanged, this, [this](int row, int col) { OnRoomCellChanged(row, col); });
		layout->addWidget(m_buildingRoomTable);

		QHBoxLayout* buttons = new QHBoxLayout();
		QPushButton* addButton = new QPushButton(tr("Add room"), m_roomsPage);
		QPushButton* removeButton = new QPushButton(tr("Remove room"), m_roomsPage);
		connect(addButton, &QPushButton::clicked, this, &GenerationWizard::AddRoom);
		connect(removeButton, &QPushButton::clicked, this, &GenerationWizard::RemoveRoom);
		buttons->addWidget(addButton);
		buttons->addWidget(removeButton);
		buttons->addStretch();
		layout->addLayout(buttons);
		MakePageScrollable(m_roomsPage, content);
		addPage(m_roomsPage);
		m_roomsPage->setTitle(tr("Rooms"));
	}

	// Page 3: the volumes.
	{
		m_berthsPage = new QWizardPage(this);
		QWidget* content = new QWidget(m_berthsPage);
		QVBoxLayout* layout = new QVBoxLayout(content);
		layout->addWidget(new QLabel(tr("What the building provides. The volumes are in cubic meters."), m_berthsPage));
		m_buildingVolumeTable = new QTableWidget(0, 2, m_berthsPage);
		m_buildingVolumeTable->setHorizontalHeaderLabels(QStringList() << tr("System") << tr("Cubic meters"));
		m_buildingVolumeTable->horizontalHeader()->setStretchLastSection(true);
		const QStringList systems = QStringList() << "offices" << "shops" << "store" << "lounge" << "homesLarge"
				<< "homesMedium" << "homesSmall" << "powerPlant" << "lifeSupport" << "capacitor" << "sensors"
				<< "shields" << "hold" << "skeleton" << "armor";
		for(const QString& sys : systems)
		{
			const int row = m_buildingVolumeTable->rowCount();
			m_buildingVolumeTable->insertRow(row);
			m_buildingVolumeTable->setItem(row, 0, new QTableWidgetItem(sys));
			m_buildingVolumeTable->setItem(row, 1, MakeItem("0"));
		}
		layout->addWidget(m_buildingVolumeTable);
		MakePageScrollable(m_berthsPage, content);
		addPage(m_berthsPage);
		m_berthsPage->setTitle(tr("Systems"));
	}

	// Page 4: the layout options.
	{
		m_optionsPage = new QWizardPage(this);
		QWidget* content = new QWidget(m_optionsPage);
		QFormLayout* form = new QFormLayout(content);
		m_strategyList = new QListWidget(m_optionsPage);
		m_strategyList->setSelectionMode(QAbstractItemView::NoSelection);
		const QStringList strategies = QStringList() << "grid" << "corridor";
		for(const QString& s : strategies)
		{
			QListWidgetItem* item = new QListWidgetItem(s, m_strategyList);
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			item->setCheckState(Qt::Checked);
		}
		form->addRow(tr("Layouts:"), m_strategyList);

		m_variationsSpin = new QSpinBox(m_optionsPage);
		m_variationsSpin->setRange(1, 12);
		m_variationsSpin->setToolTip(tr("How many blueprints to make. Every one is a little different "
				"from the others, but they all have the same look."));
		form->addRow(tr("Variations:"), m_variationsSpin);

		m_repairCheck = new QCheckBox(tr("Put the model right"), m_optionsPage);
		m_repairCheck->setChecked(true);
		m_repairCheck->setToolTip(tr("Turns the triangles that wind the wrong way round the other way "
				"round, and puts a lid over the holes in the model. A model that is not closed cannot "
				"be measured, so the game cannot tell what is inside it and the volume comes out "
				"wrong."));
		form->addRow(m_repairCheck);

		MakePageScrollable(m_optionsPage, content);
		addPage(m_optionsPage);
		m_optionsPage->setTitle(tr("Layout"));
	}
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The set of buildings.

void GenerationWizard::AddSetRow()
{
	if(!m_setTable)
		return;

	const int row = m_setTable->rowCount();
	m_setTable->insertRow(row);

	// Which building it is.
	QComboBox* typeCombo = new QComboBox(m_setTable);
	const QList<BuildingTemplate> templates = AllBuildingTemplates();
	for(const BuildingTemplate& t : templates)
		typeCombo->addItem(t.label, t.key);
	m_setTable->setCellWidget(row, 0, typeCombo);

	// What shape the model is put into.
	QComboBox* shapeCombo = new QComboBox(m_setTable);
	shapeCombo->addItem(tr("As it is"), QString("as-is"));
	for(const ShapePreset& p : ShapePresets())
	{
		if(p.key == "as-is")
			continue;
		shapeCombo->addItem(p.label, p.key);
	}
	m_setTable->setCellWidget(row, 1, shapeCombo);

	// The name.
	QLineEdit* nameEdit = new QLineEdit(m_setTable);
	m_setTable->setCellWidget(row, 2, nameEdit);

	// How many versions of it.
	QSpinBox* variationSpin = new QSpinBox(m_setTable);
	variationSpin->setRange(1, 12);
	m_setTable->setCellWidget(row, 3, variationSpin);
}

void GenerationWizard::RemoveSetRow()
{
	if(!m_setTable)
		return;
	const int row = m_setTable->currentRow();
	if(row < 0)
		return;
	m_setTable->removeRow(row);
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// The slots.

void GenerationWizard::OnTemplateChanged()
{
	const QString key = m_templateCombo->currentData().toString();
	if(key.isEmpty())
		return;

	// The pickers only offer the rooms that suit this kind of blueprint.
	if(m_request.kind == BlueprintKind::Spacecraft)
	{
		m_roomTypes = ShipRoomTypesFor(key);
		m_stationTypes = StationTypes();
	}
	else
	{
		m_roomTypes = BuildingRoomTypesFor(key);
		m_stationTypes.clear();
	}

	if(m_request.kind == BlueprintKind::Spacecraft)
	{
		Brief brief;
		ApplyTemplate(brief, key);
		m_request.shipBrief = brief;
		m_crewSpin->setValue(brief.crew);
		if(m_nameEdit->text().isEmpty())
			m_nameEdit->setText(brief.name);
		RefreshRoomTable();
		RefreshStationTable();

		// The systems of the template.
		const ModuleSpec& modules = brief.modules;
		QMap<QString, quint32> values;
		values["powerPlant"] = modules.powerPlant;
		values["maneuverDrive"] = modules.maneuverDrive;
		values["capacitor"] = modules.capacitor;
		values["fuel"] = modules.fuel;
		values["lifeSupport"] = modules.lifeSupport;
		values["armor"] = modules.armor;
		values["skeleton"] = modules.skeleton;
		values["hold"] = modules.hold;
		values["sensors"] = modules.sensors;
		values["shields"] = modules.shields;
		for(int row = 0; row < m_moduleTable->rowCount(); ++row)
		{
			const QString name = m_moduleTable->item(row, 0)->text();
			m_moduleTable->item(row, 1)->setText(QString::number(values.value(name)));
		}

		m_captSpin->setValue(brief.berths.capt);
		m_officerSpin->setValue(brief.berths.officer);
		m_crewBerthSpin->setValue(brief.berths.crew);
		m_passengerSpin->setValue(brief.berths.passenger);
		m_troopSpin->setValue(brief.berths.troop);
	}
	else
	{
		BuildingBrief brief;
		ApplyBuildingTemplate(brief, key);
		m_request.buildingBrief = brief;
		m_footprintXSpin->setValue(brief.footprintX);
		m_footprintYSpin->setValue(brief.footprintY);
		m_floorsSpin->setValue(brief.floors);
		m_floorHeightSpin->setValue(brief.floorHeight);
		if(m_nameEdit->text().isEmpty())
			m_nameEdit->setText(brief.name);
		RefreshRoomTable();

		QMap<QString, quint32> values;
		values["offices"] = brief.volumes.offices;
		values["shops"] = brief.volumes.shops;
		values["store"] = brief.volumes.store;
		values["lounge"] = brief.volumes.lounge;
		values["homesLarge"] = brief.volumes.homesLarge;
		values["homesMedium"] = brief.volumes.homesMedium;
		values["homesSmall"] = brief.volumes.homesSmall;
		values["powerPlant"] = brief.volumes.powerPlant;
		values["lifeSupport"] = brief.volumes.lifeSupport;
		values["capacitor"] = brief.volumes.capacitor;
		values["sensors"] = brief.volumes.sensors;
		values["shields"] = brief.volumes.shields;
		values["hold"] = brief.volumes.hold;
		values["skeleton"] = brief.volumes.skeleton;
		values["armor"] = brief.volumes.armor;
		for(int row = 0; row < m_buildingVolumeTable->rowCount(); ++row)
		{
			const QString name = m_buildingVolumeTable->item(row, 0)->text();
			m_buildingVolumeTable->item(row, 1)->setText(QString::number(values.value(name)));
		}
	}
	emit BriefChanged();
}

void GenerationWizard::RefreshRoomTable()
{
	QTableWidget* table = (m_request.kind == BlueprintKind::Spacecraft) ? m_roomTable : m_buildingRoomTable;
	if(!table)
		return;
	table->blockSignals(true);
	table->setRowCount(0);
	const QList<RoomSpec> rooms = (m_request.kind == BlueprintKind::Spacecraft)
			? m_request.shipBrief.rooms : m_request.buildingBrief.rooms;
	for(const RoomSpec& r : rooms)
	{
		const int row = table->rowCount();
		table->insertRow(row);
		SetComboCell(table, row, 0, m_roomTypes, r.type);
		table->setItem(row, 1, MakeItem(r.label));
		table->setItem(row, 2, MakeItem(QString::number(r.count)));
		table->setItem(row, 3, MakeItem(QString::number(r.sizeX)));
		table->setItem(row, 4, MakeItem(QString::number(r.sizeY)));
		table->setItem(row, 5, MakeItem(QString::number(r.sizeZ)));
	}
	table->blockSignals(false);
}

QStringList GenerationWizard::StationRoomTypes() const
{
	// The rooms that are in the brief, and then the whole list, so that a
	// station can be pointed at a room that is added later.
	QStringList types;
	for(const RoomSpec& r : m_request.shipBrief.rooms)
	{
		if(!types.contains(r.type))
			types << r.type;
	}
	for(const QString& t : m_roomTypes)
	{
		if(!types.contains(t))
			types << t;
	}
	return types;
}

void GenerationWizard::RefreshStationTable()
{
	if(!m_stationTable)
		return;
	m_stationTable->blockSignals(true);
	m_stationTable->setRowCount(0);
	for(const StationSpec& s : m_request.shipBrief.stations)
	{
		const int row = m_stationTable->rowCount();
		m_stationTable->insertRow(row);
		SetComboCell(m_stationTable, row, 0, m_stationTypes, s.type);
		m_stationTable->setItem(row, 1, MakeItem(QString::number(s.count)));
		SetComboCell(m_stationTable, row, 2, StationRoomTypes(), s.roomType);
	}
	m_stationTable->blockSignals(false);
}

void GenerationWizard::OnRoomCellChanged(int row, int column)
{
	QTableWidget* table = (m_request.kind == BlueprintKind::Spacecraft) ? m_roomTable : m_buildingRoomTable;
	if(!table || row < 0 || row >= table->rowCount())
		return;
	QList<RoomSpec>& rooms = (m_request.kind == BlueprintKind::Spacecraft)
			? m_request.shipBrief.rooms : m_request.buildingBrief.rooms;
	if(row >= rooms.size())
		return;
	RoomSpec& r = rooms[row];
	if(column == 0)
	{
		const QString type = CellValue(table, row, column);
		if(type != r.type)
		{
			// Keep the name in step with the type unless it was changed.
			if(r.label == r.type || r.label.isEmpty())
				r.label = type;
			r.type = type;
			table->item(row, 1)->setText(r.label);
		}
		return;
	}
	QTableWidgetItem* item = table->item(row, column);
	if(!item)
		return;
	switch(column)
	{
	case 1: r.label = item->text(); break;
	case 2: r.count = item->text().toInt(); break;
	case 3: r.sizeX = item->text().toDouble(); break;
	case 4: r.sizeY = item->text().toDouble(); break;
	case 5: r.sizeZ = item->text().toDouble(); break;
	default: break;
	}
	emit BriefChanged();
}

void GenerationWizard::OnStationCellChanged(int row, int column)
{
	if(!m_stationTable || row < 0 || row >= m_stationTable->rowCount())
		return;
	if(row >= m_request.shipBrief.stations.size())
		return;
	StationSpec& s = m_request.shipBrief.stations[row];
	if(column == 0 || column == 2)
	{
		if(column == 0)
			s.type = CellValue(m_stationTable, row, column);
		else
			s.roomType = CellValue(m_stationTable, row, column);
		return;
	}
	QTableWidgetItem* item = m_stationTable->item(row, column);
	if(!item)
		return;
	switch(column)
	{
	case 1: s.count = item->text().toInt(); break;
	default: break;
	}
	emit BriefChanged();
}

void GenerationWizard::AddRoom()
{
	QTableWidget* table = (m_request.kind == BlueprintKind::Spacecraft) ? m_roomTable : m_buildingRoomTable;
	if(!table)
		return;
	const int row = table->rowCount();
	table->insertRow(row);
	SetComboCell(table, row, 0, m_roomTypes, m_roomTypes.isEmpty() ? QString("Hall") : m_roomTypes.first());
	table->setItem(row, 1, MakeItem(tr("New room")));
	table->setItem(row, 2, MakeItem("1"));
	table->setItem(row, 3, MakeItem("20"));
	table->setItem(row, 4, MakeItem("16"));
	table->setItem(row, 5, MakeItem("10"));
	if(m_request.kind == BlueprintKind::Spacecraft)
		m_request.shipBrief.rooms << RoomSpec("room", tr("New room"), 1, 20, 16, 10);
	else
		m_request.buildingBrief.rooms << RoomSpec("room", tr("New room"), 1, 20, 16, 10);
	emit BriefChanged();
}

void GenerationWizard::RemoveRoom()
{
	QTableWidget* table = (m_request.kind == BlueprintKind::Spacecraft) ? m_roomTable : m_buildingRoomTable;
	if(!table)
		return;
	const int row = table->currentRow();
	if(row < 0)
		return;
	table->removeRow(row);
	QList<RoomSpec>& rooms = (m_request.kind == BlueprintKind::Spacecraft)
			? m_request.shipBrief.rooms : m_request.buildingBrief.rooms;
	if(row < rooms.size())
		rooms.removeAt(row);
	emit BriefChanged();
}

void GenerationWizard::AddStation()
{
	if(!m_stationTable)
		return;
	const int row = m_stationTable->rowCount();
	m_stationTable->insertRow(row);
	SetComboCell(m_stationTable, row, 0, m_stationTypes, m_stationTypes.isEmpty() ? QString("helm") : m_stationTypes.first());
	m_stationTable->setItem(row, 1, MakeItem("1"));
	SetComboCell(m_stationTable, row, 2, StationRoomTypes(), QString());
	m_request.shipBrief.stations << StationSpec("helm", 1, QString());
	emit BriefChanged();
}

void GenerationWizard::RemoveStation()
{
	if(!m_stationTable)
		return;
	const int row = m_stationTable->currentRow();
	if(row < 0)
		return;
	m_stationTable->removeRow(row);
	if(row < m_request.shipBrief.stations.size())
		m_request.shipBrief.stations.removeAt(row);
	emit BriefChanged();
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

void GenerationWizard::SyncRequest()
{
	m_request.name = m_nameEdit->text();
	m_request.description = m_descriptionEdit->toPlainText();
	m_request.templateKey = m_templateCombo->currentData().toString();
	if(m_crewSpin)
		m_request.crew = m_crewSpin->value();
	if(m_modelList)
	{
		m_request.modelFiles = m_modelList->Files();
		m_request.hullFile = m_hullEdit->text();
		m_request.scale = m_scaleSpin->value();
		m_request.decimate = m_decimateSpin->value();
		m_request.fit = m_fitSpin->value();
	}
	if(m_shapePanel)
		m_request.shape = m_shapePanel->Shape();
	if(m_basePanel)
		m_request.base = m_basePanel->Shape();
	if(m_textureList)
		m_request.paint.textures = m_textureList->Files();
	if(m_paintCombo)
		PaintProjectionFromName(m_paintCombo->currentData().toString(), m_request.paint.projection);
	if(m_paintSizeSpin)
		m_request.paint.size = m_paintSizeSpin->value();
	if(m_paintPerPartCheck)
		m_request.paint.perPart = m_paintPerPartCheck->isChecked();
	if(m_variationsSpin)
		m_request.variations = m_variationsSpin->value();
	if(m_repairCheck)
		m_request.repair = m_repairCheck->isChecked();
	if(m_strategyList)
	{
		m_request.strategies.clear();
		for(int i = 0; i < m_strategyList->count(); ++i)
		{
			QListWidgetItem* item = m_strategyList->item(i);
			if(item->checkState() == Qt::Checked)
				m_request.strategies << item->text();
		}
	}
	if(m_wallSpin)
	{
		m_request.wall = m_wallSpin->value();
		m_request.deck = m_deckSpin->value();
	}

	if(m_request.kind == BlueprintKind::Spacecraft)
	{
		Brief& brief = m_request.shipBrief;
		brief.name = m_request.name;
		brief.description = m_request.description;
		brief.crew = m_request.crew;
		if(m_captSpin)
		{
			brief.berths.capt = m_captSpin->value();
			brief.berths.officer = m_officerSpin->value();
			brief.berths.crew = m_crewBerthSpin->value();
			brief.berths.passenger = m_passengerSpin->value();
			brief.berths.troop = m_troopSpin->value();
		}
		if(m_moduleTable)
		{
			for(int row = 0; row < m_moduleTable->rowCount(); ++row)
			{
				const QString name = m_moduleTable->item(row, 0)->text();
				const quint32 value = m_moduleTable->item(row, 1)->text().toUInt();
				if(name == "powerPlant")		brief.modules.powerPlant = value;
				else if(name == "maneuverDrive")	brief.modules.maneuverDrive = value;
				else if(name == "capacitor")		brief.modules.capacitor = value;
				else if(name == "fuel")			brief.modules.fuel = value;
				else if(name == "lifeSupport")	brief.modules.lifeSupport = value;
				else if(name == "armor")			brief.modules.armor = value;
				else if(name == "skeleton")		brief.modules.skeleton = value;
				else if(name == "hold")			brief.modules.hold = value;
				else if(name == "sensors")		brief.modules.sensors = value;
				else if(name == "shields")		brief.modules.shields = value;
			}
		}
	}
	else
	{
		BuildingBrief& brief = m_request.buildingBrief;
		brief.name = m_request.name;
		brief.description = m_request.description;
		if(m_buildingModelList)
		{
			m_request.buildingModelFiles = m_buildingModelList->Files();
			m_request.buildingFit = m_buildingFitSpin->value();
		}
		if(m_buildingShapePanel)
			m_request.buildingShape = m_buildingShapePanel->Shape();
		m_request.buildingSet.clear();
		if(m_setTable)
		{
			for(int row = 0; row < m_setTable->rowCount(); ++row)
			{
				BuildingSetEntry entry;
				if(QComboBox* combo = qobject_cast<QComboBox*>(m_setTable->cellWidget(row, 0)))
					entry.templateKey = combo->currentData().toString();
				if(QComboBox* combo = qobject_cast<QComboBox*>(m_setTable->cellWidget(row, 1)))
					entry.shapeKey = combo->currentData().toString();
				if(QLineEdit* edit = qobject_cast<QLineEdit*>(m_setTable->cellWidget(row, 2)))
					entry.name = edit->text();
				if(QSpinBox* spin = qobject_cast<QSpinBox*>(m_setTable->cellWidget(row, 3)))
					entry.variations = spin->value();
				if(entry.templateKey.isEmpty() && entry.name.isEmpty())
					continue;
				m_request.buildingSet << entry;
			}
		}
		if(m_footprintXSpin)
		{
			brief.footprintX = m_footprintXSpin->value();
			brief.footprintY = m_footprintYSpin->value();
			brief.floors = m_floorsSpin->value();
			brief.floorHeight = m_floorHeightSpin->value();
		}
		if(m_buildingVolumeTable)
		{
			BuildingVolumes& v = brief.volumes;
			for(int row = 0; row < m_buildingVolumeTable->rowCount(); ++row)
			{
				const QString name = m_buildingVolumeTable->item(row, 0)->text();
				const quint32 value = m_buildingVolumeTable->item(row, 1)->text().toUInt();
				if(name == "offices")			v.offices = value;
				else if(name == "shops")		v.shops = value;
				else if(name == "store")		v.store = value;
				else if(name == "lounge")		v.lounge = value;
				else if(name == "homesLarge")	v.homesLarge = value;
				else if(name == "homesMedium")	v.homesMedium = value;
				else if(name == "homesSmall")	v.homesSmall = value;
				else if(name == "powerPlant")	v.powerPlant = value;
				else if(name == "lifeSupport")	v.lifeSupport = value;
				else if(name == "capacitor")		v.capacitor = value;
				else if(name == "sensors")		v.sensors = value;
				else if(name == "shields")		v.shields = value;
				else if(name == "hold")			v.hold = value;
				else if(name == "skeleton")		v.skeleton = value;
				else if(name == "armor")			v.armor = value;
			}
		}
	}
}

void GenerationWizard::ChooseTemplate(const QString& templateKey)
{
	const int index = m_templateCombo->findData(templateKey);
	if(index < 0)
		return;
	m_templateCombo->setCurrentIndex(index);
	OnTemplateChanged();
}

void GenerationWizard::SetBrief(const Brief& brief)
{
	m_request.shipBrief = brief;
	m_crewSpin->setValue(brief.crew);
	m_nameEdit->setText(brief.name);
	RefreshRoomTable();
	RefreshStationTable();
}

void GenerationWizard::SetBuildingBrief(const BuildingBrief& brief)
{
	m_request.buildingBrief = brief;
	m_footprintXSpin->setValue(brief.footprintX);
	m_footprintYSpin->setValue(brief.footprintY);
	m_floorsSpin->setValue(brief.floors);
	m_floorHeightSpin->setValue(brief.floorHeight);
	m_nameEdit->setText(brief.name);
	RefreshRoomTable();
}
