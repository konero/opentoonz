#include "orientation.h"
#include "toonz/columnfan.h"

#include <QPainterPath>
#include <QBoxLayout>
#include <math.h>
#include <stdexcept>

namespace {
const int KEY_ICON_WIDTH      = 11;
const int KEY_ICON_HEIGHT     = 13;
const int EASE_TRIANGLE_SIZE  = 4;
const int PLAY_MARKER_SIZE    = 10;
const int PINNED_SIZE         = 11;
const int NAV_TAG_WIDTH       = 7;
const int NAV_TAG_HEIGHT      = 13;
const int FRAME_MARKER_SIZE   = 4;
const int FOLDED_CELL_SIZE    = 9;
const int SHIFTTRACE_DOT_SIZE = 12;

QRect iconRect(const QRect& areaRect, const int iconWidth, const int iconHeight,
               const int xOffset = 0) {
  return QRect(
      areaRect.left() + xOffset + ((areaRect.width() / 2) - (iconWidth / 2)),
      areaRect.top() + ((areaRect.height() / 2) - (iconHeight / 2)), iconWidth,
      iconHeight);
}

}  // namespace

class LeftToRightOrientation : public Orientation {
  const int CELL_WIDTH           = 50;
  const int CELL_HEIGHT          = 24;
  const int CELL_DRAG_HEIGHT     = 7;
  const int EXTENDER_WIDTH       = 8;
  const int EXTENDER_HEIGHT      = 14;
  const int SOUND_PREVIEW_HEIGHT = 6;
  const int FRAME_HEADER_HEIGHT  = 50;
  const int ONION_X = 0, ONION_Y = 0;
  const int ONION_SIZE         = 19;
  const int ONION_DOT_SIZE     = 8;
  const int PLAY_RANGE_Y       = ONION_SIZE;
  const int ICON_WIDTH         = 20;
  const int ICON_HEIGHT        = 20;
  const int ICON_OFFSET        = ICON_WIDTH;
  const int ICONS_WIDTH        = ICON_OFFSET * 4;  // 88
  const int THUMBNAIL_WIDTH    = 43;
  const int LAYER_NUMBER_WIDTH = 20;
  const int LAYER_NAME_WIDTH   = 150;
  const int LAYER_HEADER_WIDTH =
      ICONS_WIDTH + THUMBNAIL_WIDTH + LAYER_NUMBER_WIDTH + LAYER_NAME_WIDTH;
  const int FOLDED_LAYER_HEADER_HEIGHT = 8;
  const int FOLDED_LAYER_HEADER_WIDTH  = LAYER_HEADER_WIDTH;
  const int TRACKLEN                   = 60;
  const int SHIFTTRACE_DOT_OFFSET      = 5;
  const int LAYER_HEADER_PANEL_HEIGHT  = 20;
  const int LAYER_FOOTER_PANEL_HEIGHT  = 16;
  const int CAMERA_CELL_HEIGHT         = CELL_HEIGHT;

public:
  LeftToRightOrientation();

  virtual CellPosition xyToPosition(const QPoint& xy,
                                    const ColumnFan* fan) const override;
  virtual QPoint positionToXY(const CellPosition& position,
                              const ColumnFan* fan) const override;
  virtual CellPositionRatio xyToPositionRatio(const QPointF& xy) const override;
  virtual QPointF positionRatioToXY(
      const CellPositionRatio& ratio) const override;

  virtual int colToLayerAxis(int layer, const ColumnFan* fan) const override;
  virtual int rowToFrameAxis(int frame) const override;

  virtual QPoint frameLayerToXY(int frameAxis, int layerAxis) const override;
  virtual int layerAxis(const QPoint& xy) const override;
  virtual int frameAxis(const QPoint& xy) const override;

  virtual NumberRange layerSide(const QRect& area) const override;
  virtual NumberRange frameSide(const QRect& area) const override;
  virtual QPoint topRightCorner(const QRect& area) const override;

  virtual CellPosition arrowShift(int direction) const override;

  virtual QString name() const override { return "LeftToRight"; }
  virtual QString caption() const override { return QObject::tr("Timeline"); }

  virtual bool isVerticalTimeline() const override { return false; }
  virtual bool flipVolume() const override { return false; }

  virtual int cellWidth() const override { return CELL_WIDTH; }
  virtual int cellHeight() const override { return CELL_HEIGHT; }
  virtual int foldedCellSize() const override { return FOLDED_CELL_SIZE; }
};

LeftToRightOrientation::LeftToRightOrientation() {
  //
  // Ranges
  //

  // Cell viewer
  QRect cellRect(0, 0, CELL_WIDTH, CELL_HEIGHT);
  addRect(PredefinedRect::CELL, cellRect);
  addRect(PredefinedRect::CAMERA_CELL, cellRect);
  addRect(PredefinedRect::DRAG_HANDLE_CORNER,
          QRect(0, 0, CELL_WIDTH, CELL_DRAG_HEIGHT));
  QRect keyRect((CELL_WIDTH - KEY_ICON_WIDTH) / 2,
                CELL_HEIGHT - KEY_ICON_HEIGHT - 2, KEY_ICON_WIDTH,
                KEY_ICON_HEIGHT);
  addRect(PredefinedRect::KEY_ICON, keyRect);
  addRect(PredefinedRect::CAMERA_KEY_ICON, keyRect);
  QRect nameRect = cellRect.adjusted(4, 4, -6, 0);
  addRect(PredefinedRect::CELL_NAME, nameRect);
  addRect(PredefinedRect::CELL_NAME_WITH_KEYFRAME, nameRect);
  addRect(PredefinedRect::END_EXTENDER,
          QRect(0, -EXTENDER_HEIGHT - 4, EXTENDER_WIDTH, EXTENDER_HEIGHT));
  addRect(PredefinedRect::BEGIN_EXTENDER,
          QRect(-EXTENDER_WIDTH, -EXTENDER_HEIGHT - 4, EXTENDER_WIDTH,
                EXTENDER_HEIGHT));
  addRect(PredefinedRect::KEYFRAME_AREA, keyRect);
  addRect(PredefinedRect::DRAG_AREA, QRect(0, 0, CELL_WIDTH, CELL_DRAG_HEIGHT));
  int markSize = CELL_HEIGHT / 2;  // 50% size (12px)
  addRect(PredefinedRect::CELL_MARK_AREA,
          QRect(1, CELL_DRAG_HEIGHT + 1, markSize, markSize));
  QRect soundRect(0, CELL_DRAG_HEIGHT, CELL_WIDTH,
                  CELL_HEIGHT - CELL_DRAG_HEIGHT - SOUND_PREVIEW_HEIGHT);
  addRect(PredefinedRect::SOUND_TRACK, soundRect);
  addRect(PredefinedRect::PREVIEW_TRACK,
          QRect(0, CELL_HEIGHT - SOUND_PREVIEW_HEIGHT + 1, CELL_WIDTH,
                SOUND_PREVIEW_HEIGHT));
  addRect(PredefinedRect::BEGIN_SOUND_EDIT,
          QRect(0, CELL_DRAG_HEIGHT, 2, CELL_HEIGHT - CELL_DRAG_HEIGHT));
  addRect(PredefinedRect::END_SOUND_EDIT,
          QRect(CELL_WIDTH - 2, CELL_DRAG_HEIGHT, 2,
                CELL_HEIGHT - CELL_DRAG_HEIGHT));
  addRect(PredefinedRect::LOOP_ICON, QRect(0, keyRect.top(), 10, 11));
  addRect(PredefinedRect::CAMERA_LOOP_ICON, rect(PredefinedRect::LOOP_ICON));
  QRect frameMarker((CELL_WIDTH - FRAME_MARKER_SIZE) / 2 - 1,
                    CELL_HEIGHT - FRAME_MARKER_SIZE - 7, FRAME_MARKER_SIZE,
                    FRAME_MARKER_SIZE);
  addRect(PredefinedRect::FRAME_MARKER_AREA, frameMarker);
  addRect(PredefinedRect::CAMERA_FRAME_MARKER_AREA,
          rect(PredefinedRect::FRAME_MARKER_AREA));

  // Notes viewer
  addRect(
      PredefinedRect::NOTE_AREA,
      QRect(QPoint(0, 0), QSize(LAYER_HEADER_WIDTH - 1, FRAME_HEADER_HEIGHT)));
  addRect(PredefinedRect::NOTE_ICON,
          QRect(QPoint(0, 0), QSize(CELL_WIDTH - 2, CELL_HEIGHT - 2)));

  // Layer header panel
  addRect(PredefinedRect::LAYER_HEADER_PANEL,
          QRect(0, FRAME_HEADER_HEIGHT - CELL_HEIGHT, LAYER_HEADER_WIDTH,
                LAYER_HEADER_PANEL_HEIGHT));
  QRect panelEye(1, 0, ICON_WIDTH, ICON_HEIGHT);
  addRect(PredefinedRect::PANEL_EYE, panelEye.adjusted(1, 1, -1, -1));
  addRect(PredefinedRect::PANEL_PREVIEW_LAYER,
          panelEye.translated(ICON_OFFSET, 0).adjusted(1, 1, -1, -1));
  addRect(PredefinedRect::PANEL_LOCK,
          panelEye.translated(2 * ICON_OFFSET, 0).adjusted(1, 1, -1, -1));
  QRect panelName(ICONS_WIDTH + THUMBNAIL_WIDTH + 1, 0,
                  LAYER_NAME_WIDTH + LAYER_NUMBER_WIDTH - 4,
                  LAYER_HEADER_PANEL_HEIGHT);
  addRect(PredefinedRect::PANEL_LAYER_NAME, panelName);

  // Row viewer
  addRect(PredefinedRect::FRAME_LABEL,
          QRect(CELL_WIDTH / 4, 1, CELL_WIDTH / 2, FRAME_HEADER_HEIGHT - 2));
  addRect(PredefinedRect::FRAME_HEADER,
          QRect(0, 0, CELL_WIDTH, FRAME_HEADER_HEIGHT));
  addRect(PredefinedRect::PLAY_RANGE,
          QRect(0, PLAY_RANGE_Y, CELL_WIDTH, PLAY_MARKER_SIZE));
  addRect(PredefinedRect::ONION,
          QRect(ONION_X + (CELL_WIDTH - ONION_SIZE) / 2 - 1,
                ONION_Y + (3 * ONION_DOT_SIZE - ONION_SIZE) / 2, ONION_SIZE,
                ONION_SIZE)
              .adjusted(1, 2, 1, 2));
  int adjustOnion = (ONION_SIZE - ONION_DOT_SIZE) / 2;
  addRect(PredefinedRect::ONION_DOT,
          QRect(ONION_X + adjustOnion + (CELL_WIDTH - ONION_SIZE) / 2,
                ONION_Y + ONION_DOT_SIZE, ONION_DOT_SIZE, ONION_DOT_SIZE)
              .adjusted(1, 1, 1, 1));
  addRect(PredefinedRect::ONION_DOT_FIXED,
          QRect(ONION_X + adjustOnion + (CELL_WIDTH - ONION_SIZE) / 2, ONION_Y,
                ONION_DOT_SIZE, ONION_DOT_SIZE)
              .adjusted(1, 1, 1, 1));
  addRect(PredefinedRect::ONION_AREA,
          QRect(ONION_X, ONION_Y, CELL_WIDTH, ONION_SIZE));
  addRect(PredefinedRect::ONION_FIXED_DOT_AREA,
          QRect(ONION_X, ONION_Y, CELL_WIDTH, ONION_DOT_SIZE + 1));
  addRect(PredefinedRect::ONION_DOT_AREA,
          QRect(ONION_X, ONION_Y + ONION_DOT_SIZE + 1, CELL_WIDTH,
                ONION_DOT_SIZE + 1));
  addRect(
      PredefinedRect::PINNED_CENTER_KEY,
      QRect((CELL_WIDTH - PINNED_SIZE) / 2,
            (FRAME_HEADER_HEIGHT - PINNED_SIZE) / 2, PINNED_SIZE, PINNED_SIZE));
  addRect(
      PredefinedRect::PREVIEW_FRAME_AREA,
      QRect(0, PLAY_RANGE_Y, CELL_WIDTH, (FRAME_HEADER_HEIGHT - PLAY_RANGE_Y)));

  addRect(PredefinedRect::SHIFTTRACE_DOT,
          QRect((CELL_WIDTH - SHIFTTRACE_DOT_SIZE) / 2, SHIFTTRACE_DOT_OFFSET,
                SHIFTTRACE_DOT_SIZE, SHIFTTRACE_DOT_SIZE)
              .adjusted(-1, 0, -1, 0));
  addRect(PredefinedRect::SHIFTTRACE_DOT_AREA,
          QRect(0, SHIFTTRACE_DOT_OFFSET, CELL_WIDTH, SHIFTTRACE_DOT_SIZE));
  addRect(PredefinedRect::NAVIGATION_TAG_AREA,
          QRect((CELL_WIDTH - NAV_TAG_WIDTH) / 2,
                (FRAME_HEADER_HEIGHT - NAV_TAG_HEIGHT) / 2, NAV_TAG_WIDTH,
                NAV_TAG_HEIGHT));

  // Column viewer
  addRect(PredefinedRect::LAYER_HEADER,
          QRect(1, 0, LAYER_HEADER_WIDTH - 2, CELL_HEIGHT));
  addRect(
      PredefinedRect::FOLDED_LAYER_HEADER,
      QRect(1, 0, FOLDED_LAYER_HEADER_WIDTH - 2, FOLDED_LAYER_HEADER_HEIGHT));
  addRect(PredefinedRect::CAMERA_LAYER_HEADER,
          QRect(1, 0, LAYER_HEADER_WIDTH - 2, CAMERA_CELL_HEIGHT));
  QRect columnName(ICONS_WIDTH + THUMBNAIL_WIDTH + 1, 0,
                   LAYER_NAME_WIDTH + LAYER_NUMBER_WIDTH - 4, CELL_HEIGHT);
  addRect(PredefinedRect::RENAME_COLUMN, columnName);
  QRect eyeArea(1, 0, ICON_WIDTH, CELL_HEIGHT);
  QRect eye(1,
            eyeArea.top() + ((eyeArea.height() / 2) - ((ICON_HEIGHT - 1) / 2)),
            ICON_WIDTH, ICON_HEIGHT);
  addRect(PredefinedRect::EYE_AREA, eyeArea);
  addRect(PredefinedRect::EYE, eye.adjusted(1, 1, -1, -1));
  addRect(PredefinedRect::PREVIEW_LAYER_AREA,
          eyeArea.translated(ICON_OFFSET, 0));
  addRect(PredefinedRect::PREVIEW_LAYER,
          eye.translated(ICON_OFFSET, 0).adjusted(1, 1, -1, -1));

  addRect(PredefinedRect::UNIFIEDVIEW_LAYER_AREA, eyeArea);
  addRect(PredefinedRect::UNIFIEDVIEW_LAYER, eye.adjusted(1, 1, -1, -1));

  addRect(PredefinedRect::LOCK_AREA, eyeArea.translated(2 * ICON_OFFSET, 0));
  addRect(PredefinedRect::LOCK,
          eye.translated(2 * ICON_OFFSET, 0).adjusted(1, 1, -1, -1));
  addRect(PredefinedRect::CAMERA_LOCK_AREA, rect(PredefinedRect::LOCK_AREA));
  addRect(PredefinedRect::CAMERA_LOCK, rect(PredefinedRect::LOCK));
  addRect(PredefinedRect::CONFIG_AREA, eyeArea.translated(3 * ICON_OFFSET, 0));
  addRect(PredefinedRect::CONFIG,
          eye.translated(3 * ICON_OFFSET, 0).adjusted(1, 1, -1, -1));
  addRect(PredefinedRect::CAMERA_CONFIG_AREA,
          rect(PredefinedRect::CONFIG_AREA));
  addRect(PredefinedRect::CAMERA_CONFIG, rect(PredefinedRect::CONFIG));
  addRect(PredefinedRect::DRAG_LAYER,
          QRect(0, 0, -1, -1));  // hide drag handle in Timeline view
  // addRect(PredefinedRect::DRAG_LAYER,
  //         QRect(ICONS_WIDTH + THUMBNAIL_WIDTH + 1, 0,
  //               LAYER_HEADER_WIDTH - ICONS_WIDTH - THUMBNAIL_WIDTH - 3,
  //               CELL_DRAG_HEIGHT));
  addRect(PredefinedRect::LAYER_NAME, columnName);
  addRect(PredefinedRect::CAMERA_LAYER_NAME, rect(PredefinedRect::LAYER_NAME));
  addRect(PredefinedRect::LAYER_NUMBER,
          QRect(ICONS_WIDTH + THUMBNAIL_WIDTH + 1, 0, LAYER_NUMBER_WIDTH,
                CELL_HEIGHT));
  QRect thumbnailArea = QRect(ICONS_WIDTH + 1, 0, THUMBNAIL_WIDTH, CELL_HEIGHT);
  addRect(PredefinedRect::THUMBNAIL_AREA, thumbnailArea);
  QRect thumbnail = thumbnailArea.adjusted(1, 1, 0, 0);
  addRect(PredefinedRect::THUMBNAIL, thumbnail);
  addRect(PredefinedRect::CAMERA_ICON_AREA,
          rect(PredefinedRect::THUMBNAIL_AREA));
  addRect(PredefinedRect::CAMERA_ICON, rect(PredefinedRect::THUMBNAIL));

  addRect(PredefinedRect::FILTER_COLOR,
          QRect(thumbnail.right() - 14, thumbnail.top() + 3, 12, 12));
  addRect(PredefinedRect::PEGBAR_NAME, QRect(0, 0, -1, -1));         // hide
  addRect(PredefinedRect::PARENT_HANDLE_NAME, QRect(0, 0, -1, -1));  // hide

  addRect(PredefinedRect::SOUND_ICON,
          QRect(thumbnailArea.topLeft(), QSize(27, thumbnailArea.height()))
              .adjusted((thumbnailArea.width() / 2) - (27 / 2), 0,
                        (thumbnailArea.width() / 2) - (27 / 2), 0));

  QRect volumeArea(
      QRect(columnName.topRight(), QSize(TRACKLEN + 8, 14))
          .adjusted(-77, CELL_DRAG_HEIGHT + 1, -77, CELL_DRAG_HEIGHT + 1));
  addRect(PredefinedRect::VOLUME_AREA, volumeArea);
  QPoint soundTopLeft(volumeArea.left() + 4, volumeArea.bottom() - 6);
  addRect(PredefinedRect::VOLUME_TRACK,
          QRect(soundTopLeft, QSize(TRACKLEN, 3)));

  // Layer footer panel
  QRect layerFooterPanel(
      QRect(0, 0, LAYER_HEADER_WIDTH + 2, LAYER_FOOTER_PANEL_HEIGHT));
  addRect(PredefinedRect::LAYER_FOOTER_PANEL, layerFooterPanel);

  QRect zoomSlider, zoomIn, zoomOut;

  zoomSlider =
      QRect(layerFooterPanel.width() - 100, 0, 81, LAYER_FOOTER_PANEL_HEIGHT);
  addRect(PredefinedRect::ZOOM_SLIDER_AREA, zoomSlider);
  addRect(PredefinedRect::ZOOM_SLIDER, zoomSlider.adjusted(1, 0, 0, 0));

  zoomIn = QRect(zoomSlider.right() + 1, 0, 16, LAYER_FOOTER_PANEL_HEIGHT);
  addRect(PredefinedRect::ZOOM_IN_AREA, zoomIn);
  addRect(PredefinedRect::ZOOM_IN, zoomIn.adjusted(1, 1, 0, 0));

  zoomOut = QRect(zoomSlider.left() - 16, 0, 16, LAYER_FOOTER_PANEL_HEIGHT);
  addRect(PredefinedRect::ZOOM_OUT_AREA, zoomOut);
  addRect(PredefinedRect::ZOOM_OUT, zoomOut.adjusted(1, 1, 0, 0));

  // Flags
  addFlag(PredefinedFlag::DRAG_LAYER_BORDER, false);
  addFlag(PredefinedFlag::DRAG_LAYER_VISIBLE, false);
  addFlag(PredefinedFlag::LAYER_NAME_BORDER, true);
  addFlag(PredefinedFlag::LAYER_NAME_VISIBLE, true);
  addFlag(PredefinedFlag::LAYER_NUMBER_BORDER, true);
  addFlag(PredefinedFlag::LAYER_NUMBER_VISIBLE, true);
  addFlag(PredefinedFlag::EYE_AREA_BORDER, true);
  addFlag(PredefinedFlag::EYE_AREA_VISIBLE, true);
  addFlag(PredefinedFlag::LOCK_AREA_BORDER, true);
  addFlag(PredefinedFlag::LOCK_AREA_VISIBLE, true);
  addFlag(PredefinedFlag::PREVIEW_LAYER_AREA_BORDER, true);
  addFlag(PredefinedFlag::PREVIEW_LAYER_AREA_VISIBLE, true);
  addFlag(PredefinedFlag::CONFIG_AREA_BORDER, true);
  addFlag(PredefinedFlag::CAMERA_CONFIG_AREA_BORDER, true);
  addFlag(PredefinedFlag::CONFIG_AREA_VISIBLE, true);
  addFlag(PredefinedFlag::CAMERA_CONFIG_AREA_VISIBLE, true);
  addFlag(PredefinedFlag::PEGBAR_NAME_BORDER, false);
  addFlag(PredefinedFlag::PEGBAR_NAME_VISIBLE, false);
  addFlag(PredefinedFlag::PARENT_HANDLE_NAME_BORDER, false);
  addFlag(PredefinedFlag::PARENT_HANDLE_NAME_VISIBILE, false);
  addFlag(PredefinedFlag::THUMBNAIL_AREA_BORDER, true);
  addFlag(PredefinedFlag::THUMBNAIL_AREA_VISIBLE, true);
  addFlag(PredefinedFlag::CAMERA_ICON_VISIBLE, true);
  addFlag(PredefinedFlag::VOLUME_AREA_VERTICAL, false);
  addFlag(PredefinedFlag::NOTE_AREA_IN_POPUP, false);

  //
  // Lines
  //
  addLine(PredefinedLine::LOCKED,
          verticalLine((CELL_DRAG_HEIGHT + 1) / 2, NumberRange(0, CELL_WIDTH)));
  addLine(PredefinedLine::SEE_MARKER_THROUGH,
          horizontalLine(0, NumberRange(0, CELL_DRAG_HEIGHT)));
  addLine(PredefinedLine::CONTINUE_LEVEL,
          verticalLine(CELL_HEIGHT / 2, NumberRange(0, CELL_WIDTH)));
  addLine(PredefinedLine::CONTINUE_LEVEL_WITH_NAME,
          verticalLine(CELL_HEIGHT / 2, NumberRange(0, CELL_WIDTH)));
  addLine(PredefinedLine::EXTENDER_LINE,
          horizontalLine(0, NumberRange(-EXTENDER_WIDTH - KEY_ICON_WIDTH, 0)));
  addLine(PredefinedLine::CURRENT_TIME_INDICATOR,
          horizontalLine(CELL_WIDTH / 2 - 1, NumberRange(0, CELL_HEIGHT)));

  //
  // Dimensions
  //
  addDimension(PredefinedDimension::LAYER, CELL_HEIGHT);
  addDimension(PredefinedDimension::FRAME, CELL_WIDTH);
  addDimension(PredefinedDimension::INDEX, 1);
  addDimension(PredefinedDimension::SOUND_AMPLITUDE, soundRect.height() / 2);
  addDimension(PredefinedDimension::FRAME_LABEL_ALIGN,
               Qt::AlignHCenter | Qt::AlignBottom | Qt::TextWordWrap);
  addDimension(PredefinedDimension::ONION_TURN, 90);
  addDimension(PredefinedDimension::QBOXLAYOUT_DIRECTION,
               QBoxLayout::Direction::LeftToRight);
  addDimension(PredefinedDimension::CENTER_ALIGN, Qt::AlignVCenter);
  addDimension(PredefinedDimension::CAMERA_LAYER, CAMERA_CELL_HEIGHT);
  addDimension(PredefinedDimension::SCALE_THRESHOLD, 50);
  addDimension(PredefinedDimension::FRAME_AREA_EXPANSION, 0);  // not used

  //
  // Paths
  //
  QPainterPath corner(QPointF(CELL_WIDTH, 0));
  corner.lineTo(QPointF(CELL_WIDTH, CELL_DRAG_HEIGHT));
  corner.lineTo(QPointF(CELL_WIDTH - CELL_DRAG_HEIGHT, CELL_DRAG_HEIGHT));
  corner.lineTo(QPointF(CELL_WIDTH, 0));
  addPath(PredefinedPath::DRAG_HANDLE_CORNER, corner);

  QPainterPath diamond(QPointF(0, -4));
  diamond.lineTo(4, 0);
  diamond.lineTo(0, 4);
  diamond.lineTo(-4, 0);
  diamond.lineTo(0, -4);
  addPath(PredefinedPath::FRAME_MARKER_DIAMOND, diamond);

  QPainterPath fromTriangle(QPointF(EASE_TRIANGLE_SIZE / 2, 0));
  fromTriangle.lineTo(QPointF(-EASE_TRIANGLE_SIZE / 2, EASE_TRIANGLE_SIZE));
  fromTriangle.lineTo(QPointF(-EASE_TRIANGLE_SIZE / 2, -EASE_TRIANGLE_SIZE));
  fromTriangle.lineTo(QPointF(EASE_TRIANGLE_SIZE / 2, 0));
  addPath(PredefinedPath::BEGIN_EASE_TRIANGLE, fromTriangle);

  QPainterPath toTriangle(QPointF(-EASE_TRIANGLE_SIZE / 2, 0));
  toTriangle.lineTo(QPointF(EASE_TRIANGLE_SIZE / 2, EASE_TRIANGLE_SIZE));
  toTriangle.lineTo(QPointF(EASE_TRIANGLE_SIZE / 2, -EASE_TRIANGLE_SIZE));
  toTriangle.lineTo(QPointF(-EASE_TRIANGLE_SIZE / 2, 0));
  addPath(PredefinedPath::END_EASE_TRIANGLE, toTriangle);

  QPainterPath playFrom(QPointF(0, 0));
  playFrom.lineTo(QPointF(PLAY_MARKER_SIZE, 0));
  playFrom.lineTo(QPointF(0, PLAY_MARKER_SIZE));
  playFrom.lineTo(QPointF(0, 0));
  playFrom.translate(1, PLAY_RANGE_Y);
  addPath(PredefinedPath::BEGIN_PLAY_RANGE, playFrom);

  QPainterPath playTo(QPointF(0, 0));
  playTo.lineTo(QPointF(-PLAY_MARKER_SIZE, 0));
  playTo.lineTo(QPointF(0, PLAY_MARKER_SIZE));
  playTo.lineTo(QPointF(0, 0));
  playTo.translate(CELL_WIDTH - 2, PLAY_RANGE_Y);
  addPath(PredefinedPath::END_PLAY_RANGE, playTo);

  QPainterPath track(QPointF(0, 0));
  track.lineTo(QPointF(1, 1));
  track.lineTo(QPointF(TRACKLEN - 1, 1));
  track.lineTo(QPointF(TRACKLEN, 0));
  track.lineTo(QPointF(TRACKLEN - 1, -1));
  track.lineTo(QPointF(1, -1));
  track.lineTo(QPointF(0, 0));
  track.translate(soundTopLeft);
  addPath(PredefinedPath::VOLUME_SLIDER_TRACK, track);

  QPainterPath head(QPointF(0, 0));
  head.lineTo(QPointF(4, -4));
  head.lineTo(QPointF(4, -8));
  head.lineTo(QPointF(-4, -8));
  head.lineTo(QPointF(-4, -4));
  head.lineTo(QPointF(0, 0));
  addPath(PredefinedPath::VOLUME_SLIDER_HEAD, head);

  QPainterPath timeIndicator(QPointF(0, 0));
  timeIndicator.lineTo(QPointF(-9, -5));
  timeIndicator.lineTo(QPointF(-9, -18));
  timeIndicator.lineTo(QPointF(9, -18));
  timeIndicator.lineTo(QPointF(9, -4));
  timeIndicator.lineTo(QPointF(0, 0));
  addPath(PredefinedPath::TIME_INDICATOR_HEAD, timeIndicator);

  QPainterPath tag(QPointF(-3, 0));
  tag.lineTo(QPointF(3, 0));
  tag.lineTo(QPointF(3, 9));
  tag.lineTo(QPointF(0, 13));
  tag.lineTo(QPointF(-3, 9));
  tag.lineTo(QPointF(-3, 0));
  addPath(PredefinedPath::NAVIGATION_TAG, tag);

  //
  // Points
  //
  addPoint(PredefinedPoint::KEY_HIDDEN, QPoint(0, 10));
  addPoint(PredefinedPoint::EXTENDER_XY_RADIUS, QPoint(75, 30));
  addPoint(PredefinedPoint::VOLUME_DIVISIONS_TOP_LEFT,
           soundTopLeft + QPoint(0, 3));

  //
  // Ranges
  //
  addRange(PredefinedRange::HEADER_LAYER, NumberRange(0, FRAME_HEADER_HEIGHT));
  addRange(PredefinedRange::HEADER_FRAME, NumberRange(0, LAYER_HEADER_WIDTH));
}

CellPosition LeftToRightOrientation::xyToPosition(const QPoint& xy,
                                                  const ColumnFan* fan) const {
  int layer = fan->layerAxisToCol(xy.y());
  int frame = xy.x() / CELL_WIDTH;
  return CellPosition(frame, layer);
}
QPoint LeftToRightOrientation::positionToXY(const CellPosition& position,
                                            const ColumnFan* fan) const {
  int y = colToLayerAxis(position.layer(), fan);
  int x = rowToFrameAxis(position.frame());
  return QPoint(x, y);
}
CellPositionRatio LeftToRightOrientation::xyToPositionRatio(
    const QPointF& xy) const {
  double frame = xy.x() / (double)CELL_WIDTH;
  double layer = xy.y() / (double)CELL_HEIGHT;
  return CellPositionRatio{frame, layer};
}
QPointF LeftToRightOrientation::positionRatioToXY(
    const CellPositionRatio& ratio) const {
  double x = ratio.frame() * (double)CELL_WIDTH;
  double y = ratio.layer() * (double)CELL_HEIGHT;
  return QPointF(x, y);
}

int LeftToRightOrientation::colToLayerAxis(int layer,
                                           const ColumnFan* fan) const {
  return fan->colToLayerAxis(layer);
}
int LeftToRightOrientation::rowToFrameAxis(int frame) const {
  return frame * CELL_WIDTH;
}
QPoint LeftToRightOrientation::frameLayerToXY(int frameAxis,
                                              int layerAxis) const {
  return QPoint(frameAxis, layerAxis);
}
int LeftToRightOrientation::layerAxis(const QPoint& xy) const { return xy.y(); }
int LeftToRightOrientation::frameAxis(const QPoint& xy) const { return xy.x(); }
NumberRange LeftToRightOrientation::layerSide(const QRect& area) const {
  return NumberRange(area.top(), area.bottom());
}
NumberRange LeftToRightOrientation::frameSide(const QRect& area) const {
  return NumberRange(area.left(), area.right());
}
QPoint LeftToRightOrientation::topRightCorner(const QRect& area) const {
  return area.bottomLeft();
}
CellPosition LeftToRightOrientation::arrowShift(int direction) const {
  switch (direction) {
  case Qt::Key_Up:
    return CellPosition(0, 1);
  case Qt::Key_Down:
    return CellPosition(0, -1);
  case Qt::Key_Left:
    return CellPosition(-1, 0);
  case Qt::Key_Right:
    return CellPosition(1, 0);
  default:
    return CellPosition(0, 0);
  }
}

const Orientation* Orientations::leftToRight() {
  static const LeftToRightOrientation layout;
  return &layout;
}
