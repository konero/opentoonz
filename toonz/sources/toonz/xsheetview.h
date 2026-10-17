#pragma once

#include "xsheetviewer.h"
#include "toonzqt/spreadsheetviewer.h"

// The vertical sheet panel.
class XsheetView final : public XsheetViewer {
  Spreadsheet::FrameScroller m_frameScroller;

public:
  explicit XsheetView(QWidget* parent       = nullptr,
                      Qt::WindowFlags flags = Qt::WindowFlags());
  CellPosition xyToPosition(const QPoint& point) const override;
  QPoint positionToXY(const CellPosition& position) const override;
  int columnToLayerAxis(int column) const override;
  int pasteColumnStart(int anchor, int) const override { return anchor; }
  void positionSections() override;
  void getButton(int type, QColor& background, QString& path) const override;
  XsheetGUI::ColumnHeaderStyle columnHeaderStyle() const override;
  void zoomOnFrame(int frame, int factor) override;

protected:
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  void enterEvent(QEvent* event) override;
  QPoint contentExtent() const override;
  QSize fitContentSize(const QSize& minimum) const override;
  int frameZoomFactor(int sliderValue) const override;
  int sliderValueForScale(double scale) const override;
  QScrollBar* frameScrollBar() const override;
  QScrollBar* layerScrollBar() const override;
  QScrollBar* rulerScrollBar() const override;
  QScrollBar* headerScrollBar() const override;
};
