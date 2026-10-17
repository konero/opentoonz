#pragma once

#ifndef XSHEET_ENGINE_H
#define XSHEET_ENGINE_H

#include "toonz/txshlevel.h"
#include "toonz/tstageobjectid.h"
#include "tpixel.h"

#include <map>

class TXsheet;
class TXshColumn;

// Document queries and operations shared by sheet editors. No view geometry or
// widget state belongs here. Each editor owns its query cache.
class XsheetEngine {
  TXsheet* m_sheet = nullptr;
  std::map<int, TXshLevelP> m_singleLevels;

public:
  void invalidate();
  TXshLevel* singleLevel(TXsheet* sheet, int column);

  struct ColumnHeaderData {
    TXshColumn* column = nullptr;  // Valid for the current synchronous paint.
    bool empty         = true;
    bool cameraActive  = false;
    TXshLevelP reservedLevel;
    TStageObjectId objectId, parentId;
    std::string name, parentName, parentHandle;
    bool parentHasName = false;
    TPixel32 filterColor;
  };
  ColumnHeaderData columnHeader(TXsheet* sheet, int column, bool linkEmptyNames,
                                bool showLevelNames);

  enum class ColumnToggle { Preview, Camstand, Lock };
  enum class ColumnTargets { NonEmpty, Existing };
  struct ToggleResult {
    bool changed      = false;
    bool soundChanged = false;
  };

  static ToggleResult toggleColumns(
      TXsheet* sheet, ColumnToggle toggle, bool includeCamera,
      bool excludeSpecialColumns, bool unifyVisibility,
      ColumnTargets targets = ColumnTargets::NonEmpty);
};

#endif
