//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the panel that chooses a primitive base shape.
//
// A building can begin with a simple solid instead of a model. The models are
// then put on top of that solid, one after another. Without a base the first
// model is the anchor and the rest are treated as parts of it.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_BASEPANEL_H
#define SHIPGEN_BASEPANEL_H

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>

#include "base.h"

class BaseShapePanel : public QGroupBox
{
	Q_OBJECT

public:
	explicit BaseShapePanel(const QString& title, QWidget* parent = 0);

	// The base the user has chosen.
	BaseShape	Shape() const;

	// Puts a base into the panel.
	void		SetShape(const BaseShape& base);

signals:
	// The user changed something.
	void	Changed();

private slots:
	void	OnKindChanged();
	void	OnValueChanged();
	void	UpdateEnabled();

private:
	void		SetShapeQuietly(const BaseShape& base);

	QComboBox*		m_kindCombo = 0;
	QDoubleSpinBox*	m_widthSpin = 0;
	QDoubleSpinBox*	m_depthSpin = 0;
	QDoubleSpinBox*	m_heightSpin = 0;
	QCheckBox*		m_separateCheck = 0;
	bool			m_setting = false;
};

#endif // SHIPGEN_BASEPANEL_H
