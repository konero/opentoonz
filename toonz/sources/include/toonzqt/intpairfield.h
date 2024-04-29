#pragma once

#ifndef INTPAIRFIELD_H
#define INTPAIRFIELD_H

#include "toonzqt/intfield.h"
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
/*
 * The IntPairField class provides a view of an object to manage a pair of int
 * parameters.
 *
 * This class inherits from QWidget.
 *
 * The object is composed of a QHBoxLayout which contains two pairs
 * [label, text field] and a slider with two grabs, one for each int value to
 * manage. [label, text field] are a QLabel and an IntLineEdit.
 * 
 * Objects are inserted in the following order: a label and respective text
 * field, the slider, and the other pair [label,text field].
 * 
 * Object size width is not fixed, while height is equal to
 * DVGui::WidgetHeight, 20; labels width depend on its text length, text fields
 * have fixed size, while slider width changes in accordance with widget size.
 *
 * Objects contained in this widget are connected, so if you change one value
 * the other automatically changes, if it is necessary. You can set current
 * value, getValues(), minimum and max value, getRange(), using setValues(),
 * setRange().
 *
 * To know when one of two int parameter values change, the class provides a
 * signal, valuesChanged(); the class emits a signal when a grab slider position
 * changes or when editing text field, left or right is finished and current
 * value is changed.
 * 
 * Editing text field finished may occur if focus is lost or enter key is
 * pressed.
 * 
 * See SLOT: onLeftEditingFinished() and onRightEditingFinished().
 *
 * Example:
 *     IntPairField* intPairFieldExample = new IntPairField(this);
 *     intPairFieldExample->setLeftText(tr("Left Value:"));
 *     intPairFieldExample->setRightText(tr("Right Value:"));
 *     intPairFieldExample->setRange(0,10);
 *     intPairFieldExample->setValues(std::make_pair(3,8));
 *
 * Result:
 *     Refer to the image 'DoublePairField.jpg' for the result.
 */

class DVAPI IntPairField : public QWidget {
  Q_OBJECT

  QPixmap m_handlePixmap, m_handleGrayPixmap;
  QColor m_grooveColor, m_valueColor, m_borderColor;
  Q_PROPERTY(QPixmap HandlePixmap READ getHandlePixmap WRITE setHandlePixmap);
  Q_PROPERTY(QPixmap HandleGrayPixmap READ getHandleGrayPixmap WRITE
                 setHandleGrayPixmap);
  Q_PROPERTY(QColor GrooveColor READ getGrooveColor WRITE setGrooveColor);
  Q_PROPERTY(QColor ValueColor READ getValueColor WRITE setValueColor);
  Q_PROPERTY(QColor BorderColor READ getBorderColor WRITE setBorderColor);

  IntLineEdit *m_leftLineEdit;
  IntLineEdit *m_rightLineEdit;

  QLabel *m_leftLabel, *m_rightLabel;

  std::pair<int, int> m_values;
  int m_minValue, m_maxValue;
  int m_grabOffset, m_grabIndex;
  int m_leftMargin, m_rightMargin;

  bool m_isMaxRangeLimited;

  bool m_isLinear;

public:
  IntPairField(QWidget *parent = 0, bool isMaxRangeLimited = true);
  ~IntPairField() {}

  // Set current values to values.
  void setValues(const std::pair<int, int> &values);

  // Return a pair containing current values.
  std::pair<int, int> getValues() const { return m_values; }

  // Set left label string to QString text. Recompute left margin adding label
  // width.
  void setLeftText(const QString &text);

  // Set right label string to QString text. Recompute right margin adding label
  //width.
  void setRightText(const QString &text);

  void setLabelsEnabled(bool enable);

  // Set range of IntPairField to minValue, maxValue.
  void setRange(int minValue, int maxValue);

  // Set minValue and maxValue to IntPairField range.
  void getRange(int &minValue, int &maxValue);

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
  int pos2value(int x) const;

  // Return x position corresponding value.
  int value2pos(int v) const;

  // Set current value to value.
  // Set left or right value, or both, to value depending on current slider grab
  // edited and value.
  void setValue(int v);

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
   * right value is lower than left, change also current left value.
   * This protected slot is called when text editing is finished.
   * Emit valuesChanged().
   * If current right value is equal to right text field value return and do
   * nothing.
   */
  void onRightEditingFinished();

signals:
  /*
   * This signal is emitted when change one of two IntField value;
   * if one slider grab position change or if one text field editing is
   * finished.
   */
  void valuesChanged(bool isDragging);
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // INTPAIRFIELD_H
