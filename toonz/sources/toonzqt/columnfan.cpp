

#include "toonz/columnfan.h"

// STD includss
#include <algorithm>
#include <assert.h>

//=============================================================================
// ColumnFan

//=============================================================================
ColumnFan::ColumnFan()
    : m_firstFreePos(0)
    , m_unfolded(74)
    , m_folded(9)
    , m_cameraColumnDim(22) {}

//-----------------------------------------------------------------------------

void ColumnFan::setDimensions(int unfolded, int cameraColumn) {
  m_unfolded        = unfolded;
  m_cameraColumnDim = cameraColumn;
  // folded always 9
  update();
}

//-----------------------------------------------------------------------------

void ColumnFan::update() {
  int lastFoldedColumn = -1;
  for (int column : foldStateData().columns())
    if (column >= 0) lastFoldedColumn = std::max(lastFoldedColumn, column);
  m_columns.resize(lastFoldedColumn + 1);

  int lastPos     = -m_unfolded;
  bool lastActive = true;
  int m           = m_columns.size();
  int i;
  for (i = 0; i < m; i++) {
    const bool active = !foldStateData().isFolded(i);
    m_columns[i].m_active = active;
    if (lastActive)
      lastPos += m_unfolded;
    else if (active)
      lastPos += m_folded;
    m_columns[i].m_pos = lastPos;
    lastActive         = active;
  }
  m_firstFreePos = lastPos + (lastActive ? m_unfolded : m_folded);
  m_table.clear();
  for (i = 0; i < m; i++)
    if (m_columns[i].m_active)
      m_table[m_columns[i].m_pos + m_unfolded - 1] = i;
    else if (i + 1 < m && m_columns[i + 1].m_active)
      m_table[m_columns[i + 1].m_pos - 1] = i;
    else if (i + 1 == m)
      m_table[m_firstFreePos - 1] = i;
}

//-----------------------------------------------------------------------------

int ColumnFan::layerAxisToCol(int coord) const {
  if (m_cameraVisible) {
    const bool cameraActive = !foldStateData().isFolded(-1);
    int firstCol =
        cameraActive
            ? m_cameraColumnDim
            : ((m_columns.size() > 0 && !m_columns[0].m_active) ? 0
                                                                : m_folded);
    if (coord < firstCol) return -1;
    coord -= firstCol;
  }
  if (coord < m_firstFreePos) {
    std::map<int, int>::const_iterator it = m_table.lower_bound(coord);
    if (it == m_table.end()) return -3;
    assert(it != m_table.end());
    return it->second;
  } else
    return m_columns.size() + (coord - m_firstFreePos) / m_unfolded;
}

//-----------------------------------------------------------------------------

int ColumnFan::colToLayerAxis(int col) const {
  int m        = m_columns.size();
  int firstCol = 0;
  if (m_cameraVisible) {
    if (col < -1) return -m_cameraColumnDim;
    if (col < 0) return 0;
    const bool cameraActive = !foldStateData().isFolded(-1);
    firstCol =
        cameraActive
            ? m_cameraColumnDim
            : ((m_columns.size() > 0 && !m_columns[0].m_active) ? 0
                                                                : m_folded);
  }
  if (col >= 0 && col < m)
    return firstCol + m_columns[col].m_pos;
  else
    return firstCol + m_firstFreePos + (col - m) * m_unfolded;
}

//-----------------------------------------------------------------------------

void ColumnFan::activate(int col) {
  ColumnFoldState state = foldedState();
  state.setFolded(col < 0 ? -1 : col, false);
  setFoldedState(state);
}

//-----------------------------------------------------------------------------

void ColumnFan::deactivate(int col) {
  ColumnFoldState state = foldedState();
  state.setFolded(col < 0 ? -1 : col, true);
  setFoldedState(state);
}

//-----------------------------------------------------------------------------

bool ColumnFan::isActive(int col) const {
  if (col < 0) return !foldStateData().isFolded(-1);
  return col < int(m_columns.size()) ? m_columns[col].m_active
                                     : !foldStateData().isFolded(col);
}

//-----------------------------------------------------------------------------


ColumnFoldState ColumnFan::foldedState() const {
  return foldStateData();
}

void ColumnFan::setFoldedState(const ColumnFoldState& state) {
  if (m_foldState) {
    if (m_onFoldStateChange) m_onFoldStateChange(m_foldStateContext, state);
    return;
  }
  m_localFoldState = state;
  update();
}

void ColumnFan::setSharedFoldState(
    const ColumnFoldState& state, void* context,
    ColumnFoldStateChangeCallback onChange) {
  assert(onChange);
  m_foldState          = &state;
  m_foldStateContext   = context;
  m_onFoldStateChange  = onChange;
  update();
}

void ColumnFan::refreshProjection() { update(); }
