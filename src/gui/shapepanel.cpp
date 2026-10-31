//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the panel that changes the shape of a model.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "shapepanel.h"

#include <QFormLayout>
#include <QLabel>
#include <QGridLayout>

#include "wizard.h"

ShapePanel::ShapePanel(const QString& title, QWidget* parent) :
	QGroupBox(title, parent)
{
	QGridLayout* grid = new QGridLayout(this);
	grid->setColumnStretch(1, 1);

	int row = 0;

	m_presetCombo = new QComboBox(this);
	m_presetCombo->addItem(tr("As it is"), QString("as-is"));
	for(const ShapePreset& p : ShapePresets())
	{
		if(p.key == "as-is")
			continue;
		m_presetCombo->addItem(p.label, p.key);
	}
	m_presetCombo->setToolTip(tr("Pick a look. The numbers under it can be changed afterwards."));
	grid->addWidget(new QLabel(tr("Look:")), row, 0);
	grid->addWidget(m_presetCombo, row, 1, 1, 3);

	row++;

	m_taperTopSpin = new QDoubleSpinBox(this);
	m_taperTopSpin->setRange(0.02, 4.);
	m_taperTopSpin->setSingleStep(0.05);
	m_taperTopSpin->setDecimals(2);
	m_taperTopSpin->setValue(1.);
	m_taperTopSpin->setToolTip(tr("How wide the model is at the top, as a part of how wide it is at the bottom."));
	grid->addWidget(new QLabel(tr("Narrow at the top:")), row, 0);
	grid->addWidget(m_taperTopSpin, row, 1);

	m_taperBottomSpin = new QDoubleSpinBox(this);
	m_taperBottomSpin->setRange(0.02, 4.);
	m_taperBottomSpin->setSingleStep(0.05);
	m_taperBottomSpin->setDecimals(2);
	m_taperBottomSpin->setValue(1.);
	m_taperBottomSpin->setToolTip(tr("How wide the model is at the bottom, as a part of how wide it is at the top."));
	grid->addWidget(new QLabel(tr("Narrow at the bottom:")), row, 2);
	grid->addWidget(m_taperBottomSpin, row, 3);

	row++;

	m_twistSpin = new QDoubleSpinBox(this);
	m_twistSpin->setRange(-1440., 1440.);
	m_twistSpin->setSingleStep(15.);
	m_twistSpin->setDecimals(0);
	m_twistSpin->setSuffix(tr(" degrees"));
	m_twistSpin->setToolTip(tr("How far the model turns about its axis from the bottom to the top."));
	grid->addWidget(new QLabel(tr("Twist:")), row, 0);
	grid->addWidget(m_twistSpin, row, 1);

	m_stretchSpin = new QDoubleSpinBox(this);
	m_stretchSpin->setRange(0.05, 5.);
	m_stretchSpin->setSingleStep(0.05);
	m_stretchSpin->setDecimals(2);
	m_stretchSpin->setValue(1.);
	m_stretchSpin->setToolTip(tr("How tall the model is, as a part of how tall it was."));
	grid->addWidget(new QLabel(tr("Height:")), row, 2);
	grid->addWidget(m_stretchSpin, row, 3);

	row++;

	m_repeatSpin = new QSpinBox(this);
	m_repeatSpin->setRange(1, 12);
	m_repeatSpin->setToolTip(tr("How many copies of the model are put around a circle."));
	grid->addWidget(new QLabel(tr("Copies around:")), row, 0);
	grid->addWidget(m_repeatSpin, row, 1);

	m_repeatRadiusSpin = new QDoubleSpinBox(this);
	m_repeatRadiusSpin->setRange(0., 3.);
	m_repeatRadiusSpin->setSingleStep(0.05);
	m_repeatRadiusSpin->setDecimals(2);
	m_repeatRadiusSpin->setValue(0.75);
	m_repeatRadiusSpin->setToolTip(tr("How far apart the copies are, as a part of the width of the model."));
	grid->addWidget(new QLabel(tr("Distance apart:")), row, 2);
	grid->addWidget(m_repeatRadiusSpin, row, 3);

	row++;

	m_stackSpin = new QSpinBox(this);
	m_stackSpin->setRange(1, 8);
	m_stackSpin->setToolTip(tr("How many copies of the model are put on top of each other."));
	grid->addWidget(new QLabel(tr("Copies upwards:")), row, 0);
	grid->addWidget(m_stackSpin, row, 1);

	m_stackShrinkSpin = new QDoubleSpinBox(this);
	m_stackShrinkSpin->setRange(0.2, 1.);
	m_stackShrinkSpin->setSingleStep(0.05);
	m_stackShrinkSpin->setDecimals(2);
	m_stackShrinkSpin->setValue(0.8);
	m_stackShrinkSpin->setToolTip(tr("How much smaller each copy is than the one under it."));
	grid->addWidget(new QLabel(tr("Shrink:")), row, 2);
	grid->addWidget(m_stackShrinkSpin, row, 3);

	row++;

	m_noiseSpin = new QDoubleSpinBox(this);
	m_noiseSpin->setRange(0., 0.3);
	m_noiseSpin->setSingleStep(0.01);
	m_noiseSpin->setDecimals(3);
	m_noiseSpin->setValue(0.);
	m_noiseSpin->setToolTip(tr("How much the surface is pushed out and in, as a part of the width of the model."));
	grid->addWidget(new QLabel(tr("Roughness:")), row, 0);
	grid->addWidget(m_noiseSpin, row, 1);

	m_noiseScaleSpin = new QDoubleSpinBox(this);
	m_noiseScaleSpin->setRange(0.01, 1.);
	m_noiseScaleSpin->setSingleStep(0.05);
	m_noiseScaleSpin->setDecimals(2);
	m_noiseScaleSpin->setValue(0.15);
	m_noiseScaleSpin->setToolTip(tr("How big the lumps are."));
	grid->addWidget(new QLabel(tr("Lump size:")), row, 2);
	grid->addWidget(m_noiseScaleSpin, row, 3);

	row++;

	m_seedSpin = new QSpinBox(this);
	m_seedSpin->setRange(1, 1000000);
	m_seedSpin->setToolTip(tr("The seed for the roughness. The same seed always gives the same shape."));
	grid->addWidget(new QLabel(tr("Seed:")), row, 0);
	grid->addWidget(m_seedSpin, row, 1);

	connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShapePanel::OnPresetChanged);
	const QList<QDoubleSpinBox*> spins = { m_taperTopSpin, m_taperBottomSpin, m_twistSpin, m_stretchSpin,
			m_repeatRadiusSpin, m_stackShrinkSpin, m_noiseSpin, m_noiseScaleSpin };
	for(QDoubleSpinBox* spin : spins)
		connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ShapePanel::OnValueChanged);
	connect(m_repeatSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ShapePanel::OnValueChanged);
	connect(m_stackSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ShapePanel::OnValueChanged);
	connect(m_seedSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ShapePanel::OnValueChanged);
}

ModelShape ShapePanel::Shape() const
{
	ModelShape shape;
	shape.taperTop = m_taperTopSpin->value();
	shape.taperBottom = m_taperBottomSpin->value();
	shape.twist = m_twistSpin->value();
	shape.repeats = m_repeatSpin->value();
	shape.repeatRadius = m_repeatSpin->value() > 1 ? m_repeatRadiusSpin->value() : 0.;
	shape.stack = m_stackSpin->value();
	shape.stackShrink = m_stackShrinkSpin->value();
	shape.noise = m_noiseSpin->value();
	shape.noiseScale = m_noiseScaleSpin->value();
	shape.stretch = m_stretchSpin->value();
	shape.seed = m_seedSpin->value();
	return shape;
}

void ShapePanel::SetShape(const ModelShape& shape)
{
	SetShapeQuietly(shape);
	emit Changed();
}

void ShapePanel::SetShapeQuietly(const ModelShape& shape)
{
	m_setting = true;
	m_taperTopSpin->setValue(shape.taperTop);
	m_taperBottomSpin->setValue(shape.taperBottom);
	m_twistSpin->setValue(shape.twist);
	m_repeatSpin->setValue(shape.repeats);
	m_repeatRadiusSpin->setValue(shape.repeatRadius > 0 ? shape.repeatRadius : 0.75);
	m_stackSpin->setValue(shape.stack);
	m_stackShrinkSpin->setValue(shape.stackShrink);
	m_noiseSpin->setValue(shape.noise);
	m_noiseScaleSpin->setValue(shape.noiseScale);
	m_stretchSpin->setValue(shape.stretch);
	m_seedSpin->setValue(shape.seed);
	m_setting = false;
}

QString ShapePanel::PresetKey() const
{
	return m_presetCombo->currentData().toString();
}

void ShapePanel::OnPresetChanged()
{
	if(m_setting)
		return;

	// Find the preset that was picked.
	const QString key = m_presetCombo->currentData().toString();
	ShapePreset preset;
	if(FindShapePreset(key, preset))
		SetShapeQuietly(preset.shape);
	emit Changed();
}

void ShapePanel::OnValueChanged()
{
	if(m_setting)
		return;

	// The numbers have been changed by hand, so the look is no longer one of
	// the ones on offer.
	if(m_presetCombo->currentData().toString() != "as-is")
	{
		m_setting = true;
		m_presetCombo->setCurrentIndex(0);
		m_setting = false;
	}
	emit Changed();
}
