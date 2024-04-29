#pragma once

#ifndef DOUBLEPAIRFIELD_H
#define DOUBLEPAIRFIELD_H

#include "toonzqt/doublefield.h"
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class QSlider;
class QLabel;

//=============================================================================

namespace DVGui {

//=============================================================================
/*!
 * The DoubleValuePairField class provides a view to manage a pair of double
 * parameters. Inherits QWidget.
 *
 * The object is composed of a horizontal layout QHBoxLayout which contains two
 * pairs [label, text field] and a slider with two grabs, one for each double
 * value to manage. [label, text field] are a QLabel and a DoubleLineEdit.
 * Objects are inserted in the following order: a label and respective text
 * field, the slider, and the other pair [label, text field].
 *
 * Object width is not fixed, while height is equal to DVGui::WidgetHeight (20).
 * Labels' width depends on their text length, text fields have a fixed size,
 * while the slider width changes according to the widget size.
 *
 * Objects contained in this widget are connected, so if you change one value,
 * the other automatically changes, if necessary. You can set the current value,
 * getValues(), minimum and maximum value, getRange(), using setValues(),
 * setRange().
 *
 * To know when one of the two double parameter values changes, the class
 * provides a signal valuesChanged(); the class emits the signal when a grab
 * slider position changes or when editing a text field (left or right) is
 * finished and the current value changes. Editing a text field finished may
 * occur if focus is lost or the enter key is pressed. See SLOT:
 * onLeftEditingFinished() and onRightEditingFinished().
 */

class DVAPI DoubleValuePairField : public QWidget {
  Q_OBJECT

protected:
  QPixmap m_handlePixmap, m_handleGrayPixmap;
  QColor m_grooveColor, m_valueColor, m_borderColor;
  Q_PROPERTY(QPixmap HandlePixmap READ getHandlePixmap WRITE setHandlePixmap);
  Q_PROPERTY(QPixmap HandleGrayPixmap READ getHandleGrayPixmap WRITE
                 setHandleGrayPixmap);
  Q_PROPERTY(QColor GrooveColor READ getGrooveColor WRITE setGrooveColor);
  Q_PROPERTY(QColor ValueColor READ getValueColor WRITE setValueColor);
  Q_PROPERTY(QColor BorderColor READ getBorderColor WRITE setBorderColor);

  DoubleValueLineEdit *m_leftLineEdit;
  DoubleValueLineEdit *m_rightLineEdit;

  QLabel *m_leftLabel, *m_rightLabel;

  std::pair<double, double> m_values;
  double m_minValue, m_maxValue;
  int m_grabOffset, m_grabIndex;
  int m_leftMargin, m_rightMargin;

  bool m_isMaxRangeLimited;

  bool m_isLinear;

public:
  DoubleValuePairField(QWidget *parent, bool isMaxRangeLimited,
                       DoubleValueLineEdit *leftLineEdit,
                       DoubleValueLineEdit *rightLineEdit);
  ~DoubleValuePairField() {}

  // Set current values to values.
  void setValues(const std::pair<double, double> &values);

  // Return a pair containing current values.
  std::pair<double, double> getValues() const { return m_values; }

  // Set left label string to QString text. Recompute left margin adding label
  // width.
  void setLeftText(const QString &text);

  // Set right label string to QString text. Recompute right margin adding label
  // width.
  void setRightText(const QString &text);

  void setLabelsEnabled(bool enable);

  // Set range of DoublePairField to minValue, maxValue.
  void setRange(double minValue, double maxValue);

  // Set minValue and maxValue to DoublePairField range.
  void getRange(double &minValue, double &maxValue);

  QPixmap getHandlePixmap() const { return m_handlePixmap; }
  void setHandlePixmap(const QPixmap &pixmap) { m_handlePixmap = pixmap; }
  QPixmap getHandleGrayPixmap() const { return m_handleGrayPixmap; }
  void setHandleGrayPixmap(const QPixmap &pixmap) {
    m_handleGrayPixmap = pixmap;
  }
  QColor getGrooveColor() const { return m_grooveColor; }
  void setGrooveColor(const QColor &color) { m_grooveColor = color; }
  QColor getValueColor() const { return m_valueColor; }
  void setValueColor(const QColor &color) { m_valueColor = color; }
  QColor getBorderColor() const { return m_borderColor; }
  void setBorderColor(const QColor &color) { m_borderColor = color; }

protected:
  // Return value corresponding x position.
  double pos2value(int x) const;

  // Return x position corresponding value.
  int value2pos(double v) const;

  // Set current value to value.
  // Set left or right value, or both, to value depending on current slider grab edited and value.
  void setValue(double v);

  void paintEvent(QPaintEvent *) override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

  void setLinearSlider(bool linear) { m_isLinear = linear; }

protected slots:
  /*
   * Set current left value to value in left text field; if necessary, if left
   * value is greater than right, change also current right value.
   * This protected slot is called when text editing is finished.
   * Emit valuesChanged().
   * If current left value is equal to left text field value, return and do
   * nothing.
   */
  void onLeftEditingFinished();

  /*
   * Set current right value to value in right text field; if necessary, if
   * right value is lower than left, change also current left value. This
   * protected slot is called when text editing is finished. Emit
   * valuesChanged(). If current right value is equal to right text field value
   * return and do nothing.
   */
  void onRightEditingFinished();

signals:
  /*
   * This signal is emitted when change one of two DoubleField value;
   * if one slider grab position change or if one text field editing is
   * finished.
   */
  void valuesChanged(bool isDragging);
};

//=============================================================================
/*
 * The DoublePairField class provides a DoubleValuePairField with
 * left DoubleLineEdit and right DoubleLineEdit.
 *
 * This class inherits from DoubleValuePairField.
 *
 * Example:
 *   DoublePairField* doublePairFieldExample = new DoublePairField(this);
 *   doublePairFieldExample->setLeftText(tr("Left Value:"));
 *   doublePairFieldExample->setRightText(tr("Right Value:"));
 *   doublePairFieldExample->setRange(0,10);
 *   doublePairFieldExample->setValues(std::make_pair(3.58,7.65));
 *
 * Result:
 *   Refer to the image 'DoublePairField.jpg' for the result.
 */

class DVAPI DoublePairField : public DoubleValuePairField {
public:
  DoublePairField(QWidget *parent = 0, bool isMaxRangeLimited = true);
  ~DoublePairField() {}
};

//=============================================================================
/*
 * The MeasuredDoublePairField class provides a DoubleValuePairField with
 * left MeasuredDoubleLineEdit and right DoubleLineEdit.
 *
 * This class inherits from DoubleValuePairField.
 *
 * Example:
 *   MeasuredDoublePairField *doublePairFieldExample =
 *     new MeasuredDoublePairField(this);
 *   doublePairFieldExample->setLeftText(tr("Left Value:"));
 *   doublePairFieldExample->setRightText(tr("Right Value:"));
 *   doublePairFieldExample->setRange(0, 10);
 *   doublePairFieldExample->setValues(std::make_pair(3.58, 7.65));
 *
 * Result:
 *   Refer to the image 'DoublePairField.jpg' for the result.
 */

class DVAPI MeasuredDoublePairField final : public DoubleValuePairField {
public:
  MeasuredDoublePairField(QWidget *parent = 0, bool isMaxRangeLimited = true);
  ~MeasuredDoublePairField() {}

  void setMeasure(std::string measureName);

  void setPrecision(int precision);
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // DOUBLEPAIRFIELD_H
