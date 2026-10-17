#include "columnheaderpainter.h"
#include "xsheetviewer.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshmeshcolumn.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/levelset.h"
#include "toonz/preferences.h"
#include "toonzqt/gutil.h"
#include "columnselection.h"
#include "tconvert.h"

#include <QPainter>
#include <QSvgRenderer>
#include <QtMath>
#include <cmath>

namespace XsheetGUI {

//-----------------------------------------------------------------------------
ColumnHeaderPainter::ColumnHeaderPainter(
    XsheetViewer* viewer, const QPoint& hoverPos, QPainter& painter, int index,
    const ColumnHeaderInteraction& interaction)
    : p(painter)
    , col(index)
    , m_viewer(viewer)
    , o(viewer->orientation())
    , xsh(viewer->getXsheet())
    , m_hoverPos(hoverPos)
    , m_style(viewer->columnHeaderStyle())
    , m_interaction(interaction) {
  const Preferences* preferences = Preferences::instance();
  m_data                         = viewer->engine().columnHeader(
      xsh, col, preferences->isLinkColumnNameWithLevelEnabled(),
      preferences->getLevelNameDisplayType() ==
          Preferences::ShowLevelNameOnColumnHeader);
  column        = m_data.column;
  isEmpty       = m_data.empty;
  isCurrent     = interaction.current;
  reservedLevel = m_data.reservedLevel.getPointer();
  orig          = viewer->positionToXY(CellPosition(0, std::max(col, -1)));
}

void ColumnHeaderPainter::prepare() const {
  p.setFont(m_viewer->headerResources().font());
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
}

//-----------------------------------------------------------------------------
void ColumnHeaderPainter::levelColors(QColor& columnColor,
                                      QColor& dragColor) const {
  if (col < 0) {
    const bool isActive = m_data.cameraActive;
    columnColor         = isActive ? m_viewer->getActiveCameraColor()
                                   : m_viewer->getOtherCameraColor();
    dragColor           = isActive ? m_viewer->getActiveCameraColor()
                                   : m_viewer->getOtherCameraColor();
    return;
  }
  enum { Normal, Reference, Control } usage = Reference;
  if (column) {
    if (column->isControl()) usage = Control;
    if (column->isRendered() || column->getMeshColumn()) usage = Normal;
  }

  if (reservedLevel) {
    int ltype;
    m_viewer->getCellTypeAndColors(ltype, columnColor, dragColor,
                                   TXshCell(reservedLevel, TFrameId()));
  } else if (usage == Reference) {
    columnColor = m_viewer->getReferenceColumnColor();
    dragColor   = m_viewer->getReferenceColumnBorderColor();
  } else
    m_viewer->getColumnColor(columnColor, dragColor, col, xsh);
}

void ColumnHeaderPainter::paletteColors(QColor& columnColor,
                                        QColor& dragColor) const {
  enum { Normal, Reference, Control } usage = Reference;
  if (column) {  // Check if column is a mask
    if (column->isControl()) usage = Control;
    if (column->isRendered()) usage = Normal;
  }

  if (usage == Reference) {
    columnColor = m_viewer->getReferenceColumnColor();
    dragColor   = m_viewer->getReferenceColumnBorderColor();
  } else {
    columnColor = m_viewer->getPaletteColumnColor();
    dragColor   = m_viewer->getPaletteColumnBorderColor();
  }
}

void ColumnHeaderPainter::drawBaseFill(const QColor& columnColor,
                                       const QColor& dragColor) const {
  // check if the column is reference
  const bool isEditingSpline = m_interaction.editingSpline;

  QRect rect = o->rect((col < 0) ? PredefinedRect::CAMERA_LAYER_HEADER
                                 : PredefinedRect::LAYER_HEADER)
                   .translated(orig);

  int x0 = rect.left();
  int x1 = rect.right();
  int y0 = rect.top();
  int y1 = rect.bottom();

  // Fill base color, in timeline view adjust it right upto thumbnail so column
  // head color doesn't show under icon switches.
  if (isEmpty && !reservedLevel)
    p.fillRect(rect.adjusted(m_style.fillLeftInset, 0, 0, 0),
               m_viewer->getEmptyColumnHeadColor());
  else if (col < 0)
    p.fillRect(rect.adjusted(m_style.fillLeftInset, 0, 0, 0), columnColor);
  else {
    QBrush brush(columnColor,
                 (reservedLevel) ? Qt::DiagCrossPattern : Qt::SolidPattern);
    p.fillRect(rect.adjusted(m_style.fillLeftInset, 0, 0, 0), brush);

    if (o->flag(PredefinedFlag::DRAG_LAYER_VISIBLE)) {
      // column handle
      QRect sideBar = o->rect(PredefinedRect::DRAG_LAYER).translated(x0, y0);

      if (o->flag(PredefinedFlag::DRAG_LAYER_BORDER)) {
        p.setPen(m_viewer->getVerticalLineColor());
        p.drawRect(sideBar);
      }

      p.fillRect(sideBar, sideBar.contains(m_hoverPos)
                              ? m_viewer->getXsheetDragBarHighlightColor()
                              : dragColor);
    }
  }

  p.setPen(m_viewer->getVerticalLineHeadColor());
  QLine vertical =
      o->verticalLine(m_viewer->columnToLayerAxis(col), o->frameSide(rect));
  if (isEmpty || m_style.drawLayerBoundary) p.drawLine(vertical);

  // highlight selection
  bool isSelected = m_interaction.selected && !isEditingSpline;

  QColor pastelizer(m_viewer->getColumnHeadPastelizer());

  QColor colorSelection(m_viewer->getSelectedColumnHead());
  p.fillRect(rect.adjusted(m_style.fillLeftInset, 0, 0, 0),
             isSelected ? colorSelection : pastelizer);
}

void ColumnHeaderPainter::drawEye() const {
  if (isEmpty || !o->flag(PredefinedFlag::EYE_AREA_VISIBLE)) return;
  if (col < 0 && !m_style.cameraVisibilityAreas)
    return;  // no preview eye in the camera column
  QColor bgColor;
  QString svgFilePath;
  int buttonType = !column->isPreviewVisible() ? PREVIEW_OFF_XSHBUTTON
                                               : PREVIEW_ON_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, svgFilePath);

  QRect prevViewRect = o->rect(PredefinedRect::EYE_AREA).translated(orig);
  QRect eyeRect      = o->rect(PredefinedRect::EYE).translated(orig);
  // preview visible toggle
  p.setPen(m_style.iconBorder);

  if (col < 0 || column->getSoundTextColumn()) {
    if (o->flag(PredefinedFlag::EYE_AREA_BORDER)) p.drawRect(prevViewRect);
    return;
  }

  p.fillRect(prevViewRect, bgColor);  //   PreviewVisibleColor);
  if (o->flag(PredefinedFlag::EYE_AREA_BORDER)) p.drawRect(prevViewRect);

  // For Legacy (layout=1), Preview Off button is not displayed in Xsheet mode
  if (m_style.hideInactiveIcons && buttonType == PREVIEW_OFF_XSHBUTTON) return;

  QPixmap icon = m_viewer->headerResources().svg(
      svgFilePath, eyeRect.size(), bgColor, m_viewer->devicePixelRatioF());

  p.drawPixmap(eyeRect, icon);
}

void ColumnHeaderPainter::drawPreviewToggle(int opacity) const {
  if (isEmpty || !o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_VISIBLE)) return;
  if (col < 0 && !m_style.cameraVisibilityAreas)
    return;  // no camstand toggle in the camera column
  // camstand visible toggle
  QColor bgColor;
  QString svgFilePath;
  int buttonType = !column->isCamstandVisible() ? CAMSTAND_OFF_XSHBUTTON
                   : opacity < 255              ? CAMSTAND_TRANSP_XSHBUTTON
                                                : CAMSTAND_ON_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, svgFilePath);

  QRect tableViewRect =
      o->rect(PredefinedRect::PREVIEW_LAYER_AREA).translated(orig);
  QRect tableViewImgRect =
      o->rect(PredefinedRect::PREVIEW_LAYER).translated(orig);

  p.setPen(m_style.iconBorder);

  if (col < 0 || column->getPaletteColumn() || column->getSoundTextColumn()) {
    if (o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_BORDER))
      p.drawRect(tableViewRect);
    return;
  }

  p.fillRect(tableViewRect, bgColor);  //   CamStandVisibleColor);
  if (o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_BORDER))
    p.drawRect(tableViewRect);

  // For Legacy (layout=1), Camstand Off button is not displayed in Xsheet mode
  if (m_style.hideInactiveIcons && buttonType == CAMSTAND_OFF_XSHBUTTON) return;

  QPixmap icon =
      m_viewer->headerResources().svg(svgFilePath, tableViewImgRect.size(),
                                      bgColor, m_viewer->devicePixelRatioF());

  p.drawPixmap(tableViewImgRect, icon);
}

// shows united visibility toggle, which has the same state as the camstand
// toggle (preview toggle should synchonize with the camstand toggle)
void ColumnHeaderPainter::drawUnifiedViewToggle(int opacity) const {
  if (isEmpty || !o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_VISIBLE)) return;
  if (col < 0 && !m_style.cameraVisibilityAreas)
    return;  // no camstand toggle in the camera column
  // camstand visible toggle
  QColor bgColor;
  QString svgFilePath;
  int buttonType = !column->isCamstandVisible() ? PREVIEW_OFF_XSHBUTTON
                   : opacity < 255              ? UNIFIED_TRANSP_XSHBUTTON
                                                : PREVIEW_ON_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, svgFilePath);

  QRect unifiedViewRect =
      o->rect(PredefinedRect::UNIFIEDVIEW_LAYER_AREA).translated(orig);
  QRect unifiedViewImgRect =
      o->rect(PredefinedRect::UNIFIEDVIEW_LAYER).translated(orig);

  p.setPen(m_style.iconBorder);

  if (col < 0 || column->getPaletteColumn() || column->getSoundTextColumn()) {
    if (o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_BORDER))
      p.drawRect(unifiedViewRect);
    return;
  }

  p.fillRect(unifiedViewRect, bgColor);
  if (o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_BORDER))
    p.drawRect(unifiedViewRect);

  QPixmap icon =
      m_viewer->headerResources().svg(svgFilePath, unifiedViewImgRect.size(),
                                      bgColor, m_viewer->devicePixelRatioF());

  p.drawPixmap(unifiedViewImgRect, icon);
}

void ColumnHeaderPainter::drawLock() const {
  if (isEmpty || !o->flag(PredefinedFlag::LOCK_AREA_VISIBLE)) return;
  QColor bgColor;
  QString svgFilePath;
  int buttonType = !column->isLocked() ? LOCK_OFF_XSHBUTTON : LOCK_ON_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, svgFilePath);

  QRect lockModeRect = o->rect((col < 0) ? PredefinedRect::CAMERA_LOCK_AREA
                                         : PredefinedRect::LOCK_AREA)
                           .translated(orig);
  QRect lockModeImgRect =
      o->rect((col < 0) ? PredefinedRect::CAMERA_LOCK : PredefinedRect::LOCK)
          .translated(orig);

  if (m_style.hideInactiveIcons && buttonType == LOCK_OFF_XSHBUTTON &&
      !bgColor.alpha())
    bgColor = QColor(255, 255, 255, 128);

  // lock button
  p.setPen(m_style.iconBorder);

  p.fillRect(lockModeRect, bgColor);
  if (o->flag(PredefinedFlag::LOCK_AREA_BORDER)) p.drawRect(lockModeRect);

  // For Legacy (layout=1), Lock Off button is not displayed in Xsheet mode
  if (m_style.hideInactiveIcons && buttonType == LOCK_OFF_XSHBUTTON) return;

  QPixmap icon =
      m_viewer->headerResources().svg(svgFilePath, lockModeImgRect.size(),
                                      bgColor, m_viewer->devicePixelRatioF());

  p.drawPixmap(lockModeImgRect, icon);
}

void ColumnHeaderPainter::drawConfig() const {
  if (isEmpty || (col >= 0 && !o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE)) ||
      (col < 0 && !o->flag(PredefinedFlag::CAMERA_CONFIG_AREA_VISIBLE)))
    return;
  QColor bgColor;
  QString svgIconPath;

  int buttonType = CONFIG_XSHBUTTON;

  m_viewer->getButton(buttonType, bgColor, svgIconPath);

  QRect configRect    = o->rect((col < 0) ? PredefinedRect::CAMERA_CONFIG_AREA
                                          : PredefinedRect::CONFIG_AREA)
                            .translated(orig);
  QRect configImgRect = o->rect((col < 0) ? PredefinedRect::CAMERA_CONFIG
                                          : PredefinedRect::CONFIG)
                            .translated(orig);

  // config button
  p.setPen(m_style.iconBorder);
  p.fillRect(configRect, bgColor);
  if (o->flag((col < 0) ? PredefinedFlag::CAMERA_CONFIG_AREA_BORDER
                        : PredefinedFlag::CONFIG_AREA_BORDER))
    p.drawRect(configRect);

  TXshZeraryFxColumn* zColumn = dynamic_cast<TXshZeraryFxColumn*>(column);

  if (zColumn || column->getPaletteColumn() || column->getSoundTextColumn())
    return;

  QPixmap icon =
      m_viewer->headerResources().svg(svgIconPath, configImgRect.size(),
                                      bgColor, m_viewer->devicePixelRatioF());

  p.drawPixmap(configImgRect, icon);
}

void ColumnHeaderPainter::drawColumnNumber() const {
  if (col < 0 || isEmpty || !o->flag(PredefinedFlag::LAYER_NUMBER_VISIBLE) ||
      !Preferences::instance()->isShowColumnNumbersEnabled())
    return;

  QRect pos = o->rect(PredefinedRect::LAYER_NUMBER).translated(orig);

  p.setPen(m_viewer->getVerticalLineColor());
  if (o->flag(PredefinedFlag::LAYER_NUMBER_BORDER)) p.drawRect(pos);

  p.setPen((isCurrent) ? m_viewer->getSelectedColumnTextColor()
                       : m_viewer->getTextColor());

  p.drawText(pos, m_style.numberAlignment | Qt::TextSingleLine,
             QString::number(col + 1));
}

void ColumnHeaderPainter::drawColumnName() const {
  if (!o->flag(PredefinedFlag::LAYER_NAME_VISIBLE)) return;

  const std::string& name = m_data.name;

  QRect columnName = o->rect((col < 0) ? PredefinedRect::CAMERA_LAYER_NAME
                                       : PredefinedRect::LAYER_NAME)
                         .translated(orig);

  bool nameBacklit   = false;
  const int rightadj = m_style.nameRightInset;
  const int leftadj  = m_style.nameLeftInset;

  if (!isEmpty) {
    if (m_style.highlightNameOnHover)  // Legacy - No background
    {
      if (columnName.contains(m_hoverPos) && col >= 0) {
        p.fillRect(columnName.adjusted(0, -1, 0, 0),
                   m_viewer->getXsheetDragBarHighlightColor());
        nameBacklit = true;
      } else
        p.fillRect(columnName, m_viewer->getXsheetColumnNameBgColor());
    }

    if (o->flag(PredefinedFlag::LAYER_NAME_BORDER))
      p.drawRect(columnName.adjusted(0, 0, 2, 0));

    if (m_style.previewBacklightsName && column->isPreviewVisible() &&
        !column->getSoundTextColumn() && !column->getPaletteColumn() &&
        col >= 0)
      nameBacklit = true;

    p.setPen((isCurrent)   ? m_viewer->getSelectedColumnTextColor()
             : nameBacklit ? m_viewer->getHighlightColumnTextColor()
                           : m_viewer->getTextColor());
  } else
    p.setPen((isCurrent) ? m_viewer->getSelectedColumnTextColor()
                         : m_viewer->getColumnTextColor());

  if (m_style.cameraNameRotation && col < 0) {
    QString cameraName = QString::fromStdString(name);
    p.save();
    p.translate(columnName.topRight());
    p.rotate(m_style.cameraNameRotation);
    p.drawText(columnName.translated(-columnName.topLeft())
                   .transposed()
                   .adjusted(5, 0, 0, 0),
               Qt::AlignLeft | Qt::AlignVCenter, cameraName);
    p.restore();
    return;
  }

  p.drawText(columnName.adjusted(leftadj, 0, rightadj, 0),
             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
             QString(name.c_str()));
}

void ColumnHeaderPainter::drawThumbnail(const QPixmap& iconPixmap) const {
  if (isEmpty) return;

  QRect thumbnailRect = o->rect((col < 0) ? PredefinedRect::CAMERA_ICON_AREA
                                          : PredefinedRect::THUMBNAIL_AREA)
                            .translated(orig);

  // Minimum layout has no thumbnail area
  if (thumbnailRect.isEmpty()) return;

  p.setPen(m_viewer->getVerticalLineColor());
  if (o->flag(PredefinedFlag::THUMBNAIL_AREA_BORDER)) p.drawRect(thumbnailRect);

  // sound thumbnail
  if (column->getSoundColumn()) {
    TXshSoundColumn* sc =
        xsh->getColumn(col) ? xsh->getColumn(col)->getSoundColumn() : 0;

    drawSoundIcon(sc->isPlaying());
    // Volume control now in config button. If no config button
    // draw control
    if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE))
      drawVolumeControl(sc->getVolume());
    return;
  }

  if (!o->flag((col < 0) ? PredefinedFlag::CAMERA_ICON_VISIBLE
                         : PredefinedFlag::THUMBNAIL_AREA_VISIBLE))
    return;
  QRect thumbnailImageRect = o->rect((col < 0) ? PredefinedRect::CAMERA_ICON
                                               : PredefinedRect::THUMBNAIL)
                                 .translated(orig);

  // palette thumbnail
  if (column->getPaletteColumn()) {
    p.drawPixmap(thumbnailImageRect, iconPixmap);
    return;
  }

  // soundtext thumbnail
  if (column->getSoundTextColumn()) {
    p.drawPixmap(thumbnailImageRect, iconPixmap);
    return;
  }

  // All other thumbnails
  p.setPen(m_viewer->getTextColor());

  TXshLevelColumn* levelColumn = column->getLevelColumn();
  TXshMeshColumn* meshColumn   = column->getMeshColumn();
  TXshZeraryFxColumn* zColumn  = dynamic_cast<TXshZeraryFxColumn*>(column);

  if (Preferences::instance()->getColumnIconLoadingPolicy() ==
          Preferences::LoadOnDemand &&
      ((levelColumn && !levelColumn->isIconVisible()) ||
       (meshColumn && !meshColumn->isIconVisible()) ||
       (zColumn && !zColumn->isIconVisible())) &&
      col >= 0) {
    // display nothing
  } else {
    if (!iconPixmap.isNull()) {
      p.drawPixmap(thumbnailImageRect, iconPixmap);
    }
  }
}

void ColumnHeaderPainter::drawPegbarName() const {
  if (isEmpty || !o->flag(PredefinedFlag::PEGBAR_NAME_VISIBLE)) return;
  // the camera column may have parent pegbar, but it is not displayed for now
  if (col < 0) return;

  const TStageObjectId parentId = m_data.parentId;
  QString name = m_data.parentHasName
                     ? QString::fromStdString(m_data.parentName)
                     : ChangeObjectParent::getNameTr(parentId);

  const QFont font = p.font();

  int handleWidth           = 20;
  const std::string& handle = m_data.parentHandle;
  if (handle == "B") handleWidth = 0;  // Default handle

  const QFontMetrics metrics(font);
  const int available =
      std::max(0, o->rect(PredefinedRect::PEGBAR_NAME).width() - handleWidth);
  int first = 0, last = name.size();
  while (first < last) {
    const int middle = (first + last + 1) / 2;
    if (metrics.horizontalAdvance(name.left(middle)) <= available)
      first = middle;
    else
      last = middle - 1;
  }
  name.truncate(first);

  // pegbar name
  QRect pegbarnamerect = o->rect(PredefinedRect::PEGBAR_NAME).translated(orig);
  p.setPen(m_viewer->getVerticalLineColor());
  if (o->flag(PredefinedFlag::PEGBAR_NAME_BORDER)) p.drawRect(pegbarnamerect);

  if (column->getSoundColumn() || column->getSoundTextColumn()) return;

  if (Preferences::instance()->isParentColorsInXsheetColumnEnabled() &&
      column->isPreviewVisible()) {
    QColor parentColor = Qt::transparent;
    if (parentId.isCamera()) {
      bool isActive =
          (parentId == xsh->getStageObjectTree()->getCurrentCameraId());
      parentColor = isActive ? m_viewer->getActiveCameraColor()
                             : m_viewer->getOtherCameraColor();
    } else if (parentId.isPegbar()) {
      parentColor = m_viewer->getPegColor();
    } else if (parentId.isTable()) {
      parentColor = m_viewer->getTableColor();
    } else if (parentId.isColumn()) {
      int columnIndex = parentId.getIndex();
      QColor unused;
      m_viewer->getColumnColor(parentColor, unused, columnIndex, xsh);
    }
    if (parentColor != Qt::transparent) {
      QRect parentrect = pegbarnamerect;
      parentrect.adjust(1, 1, 0, 0);
      p.fillRect(parentrect, parentColor);
    }
  }

  p.setPen(m_viewer->getTextColor());

  p.drawText(pegbarnamerect.adjusted(3, 0, 0, 0),
             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, name);
}

void ColumnHeaderPainter::drawParentHandleName() const {
  if (col < 0 || isEmpty ||
      !o->flag(PredefinedFlag::PARENT_HANDLE_NAME_VISIBILE) ||
      column->getSoundColumn() || column->getSoundTextColumn())
    return;

  QRect parenthandleRect =
      o->rect(PredefinedRect::PARENT_HANDLE_NAME).translated(orig);
  p.setPen(Qt::yellow);  // m_viewer->getVerticalLineColor());
  if (o->flag(PredefinedFlag::PARENT_HANDLE_NAME_BORDER))
    p.drawRect(parenthandleRect);

  std::string handle = m_data.parentHandle;
  if (handle.length() > 1 && handle[0] == 'H') handle = handle.substr(1);

  if (handle == "B") {  // Default handle
    QPen pen(m_viewer->getVerticalLineColor());
    pen.setStyle(Qt::PenStyle::DotLine);
    p.setPen(pen);
    int offset = parenthandleRect.x() + 2;
    p.drawLine(offset, parenthandleRect.y(), offset, parenthandleRect.bottom());
    return;
  }

  p.setPen(m_viewer->getVerticalLineColor());
  p.drawRect(parenthandleRect.adjusted(2, 0, 0, 0));

  p.setPen(m_viewer->getTextColor());
  p.drawText(parenthandleRect,
             Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextSingleLine,
             QString::fromStdString(handle));
}

void ColumnHeaderPainter::drawFilterColor() const {
  if (col < 0 || isEmpty || column->getColorFilterId() == 0 ||
      column->getSoundColumn() || column->getSoundTextColumn() ||
      column->getPaletteColumn())
    return;

  const TPixel32 filterColor = m_data.filterColor;

  QRect filterColorRect =
      o->rect(PredefinedRect::FILTER_COLOR).translated(orig);
  p.drawPixmap(filterColorRect, m_viewer->headerResources().colorChip(
                                    QColor(filterColor.r, filterColor.g,
                                           filterColor.b, filterColor.m)));
}

void ColumnHeaderPainter::drawSoundIcon(bool isPlaying) const {
  QRect rect = m_viewer->orientation()
                   ->rect(PredefinedRect::SOUND_ICON)
                   .translated(orig);
  p.drawPixmap(rect, m_viewer->headerResources().sound(
                         isPlaying, m_viewer->devicePixelRatioF()));
}

void ColumnHeaderPainter::drawVolumeControl(double volume) const {
  // slider subdivisions
  p.setPen(m_viewer->getTextColor());
  QPoint tick = o->point(PredefinedPoint::VOLUME_DIVISIONS_TOP_LEFT) + orig;
  for (int i = 0; i <= 20; ++i, tick += m_style.volumeTickAdvance) {
    const int length = i % 10 == 0 ? 3 : (i & 1) ? 0 : 2;
    p.drawLine(tick, tick + m_style.volumeTickDirection * length);
  }

  // slider track
  QPainterPath track =
      o->path(PredefinedPath::VOLUME_SLIDER_TRACK).translated(orig);
  p.drawPath(track);

  // cursor
  QRect trackRect = o->rect(PredefinedRect::VOLUME_TRACK).translated(orig);
  if (o->flag(PredefinedFlag::VOLUME_AREA_VERTICAL)) volume = 1 - volume;

  QPoint cursor;
  if (trackRect.width() >= trackRect.height())
    cursor =
        QPoint(NumberRange(trackRect.left(), trackRect.right()).weight(volume),
               trackRect.center().y());
  else
    cursor =
        QPoint(trackRect.center().x(),
               NumberRange(trackRect.top(), trackRect.bottom()).weight(volume));
  cursor += QPoint(1, 0);
  QPainterPath head =
      o->path(PredefinedPath::VOLUME_SLIDER_HEAD).translated(cursor);
  p.fillPath(head, QBrush(Qt::white));
  p.setPen(m_viewer->getLightLineColor());
  p.drawPath(head);
}

void ColumnHeaderPainter::paint(const QPixmap& thumbnail) const {
  prepare();
  const bool palette = column && column->getPaletteColumn() && !isEmpty;
  const bool text    = column && column->getSoundTextColumn() && !isEmpty;
  const bool sound   = column && column->getSoundColumn() && !isEmpty;
  const bool level   = !palette && !text && !sound;
  QColor columnColor, dragColor;
  if (palette || text)
    paletteColors(columnColor, dragColor);
  else
    levelColors(columnColor, dragColor);
  drawBaseFill(columnColor, dragColor);
  const int opacity =
      sound     ? troundp(255.0 * column->getSoundColumn()->getVolume())
      : text    ? 255
      : palette ? 0
      : column  ? column->getOpacity()
                : 0;
  if (level && Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled())
    drawUnifiedViewToggle(opacity);
  else {
    drawEye();
    drawPreviewToggle(opacity);
  }
  drawLock();
  if (!level) drawConfig();
  drawColumnName();
  drawColumnNumber();
  drawThumbnail(thumbnail);
  if (level) {
    drawFilterColor();
    drawConfig();
  }
  drawPegbarName();
  drawParentHandleName();
}

}  // namespace XsheetGUI
