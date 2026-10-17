#include "timelineview.h"
#include "timelineviewgeometry.h"

#include "toonz/txsheet.h"
#include "toonz/columnfan.h"
#include "toonz/preferences.h"

#include <QScrollBar>
#include <cmath>

TimelineView::TimelineView(QWidget* parent, Qt::WindowFlags flags)
    : XsheetViewer(Orientations::leftToRight(), QString(), parent, flags) {
  initialize();
}

//-----------------------------------------------------------------------------
CellPosition TimelineView::xyToPosition(const QPoint& point) const {
  if (!getXsheet()) return CellPosition();
  return TimelineViewGeometry::cellAt(
      point, *columnFan(), orientation()->dimension(PredefinedDimension::FRAME),
      orientation()->cellHeight(), getXsheet()->getColumnCount(),
      getFrameZoomFactor());
}

//-----------------------------------------------------------------------------
QPoint TimelineView::positionToXY(const CellPosition& position) const {
  if (!getXsheet()) return QPoint();
  return TimelineViewGeometry::pointAt(
      position, *columnFan(),
      orientation()->dimension(PredefinedDimension::FRAME),
      orientation()->cellHeight(), orientation()->foldedCellSize(),
      getXsheet()->getColumnCount(), getFrameZoomFactor());
}

//-----------------------------------------------------------------------------
int TimelineView::columnToLayerAxis(int column) const {
  return positionToXY(CellPosition(0, column)).y();
}

void TimelineView::positionSections() {
  if (!isVisible()) return;
  const Orientation* o    = orientation();
  const int top           = positionToolbar();
  NumberRange headerFrame = o->range(PredefinedRange::HEADER_FRAME);
  NumberRange headerLayer =
      o->range(PredefinedRange::HEADER_LAYER).adjusted(top, top);
  NumberRange bodyFrame(headerFrame.to(), width() - 1);
  NumberRange bodyLayer(headerLayer.to(), height() - 1);
  m_noteScrollArea->setGeometry(o->frameLayerRect(headerFrame, headerLayer));
  m_cellScrollArea->setGeometry(o->frameLayerRect(bodyFrame, bodyLayer));
  m_columnScrollArea->setGeometry(o->frameLayerRect(
      headerFrame.adjusted(-1, -1), bodyLayer.adjusted(0, -16)));
  m_rowScrollArea->setGeometry(
      o->frameLayerRect(bodyFrame.adjusted(0, -16), headerLayer));
  m_layerFooterPanel->setGeometry(0,
                                  m_columnScrollArea->geometry().bottom() + 1,
                                  m_columnScrollArea->width(), 14);
  m_layerFooterPanel->showOrHide(o);
}

QPoint TimelineView::contentExtent() const {
  TXsheet* sheet = getXsheet();
  if (!sheet) return QPoint();
  const int first =
      Preferences::instance()->isXsheetCameraColumnVisible() ? -1 : 0;
  QPoint result = positionToXY(CellPosition(sheet->getFrameCount() + 1, first));
  result.ry() +=
      1 + (columnFan()->isActive(first) ? orientation()->cellHeight()
                                        : orientation()->foldedCellSize());
  return result;
}

QSize TimelineView::fitContentSize(const QSize& minimum) const {
  const QPoint extent = contentExtent();
  return QSize(std::max(extent.x(), minimum.width()), std::max(1, extent.y()));
}

int TimelineView::frameZoomFactor(int value) const { return value; }
int TimelineView::sliderValueForScale(double scale) const {
  return int(std::round(scale * 100));
}
QScrollBar* TimelineView::frameScrollBar() const {
  return m_cellScrollArea->horizontalScrollBar();
}
QScrollBar* TimelineView::layerScrollBar() const {
  return m_cellScrollArea->verticalScrollBar();
}

//-----------------------------------------------------------------------------
void TimelineView::getButton(int type, QColor& background,
                             QString& path) const {
  switch (type) {
  case PREVIEW_ON_XSHBUTTON:
    background = getTimelinePreviewButtonBgOnColor();
    path       = getTimelinePreviewButtonOnImage();
    break;
  case PREVIEW_OFF_XSHBUTTON:
    background = getTimelinePreviewButtonBgOffColor();
    path       = getTimelinePreviewButtonOffImage();
    break;
  case CAMSTAND_ON_XSHBUTTON:
    background = getTimelineCamstandButtonBgOnColor();
    path       = getTimelineCamstandButtonOnImage();
    break;
  case CAMSTAND_OFF_XSHBUTTON:
    background = getTimelineCamstandButtonBgOffColor();
    path       = getTimelineCamstandButtonOffImage();
    break;
  case CAMSTAND_TRANSP_XSHBUTTON:
    background = getTimelineCamstandButtonBgOnColor();
    path       = getTimelineCamstandButtonTranspImage();
    break;
  case LOCK_ON_XSHBUTTON:
    background = getTimelineLockButtonBgOnColor();
    path       = getTimelineLockButtonOnImage();
    break;
  case LOCK_OFF_XSHBUTTON:
    background = getTimelineLockButtonBgOffColor();
    path       = getTimelineLockButtonOffImage();
    break;
  case CONFIG_XSHBUTTON:
    background = getTimelineConfigButtonBgColor();
    path       = getTimelineConfigButtonImage();
    break;
  case UNIFIED_TRANSP_XSHBUTTON:
    background = getTimelinePreviewButtonBgOnColor();
    path       = getTimelineUnifiedButtonTranspImage();
    break;
  default:
    background = QColor(210, 210, 210);
    path.clear();
  }
}

XsheetGUI::ColumnHeaderStyle TimelineView::columnHeaderStyle() const {
  XsheetGUI::ColumnHeaderStyle style;
  style.iconBorder = getTimelineIconLineColor();
  style.fillLeftInset =
      orientation()->rect(PredefinedRect::THUMBNAIL_AREA).left();
  style.drawLayerBoundary     = false;
  style.cameraVisibilityAreas = true;
  style.numberAlignment       = Qt::AlignHCenter | Qt::AlignBottom;
  if (Preferences::instance()->isShowColumnNumbersEnabled())
    style.nameLeftInset = 24;
  style.volumeTickAdvance   = QPoint(3, 0);
  style.volumeTickDirection = QPoint(0, 1);
  return style;
}

QScrollBar* TimelineView::rulerScrollBar() const {
  return m_rowScrollArea->horizontalScrollBar();
}
QScrollBar* TimelineView::headerScrollBar() const {
  return m_columnScrollArea->verticalScrollBar();
}
