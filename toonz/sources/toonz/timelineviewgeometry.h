#pragma once

#include "cellposition.h"
#include "toonz/columnfan.h"
#include <QPoint>

// Horizontal panel coordinates. Reversed layer boundaries are half-open so
// the top pixel of a layer belongs to that layer, not its neighbor.
namespace TimelineViewGeometry {
inline QPoint pointAt(const CellPosition& cell, const ColumnFan& columns,
                      int frameSize, int layerSize, int foldedSize,
                      int columnCount, int zoom) {
  const int height    = columns.colToLayerAxis(std::max(1, columnCount));
  const int thickness = columns.isActive(cell.layer()) ? layerSize : foldedSize;
  return QPoint(
      int(qint64(cell.frame()) * frameSize * zoom / 100),
      height ? height - columns.colToLayerAxis(cell.layer()) - thickness : 0);
}

inline CellPosition cellAt(const QPoint& point, const ColumnFan& columns,
                           int frameSize, int layerSize, int columnCount,
                           int zoom) {
  const qint64 denominator = qint64(frameSize) * zoom;
  const int frame = point.x() >= 0
                        ? int(((qint64(point.x()) + 1) * 100 - 1) / denominator)
                        : int(qint64(point.x()) * 100 / denominator);
  const int height = columns.colToLayerAxis(std::max(1, columnCount));
  const int layer  = point.y() >= height
                         ? -(point.y() - height) / layerSize - 1
                         : columns.layerAxisToCol(height - 1 - point.y());
  return CellPosition(frame, layer);
}
}  // namespace TimelineViewGeometry
