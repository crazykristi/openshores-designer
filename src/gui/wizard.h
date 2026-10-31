//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the generation wizard.
//
// The wizard walks the user through choosing the hull, saying what the ship
// or building is for, listing the rooms and stations it needs and picking
// the layout options. It works on a GenerationRequest, which the main window
// then hands to the generator.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_WIZARD_H
#define SHIPGEN_WIZARD_H

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QWizard>
#include <QWizardPage>

#include "building.h"
#include "names.h"
#include "generate.h"
#include "shape.h"
#include "shipgen.h"

class BaseShapePanel;
class FileList;
class ShapePanel;

// What kind of blueprint is being made.
enum class BlueprintKind
{
	Spacecraft,
	Building
};

// Everything the wizard collects.
struct GenerationRequest
{
	BlueprintKind	kind = BlueprintKind::Spacecraft;

	// The source of the hull.
	QStringList		modelFiles;		// 3ds or stl models.
	double			fit = 0.0;		// Scale the model so its largest side is this many feet.
	QString			hullFile;		// An existing design to take the hull from.
	PaintOptions	paint;			// Textures to put on the hull.
	double			scale = 1.0;
	double			decimate = 0.0;
	ModelShape		shape;			// What to do to the shape of the model.
	BaseShape		base;			// A primitive the models are put on, if any.
	int				variations = 1;	// How many different shapes to make.

	// The brief.
	QString			name;
	QString			description;
	QString			templateKey;
	int				crew = 0;
	QStringList		buildingModelFiles;	// The model that becomes the shell of a building.
	double			buildingFit = 0.0;
	ModelShape		buildingShape;	// What to do to the shape of that model.
	bool			repair = true;	// Close the holes and put the windings right.
	QList<BuildingSetEntry>	buildingSet;	// Other buildings to make from the same model.

	// The layout options.
	QStringList		strategies;
	double			wall = 8.0;
	double			deck = 4.0;

	// The brief itself.
	Brief			shipBrief;
	BuildingBrief	buildingBrief;
};

// The wizard.
class GenerationWizard : public QWizard
{
	Q_OBJECT

public:
	explicit GenerationWizard(BlueprintKind kind, QWidget* parent = 0);

	// What the user filled in.
	GenerationRequest	Request() const { return m_request; }

	// Picks a template and fills the pages in from it.
	void				ChooseTemplate(const QString& templateKey);

	// Fills the wizard with a brief that was read from a file.
	void				SetBrief(const Brief& brief);
	void				SetBuildingBrief(const BuildingBrief& brief);

public slots:
	// Copies everything the user typed back into the brief.
	void				SyncRequest();

signals:
	// The brief changed.
	void	BriefChanged();

private slots:
	void	OnTemplateChanged();
	void	OnRoomCellChanged(int row, int column);
	void	OnStationCellChanged(int row, int column);
	void	AddRoom();
	void	RemoveRoom();
	void	AddStation();
	void	RemoveStation();
	void	AddSetRow();
	void	RemoveSetRow();

private:
	void	BuildShipPages();
	void	BuildBuildingPages();
	QWidget*	MakePaintPanel(QWidget* parent);
	void	RefreshRoomTable();
	void	RefreshStationTable();

	// The rooms a station can be put in.
	QStringList	StationRoomTypes() const;

	GenerationRequest	m_request;

	// The pages.
	QWizardPage*		m_sourcePage = 0;
	QWizardPage*		m_purposePage = 0;
	QWizardPage*		m_roomsPage = 0;
	QWizardPage*		m_stationsPage = 0;
	QWizardPage*		m_berthsPage = 0;
	QWizardPage*		m_optionsPage = 0;
	QWizardPage*		m_buildingPage = 0;

	// The widgets.
	BaseShapePanel*	m_basePanel = 0;
	FileList*		m_modelList = 0;
	QLineEdit*		m_hullEdit = 0;
	FileList*		m_textureList = 0;
	QComboBox*		m_paintCombo = 0;
	QDoubleSpinBox*	m_paintSizeSpin = 0;
	QCheckBox*		m_paintPerPartCheck = 0;
	QDoubleSpinBox*	m_scaleSpin = 0;
	QDoubleSpinBox*	m_fitSpin = 0;
	QDoubleSpinBox*	m_decimateSpin = 0;
	ShapePanel*		m_shapePanel = 0;
	QSpinBox*		m_variationsSpin = 0;
	QComboBox*		m_templateCombo = 0;
	QLineEdit*		m_nameEdit = 0;
	QPlainTextEdit*	m_descriptionEdit = 0;
	QSpinBox*		m_crewSpin = 0;
	QTableWidget*	m_roomTable = 0;
	QTableWidget*	m_stationTable = 0;
	QSpinBox*		m_captSpin = 0;
	QSpinBox*		m_officerSpin = 0;
	QSpinBox*		m_crewBerthSpin = 0;
	QSpinBox*		m_passengerSpin = 0;
	QSpinBox*		m_troopSpin = 0;
	QTableWidget*	m_moduleTable = 0;
	QListWidget*	m_strategyList = 0;
	QCheckBox*		m_repairCheck = 0;
	QDoubleSpinBox*	m_wallSpin = 0;
	QDoubleSpinBox*	m_deckSpin = 0;

	// The building page widgets.
	FileList*		m_buildingModelList = 0;
	QDoubleSpinBox*	m_buildingFitSpin = 0;
	ShapePanel*		m_buildingShapePanel = 0;
	QDoubleSpinBox*	m_footprintXSpin = 0;
	QDoubleSpinBox*	m_footprintYSpin = 0;
	QSpinBox*		m_floorsSpin = 0;
	QDoubleSpinBox*	m_floorHeightSpin = 0;
	QTableWidget*	m_buildingRoomTable = 0;
	QTableWidget*	m_buildingVolumeTable = 0;
	QTableWidget*	m_setTable = 0;

	// The room types the pickers offer.
	QStringList	m_roomTypes;
	QStringList	m_stationTypes;
};

#endif // SHIPGEN_WIZARD_H
