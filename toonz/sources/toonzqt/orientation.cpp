#include "orientation.h"

int NumberRange::weight(double toWeight) const {  // weight ranging 0..1
  return _from + (_to - _from) * toWeight;
}

NumberRange NumberRange::adjusted(int addFrom, int addTo) const {
  return NumberRange(_from + addFrom, _to + addTo);
}

double NumberRange::ratio(int at) const {
  double result = ((double)at - _from) / (_to - _from);
  if (result < 0) result = 0;
  if (result > 1) result = 1;
  return result;
}

/// -------------------------------------------------------------------------------

QLine Orientation::verticalLine(int layerAxis,
                                const NumberRange& frameAxis) const {
  QPoint first  = frameLayerToXY(frameAxis.from(), layerAxis);
  QPoint second = frameLayerToXY(frameAxis.to(), layerAxis);
  return QLine(first, second);
}
QLine Orientation::horizontalLine(int frameAxis,
                                  const NumberRange& layerAxis) const {
  QPoint first  = frameLayerToXY(frameAxis, layerAxis.from());
  QPoint second = frameLayerToXY(frameAxis, layerAxis.to());
  return QLine(first, second);
}
QRect Orientation::frameLayerRect(const NumberRange& frameAxis,
                                  const NumberRange& layerAxis) const {
  QPoint topLeft     = frameLayerToXY(frameAxis.from(), layerAxis.from());
  QPoint bottomRight = frameLayerToXY(frameAxis.to(), layerAxis.to());
  return QRect(topLeft, bottomRight);
}

QRect Orientation::foldedRectangle(int layerAxis, const NumberRange& frameAxis,
                                   int i) const {
  QPoint topLeft = frameLayerToXY(frameAxis.from(), layerAxis + 1 + i * 3);
  QPoint size    = frameLayerToXY(frameAxis.length(), 2);
  return QRect(topLeft, QSize(size.x(), size.y()));
}
QLine Orientation::foldedRectangleLine(int layerAxis,
                                       const NumberRange& frameAxis,
                                       int i) const {
  return verticalLine(layerAxis + i * 3, frameAxis);
}

void Orientation::addRect(PredefinedRect which, const QRect& rect) {
  _rects.erase(which);
  _rects.emplace(which, rect);
}
void Orientation::addLine(PredefinedLine which, const QLine& line) {
  _lines.erase(which);
  _lines.emplace(which, line);
}
void Orientation::addDimension(PredefinedDimension which, int dimension) {
  _dimensions.erase(which);
  _dimensions.emplace(which, dimension);
}
void Orientation::addPath(PredefinedPath which, const QPainterPath& path) {
  _paths.erase(which);
  _paths.emplace(which, path);
}
void Orientation::addPoint(PredefinedPoint which, const QPoint& point) {
  _points.erase(which);
  _points.emplace(which, point);
}
void Orientation::addRange(PredefinedRange which, const NumberRange& range) {
  _ranges.erase(which);
  _ranges.emplace(which, range);
}
void Orientation::addFlag(PredefinedFlag which, const bool& flag) {
  _flags.erase(which);
  _flags.emplace(which, flag);
}
/// -------------------------------------------------------------------------------
