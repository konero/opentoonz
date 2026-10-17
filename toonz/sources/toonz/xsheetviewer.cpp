

#include "xsheetviewer.h"
#include "sceneviewerevents.h"
#include "tapp.h"
#include "floatingpanelcommand.h"
#include "menubarcommandids.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/preferences.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/icongenerator.h"
#include "cellselection.h"
#include "keyframeselection.h"
#include "cellkeyframeselection.h"
#include "columnselection.h"
#include "xsheetdragtool.h"
#include "toonzqt/gutil.h"

#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/toonzscene.h"
#include "toonz/columnfan.h"
#include "toonz/txshnoteset.h"
#include "toonz/childstack.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tproject.h"
#include "tconvert.h"
#include "toonz/navigationtags.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/txshchildlevel.h"
#include "toonz/levelset.h"

#include "tenv.h"

#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QMainWindow>

TEnv::IntVar FrameDisplayStyleInXsheetRowArea(
    "FrameDisplayStyleInXsheetRowArea", 0);

//=============================================================================
namespace XsheetGUI {
//-----------------------------------------------------------------------------

const int ColumnWidth       = 74;
const int RowHeight         = 20;
const int SCROLLBAR_WIDTH   = 16;
const int TOOLBAR_HEIGHT    = 29;
const int BREADCRUMB_HEIGHT = 29;
const int ZOOM_FACTOR_MAX   = 100;
const int ZOOM_FACTOR_MIN   = 20;
}  // namespace XsheetGUI

//=============================================================================
// XsheetViewer
//-----------------------------------------------------------------------------

void XsheetViewer::getCellTypeAndColors(int& ltype, QColor& cellColor,
                                       QColor& sideColor, const TXshCell& cell,
                                       bool isSelected) {
  if (cell.isEmpty())
    ltype = NO_XSHLEVEL;
  else {
    ltype = cell.m_level->getType();
    switch (ltype) {
    case TZI_XSHLEVEL:
    case OVL_XSHLEVEL:
      cellColor = (isSelected) ? getSelectedFullcolorColumnColor()
                               : getFullcolorColumnColor();
      sideColor = getFullcolorColumnBorderColor();
      break;
    case PLI_XSHLEVEL:
      cellColor = (isSelected) ? getSelectedVectorColumnColor()
                               : getVectorColumnColor();
      sideColor = getVectorColumnBorderColor();
      break;
    case TZP_XSHLEVEL:
      cellColor =
          (isSelected) ? getSelectedLevelColumnColor() : getLevelColumnColor();
      sideColor = getLevelColumnBorderColor();
      break;
    case PLT_XSHLEVEL:
      cellColor = (isSelected) ? getSelectedPaletteColumnColor()
                               : getPaletteColumnColor();
      sideColor = getPaletteColumnBorderColor();
      break;
    case ZERARYFX_XSHLEVEL:
      cellColor =
          (isSelected) ? getSelectedFxColumnColor() : getFxColumnColor();
      sideColor = getFxColumnBorderColor();
      break;
    case CHILD_XSHLEVEL:
      cellColor =
          (isSelected) ? getSelectedChildColumnColor() : getChildColumnColor();
      sideColor = getChildColumnBorderColor();
      break;
    case SND_XSHLEVEL:
      cellColor =
          (isSelected) ? m_selectedSoundColumnColor : m_soundColumnColor;
      sideColor = m_soundColumnBorderColor;
      break;
    case SND_TXT_XSHLEVEL:
      cellColor = (isSelected) ? getSelectedSoundTextColumnColor()
                               : getSoundTextColumnColor();
      sideColor = getSoundTextColumnBorderColor();
      break;
    case MESH_XSHLEVEL:
      cellColor =
          (isSelected) ? getSelectedMeshColumnColor() : getMeshColumnColor();
      sideColor = getMeshColumnBorderColor();
      break;
    case META_XSHLEVEL:
      cellColor =
          (isSelected) ? getSelectedMetaColumnColor() : getMetaColumnColor();
      sideColor = getMetaColumnBorderColor();
      break;
    case UNKNOWN_XSHLEVEL:
    case NO_XSHLEVEL:
    default:
      // non dovrebbe succedere
      cellColor = grey210;
      sideColor = grey150;
    }
  }
}

//-----------------------------------------------------------------------------
void XsheetViewer::getColumnColor(QColor& color, QColor& sideColor, int index,
                                 TXsheet* xsh) {
  if (index < 0 || xsh->isColumnEmpty(index)) return;
  int r0, r1;
  xsh->getCellRange(index, r0, r1);
  if (0 <= r0 && r0 <= r1) {
    // column color depends on the level type in the top-most occupied cell
    if (xsh->getColumn(index)->getSoundColumn()) {
      color     = m_soundColumnColor;
      sideColor = m_soundColumnBorderColor;
    } else {
      TXshCell cell = xsh->getCell(r0, index);
      int ltype;
      getCellTypeAndColors(ltype, color, sideColor, cell);
    }
  }
  if (xsh->getColumn(index)->isMask()) color = QColor(255, 0, 255);
}

//-----------------------------------------------------------------------------
XsheetViewer::XsheetViewer(const Orientation* orientation, const QString& layout,
                         QWidget* parent, Qt::WindowFlags flags)
    : QFrame(parent, flags)
    , m_timerId(0)
    , m_autoPanSpeed(0, 0)
    , m_dragTool(0)
    , m_columnSelection(new TColumnSelection())
    , m_cellKeyframeSelection(new TCellKeyframeSelection(
          new TCellSelection(), new TKeyframeSelection()))
    , m_scrubCol(-1)
    , m_scrubRow0(-1)
    , m_scrubRow1(-1)
    , m_isCurrentFrameSwitched(false)
    , m_isCurrentColumnSwitched(false)
    , m_isComputingSize(false)
    , m_currentNoteIndex(0)
    , m_qtModifiers(Qt::KeyboardModifiers())
    , m_frameDisplayStyle(to_enum(FrameDisplayStyleInXsheetRowArea))
    , m_orientation(orientation)
    , m_xsheetLayout(layout)
    , m_frameZoomFactor(100) {}

void XsheetViewer::initialize() {
  setFocusPolicy(Qt::StrongFocus);

  setFrameStyle(QFrame::StyledPanel);
  setObjectName("XsheetViewer");

  m_cellKeyframeSelection->setXsheetHandle(
      TApp::instance()->getCurrentXsheet());

  m_toolbarScrollArea = new XsheetScrollArea(this);
  m_toolbarScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_toolbarScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_toolbar = new XsheetGUI::XSheetToolbar(this, Qt::WindowFlags(), true);
  m_toolbarScrollArea->setWidget(m_toolbar);

  m_breadcrumbArea = new XsheetGUI::BreadcrumbArea(this, Qt::WindowFlags());
  m_breadcrumbScrollArea = new XsheetScrollArea(this);
  m_breadcrumbScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_breadcrumbScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_breadcrumbScrollArea->setWidget(m_breadcrumbArea);

  m_noteArea       = new XsheetGUI::NoteArea(this);
  m_noteScrollArea = new XsheetScrollArea(this);
  m_noteScrollArea->setObjectName("xsheetArea");
  m_noteScrollArea->setWidget(m_noteArea);
  m_noteScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_noteScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_cellArea       = new XsheetGUI::CellArea(this);
  m_cellScrollArea = new XsheetScrollArea(this);
  m_cellScrollArea->setObjectName("xsheetArea");
  m_cellScrollArea->setWidget(m_cellArea);
  m_cellScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_cellScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  m_columnArea       = new XsheetGUI::ColumnArea(this);
  m_columnScrollArea = new XsheetScrollArea(this);
  m_columnScrollArea->setObjectName("xsheetArea");
  m_columnScrollArea->setWidget(m_columnArea);
  m_columnScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_columnScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_rowArea       = new XsheetGUI::RowArea(this);
  m_rowScrollArea = new XsheetScrollArea(this);
  m_rowScrollArea->setObjectName("xsheetArea");
  m_rowScrollArea->setWidget(m_rowArea);
  m_rowScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_rowScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_layerFooterPanel = new LayerFooterPanel(this, this);

  connectScrollBars();

  connect(m_toolbar, SIGNAL(updateVisibility()), this,
          SLOT(positionSections()));

  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, [this] { m_engine.invalidate(); });
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
          this, [this] {
            m_engine.invalidate();
            resetInteraction();
          });
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, [this] {
            m_viewStates.clear();
            m_engine.invalidate();
            resetInteraction();
          });
  m_initialized = true;
  onPreferenceChanged("XsheetCamera");
}

//-----------------------------------------------------------------------------
XsheetViewer::~XsheetViewer() {
  disconnectVisibleSignals();
  if (TApp::instance()->getCurrentSheetViewer() == this)
    TApp::instance()->setCurrentSheetViewer(nullptr);
  auto* selection = TApp::instance()->getCurrentSelection();
  if (selection->getSelection() == m_columnSelection ||
      selection->getSelection() == getCellSelection() ||
      selection->getSelection() == getKeyframeSelection() ||
      selection->getSelection() == m_cellKeyframeSelection)
    selection->setSelection(nullptr);
  delete m_columnSelection;
  delete m_cellKeyframeSelection;
  delete m_dragTool;
}

//-----------------------------------------------------------------------------
void XsheetViewer::setDragTool(XsheetGUI::DragTool* dragTool) {
  assert(m_dragTool == 0);
  m_dragTool = dragTool;
}

//-----------------------------------------------------------------------------
void XsheetViewer::dragToolClick(QMouseEvent* e) {
  if (m_dragTool) m_dragTool->onClick(e);
}

void XsheetViewer::dragToolDrag(QMouseEvent* e) {
  if (m_dragTool) m_dragTool->onDrag(e);
}

void XsheetViewer::dragToolRelease(QMouseEvent* e) {
  if (m_dragTool) {
    m_dragTool->onRelease(e);
    delete getDragTool();
    m_dragTool = 0;
  }
}

//-----------------------------------------------------------------------------
void XsheetViewer::dragToolClick(QDropEvent* e) {
  if (m_dragTool) m_dragTool->onClick(e);
}

void XsheetViewer::dragToolDrag(QDropEvent* e) {
  if (m_dragTool) m_dragTool->onDrag(e);
}

void XsheetViewer::dragToolRelease(QDropEvent* e) {
  if (m_dragTool) {
    m_dragTool->onRelease(e);
    delete getDragTool();
    m_dragTool = 0;
  }
}

void XsheetViewer::dragToolLeave(QEvent* e) {
  if (m_dragTool) {
    delete getDragTool();
    m_dragTool = 0;
  }
}

const Orientation* XsheetViewer::orientation() const {
  if (!m_orientation) throw std::runtime_error("!m_orientation");
  return m_orientation;
}

int XsheetViewer::positionToolbar() {
  int top            = 0;
  const int width    = geometry().width();
  const bool toolbar = Preferences::instance()->isShowXSheetToolbarEnabled();
  m_toolbar->showToolbar(toolbar);
  m_toolbarScrollArea->setGeometry(0, 0, width,
                                   toolbar ? XsheetGUI::TOOLBAR_HEIGHT : 0);
  m_toolbar->setFixedWidth(width);
  if (toolbar) top += XsheetGUI::TOOLBAR_HEIGHT;
  const bool breadcrumbs =
      Preferences::instance()->isShowXsheetBreadcrumbsEnabled();
  m_breadcrumbArea->showBreadcrumbs(breadcrumbs);
  m_breadcrumbScrollArea->setGeometry(
      0, top, width, breadcrumbs ? XsheetGUI::BREADCRUMB_HEIGHT : 0);
  m_breadcrumbArea->setFixedWidth(width);
  if (breadcrumbs) {
    top += XsheetGUI::BREADCRUMB_HEIGHT;
    m_breadcrumbArea->updateBreadcrumbs();
  }
  return top;
}

void XsheetViewer::connectScrollBars() {
  QScrollBar* frame   = frameScrollBar();
  QScrollBar* layer   = layerScrollBar();
  QScrollBar* ruler   = rulerScrollBar();
  QScrollBar* headers = headerScrollBar();
  connect(frame, &QScrollBar::valueChanged, ruler, &QScrollBar::setValue);
  connect(ruler, &QScrollBar::valueChanged, frame, &QScrollBar::setValue);
  connect(layer, &QScrollBar::valueChanged, headers, &QScrollBar::setValue);
  connect(headers, &QScrollBar::valueChanged, layer, &QScrollBar::setValue);
  connect(frame, &QScrollBar::valueChanged, this,
          &XsheetViewer::updateCellRowAree);
  connect(layer, &QScrollBar::valueChanged, this,
          &XsheetViewer::updateCellColumnAree);
}

bool XsheetViewer::connectVisible(QObject* sender, const char* signal,
                                 const char* slot) {
  auto connection = connect(sender, signal, this, slot, Qt::UniqueConnection);
  if (connection) m_visibleConnections.push_back(connection);
  return bool(connection);
}

void XsheetViewer::resetInteraction() {
  stopAutoPan();
  delete m_dragTool;
  m_dragTool    = nullptr;
  m_qtModifiers = Qt::NoModifier;
  resetScrubHighlight();
  m_columnArea->resetInteraction();
  m_cellArea->resetInteraction();
  m_noteArea->resetInteraction();
  clearNoteWidgets();
  m_currentNoteIndex = -1;
}

void XsheetViewer::disconnectVisibleSignals() {
  for (const auto& connection : m_visibleConnections) disconnect(connection);
  m_visibleConnections.clear();
}

ColumnFan* XsheetViewer::columnFan() const {
  TXsheet* sheet = getXsheet();
  if (!sheet) return nullptr;
  auto& state = m_viewStates[sheet];
  if (!state) {
    XsheetViewer* viewer = const_cast<XsheetViewer*>(this);
    state             = std::make_unique<SheetViewState>(
        sheet, orientation()->dimension(PredefinedDimension::LAYER),
        orientation()->dimension(PredefinedDimension::CAMERA_LAYER),
        [viewer] {
          if (!viewer->m_initialized) return;
          viewer->refreshContentSize(0, 0);
          viewer->updateAllAree();
        });
  }
  state->columns()->setCameraVisible(
      Preferences::instance()->isXsheetCameraColumnVisible());
  return state->columns();
}

void XsheetViewer::unfoldColumns(int column) {
  ColumnFan* fan = columnFan();
  if (!fan) return;
  ColumnFoldState folded = fan->foldedState();
  for (int i = column; i >= -1 && folded.isFolded(i); --i)
    folded.setFolded(i, false);
  if (getXsheet()->setColumnFoldState(folded))
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

TXsheet* XsheetViewer::getXsheet() const {
  return TApp::instance()->getCurrentXsheet()->getXsheet();
}

//-----------------------------------------------------------------------------
int XsheetViewer::getCurrentColumn() const {
  return TApp::instance()->getCurrentColumn()->getColumnIndex();
}

//-----------------------------------------------------------------------------
int XsheetViewer::getCurrentRow() const {
  return TApp::instance()->getCurrentFrame()->getFrame();
}

//-----------------------------------------------------------------------------
TStageObjectId XsheetViewer::getObjectId(int col) const {
  TXsheet *xsh = getXsheet();
  if (col < 0) return TStageObjectId::CameraId(xsh->getCameraColumnIndex());
  return TStageObjectId::ColumnId(col);
}
//-----------------------------------------------------------------------------

void XsheetViewer::setCurrentColumn(int col) {
  TColumnHandle *columnHandle = TApp::instance()->getCurrentColumn();
  if (col != columnHandle->getColumnIndex() || !columnHandle->getColumn()) {
    columnHandle->setColumnIndex(col);
    // E' necessario per il caso in cui si passa da colonna di camera a altra
    // colonna
    // o nel caso in cui si passa da una spline a una colonna.
    TObjectHandle *objectHandle = TApp::instance()->getCurrentObject();
    if (col >= 0 && objectHandle->isSpline()) objectHandle->setIsSpline(false);
    updateCellColumnAree();
    if (col >= 0) {
      objectHandle->setObjectId(TStageObjectId::ColumnId(col));
      TXsheet *xsh       = getXsheet();
      TXshColumn *column = xsh->getColumn(col);
      if (!column) return;
      // switching the current fx
      TFx *fx                 = nullptr;
      bool doSwitchFxSettings = false;
      if (TXshLevelColumn *lc = column->getLevelColumn())
        fx = lc->getLevelColumnFx();
      else if (TXshPaletteColumn *pc =
                   dynamic_cast<TXshPaletteColumn *>(column))
        fx = pc->getPaletteColumnFx();
      else if (TXshZeraryFxColumn *zc =
                   dynamic_cast<TXshZeraryFxColumn *>(column)) {
        fx                 = zc->getZeraryColumnFx();
        doSwitchFxSettings = true;
      }
      if (!fx) return;
      TApp::instance()->getCurrentFx()->setFx(fx, doSwitchFxSettings);
    }
    return;
  }
}

//-----------------------------------------------------------------------------
void XsheetViewer::setCurrentRow(int row) {
  // POTREBBE NON ESSER PIU' NECESSARIO CON LE NUOVE MODIFICHE PER CLEANUP A
  // COLORI
  /*TFrameHandle* frameHandle = TApp::instance()->getCurrentFrame();
if(row == frameHandle->getFrameIndex() && frameHandle->getFrameType() ==
TFrameHandle::SceneFrame)
{
//E' necessario per il caso in cui la paletta corrente e' la paletta di cleanup.
TPaletteHandle* levelPaletteHandle =
TApp::instance()->getPaletteController()->getCurrentLevelPalette();
TXshLevel *xl = TApp::instance()->getCurrentLevel()->getLevel();
if(xl && xl->getSimpleLevel())
                  levelPaletteHandle->setPalette(xl->getSimpleLevel()->getPalette());
else if(xl && xl->getPaletteLevel())
                  levelPaletteHandle->setPalette(xl->getPaletteLevel()->getPalette());
else
levelPaletteHandle->setPalette(0);
return;
}
frameHandle->setFrame(row);*/
  TApp::instance()->getCurrentFrame()->setFrame(row);
}

//-----------------------------------------------------------------------------
void XsheetViewer::scroll(QPoint delta) {
  int x = delta.x();
  int y = delta.y();

  int valueH    = m_cellScrollArea->horizontalScrollBar()->value() + x;
  int valueV    = m_cellScrollArea->verticalScrollBar()->value() + y;
  int maxValueH = m_cellScrollArea->horizontalScrollBar()->maximum();
  int maxValueV = m_cellScrollArea->verticalScrollBar()->maximum();

  bool notUpdateSizeH = maxValueH > valueH && x >= 0;
  bool notUpdateSizeV = maxValueV > valueV && y >= 0;
  if (!notUpdateSizeH && !notUpdateSizeV)  // Resize orizzontale e verticale
    refreshContentSize(x, y);
  else if (notUpdateSizeH && !notUpdateSizeV)  // Resize verticale
    refreshContentSize(0, y);
  else if (!notUpdateSizeH && notUpdateSizeV)  // Resize orizzontale
    refreshContentSize(x, 0);

  // Recheck in case refreshContentSize changed the max
  if (!notUpdateSizeH)
    maxValueH = m_cellScrollArea->horizontalScrollBar()->maximum();
  if (!notUpdateSizeV)
    maxValueV = m_cellScrollArea->verticalScrollBar()->maximum();

  if (valueH > maxValueH && x > 0)  // Se il valore e' maggiore del max e x>0
                                    // scrollo al massimo valore orizzontale
    valueH = m_cellScrollArea->horizontalScrollBar()->maximum();

  if (valueV > maxValueV && y > 0)  // Se il valore e' maggiore del max e y>0
                                    // scrollo al massimo valore verticale
    valueV = m_cellScrollArea->verticalScrollBar()->maximum();

  m_cellScrollArea->horizontalScrollBar()->setValue(valueH);
  m_cellScrollArea->verticalScrollBar()->setValue(valueV);
}

//-----------------------------------------------------------------------------
int XsheetViewer::getColumnScrollValue() { return layerScrollBar()->value(); }

//-----------------------------------------------------------------------------
void XsheetViewer::setAutoPanSpeed(const QPoint& speed) {
  bool wasAutoPanning = isAutoPanning();
  m_autoPanSpeed      = speed;
  if (isAutoPanning() && !wasAutoPanning && m_timerId == 0)
    m_timerId = startTimer(40);
  else if (!isAutoPanning() && wasAutoPanning && m_timerId != 0) {
    killTimer(m_timerId);
    m_timerId = 0;
  }
}

static int getAutoPanSpeed(int pixels) {
  int f = 40;
  return std::min(100, (f - 1 + pixels * f) / 100);
}

//-----------------------------------------------------------------------------
void XsheetViewer::setAutoPanSpeed(const QRect& widgetBounds,
                                  const QPoint& mousePos) {
  QPoint speed;
  int limit = 100, factor = 30;
  if (mousePos.x() < widgetBounds.left())
    speed.setX(-getAutoPanSpeed(widgetBounds.left() - mousePos.x()));
  else if (mousePos.x() > widgetBounds.right())
    speed.setX(getAutoPanSpeed(mousePos.x() - widgetBounds.right()));
  if (mousePos.y() < widgetBounds.top())
    speed.setY(-getAutoPanSpeed(widgetBounds.top() - mousePos.y()));
  else if (mousePos.y() > widgetBounds.bottom())
    speed.setY(getAutoPanSpeed(mousePos.y() - widgetBounds.bottom()));
  setAutoPanSpeed(speed);
  m_lastAutoPanPos = mousePos;
}

//-----------------------------------------------------------------------------
void XsheetViewer::timerEvent(QTimerEvent*) {
  if (!isAutoPanning()) return;
  scroll(m_autoPanSpeed);
  if (!m_dragTool) return;
  QMouseEvent mouseEvent(QEvent::MouseMove, m_lastAutoPanPos - m_autoPanSpeed,
                         Qt::NoButton, Qt::MouseButtons(), m_qtModifiers);
  m_dragTool->onDrag(&mouseEvent);
  m_lastAutoPanPos += m_autoPanSpeed;
}

//-----------------------------------------------------------------------------
// adjust sizes after scrolling event
bool XsheetViewer::refreshContentSize(int dx, int dy) {
  const QSize viewport = m_cellScrollArea->viewport()->size();
  const QPoint offset  = m_cellArea->pos() - QPoint(dx, dy);
  const QSize minimum(viewport.width() - std::min(0, offset.x()),
                      viewport.height() - std::min(0, offset.y()));
  const QSize actual = fitContentSize(minimum);
  if (actual == m_cellArea->size()) return false;
  const Orientation *o = orientation();
  const QRect content(QPoint(), actual);
  m_isComputingSize = true;
  m_cellArea->setFixedSize(actual);
  m_rowArea->setFixedSize(
      o->frameLayerRect(o->frameSide(content),
                        o->range(PredefinedRange::HEADER_LAYER))
          .size());
  m_columnArea->setFixedSize(
      o->frameLayerRect(o->range(PredefinedRange::HEADER_FRAME),
                        o->layerSide(content))
          .size());
  m_isComputingSize = false;
  return true;
}

//-----------------------------------------------------------------------------
// call when in doubt
void XsheetViewer::updateAreeSize() { refreshContentSize(0, 0); }

CellPosition XsheetViewer::xyToPosition(const TPoint& point) const {
  return xyToPosition(QPoint(point.x, point.y));
}
CellPosition XsheetViewer::xyToPosition(const TPointD& point) const {
  return xyToPosition(QPoint((int)point.x, (int)point.y));
}

int XsheetViewer::rowToFrameAxis(int frame) const {
  int result = orientation()->rowToFrameAxis(frame);
  result = (result * getFrameZoomFactor()) / 100;
  return result;
}

//-----------------------------------------------------------------------------
CellRange XsheetViewer::xyRectToRange(const QRect& rect) const {
  CellPosition topLeft     = xyToPosition(rect.topLeft());
  CellPosition bottomRight = xyToPosition(rect.bottomRight());
  return CellRange(topLeft, bottomRight);
}

//-----------------------------------------------------------------------------
QRect XsheetViewer::rangeToXYRect(const CellRange& range) const {
  QPoint from = positionToXY(range.from());
  QPoint to   = positionToXY(range.to());
  QPoint topLeft =
      QPoint(std::min(from.x(), to.x()), std::min(from.y(), to.y()));
  QPoint bottomRight =
      QPoint(std::max(from.x(), to.x()), std::max(from.y(), to.y()));
  return QRect(topLeft, bottomRight);
}

//-----------------------------------------------------------------------------
void XsheetViewer::drawPredefinedPath(QPainter& p, PredefinedPath which,
                                     const CellPosition& pos,
                                     optional<QColor> fill,
                                     optional<QColor> outline) const {
  QPoint xy         = positionToXY(pos);
  QPainterPath path = orientation()->path(which).translated(xy);
  if (fill) p.fillPath(path, QBrush(*fill));
  if (outline) {
    p.setPen(*outline);
    p.drawPath(path);
  }
}

//-----------------------------------------------------------------------------
void XsheetViewer::drawPredefinedPath(QPainter& p, PredefinedPath which,
                                     QPoint xy, optional<QColor> fill,
                                     optional<QColor> outline) const {
  QPainterPath path = orientation()->path(which).translated(xy);
  if (fill) p.fillPath(path, QBrush(*fill));
  if (outline) {
    p.setPen(*outline);
    p.drawPath(path);
  }
}

//-----------------------------------------------------------------------------
bool XsheetViewer::areCellsSelectedEmpty() {
  int r0, c0, r1, c1;
  getCellSelection()->getSelectedCells(r0, c0, r1, c1);
  int i, j;
  for (i = r0; i <= r1; i++)
    for (j = c0; j <= c1; j++)
      if (!getXsheet()->getCell(i, j).isEmpty()) return false;
  return true;
}

//-----------------------------------------------------------------------------
bool XsheetViewer::areSoundCellsSelected() {
  int r0, c0, r1, c1;
  getCellSelection()->getSelectedCells(r0, c0, r1, c1);
  if (c0 < 0) return false;
  int i, j;
  for (i = r0; i <= r1; i++)
    for (j = c0; j <= c1; j++) {
      TXshCell cell = getXsheet()->getCell(i, j);
      if (cell.isEmpty() || cell.getSoundLevel()) continue;
      return false;
    }
  return !areCellsSelectedEmpty();
}

//-----------------------------------------------------------------------------
bool XsheetViewer::areSoundTextCellsSelected() {
  int r0, c0, r1, c1;
  getCellSelection()->getSelectedCells(r0, c0, r1, c1);
  if (c0 < 0) return false;
  int i, j;
  for (i = r0; i <= r1; i++)
    for (j = c0; j <= c1; j++) {
      TXshCell cell = getXsheet()->getCell(i, j);
      if (cell.isEmpty() || cell.getSoundTextLevel()) continue;
      return false;
    }
  return !areCellsSelectedEmpty();
}

//-----------------------------------------------------------------------------
bool XsheetViewer::areCameraCellsSelected() {
  int r0, c0, r1, c1;
  getCellSelection()->getSelectedCells(r0, c0, r1, c1);
  return c0 < 0;
}

//-----------------------------------------------------------------------------
void XsheetViewer::setScrubHighlight(int row, int startRow, int col) {
  if (m_scrubCol == -1) m_scrubCol = col;
  m_scrubRow0 = std::min(row, startRow);
  m_scrubRow1 = std::max(row, startRow);
  return;
}

//-----------------------------------------------------------------------------
void XsheetViewer::resetScrubHighlight() {
  m_scrubCol = m_scrubRow0 = m_scrubRow1 = -1;
  return;
}

//-----------------------------------------------------------------------------
void XsheetViewer::getScrubHeighlight(int& R0, int& R1) {
  R0 = m_scrubRow0;
  R1 = m_scrubRow1;
  return;
}

//-----------------------------------------------------------------------------
bool XsheetViewer::isScrubHighlighted(int row, int col) {
  if (m_scrubCol == col && m_scrubRow0 <= row && row <= m_scrubRow1)
    return true;
  return false;
}

//-----------------------------------------------------------------------------
void XsheetViewer::showEvent(QShowEvent*) {
  disconnectVisibleSignals();
  if (m_isCurrentFrameSwitched) onCurrentFrameSwitched();
  if (m_isCurrentColumnSwitched) onCurrentColumnSwitched();
  m_isCurrentFrameSwitched = false;

  TApp *app = TApp::instance();

  bool ret =
      connectVisible(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()),
                     SLOT(onCurrentColumnSwitched()));
  ret = ret && connectVisible(app->getCurrentFrame(), SIGNAL(frameSwitched()),
                              SLOT(onCurrentFrameSwitched()));
  ret = ret &&
        connectVisible(app->getCurrentFrame(), SIGNAL(isPlayingStatusChanged()),
                       SLOT(onPlayingStatusChanged()));
  ret = ret && connectVisible(app->getCurrentFrame(), SIGNAL(scrubStopped()),
                              SLOT(onScrubStopped()));

  ret = ret &&
        connectVisible(app->getCurrentObject(), SIGNAL(objectChanged(bool)),
                       SLOT(updateAllAree(bool)));

  TSceneHandle *sceneHandle = app->getCurrentScene();
  ret = ret && connectVisible(sceneHandle, SIGNAL(sceneSwitched()),
                              SLOT(onSceneSwitched()));
  ret = ret && connectVisible(sceneHandle, SIGNAL(nameSceneChanged()),
                              SLOT(changeWindowTitle()));
  ret = ret &&
        connectVisible(sceneHandle, SIGNAL(preferenceChanged(const QString&)),
                       SLOT(onPreferenceChanged(const QString&)));

  TXsheetHandle *xsheetHandle = app->getCurrentXsheet();
  ret = ret && connectVisible(xsheetHandle, SIGNAL(xsheetSwitched()),
                              SLOT(onXsheetChanged()));
  ret = ret && connectVisible(xsheetHandle, SIGNAL(xsheetSwitched()),
                              SLOT(resetXsheetNotes()));
  ret = ret && connectVisible(xsheetHandle, SIGNAL(navigationTagsChanged()),
                              SLOT(updateCellRowAree()));
  ret = ret && connectVisible(xsheetHandle, SIGNAL(xsheetChanged()),
                              SLOT(onXsheetChanged()));
  ret = ret && connectVisible(xsheetHandle, SIGNAL(xsheetChanged()),
                              SLOT(changeWindowTitle()));

  ret = ret &&
        connectVisible(app->getCurrentSelection(),
                       SIGNAL(selectionSwitched(TSelection*, TSelection*)),
                       SLOT(onSelectionSwitched(TSelection*, TSelection*)));
  // update titlebar when the cell selection region is changed
  ret = ret && connectVisible(app->getCurrentSelection(),
                              SIGNAL(selectionChanged(TSelection*)),
                              SLOT(onSelectionChanged(TSelection*)));
  // show the current level name to title bar
  ret = ret && connectVisible(app->getCurrentLevel(),
                              SIGNAL(xshLevelSwitched(TXshLevel*)),
                              SLOT(changeWindowTitle()));

  ret =
      ret && connectVisible(IconGenerator::instance(), SIGNAL(iconGenerated()),
                            SLOT(updateColumnArea()));

  assert(ret);
  positionSections();
  refreshContentSize(0, 0);
  restoreScrollPosition();
  m_columnArea->prepareThumbnails();
  changeWindowTitle();
}

void XsheetViewer::restoreScrollPosition() {
  if (!m_restoreScrollPosition) return;
  const QPoint current(m_cellScrollArea->horizontalScrollBar()->value(),
                       m_cellScrollArea->verticalScrollBar()->value());
  scroll(m_pendingScrollPosition - current);
  m_restoreScrollPosition = false;
}

//-----------------------------------------------------------------------------
void XsheetViewer::hideEvent(QHideEvent*) {
  disconnectVisibleSignals();
  m_cellArea->onControlPressed(false);
  m_columnArea->onControlPressed(false);
  m_layerFooterPanel->onControlPressed(false);
  if (TApp::instance()->getCurrentSheetViewer() == this)
    TApp::instance()->setCurrentSheetViewer(nullptr);
}

void XsheetViewer::changeEvent(QEvent* event) {
  QFrame::changeEvent(event);
  if (event->type() == QEvent::StyleChange ||
      event->type() == QEvent::FontChange ||
      event->type() == QEvent::PaletteChange) {
    m_headerResources.clear();
    if (m_initialized) updateAllAree();
  }
}

//-----------------------------------------------------------------------------
void XsheetViewer::resizeEvent(QResizeEvent* event) {
  positionSections();

  //(New Layout Manager) introduced automatic refresh
  refreshContentSize(
      0,
      0);  // Don't updateAreeSize because you have to account scrollbars
  updateAllAree();
}

//-----------------------------------------------------------------------------
void XsheetViewer::wheelEvent(QWheelEvent* event) {
  switch (event->source()) {
  case Qt::MouseEventNotSynthesized: {
    if (0 != (event->modifiers() & Qt::ControlModifier) &&
        event->angleDelta().y() != 0) {
      const QPoint pos = m_cellArea->mapFrom(this, event->position().toPoint());
      const int targetFrame = xyToPosition(pos).frame();

      int newFactor =
          m_frameZoomFactor + ((event->angleDelta().y() > 0 ? 1 : -1) * 10);
      if (newFactor > XsheetGUI::ZOOM_FACTOR_MAX)
        newFactor = XsheetGUI::ZOOM_FACTOR_MAX;
      else if (newFactor < XsheetGUI::ZOOM_FACTOR_MIN)
        newFactor = XsheetGUI::ZOOM_FACTOR_MIN;
      zoomOnFrame(targetFrame, newFactor);

      event->accept();
      return;
    }

    int markerDistance = 0, markerOffset = 0, secDistance;
    TApp::instance()
        ->getCurrentScene()
        ->getScene()
        ->getProperties()
        ->getMarkers(markerDistance, markerOffset, secDistance);

    if (event->angleDelta().x() == 0) {  // vertical scroll
      if (!orientation()->frameAxis(QPoint(0, 1))) markerDistance = 1;
      int scrollPixels = (event->angleDelta().y() > 0 ? 1 : -1) *
                         markerDistance * orientation()->cellHeight();
      scroll(QPoint(0, -scrollPixels));
    } else {  // horizontal scroll
      if (!orientation()->frameAxis(QPoint(1, 0))) markerDistance = 1;
      int scrollPixels = (event->angleDelta().x() > 0 ? 1 : -1) *
                         markerDistance * orientation()->cellWidth();
      scroll(QPoint(-scrollPixels, 0));
    }
    break;
  }

  case Qt::MouseEventSynthesizedBySystem:  // macbook touch-pad
  {
    QPoint numPixels  = event->pixelDelta();
    QPoint numDegrees = event->angleDelta() / 8;
    if (!numPixels.isNull()) {
      scroll(-numPixels);
    } else if (!numDegrees.isNull()) {
      QPoint numSteps = numDegrees / 15;
      scroll(-numSteps);
    }
    break;
  }

  default:  // Qt::MouseEventSynthesizedByQt,
            // Qt::MouseEventSynthesizedByApplication
  {
    std::cout << "not supported event: Qt::MouseEventSynthesizedByQt, "
                 "Qt::MouseEventSynthesizedByApplication"
              << std::endl;
    break;
  }

  }  // end switch
}

//-----------------------------------------------------------------------------
void XsheetViewer::keyPressEvent(QKeyEvent* event) {
  struct Locals {
    XsheetViewer* m_this;

    void scrollVertTo(double y, const QRect &visibleRect) {
      int deltaY = (y < visibleRect.top()) ? y - visibleRect.top()
                                           : y - visibleRect.bottom();

      m_this->scroll(QPoint(0, deltaY));
    }

    void scrollHorizTo(double x, const QRect &visibleRect) {
      int deltaX = (x < visibleRect.left()) ? x - visibleRect.left()
                                            : x - visibleRect.right();

      m_this->scroll(QPoint(deltaX, 0));
    }
  } locals = {this};

  if (changeFrameSkippingHolds(event)) return;

  int frameCount = getXsheet()->getFrameCount();
  CellPosition now(getCurrentRow(), getCurrentColumn());
  CellPosition shift = orientation()->arrowShift(event->key());
  CellPosition stride(1, 1);  // stride in row and column axes

  TCellSelection *cellSel =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());
  int firstCol =
      Preferences::instance()->isXsheetCameraColumnVisible() ? -1 : 0;
  // Use arrow keys to shift the cell selection. Ctrl + arrow keys to resize the
  // selection range.
  if (Preferences::instance()->isUseArrowKeyToShiftCellSelectionEnabled() &&
      cellSel && !cellSel->isEmpty()) {
    int rowA, colA, rowB, colB;
    cellSel->getSelectedCells(rowA, colA, rowB, colB);
    stride.setFrame(cellSel->getSelectedCells().getRowCount());

    if (m_cellArea->isControlPressed()) {  // resize

      // resize selection of frames/rows forward or backwards
      assert(cellSel->getResizePivotRow() >= 0);
      if (rowA < cellSel->getResizePivotRow())
        rowA += shift.frame();
      else
        rowB += shift.frame();

      // resize selection of layers/columns "up" or "down"
      if (colA < getCurrentColumn())
        colA += shift.layer();
      else
        colB += shift.layer();

      // keep selection inside the xsheet/timeline
      if (colA < firstCol || colB < firstCol || rowA < 0 || rowB < 0) return;

      // apply new selection rectangle
      cellSel->selectCells(rowA, colA, rowB, colB);
      updateCells();
      TApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return;

    } else {  // shift
      CellPosition offset(shift * stride);
      int movedRow0 = std::max(0, rowA + offset.frame());
      int movedCol0 = std::max(firstCol, colA + offset.layer());
      int diffRow   = movedRow0 - rowA;
      int diffCol   = movedCol0 - colA;
      cellSel->selectCells(rowA + diffRow, colA + diffCol, rowB + diffRow,
                           colB + diffCol);
      TApp::instance()->getCurrentSelection()->notifySelectionChanged();
    }
  }

  if (shift) {
    now = now + shift * stride;
    now.ensureValid();
    if (now.layer() < firstCol) now.setLayer(firstCol);
    setCurrentRow(now.frame());
    setCurrentColumn(now.layer());
    return;
  }

  switch (int key = event->key()) {
  case Qt::Key_Control:
    // display the upper-directional smart tab only when the ctrl key is pressed
    m_cellArea->onControlPressed(true);
    m_columnArea->onControlPressed(true);
    m_layerFooterPanel->onControlPressed(true);
    break;

  default: {
    QRect visibleRect   = m_cellArea->visibleRegion().boundingRect();
    int visibleRowCount = visibleRect.height() / orientation()->cellHeight();

    switch (key) {
    case Qt::Key_PageUp:
      locals.scrollVertTo(
          visibleRect.top() - visibleRowCount * orientation()->cellHeight(),
          visibleRect);
      break;
    case Qt::Key_PageDown:
      locals.scrollVertTo(
          visibleRect.bottom() + visibleRowCount * orientation()->cellHeight(),
          visibleRect);
      break;
    case Qt::Key_Home:
      scrollToAxisRange(0, 0, orientation()->frameLayerToXY(1, 0));
      break;
    case Qt::Key_End: {
      const int end = rowToFrameAxis(frameCount + 1);
      scrollToAxisRange(end, end, orientation()->frameLayerToXY(1, 0));
      break;
    }
    default:
      if (Preferences::instance()->isInputCellsUsingNumpad() &&
          key >= Qt::Key_0 && key <= Qt::Key_9) {
        int number = key - Qt::Key_0;
        m_cellArea->onNumberPressed(number);
      }
      break;
    }
    break;
  }
  }
}

//-----------------------------------------------------------------------------
// display the upper-directional smart tab only when the ctrl key is pressed
void XsheetViewer::keyReleaseEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Control) {
    m_cellArea->onControlPressed(false);
    m_columnArea->onControlPressed(false);
    m_layerFooterPanel->onControlPressed(false);
  }
}

void XsheetViewer::enterEvent(QEvent*) {
  m_cellArea->onControlPressed(false);
  m_columnArea->onControlPressed(false);
  TApp *app = TApp::instance();
  app->setCurrentSheetViewer(this);
}

//-----------------------------------------------------------------------------
/*! scroll the cell area to make a cell at (row,col) visible
 */
void XsheetViewer::scrollTo(int row, int col) {
  QRect visibleRect = m_cellArea->visibleRegion().boundingRect();
  QPoint topLeft    = positionToXY(CellPosition(row, col));
  QRect cellRect(
      topLeft, QSize(orientation()->cellWidth(), orientation()->cellHeight()));

  int deltaX = 0;
  int deltaY = 0;

  if (cellRect.left() < visibleRect.left())
    deltaX = cellRect.left() - visibleRect.left();
  else if (cellRect.right() > visibleRect.right())
    deltaX = cellRect.left() - visibleRect.left();

  if (cellRect.top() < visibleRect.top())
    deltaY = cellRect.top() - visibleRect.top();
  else if (cellRect.bottom() > visibleRect.bottom())
    deltaY = cellRect.bottom() - visibleRect.bottom();

  if (deltaX != 0 || deltaY != 0) {
    scroll(QPoint(deltaX, deltaY));
  }
}

//-----------------------------------------------------------------------------
void XsheetViewer::onSceneSwitched() {
  m_engine.invalidate();
  refreshContentSize(0, 0);
  updateAreeSize();
  updateAllAree();
  clearNoteWidgets();
  m_noteArea->updateButtons();
}

//-----------------------------------------------------------------------------
void XsheetViewer::onXsheetChanged() {
  m_engine.invalidate();
  refreshContentSize(0, 0);
  updateAllAree();

  int row                 = TApp::instance()->getCurrentFrame()->getFrame();
  TXsheet *xsh            = getXsheet();
  NavigationTags *navTags = xsh->getNavigationTags();
  int lastTag             = navTags->getPrevTag(INT_MAX);
  int firstTag            = navTags->getNextTag(-1);
  CommandManager::instance()->enable(MI_NextTaggedFrame, (row < lastTag));
  CommandManager::instance()->enable(MI_PrevTaggedFrame,
                                     firstTag != -1 && row > firstTag);
  CommandManager::instance()->enable(MI_EditTaggedFrame,
                                     navTags->isTagged(row));
  CommandManager::instance()->enable(MI_ClearTags, (navTags->getCount() > 0));
}

//-----------------------------------------------------------------------------
void XsheetViewer::onPreferenceChanged(const QString& prefName) {
  if (prefName == "XSheetToolbar" || prefName == "XsheetBreadcrumbs") {
    positionSections();
    refreshContentSize(0, 0);
  } else if (prefName == "XsheetCamera") {
    refreshContentSize(0, 0);
  }
  m_headerResources.clear();
  updateAllAree();
}

//-----------------------------------------------------------------------------
void XsheetViewer::onCurrentFrameSwitched() {
  int row           = TApp::instance()->getCurrentFrame()->getFrame();
  QRect visibleRect = m_cellArea->visibleRegion().boundingRect();
  if (visibleRect.isEmpty()) {
    m_isCurrentFrameSwitched = true;
    return;
  }
  m_isCurrentFrameSwitched = false;
  scrollToRow(row);

  TXsheet *xsh            = getXsheet();
  NavigationTags *navTags = xsh->getNavigationTags();
  int lastTag             = navTags->getPrevTag(INT_MAX);
  int firstTag            = navTags->getNextTag(-1);
  CommandManager::instance()->enable(MI_NextTaggedFrame, (row < lastTag));
  CommandManager::instance()->enable(MI_PrevTaggedFrame,
                                     firstTag != -1 && row > firstTag);
  CommandManager::instance()->enable(MI_EditTaggedFrame,
                                     navTags->isTagged(row));
}

//-----------------------------------------------------------------------------
void XsheetViewer::onPlayingStatusChanged() {
  if (!Preferences::instance()->isXsheetAutopanEnabled())
    onCurrentFrameSwitched();
}

//-----------------------------------------------------------------------------
void XsheetViewer::onCurrentColumnSwitched() {
  int col           = TApp::instance()->getCurrentColumn()->getColumnIndex();
  QRect visibleRect = m_columnArea->visibleRegion().boundingRect();
  if (visibleRect.isEmpty()) {
    m_isCurrentColumnSwitched = true;
    return;
  }
  m_isCurrentColumnSwitched = false;
  scrollToColumn(col);
}

//-----------------------------------------------------------------------------
void XsheetViewer::scrollToColumn(int column) {
  const int start = columnToLayerAxis(column);
  const int size  = !columnFan()->isActive(column)
                        ? orientation()->foldedCellSize()
                        : orientation()->dimension(
                              column < 0 ? PredefinedDimension::CAMERA_LAYER
                                         : PredefinedDimension::LAYER);
  scrollToAxisRange(start, start + size, orientation()->frameLayerToXY(0, 1));
  updateCellColumnAree();
}

//-----------------------------------------------------------------------------
void XsheetViewer::scrollToHorizontalRange(int first, int last) {
  scrollToAxisRange(first, last, QPoint(1, 0));
}

//-----------------------------------------------------------------------------
void XsheetViewer::scrollToRow(int row) {
  scrollToAxisRange(rowToFrameAxis(row), rowToFrameAxis(row + 1),
                    orientation()->frameLayerToXY(1, 0));
  updateCellRowAree();
}

//-----------------------------------------------------------------------------
void XsheetViewer::scrollToVerticalRange(int first, int last) {
  scrollToAxisRange(first, last, QPoint(0, 1));
}

void XsheetViewer::scrollToAxisRange(int first, int last,
                                    const QPoint& direction) {
  const QRect visible = m_cellArea->visibleRegion().boundingRect();
  if (visible.isEmpty()) return;
  if (TApp::instance()->getCurrentFrame()->isPlaying() &&
      !Preferences::instance()->isXsheetAutopanEnabled())
    return;
  const int begin = QPoint::dotProduct(visible.topLeft(), direction);
  const int end   = QPoint::dotProduct(visible.bottomRight(), direction);
  const int low = std::min(first, last), high = std::max(first, last);
  const int delta = low < begin ? low - begin : high > end ? high + 2 - end : 0;
  if (delta) scroll(direction * delta);
}

//-----------------------------------------------------------------------------
void XsheetViewer::onSelectionSwitched(TSelection* oldSelection,
                                      TSelection* newSelection) {
  if ((TSelection *)getCellSelection() == oldSelection) {
    m_cellArea->update(m_cellArea->visibleRegion());
    m_rowArea->update(m_rowArea->visibleRegion());
    changeWindowTitle();
  } else if ((TSelection *)m_columnSelection == oldSelection)
    m_columnArea->update(m_columnArea->visibleRegion());
}

//-----------------------------------------------------------------------------
/*! update display of the cell selection range in title bar
 */
void XsheetViewer::onSelectionChanged(TSelection* selection) {
  if ((TSelection *)getCellSelection() == selection) {
    changeWindowTitle();
    if (Preferences::instance()->isInputCellsWithoutDoubleClickingEnabled()) {
      TCellSelection *cellSel = getCellSelection();
      if (cellSel->isEmpty())
        m_cellArea->hideRenameField();
      else
        m_cellArea->showRenameField(
            cellSel->getSelectedCells().m_r0, cellSel->getSelectedCells().m_c0,
            cellSel->getSelectedCells().getColCount() > 1);
    }
  }
}

//-----------------------------------------------------------------------------
void XsheetViewer::updateAllAree(bool isDragging) {
  m_columnArea->prepareThumbnails();
  m_cellArea->update(m_cellArea->visibleRegion());
  if (!isDragging) {
    m_rowArea->update(m_rowArea->visibleRegion());
    m_columnArea->update(m_columnArea->visibleRegion());
  }
  m_toolbar->update(m_toolbar->visibleRegion());
}

//-----------------------------------------------------------------------------
void XsheetViewer::updateColumnArea() {
  m_columnArea->prepareThumbnails();
  m_columnArea->update(m_columnArea->visibleRegion());
}

//-----------------------------------------------------------------------------
void XsheetViewer::updateCellColumnAree() {
  m_columnArea->prepareThumbnails();
  m_columnArea->update(m_columnArea->visibleRegion());
  m_cellArea->update(m_cellArea->visibleRegion());
}

//-----------------------------------------------------------------------------
void XsheetViewer::updateCellRowAree() {
  m_rowArea->update(m_rowArea->visibleRegion());
  m_cellArea->update(m_cellArea->visibleRegion());
}

//-----------------------------------------------------------------------------
void XsheetViewer::onScrubStopped() {
  resetScrubHighlight();
  updateCells();
}

//-----------------------------------------------------------------------------
void XsheetViewer::discardNoteWidget() {
  if (m_currentNoteIndex == -1) return;
  TXshNoteSet *notes = getXsheet()->getNotes();
  int i;
  for (i = m_currentNoteIndex + 1; i < m_noteWidgets.size(); i++) {
    XsheetGUI::NoteWidget *w = m_noteWidgets.at(i);
    int index                = w->getNoteIndex();
    w->setNoteIndex(index - 1);
    w->update();
  }
  delete m_noteWidgets.at(m_currentNoteIndex);
  m_noteWidgets.removeAt(m_currentNoteIndex);
  m_noteArea->updateButtons();
  updateCells();
}

//-----------------------------------------------------------------------------
QList<XsheetGUI::NoteWidget*> XsheetViewer::getNotesWidget() const {
  return m_noteWidgets;
}

//-----------------------------------------------------------------------------
void XsheetViewer::addNoteWidget(XsheetGUI::NoteWidget* w) {
  m_noteWidgets.push_back(w);
  m_noteArea->updateButtons();
}

//-----------------------------------------------------------------------------
int XsheetViewer::getCurrentNoteIndex() const { return m_currentNoteIndex; }

//-----------------------------------------------------------------------------
void XsheetViewer::setCurrentNoteIndex(int currentNoteIndex) {
  m_currentNoteIndex = currentNoteIndex;
  m_noteArea->updateButtons();

  if (currentNoteIndex < 0) return;

  TXshNoteSet *notes = getXsheet()->getNotes();
  int row            = notes->getNoteRow(currentNoteIndex);
  int col            = notes->getNoteCol(currentNoteIndex);
  TPointD pos        = notes->getNotePos(currentNoteIndex);

  QPoint topLeft = positionToXY(CellPosition(row, col)) +
                   QPoint(pos.x, pos.y);  // actually xy
  QSize size(XsheetGUI::NoteWidth, XsheetGUI::NoteHeight);
  QRect noteRect(topLeft, size);
  scrollToHorizontalRange(noteRect.left(), noteRect.right());
  scrollToVerticalRange(noteRect.top(), noteRect.bottom());
}

//-----------------------------------------------------------------------------
void XsheetViewer::resetXsheetNotes() { m_noteArea->updateButtons(); }

//-----------------------------------------------------------------------------
void XsheetViewer::updateNoteWidgets() {
  int i;
  for (i = 0; i < m_noteWidgets.size(); i++) m_noteWidgets.at(i)->update();
  m_noteArea->updatePopup();
  updateCells();
}

//-----------------------------------------------------------------------------
void XsheetViewer::clearNoteWidgets() {
  int i;
  for (i = 0; i < m_noteWidgets.size(); i++) delete m_noteWidgets.at(i);
  m_noteWidgets.clear();
}

//-----------------------------------------------------------------------------
void XsheetViewer::changeWindowTitle() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene || !app->getCurrentFrame()->isEditingScene()) return;
  auto project      = scene->getProject();
  QString sceneName = QString::fromStdWString(scene->getSceneName());
  if (sceneName.isEmpty()) sceneName = XsheetViewer::tr("Untitled");
  if (app->getCurrentScene()->getDirtyFlag()) sceneName += QString("*");
  QString name   = XsheetViewer::tr("Scene: ") + sceneName;
  int frameCount = scene->getFrameCount();
  name = name +
         "   ::   " + XsheetViewer::tr(std::to_string(frameCount).c_str()) +
         (frameCount == 1 ? XsheetViewer::tr(" Frame")
                          : XsheetViewer::tr(" Frames"));

  // subXsheet or not
  ChildStack *childStack = scene->getChildStack();
  if (childStack && childStack->getAncestorCount() > 0) {
    name += XsheetViewer::tr("  (Sub)");
  }
  // current level name
  TXshLevel *level = app->getCurrentLevel()->getLevel();
  if (level) {
    QString levelName = QString::fromStdWString(level->getName());
    name += XsheetViewer::tr("  Level: ") + levelName;
  }
  // cell selection range
  if ((TSelection *)getCellSelection() ==
          app->getCurrentSelection()->getSelection() &&
      !getCellSelection()->isEmpty()) {
    int r0, r1, c0, c1;
    getCellSelection()->getSelectedCells(r0, c0, r1, c1);
    name += XsheetViewer::tr("   Selected: ") + QString::number(r1 - r0 + 1) +
            ((r1 - r0 + 1 == 1) ? XsheetViewer::tr(" frame : ")
                                : XsheetViewer::tr(" frames * ")) +
            QString::number(c1 - c0 + 1) +
            ((c1 - c0 + 1 == 1) ? XsheetViewer::tr(" column")
                                : XsheetViewer::tr(" columns"));
  }

  parentWidget()->setWindowTitle(name);
}

//-----------------------------------------------------------------------------
/*! convert the last one digit of the frame number to alphabet
        Ex.  12 -> 1B    21 -> 2A   30 -> 3
 */
QString XsheetViewer::getFrameNumberWithLetters(int frame) {
  int letterNum = frame % 10;
  QChar letter;

  switch (letterNum) {
  case 0:
    letter = QChar();
    break;
  case 1:
    letter = 'A';
    break;
  case 2:
    letter = 'B';
    break;
  case 3:
    letter = 'C';
    break;
  case 4:
    letter = 'D';
    break;
  case 5:
    letter = 'E';
    break;
  case 6:
    letter = 'F';
    break;
  case 7:
    letter = 'G';
    break;
  case 8:
    letter = 'H';
    break;
  case 9:
    letter = 'I';
    break;
  default:
    letter = QChar();
    break;
  }

  QString number;
  if (frame >= 10) {
    number = QString::number(frame);
    number.chop(1);
  } else
    number = "0";

  return (QChar(letter).isNull()) ? number : number.append(letter);
}
//-----------------------------------------------------------------------------

void XsheetViewer::setFrameDisplayStyle(FrameDisplayStyle style) {
  m_frameDisplayStyle              = style;
  if (m_initialized) m_noteArea->setFrameDisplayStyle(int(style));
}

//-----------------------------------------------------------------------------
void XsheetViewer::save(QSettings& settings) const {
  settings.remove("foldedColumnsBySheet");
  settings.remove("orientation");
  settings.setValue("frameZoomFactor", m_frameZoomFactor);
  settings.setValue("frameDisplayStyle", int(m_frameDisplayStyle));
  settings.setValue("scrollPosition",
                    QPoint(m_cellScrollArea->horizontalScrollBar()->value(),
                           m_cellScrollArea->verticalScrollBar()->value()));
}

void XsheetViewer::load(QSettings& settings) {
  m_pendingScrollPosition =
      settings.value("scrollPosition", QPoint()).toPoint();
  m_pendingScrollPosition.setX(std::max(0, m_pendingScrollPosition.x()));
  m_pendingScrollPosition.setY(std::max(0, m_pendingScrollPosition.y()));
  m_restoreScrollPosition = true;
  const int displayStyle =
      settings.value("frameDisplayStyle", int(m_frameDisplayStyle)).toInt();
  setFrameDisplayStyle(to_enum(qBound(0, displayStyle, 3)));
  m_viewStates.clear();
  bool valid        = false;
  const int zoom    = settings.value("frameZoomFactor", 100).toInt(&valid);
  m_frameZoomFactor = valid ? qBound(20, zoom, 100) : 100;
  m_layerFooterPanel->setZoomSliderValue(m_frameZoomFactor);
  // Legacy orientation values cannot change a fixed panel's identity.
  refreshContentSize(0, 0);
  if (isVisible()) restoreScrollPosition();
  updateAllAree();
}

//----------------------------------------------------------------
QList<int> XsheetViewer::availableFramesPerPage() {
  int frameRate = TApp::instance()
                      ->getCurrentScene()
                      ->getScene()
                      ->getProperties()
                      ->getOutputProperties()
                      ->getFrameRate();
  // 1sec, 1.5sec, 2sec, 3sec, 4sec, 6sec
  QList<int> ret;
  ret << frameRate;
  if (frameRate % 2 == 0) ret << frameRate * 3 / 2;
  ret << frameRate * 2;
  ret << frameRate * 3;
  ret << frameRate * 4;
  ret << frameRate * 6;

  // visible area size
  const int size =
      orientation()->frameSide(m_cellScrollArea->viewport()->rect()).length() +
      1;
  const int scaleMin = frameZoomFactor(20);
  int scaleMax = 100;

  int frameMin = (int)std::ceil(
      (double)size /
      ((double)orientation()->dimension(PredefinedDimension::FRAME) *
       (double)scaleMax / 100.0));
  int frameMax = (int)std::floor(
      (double)size /
      ((double)orientation()->dimension(PredefinedDimension::FRAME) *
       (double)scaleMin / 100.0));

  for (auto itr = ret.begin(); itr != ret.end();) {
    // erase unavailable items
    if (*itr < frameMin || *itr > frameMax) {
      itr = ret.erase(itr);
      continue;
    }
    itr++;
  }
  return ret;
}

//----------------------------------------------------------------
void XsheetViewer::zoomToFramesPerPage(int frames) {
  if (frames <= 0) return;
  const int size =
      orientation()->frameSide(m_cellScrollArea->viewport()->rect()).length() +
      1;
  const double scale =
      double(size) /
      (orientation()->dimension(PredefinedDimension::FRAME) * double(frames));
  zoomOnFrame(getCurrentRow(), sliderValueForScale(scale));
}

//----------------------------------------------------------------
int XsheetViewer::getFrameZoomFactor() const {
  return frameZoomFactor(m_frameZoomFactor);
}

int XsheetViewer::frameSpacingPercent() const {
  return orientation()->dimension(PredefinedDimension::FRAME) *
         getFrameZoomFactor() / XsheetGUI::RowHeight;
}

QPoint XsheetViewer::getFrameZoomAdjustment() {
  const int size = orientation()->dimension(PredefinedDimension::FRAME);
  return orientation()->frameLayerToXY(
      std::max(0, size - size * getFrameZoomFactor() / 100 - 1), 0);
}

void XsheetViewer::zoomOnFrame(int frame, int factor) {
  QPoint xyOrig = positionToXY(CellPosition(frame, -1));

  m_frameZoomFactor = qBound(20, factor, 100);
  m_layerFooterPanel->setZoomSliderValue(m_frameZoomFactor);

  QPoint xyNew = positionToXY(CellPosition(frame, -1));

  scroll(xyNew - xyOrig);

  refreshContentSize(0, 0);
  updateAllAree();
  emit frameSpacingChanged(frameSpacingPercent());
}

QColor XsheetViewer::getSelectedColumnTextColor() const {
  // get colors
  TPixel currentColumnPixel;
  Preferences::instance()->getCurrentColumnData(currentColumnPixel);
  QColor currentColumnColor((int)currentColumnPixel.r,
                            (int)currentColumnPixel.g,
                            (int)currentColumnPixel.b, 255);
  return currentColumnColor;
}

//=============================================================================
// XSheetViewerCommand
//-----------------------------------------------------------------------------

OpenFloatingPanel openXsheetViewerCommand(MI_OpenXshView, "Xsheet",
                                          QObject::tr("Xsheet"));

OpenFloatingPanel openTimelineViewerCommand(MI_OpenTimelineView, "Timeline",
                                            QObject::tr("Timeline"));
