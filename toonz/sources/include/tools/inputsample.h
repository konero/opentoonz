#pragma once

#ifndef TOOL_INPUT_SAMPLE_INCLUDED
#define TOOL_INPUT_SAMPLE_INCLUDED

#include "tgeometry.h"
#include "tools/tooltimer.h"
#include <QPointF>

struct TNativeInputSample {
  QPointF position;
  qint64 timestamp = -1;
};

// A single, already transformed point from a pointing-device stream.
struct TToolInputSample {
  TPointD position;
  double pressure = 1.0;
  TPointD tilt;
  TTimerTicks timestamp = 0;
  bool isTablet = false;
  bool isHighFrequent = false;
};

#endif
