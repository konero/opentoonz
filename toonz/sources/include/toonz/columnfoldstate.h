#pragma once

#include "tcommon.h"
#include <set>

#undef DVAPI
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

class TIStream;
class TOStream;

// Logical folded columns, independent of any panel's pixel projection.
class DVAPI ColumnFoldState {
  std::set<int> m_folded;

public:
  bool isFolded(int column) const { return m_folded.count(column) != 0; }
  void setFolded(int column, bool folded);
  const std::set<int>& columns() const { return m_folded; }
  void insertColumn(int column);
  void removeColumn(int column);
  void moveColumn(int from, int to);
  bool hasFoldedColumns() const;
  void saveData(TOStream& stream) const;
  void loadData(TIStream& stream);
};
