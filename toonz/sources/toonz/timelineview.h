#pragma once

#include "xsheetviewer.h"

// The horizontal layer/time panel. Its state and geometry are independent.
class TimelineView final : public XsheetViewer {
public:
  explicit TimelineView(QWidget* parent       = nullptr,
                        Qt::WindowFlags flags = Qt::WindowFlags());
  CellPosition xyToPosition(const QPoint& point) const override;
  QPoint positionToXY(const CellPosition& position) const override;
  int columnToLayerAxis(int column) const override;
  int pasteColumnStart(int anchor, int span) const override {
    return anchor - span + 1;
  }
  void positionSections() override;
  void getButton(int type, QColor& background, QString& path) const override;
  XsheetGUI::ColumnHeaderStyle columnHeaderStyle() const override;

protected:
  QPoint contentExtent() const override;
  QSize fitContentSize(const QSize& minimum) const override;
  int frameZoomFactor(int sliderValue) const override;
  int sliderValueForScale(double scale) const override;
  QScrollBar* frameScrollBar() const override;
  QScrollBar* layerScrollBar() const override;
  QScrollBar* rulerScrollBar() const override;
  QScrollBar* headerScrollBar() const override;
};
