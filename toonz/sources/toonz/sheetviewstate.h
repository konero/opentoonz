#pragma once

#include "toonz/txsheet.h"
#include "toonz/columnfan.h"
#include <functional>

// A panel-local geometry projection of the shared document fold state.
class SheetViewState final : public TXsheetColumnChangeListener {
  TXsheetP m_sheet;
  ColumnFan m_columns;
  std::function<void()> m_onChange;

public:
  SheetViewState(TXsheet* sheet, int layerSize, int cameraSize,
                 std::function<void()> onChange = {});
  ~SheetViewState();
  ColumnFan* columns() { return &m_columns; }
  void onColumnChange(const TXsheetColumnChange& change) override;
};
