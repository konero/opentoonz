#pragma once

#ifndef TXSHEETHANDLE_H
#define TXSHEETHANDLE_H

#include <QObject>

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TXsheet;

//=============================================================================
// TXsheetHandle
//-----------------------------------------------------------------------------

class DVAPI TXsheetHandle final : public QObject {
  Q_OBJECT

  TXsheet *m_xsheet;
  int m_zoomFactor;

public:
  TXsheetHandle();
  ~TXsheetHandle();

  TXsheet *getXsheet() const;
  void setXsheet(TXsheet *xsheet);
  void notifyXsheetChanged() { emit xsheetChanged(); }
  void notifyXsheetSwitched() { emit xsheetSwitched(); }
  void notifyXsheetSoundChanged() { emit xsheetSoundChanged(); }
  void notifyNavigationTagsChanged() { emit navigationTagsChanged(); }
  void changeXsheetCamera(int index) { emit xsheetCameraChange(index); }
  // Vertical XSheet frame spacing, relative to the standard 20-pixel frame.
  // Used by Function Editor's optional size sync; Timeline never publishes it.
  void notifyZoomScaleChanged(int factor) {
    m_zoomFactor = factor;
    emit zoomScaleChanged();
  }
  int getZoomFactor() { return m_zoomFactor; }

signals:
  void xsheetSwitched();
  void xsheetChanged();
  void xsheetSoundChanged();
  void navigationTagsChanged();
  void xsheetCameraChange(int);
  void zoomScaleChanged();
};

#endif  // TXSHEETHANDLE_H
