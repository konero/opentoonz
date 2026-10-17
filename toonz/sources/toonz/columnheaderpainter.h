#pragma once

#include <QCache>
#include <QColor>
#include <QFont>
#include <QPixmap>
#include <QPoint>
#include "xsheetengine.h"

class XsheetViewer;
class Orientation;
class TXsheet;
class TXshColumn;
class TXshLevel;
class QPainter;

namespace XsheetGUI {

// Presentation choices supplied by each panel, rather than inferred from an
// orientation flag in shared painting code.
struct ColumnHeaderStyle {
  QColor iconBorder;
  int fillLeftInset             = 0;
  Qt::Alignment numberAlignment = Qt::AlignHCenter | Qt::AlignVCenter;
  int nameLeftInset             = 3;
  int nameRightInset            = -2;
  bool drawLayerBoundary        = true;
  bool cameraVisibilityAreas    = false;
  bool highlightNameOnHover     = false;
  bool hideInactiveIcons        = false;
  bool previewBacklightsName    = false;
  int cameraNameRotation        = 0;
  QPoint volumeTickAdvance;
  QPoint volumeTickDirection;
};

struct ColumnHeaderInteraction {
  bool current       = false;
  bool selected      = false;
  bool editingSpline = false;
};

// Per-panel, bounded resources. A scene or theme change cannot affect another
// panel's cache, and a missing thumbnail never masks an icon completion.
class ColumnHeaderResources {
  QCache<QString, QPixmap> m_pixmaps{4096};  // cost in KiB
  QString m_fontName;
  QFont m_font;

public:
  QPixmap svg(const QString& path, const QSize& size, const QColor& background,
              qreal devicePixelRatio);
  QPixmap scaled(const QPixmap& source, const QSize& size);
  QPixmap sound(bool playing, qreal devicePixelRatio);
  QPixmap colorChip(const QColor& color);
  QFont font();
  void clear() { m_pixmaps.clear(); }
};

// Common column drawing primitives. The owning panel supplies geometry, theme,
// hover state and resources; this helper does not own an interaction or widget.
class ColumnHeaderPainter {
  QPainter& p;
  int col;
  XsheetViewer* m_viewer;
  const Orientation* o;
  TXsheet* xsh;
  bool isEmpty, isCurrent;
  TXshColumn* column;
  QPoint orig;
  QPoint m_hoverPos;
  TXshLevel* reservedLevel;
  ColumnHeaderStyle m_style;
  XsheetEngine::ColumnHeaderData m_data;
  ColumnHeaderInteraction m_interaction;

  void prepare() const;
  void levelColors(QColor& columnColor, QColor& dragColor) const;
  void paletteColors(QColor& columnColor, QColor& dragColor) const;
  void drawBaseFill(const QColor& columnColor, const QColor& dragColor) const;
  void drawEye() const;
  void drawPreviewToggle(int opacity) const;
  void drawUnifiedViewToggle(int opacity) const;
  void drawLock() const;
  void drawConfig() const;
  void drawColumnNumber() const;
  void drawColumnName() const;
  void drawThumbnail(const QPixmap& iconPixmap) const;
  void drawPegbarName() const;
  void drawParentHandleName() const;
  void drawFilterColor() const;
  void drawSoundIcon(bool isPlaying) const;
  void drawVolumeControl(double volume) const;

public:
  ColumnHeaderPainter(XsheetViewer* viewer, const QPoint& hoverPos, QPainter& p,
                      int column, const ColumnHeaderInteraction& interaction);
  void paint(const QPixmap& thumbnail) const;
};

}  // namespace XsheetGUI
