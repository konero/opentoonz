#include "xsheetengine.h"

#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/sceneproperties.h"
#include "tconvert.h"

void XsheetEngine::invalidate() {
  m_sheet = nullptr;
  m_singleLevels.clear();
}

TXshLevel* XsheetEngine::singleLevel(TXsheet* sheet, int column) {
  if (sheet != m_sheet) {
    invalidate();
    m_sheet = sheet;
  }
  auto found = m_singleLevels.find(column);
  if (found != m_singleLevels.end()) return found->second.getPointer();

  TXshLevelP level;
  TXshColumn* col       = sheet ? sheet->getColumn(column) : nullptr;
  TXshCellColumn* cells = col ? col->getCellColumn() : nullptr;
  if (cells) {
    int first, last;
    cells->getRange(first, last);
    for (int row = first; row <= last; ++row) {
      const TXshCell& cell = cells->getCell(row);
      if (cell.isEmpty()) continue;
      if (!level)
        level = cell.m_level;
      else if (level != cell.m_level) {
        level = TXshLevelP();
        break;
      }
    }
  }
  return m_singleLevels.emplace(column, level).first->second.getPointer();
}

XsheetEngine::ColumnHeaderData XsheetEngine::columnHeader(TXsheet* sheet,
                                                          int index,
                                                          bool linkEmptyNames,
                                                          bool showLevelNames) {
  ColumnHeaderData data;
  if (!sheet) return data;
  data.column   = sheet->getColumn(index);
  data.empty    = index >= 0 && sheet->isColumnEmpty(index);
  data.objectId = index < 0
                      ? TStageObjectId::CameraId(sheet->getCameraColumnIndex())
                      : TStageObjectId::ColumnId(index);
  TStageObject* object = sheet->getStageObject(data.objectId);
  data.name            = object->getName();
  data.parentId        = object->getParent();
  TStageObject* parent = sheet->getStageObject(data.parentId);
  data.parentName      = parent->getName();
  data.parentHasName   = parent->hasSpecifiedName();
  data.parentHandle    = object->getParentHandle();
  data.cameraActive =
      sheet->getStageObjectTree()->getCurrentCameraId().getIndex() ==
      sheet->getCameraColumnIndex();
  ToonzScene* scene = sheet->getScene();
  if (data.empty && linkEmptyNames && object->hasSpecifiedName() && scene)
    data.reservedLevel = scene->getLevelSet()->getLevel(to_wstring(data.name));
  if (data.column && !object->hasSpecifiedName() && showLevelNames)
    if (TXshLevel* level = singleLevel(sheet, index))
      data.name = to_string(level->getName());
  auto* fx = dynamic_cast<TXshZeraryFxColumn*>(data.column);
  if (fx && !data.empty)
    data.name = to_string(fx->getZeraryColumnFx()->getZeraryFx()->getName());
  if (scene && data.column)
    data.filterColor = scene->getProperties()->getColorFilterColor(
        data.column->getColorFilterId());
  return data;
}

XsheetEngine::ToggleResult XsheetEngine::toggleColumns(
    TXsheet* sheet, ColumnToggle toggle, bool includeCamera,
    bool excludeSpecialColumns, bool unifyVisibility, ColumnTargets targets) {
  ToggleResult result;
  if (!sheet) return result;
  for (int col = includeCamera ? -1 : 0; col < sheet->getColumnCount(); ++col) {
    TXshColumn* column = sheet->getColumn(col);
    if (!column ||
        (targets == ColumnTargets::NonEmpty && sheet->isColumnEmpty(col)))
      continue;
    if (excludeSpecialColumns &&
        (column->getPaletteColumn() || column->getSoundTextColumn()))
      continue;
    switch (toggle) {
    case ColumnToggle::Preview:
      column->setPreviewVisible(!column->isPreviewVisible());
      break;
    case ColumnToggle::Camstand:
      column->setCamstandVisible(!column->isCamstandVisible());
      if (unifyVisibility)
        column->setPreviewVisible(column->isCamstandVisible());
      result.soundChanged |= column->getSoundColumn() != nullptr;
      break;
    case ColumnToggle::Lock:
      column->lock(!column->isLocked());
      break;
    }
    result.changed = true;
  }
  return result;
}
