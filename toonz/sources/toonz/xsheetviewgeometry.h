#pragma once

#include "cellposition.h"
#include "toonz/columnfan.h"
#include <QPoint>

// Vertical panel coordinates, including quantized frame boundaries at zoom.
namespace XsheetViewGeometry {
inline QPoint pointAt(const CellPosition& cell, const ColumnFan& columns,
                      int frameSize, int zoom) {
  return QPoint(columns.colToLayerAxis(cell.layer()),
                int(qint64(cell.frame()) * frameSize * zoom / 100));
}

inline CellPosition cellAt(const QPoint& point, const ColumnFan& columns,
                           int frameSize, int zoom) {
  const qint64 denominator = qint64(frameSize) * zoom;
  const int frame = point.y() >= 0
                        ? int(((qint64(point.y()) + 1) * 100 - 1) / denominator)
                        : int(qint64(point.y()) * 100 / denominator);
  return CellPosition(frame, columns.layerAxisToCol(point.x()));
}
}  // namespace XsheetViewGeometry
