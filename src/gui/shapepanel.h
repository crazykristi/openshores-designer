//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the panel that changes the shape of a model.
//
// The panel gives the user a list of looks to pick from and the numbers
// behind them, so that a model can be made into something else without
// having to know what the numbers mean.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_SHAPEPANEL_H
#define SHIPGEN_SHAPEPANEL_H

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QSpinBox>

#include "shape.h"

// The panel that changes the shape of a model.
class ShapePanel : public QGroupBox
{
	Q_OBJECT

public:
	explicit ShapePanel(const QString& title, QWidget* parent = 0);

	// The shape the user has chosen.
	ModelShape	Shape() const;

	// Puts a shape into the panel. The preset combo follows it.
	void		SetShape(const ModelShape& shape);

	// The key of the preset that is picked, or an empty string when the
	// numbers have been changed by hand.
	QString		PresetKey() const;

signals:
	// The user changed something.
	void	Changed();

private slots:
	void	OnPresetChanged();
	void	OnValueChanged();

private:
	// Puts the numbers into the boxes without them firing the changed signal.
	void	SetShapeQuietly(const ModelShape& shape);

	QComboBox*		m_presetCombo = 0;
	QDoubleSpinBox*	m_taperTopSpin = 0;
	QDoubleSpinBox*	m_taperBottomSpin = 0;
	QDoubleSpinBox*	m_twistSpin = 0;
	QSpinBox*		m_repeatSpin = 0;
	QDoubleSpinBox*	m_repeatRadiusSpin = 0;
	QSpinBox*		m_stackSpin = 0;
	QDoubleSpinBox*	m_stackShrinkSpin = 0;
	QDoubleSpinBox*	m_noiseSpin = 0;
	QDoubleSpinBox*	m_noiseScaleSpin = 0;
	QDoubleSpinBox*	m_stretchSpin = 0;
	QSpinBox*		m_seedSpin = 0;

	// Stops the boxes from telling the panel it changed while it is being
	// filled in.
	bool	m_setting = false;
};

#endif // SHIPGEN_SHAPEPANEL_H
