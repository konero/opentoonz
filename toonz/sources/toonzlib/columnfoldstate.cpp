#include "toonz/columnfoldstate.h"
#include "tstream.h"
#include "texception.h"
#include <climits>

void ColumnFoldState::setFolded(int column, bool folded) {
  if (column < -1) return;
  if (folded)
    m_folded.insert(column);
  else
    m_folded.erase(column);
}

void ColumnFoldState::insertColumn(int column) {
  std::set<int> shifted;
  for (int value : m_folded)
    shifted.insert(value >= column ? value + 1 : value);
  m_folded.swap(shifted);
}

void ColumnFoldState::removeColumn(int column) {
  std::set<int> shifted;
  for (int value : m_folded) {
    if (value == column) continue;
    shifted.insert(value > column ? value - 1 : value);
  }
  m_folded.swap(shifted);
}

void ColumnFoldState::moveColumn(int from, int to) {
  std::set<int> shifted;
  for (int value : m_folded) {
    if (value == from)
      value = to;
    else if (from < to && value > from && value <= to)
      --value;
    else if (from > to && value >= to && value < from)
      ++value;
    shifted.insert(value);
  }
  m_folded.swap(shifted);
}

bool ColumnFoldState::hasFoldedColumns() const {
  return m_folded.lower_bound(0) != m_folded.end();
}

void ColumnFoldState::saveData(TOStream& stream) const {
  auto it = m_folded.lower_bound(0);
  while (it != m_folded.end()) {
    int first = *it++, count = 1;
    while (it != m_folded.end() && *it == first + count) {
      ++count;
      ++it;
    }
    stream << first << count;
  }
}

void ColumnFoldState::loadData(TIStream& stream) {
  const bool camera = isFolded(-1);
  m_folded.clear();
  setFolded(-1, camera);
  while (!stream.eos()) {
    int first = 0, count = 0;
    stream >> first >> count;
    if (first < 0 || count < 0 || first > INT_MAX - count)
      throw TException("Invalid folded column range");
    for (int i = 0; i < count; ++i) m_folded.insert(first + i);
  }
}
