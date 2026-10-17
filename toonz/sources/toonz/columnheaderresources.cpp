#include "columnheaderpainter.h"
#include "toonz/preferences.h"
#include "toonzqt/gutil.h"

#include <QPainter>
#include <QSvgRenderer>
#include <QtMath>

namespace XsheetGUI {

QPixmap ColumnHeaderResources::svg(const QString& path, const QSize& size,
                                   const QColor& background, qreal dpr) {
  if (path.isEmpty() || size.isEmpty()) return QPixmap();
  const QString key = QString("svg:%1:%2:%3:%4:%5")
                          .arg(path)
                          .arg(size.width())
                          .arg(size.height())
                          .arg(dpr, 0, 'g', 8)
                          .arg(background.rgba());
  if (auto cached = m_pixmaps.object(key)) return *cached;
  QSvgRenderer renderer(path);
  if (!renderer.isValid()) return QPixmap();
  const QSize pixels(qCeil(size.width() * dpr), qCeil(size.height() * dpr));
  QPixmap pixmap(pixels);
  pixmap.fill(background);
  QSizeF fitted = renderer.defaultSize();
  fitted.scale(QSizeF(pixels), Qt::KeepAspectRatio);
  QPainter painter(&pixmap);
  renderer.render(&painter, QRectF((pixels.width() - fitted.width()) / 2,
                                   (pixels.height() - fitted.height()) / 2,
                                   fitted.width(), fitted.height()));
  painter.end();
  pixmap.setDevicePixelRatio(dpr);
  const int cost = std::max(1, pixmap.width() * pixmap.height() * 4 / 1024);
  m_pixmaps.insert(key, new QPixmap(pixmap), cost);
  return pixmap;
}

QPixmap ColumnHeaderResources::scaled(const QPixmap& source,
                                      const QSize& size) {
  if (source.isNull() || size.isEmpty()) return QPixmap();
  const QString key = QString("thumbnail:%1:%2:%3:%4")
                          .arg(source.cacheKey())
                          .arg(size.width())
                          .arg(size.height())
                          .arg(source.devicePixelRatioF());
  if (auto cached = m_pixmaps.object(key)) return *cached;
  QPixmap result = scalePixmapKeepingAspectRatio(source, size);
  const int cost = std::max(1, result.width() * result.height() * 4 / 1024);
  m_pixmaps.insert(key, new QPixmap(result), cost);
  return result;
}

//-----------------------------------------------------------------------------
QPixmap ColumnHeaderResources::sound(bool playing, qreal dpr) {
  const QString key = QString("sound:%1:%2").arg(playing).arg(dpr);
  if (auto cached = m_pixmaps.object(key)) return *cached;
  QPixmap result = createQIcon("sound_header", false, dpr)
                       .pixmap(QSize(40, 30), QIcon::Normal,
                               playing ? QIcon::On : QIcon::Off);
  m_pixmaps.insert(key, new QPixmap(result),
                   std::max(1, result.width() * result.height() * 4 / 1024));
  return result;
}

QPixmap ColumnHeaderResources::colorChip(const QColor& color) {
  const QString key = QString("filter:%1").arg(color.rgba());
  if (auto cached = m_pixmaps.object(key)) return *cached;
  QPixmap result(12, 12);
  result.fill(Qt::white);
  QPainter painter(&result);
  if (color.alpha() != 255) {
    painter.fillRect(0, 0, 6, 6, Qt::black);
    painter.fillRect(6, 6, 6, 6, Qt::black);
  }
  painter.fillRect(result.rect(), color);
  painter.end();
  m_pixmaps.insert(key, new QPixmap(result), 1);
  return result;
}

QFont ColumnHeaderResources::font() {
  QString name = Preferences::instance()->getInterfaceFont();
  if (name.isEmpty()) {
#ifdef _WIN32
    name = "Arial";
#else
    name = "Helvetica";
#endif
  }
  if (name != m_fontName) {
    m_fontName = name;
    m_font     = QFont(name, -1, QFont::Normal);
    m_font.setPixelSize(12);
  }
  return m_font;
}

}  // namespace XsheetGUI
