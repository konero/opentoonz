#include "sheetviewstate.h"

SheetViewState::SheetViewState(TXsheet* sheet, int layerSize, int cameraSize,
                               std::function<void()> onChange)
    : m_sheet(sheet), m_onChange(onChange) {
  m_columns.setDimensions(layerSize, cameraSize);
  m_columns.setSharedFoldState(
      sheet->getColumnFoldState(), sheet,
      [](void* context, const ColumnFoldState& state) {
        static_cast<TXsheet*>(context)->setColumnFoldState(state);
      });
  sheet->addColumnChangeListener(this);
}

SheetViewState::~SheetViewState() { m_sheet->removeColumnChangeListener(this); }

void SheetViewState::onColumnChange(const TXsheetColumnChange& change) {
  m_columns.refreshProjection();
  if (change.m_type == TXsheetColumnChange::Fold && m_onChange) m_onChange();
}
