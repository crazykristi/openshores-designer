//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the panel that chooses a primitive base shape.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "basepanel.h"

#include <QGridLayout>
#include <QLabel>

BaseShapePanel::BaseShapePanel(const QString& title, QWidget* parent) :
	QGroupBox(title, parent)
{
	QGridLayout* grid = new QGridLayout(this);
	grid->setColumnStretch(1, 1);
	grid->setColumnStretch(3, 1);

	m_kindCombo = new QComboBox(this);
	m_kindCombo->addItem(tr("None - the first model is the anchor"), QString("none"));
	m_kindCombo->addItem(tr("Rectangle"), QString("rectangle"));
	m_kindCombo->addItem(tr("Circle"), QString("circle"));
	m_kindCombo->addItem(tr("Hexagon"), QString("hexagon"));
	m_kindCombo->setToolTip(tr("A simple closed solid to build on, before any model is added.\n\n"
			"With a base, every model file is put on it as its own object, laid out in a grid along "
			"the shape of the base. With no base, the files are parts of one model, and the first "
			"file is the anchor that the others keep their places around."));
	grid->addWidget(new QLabel(tr("Base shape:")), 0, 0);
	grid->addWidget(m_kindCombo, 0, 1, 1, 3);

	m_widthSpin = new QDoubleSpinBox(this);
	m_widthSpin->setRange(1., 100000.);
	m_widthSpin->setDecimals(1);
	m_widthSpin->setValue(140.);
	m_widthSpin->setSuffix(tr(" ft"));
	m_widthSpin->setToolTip(tr("How wide the base is, across the X axis."));
	grid->addWidget(new QLabel(tr("Width:")), 1, 0);
	grid->addWidget(m_widthSpin, 1, 1);

	m_depthSpin = new QDoubleSpinBox(this);
	m_depthSpin->setRange(1., 100000.);
	m_depthSpin->setDecimals(1);
	m_depthSpin->setValue(100.);
	m_depthSpin->setSuffix(tr(" ft"));
	m_depthSpin->setToolTip(tr("How deep the base is, across the Y axis."));
	grid->addWidget(new QLabel(tr("Depth:")), 1, 2);
	grid->addWidget(m_depthSpin, 1, 3);

	m_heightSpin = new QDoubleSpinBox(this);
	m_heightSpin->setRange(0.5, 10000.);
	m_heightSpin->setDecimals(1);
	m_heightSpin->setValue(4.);
	m_heightSpin->setSuffix(tr(" ft"));
	m_heightSpin->setToolTip(tr("How tall the base is. It is a solid, so it has a volume of its own. "
			"The models stand on its top."));
	grid->addWidget(new QLabel(tr("Height:")), 2, 0);
	grid->addWidget(m_heightSpin, 2, 1);

	m_separateCheck = new QCheckBox(tr("Place the models separately on the base"), this);
	m_separateCheck->setChecked(true);
	m_separateCheck->setToolTip(tr("On: every file is a separate object, put into a grid on the base. "
			"Off: the files are parts of one model and are put on the base together."));
	grid->addWidget(m_separateCheck, 3, 0, 1, 4);

	connect(m_kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BaseShapePanel::OnKindChanged);
	connect(m_widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BaseShapePanel::OnValueChanged);
	connect(m_depthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BaseShapePanel::OnValueChanged);
	connect(m_heightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BaseShapePanel::OnValueChanged);
	connect(m_separateCheck, &QCheckBox::toggled, this, &BaseShapePanel::OnValueChanged);

	UpdateEnabled();
}

BaseShape BaseShapePanel::Shape() const
{
	BaseShape base;
	FindBaseShape(m_kindCombo->currentData().toString(), base.kind);
	base.width = m_widthSpin->value();
	base.depth = m_depthSpin->value();
	base.height = m_heightSpin->value();
	base.separateModels = m_separateCheck->isChecked();
	return base;
}

void BaseShapePanel::SetShape(const BaseShape& base)
{
	SetShapeQuietly(base);
	emit Changed();
}

void BaseShapePanel::SetShapeQuietly(const BaseShape& base)
{
	m_setting = true;
	const int index = m_kindCombo->findData(base.Key());
	if(index >= 0)
		m_kindCombo->setCurrentIndex(index);
	m_widthSpin->setValue(base.width);
	m_depthSpin->setValue(base.depth);
	m_heightSpin->setValue(base.height);
	m_separateCheck->setChecked(base.separateModels);
	m_setting = false;
	UpdateEnabled();
}

void BaseShapePanel::OnKindChanged()
{
	UpdateEnabled();
	if(m_setting)
		return;
	emit Changed();
}

void BaseShapePanel::OnValueChanged()
{
	if(m_setting)
		return;
	emit Changed();
}

void BaseShapePanel::UpdateEnabled()
{
	const bool on = Shape().IsSet();
	m_widthSpin->setEnabled(on);
	m_depthSpin->setEnabled(on);
	m_heightSpin->setEnabled(on);
	m_separateCheck->setEnabled(on);
}
