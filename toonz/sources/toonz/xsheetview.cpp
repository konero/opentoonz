#include "xsheetview.h"
#include "xsheetviewgeometry.h"
#include "tapp.h"

#include "toonz/txsheet.h"
#include "toonz/columnfan.h"
#include "toonz/preferences.h"
#include "toonz/txsheethandle.h"

#include <QScrollBar>
#include <cmath>

XsheetView::XsheetView(QWidget* parent, Qt::WindowFlags flags)
    : XsheetViewer(Orientations::topToBottom(),
                  Preferences::instance()->getLoadedXsheetLayout(), parent,
                  flags) {
  initialize();
  m_frameScroller.setFrameScrollArea(m_cellScrollArea);
  connect(&m_frameScroller, &Spreadsheet::FrameScroller::prepareToScrollOffset,
          this, [this](const QPointF& offset) {
            refreshContentSize(0, int(offset.y()));
          });
  connect(&m_frameScroller, &Spreadsheet::FrameScroller::zoomScrollAdjust, this,
          [this](QPointF& offset, bool toZoom) {
            const double scale = getFrameZoomFactor() / 100.0;
            offset.setY(toZoom ? offset.y() * scale : offset.y() / scale);
          });
  connect(this, &XsheetViewer::frameSpacingChanged, this, [this](int percent) {
    if (isVisible())
      TApp::instance()->getCurrentXsheet()->notifyZoomScaleChanged(percent);
  });
}

void XsheetView::showEvent(QShowEvent* event) {
  XsheetViewer::showEvent(event);
  TApp::instance()->getCurrentXsheet()->notifyZoomScaleChanged(
      frameSpacingPercent());
  m_frameScroller.registerFrameScroller();
}

void XsheetView::hideEvent(QHideEvent* event) {
  m_frameScroller.unregisterFrameScroller();
  XsheetViewer::hideEvent(event);
}

void XsheetView::enterEvent(QEvent* event) {
  XsheetViewer::enterEvent(event);
  TApp::instance()->getCurrentXsheet()->notifyZoomScaleChanged(
      frameSpacingPercent());
}

void XsheetView::zoomOnFrame(int frame, int factor) {
  // Zoom anchoring moves the scrollbar locally; it is not a scroll gesture.
  const bool syncing = m_frameScroller.isSyncing();
  m_frameScroller.setSyncing(true);
  XsheetViewer::zoomOnFrame(frame, factor);
  m_frameScroller.setSyncing(syncing);
}

//-----------------------------------------------------------------------------
CellPosition XsheetView::xyToPosition(const QPoint& point) const {
  if (!getXsheet()) return CellPosition();
  return XsheetViewGeometry::cellAt(
      point, *columnFan(), orientation()->dimension(PredefinedDimension::FRAME),
      getFrameZoomFactor());
}

//-----------------------------------------------------------------------------
QPoint XsheetView::positionToXY(const CellPosition& position) const {
  if (!getXsheet()) return QPoint();
  return XsheetViewGeometry::pointAt(
      position, *columnFan(),
      orientation()->dimension(PredefinedDimension::FRAME),
      getFrameZoomFactor());
}

//-----------------------------------------------------------------------------
int XsheetView::columnToLayerAxis(int column) const {
  return getXsheet() ? columnFan()->colToLayerAxis(column) : 0;
}

void XsheetView::positionSections() {
  if (!isVisible()) return;
  const Orientation* o    = orientation();
  const int top           = positionToolbar();
  NumberRange headerLayer = o->range(PredefinedRange::HEADER_LAYER);
  NumberRange headerFrame =
      o->range(PredefinedRange::HEADER_FRAME).adjusted(top, top);
  NumberRange bodyLayer(headerLayer.to(), width() - 1);
  NumberRange bodyFrame(headerFrame.to(), height() - 1);
  m_noteScrollArea->setGeometry(o->frameLayerRect(headerFrame, headerLayer));
  m_cellScrollArea->setGeometry(o->frameLayerRect(bodyFrame, bodyLayer));
  m_columnScrollArea->setGeometry(o->frameLayerRect(
      headerFrame.adjusted(-1, -1), bodyLayer.adjusted(0, -16)));
  m_rowScrollArea->setGeometry(
      o->frameLayerRect(bodyFrame.adjusted(0, -16), headerLayer));
  m_layerFooterPanel->setGeometry(m_columnScrollArea->geometry().right() + 1,
                                  m_columnScrollArea->y(), 14,
                                  m_columnScrollArea->height());
  m_layerFooterPanel->showOrHide(o);
}

QPoint XsheetView::contentExtent() const {
  TXsheet* sheet = getXsheet();
  return sheet ? positionToXY(CellPosition(sheet->getFrameCount() + 1,
                                           sheet->getColumnCount() + 1))
               : QPoint();
}

QSize XsheetView::fitContentSize(const QSize& minimum) const {
  const QPoint extent = contentExtent();
  return QSize(std::max(extent.x(), minimum.width()),
               std::max(extent.y(), minimum.height()));
}

int XsheetView::frameZoomFactor(int value) const {
  return 50 + (value - 20) * 5 / 8;
}
int XsheetView::sliderValueForScale(double scale) const {
  return int(std::round((0.2 + (scale - 0.5) * 8.0 / 5.0) * 100));
}
QScrollBar* XsheetView::frameScrollBar() const {
  return m_cellScrollArea->verticalScrollBar();
}
QScrollBar* XsheetView::layerScrollBar() const {
  return m_cellScrollArea->horizontalScrollBar();
}

//-----------------------------------------------------------------------------
void XsheetView::getButton(int type, QColor& background, QString& path) const {
  switch (type) {
  case PREVIEW_ON_XSHBUTTON:
    background = getXsheetPreviewButtonBgOnColor();
    path       = getXsheetPreviewButtonOnImage();
    break;
  case PREVIEW_OFF_XSHBUTTON:
    background = getXsheetPreviewButtonBgOffColor();
    path       = getXsheetPreviewButtonOffImage();
    break;
  case CAMSTAND_ON_XSHBUTTON:
    background = getXsheetCamstandButtonBgOnColor();
    path       = getXsheetCamstandButtonOnImage();
    break;
  case CAMSTAND_OFF_XSHBUTTON:
    background = getXsheetCamstandButtonBgOffColor();
    path       = getXsheetCamstandButtonOffImage();
    break;
  case CAMSTAND_TRANSP_XSHBUTTON:
    background = getXsheetCamstandButtonBgOnColor();
    path       = getXsheetCamstandButtonTranspImage();
    break;
  case LOCK_ON_XSHBUTTON:
    background = getXsheetLockButtonBgOnColor();
    path       = getXsheetLockButtonOnImage();
    break;
  case LOCK_OFF_XSHBUTTON:
    background = getXsheetLockButtonBgOffColor();
    path       = getXsheetLockButtonOffImage();
    break;
  case CONFIG_XSHBUTTON:
    background = getXsheetConfigButtonBgColor();
    path       = getXsheetConfigButtonImage();
    break;
  case UNIFIED_TRANSP_XSHBUTTON:
    background = getXsheetPreviewButtonBgOnColor();
    path       = getXsheetUnifiedButtonTranspImage();
    break;
  default:
    background = QColor(210, 210, 210);
    path.clear();
  }
}

XsheetGUI::ColumnHeaderStyle XsheetView::columnHeaderStyle() const {
  XsheetGUI::ColumnHeaderStyle style;
  const bool classic          = getXsheetLayout() == "Classic";
  style.iconBorder            = getColumnIconLineColor();
  style.highlightNameOnHover  = !classic;
  style.hideInactiveIcons     = classic;
  style.previewBacklightsName = classic;
  style.cameraNameRotation    = 90;
  if (classic || Preferences::instance()->isShowColumnNumbersEnabled())
    style.nameRightInset = -20;
  style.volumeTickAdvance   = classic ? QPoint(0, 3) : QPoint(3, 0);
  style.volumeTickDirection = classic ? QPoint(-1, 0) : QPoint(0, 1);
  return style;
}

QScrollBar* XsheetView::rulerScrollBar() const {
  return m_rowScrollArea->verticalScrollBar();
}
QScrollBar* XsheetView::headerScrollBar() const {
  return m_columnScrollArea->horizontalScrollBar();
}
