
#include "xshcolumnviewer.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "columnheaderpainter.h"
#include "tapp.h"
#include "menubarcommandids.h"
#include "columnselection.h"
#include "xsheetdragtool.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/intfield.h"
#include "toonzqt/fxiconmanager.h"

// TnzLib includes
#include "toonz/txshcolumn.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/stage2.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/txsheet.h"
#include "toonz/toonzscene.h"
#include "toonz/txshcell.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/columnfan.h"
#include "toonz/tstageobjectcmd.h"
#include "toonz/fxcommand.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelproperties.h"
#include "toonz/preferences.h"
#include "toonz/childstack.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshmeshcolumn.h"
#include "toonz/tfxhandle.h"
#include "toonz/tcamera.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/levelset.h"

// TnzCore includes
#include "tconvert.h"

#include <QApplication>
#include <QMainWindow>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QToolTip>
#include <QTimer>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDesktopWidget>


//=============================================================================

namespace {
const QSet<TXshLevel *> getLevels(TXshColumn *column) {
  QSet<TXshLevel *> levels;

  TXshCellColumn *cellColumn = column->getCellColumn();
  if (cellColumn) {
    int i, r0, r1;
    cellColumn->getRange(r0, r1);
    for (i = r0; i <= r1; i++) {
      TXshCell cell = cellColumn->getCell(i);
      // TXshSimpleLevel *sl = cell.getSimpleLevel();
      if (!cell.isEmpty()) levels.insert(cell.m_level.getPointer());
    }
  }
  return levels;
}

bool containsRasterLevel(TColumnSelection *selection) {
  if (!selection || selection->isEmpty()) return false;
  std::set<int> indexes = selection->getIndices();
  TXsheet *xsh          = TApp::instance()->getCurrentXsheet()->getXsheet();
  for (auto const &e : indexes) {
    TXshColumn *col = xsh->getColumn(e);
    if (!col || col->getColumnType() != TXshColumn::eLevelType) continue;

    TXshCellColumn *cellCol = col->getCellColumn();
    if (!cellCol) continue;

    int i;
    for (i = 0; i < cellCol->getMaxFrame() + 1; i++) {
      TXshCell cell = cellCol->getCell(i);
      if (cell.isEmpty()) continue;
      TXshSimpleLevel *level = cell.getSimpleLevel();
      if (!level || level->getChildLevel() ||
          level->getProperties()->getDirtyFlag())
        continue;
      int type = level->getType();
      if (type == OVL_XSHLEVEL || type == TZP_XSHLEVEL) return true;
    }
  }
  return false;
}

bool containsVectorLevel(int col) {
  TXshColumn *column =
      TApp::instance()->getCurrentXsheet()->getXsheet()->getColumn(col);
  TXshColumn::ColumnType type = column->getColumnType();
  if (type != TXshColumn::eLevelType) return false;

  const QSet<TXshLevel *> levels = getLevels(column);
  QSet<TXshLevel *>::const_iterator it2;
  for (it2 = levels.begin(); it2 != levels.end(); it2++) {
    TXshLevel *lvl = *it2;
    int type       = lvl->getType();
    if (type == PLI_XSHLEVEL) {
      return true;
    }
  }
  return false;
}

QIcon createLockIcon(XsheetViewer* viewer) {
  QColor bgColor_on, bgColor_off;

  QString svgFilePath_on, svgFilePath_off;
  viewer->getButton(LOCK_ON_XSHBUTTON, bgColor_on, svgFilePath_on);
  viewer->getButton(LOCK_OFF_XSHBUTTON, bgColor_off, svgFilePath_off);

  QPixmap pm_on  = svgToPixmap(svgFilePath_on, QSize(16, 16),
                               Qt::KeepAspectRatio, bgColor_on);
  QPixmap pm_off = svgToPixmap(svgFilePath_off, QSize(16, 16),
                               Qt::KeepAspectRatio, bgColor_off);

  QIcon lockIcon;
  lockIcon.addPixmap(pm_off);
  lockIcon.addPixmap(pm_on, QIcon::Normal, QIcon::On);

  return lockIcon;
}

}  // namespace

//=============================================================================
// ColumnMaskUndo
//-----------------------------------------------------------------------------
class ColumnMaskUndo final : public TUndo {
  int m_col;
  bool m_isMask;
  std::string m_name;

public:
  ColumnMaskUndo(int column, bool isMask, std::string name)
      : m_col(column), m_isMask(isMask), m_name(name) {}
  ~ColumnMaskUndo() {}

  void undo() const override {
    TXshColumn *column =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getColumn(m_col);
    TXshColumn::ColumnType type = column->getColumnType();
    if (type != TXshColumn::eLevelType) return;

    if (containsVectorLevel(m_col)) {
      column->setIsMask(m_isMask);
      TApp::instance()->getCurrentScene()->notifySceneChanged();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    }
  }

  void redo() const override {
    TXshColumn *column =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getColumn(m_col);
    TXshColumn::ColumnType type = column->getColumnType();
    if (type != TXshColumn::eLevelType) return;

    if (containsVectorLevel(m_col)) {
      column->setIsMask(!m_isMask);
      TApp::instance()->getCurrentScene()->notifySceneChanged();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    }
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Toggle vector column as mask. ");
    return str;
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

namespace XsheetGUI {

static void getVolumeCursorRect(QRect &out, double volume,
                                const QPoint &origin) {
  int ly = 60;
  int v  = tcrop(0, ly, (int)(volume * ly));
  out.setX(origin.x() + 11);
  out.setY(origin.y() + 60 - v);
  out.setWidth(8);
  out.setHeight(8);
}

//=============================================================================
// MotionPathMenu
//-----------------------------------------------------------------------------

ChangeObjectWidget::ChangeObjectWidget(QWidget *parent)
    : QListWidget(parent), m_width(40), m_objectHandle(0), m_xsheetHandle(0) {
  setMouseTracking(true);
  setObjectName("XshColumnChangeObjectWidget");
  setAutoFillBackground(true);

  bool ret = connect(this, SIGNAL(itemClicked(QListWidgetItem *)), this,
                     SLOT(onItemSelected(QListWidgetItem *)));
  assert(ret);
}

//-----------------------------------------------------------------------------
ChangeObjectWidget::~ChangeObjectWidget() {}

//-----------------------------------------------------------------------------
void ChangeObjectWidget::show(const QPoint &pos) {
  refresh();
  int scrollbarW = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  int itemNumber = count();
  if (itemNumber > 10) {
    itemNumber = 10;
    m_width += scrollbarW;
  }
  int height = 0;
  for (int i = 0; i < itemNumber; i++) height += sizeHintForRow(i);
  setGeometry(pos.x(), pos.y(), m_width, height + 2);
  QListWidget::show();
  raise();
  setFocus();
  scrollToItem(currentItem());
}

//-----------------------------------------------------------------------------
void ChangeObjectWidget::setObjectHandle(TObjectHandle *objectHandle) {
  m_objectHandle = objectHandle;
}

//-----------------------------------------------------------------------------
void ChangeObjectWidget::setXsheetHandle(TXsheetHandle *xsheetHandle) {
  m_xsheetHandle = xsheetHandle;
}

//-----------------------------------------------------------------------------
void ChangeObjectWidget::mouseMoveEvent(QMouseEvent *event) {
  QListWidgetItem *currentWidgetItem = itemAt(event->pos());
  if (!currentWidgetItem) return;
  clearSelection();
  currentWidgetItem->setSelected(true);
}

//-----------------------------------------------------------------------------
void ChangeObjectWidget::wheelEvent(QWheelEvent *event) {
  QListWidget::wheelEvent(event);
  event->accept();
}

//-----------------------------------------------------------------------------
void ChangeObjectWidget::focusOutEvent(QFocusEvent *e) {
  if (!isVisible()) return;
  hide();
  if (parentWidget()) {
    parentWidget()->update();
  }
}

//-----------------------------------------------------------------------------
void ChangeObjectWidget::addText(const QString &text, const QString &display) {
  QListWidgetItem *item = new QListWidgetItem(display);
  item->setData(Qt::UserRole, text);
  addItem(item);
}

//-----------------------------------------------------------------------------
void ChangeObjectWidget::addText(const QString &text, const QColor &textColor) {
  QListWidgetItem *item = new QListWidgetItem(text);
  item->setForeground(textColor);
  addItem(item);
}

//-----------------------------------------------------------------------------
void ChangeObjectWidget::addText(const TStageObjectId &id,
                                 const QString &display,
                                 const QColor &identColor) {
  QListWidgetItem *item = new QListWidgetItem(display);
  QPixmap pixmap(4, 8);
  pixmap.fill(identColor);
  QIcon icon(pixmap);
  item->setIcon(icon);
  item->setData(Qt::UserRole, id.getCode());
  addItem(item);
}

//=============================================================================
// ChangeObjectParent
//-----------------------------------------------------------------------------

ChangeObjectParent::ChangeObjectParent(QWidget *parent)
    : ChangeObjectWidget(parent) {}

//-----------------------------------------------------------------------------
ChangeObjectParent::~ChangeObjectParent() {}

//-----------------------------------------------------------------------------
void ChangeObjectParent::refresh() {
  clear();
  assert(m_xsheetHandle);
  assert(m_objectHandle);
  XsheetViewer* viewer = qobject_cast<XsheetViewer*>(parentWidget());
  if (!viewer) return;
  TXsheet *xsh                   = m_xsheetHandle->getXsheet();
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  TStageObjectId parentId = xsh->getStageObject(currentObjectId)->getParent();
  TStageObject *currentObject        = xsh->getStageObject(currentObjectId);
  std::list<TStageObject *> children = currentObject->getChildren();
  TStageObjectTree *tree             = xsh->getStageObjectTree();
  int objectCount                    = tree->getStageObjectCount();
  QList<TStageObjectId> pegbarListID, columnListID;
  QList<QString> pegbarListTr, columnListTr;
  QList<QColor> pegbarListColor, columnListColor;
  TStageObjectId currentId;
  QString theLongestTxt;
  int i;
  for (i = 0; i < objectCount; i++) {
    TStageObjectId id = tree->getStageObject(i)->getId();
    int index         = id.getIndex();
    QString indexStr(std::to_string(id.getIndex() + 1).c_str());
    QColor newTextBG;

    // Remove childs from parent list
    bool found = (std::find(children.begin(), children.end(),
                            xsh->getStageObject(id)) != children.end());
    if (id == currentObjectId || found) continue;

    TStageObjectId newTextID = id;
    QString newTextTr;
    if (tree->getStageObject(i)->hasSpecifiedName())
      newTextTr = QString::fromStdString(tree->getStageObject(i)->getName());
    else
      newTextTr = getNameTr(id);

    if (id.isTable())
      newTextBG = viewer->getTableColor();
    else if (id.isPegbar())
      newTextBG = viewer->getPegColor();
    else if (id.isCamera()) {
      bool isActive = (id == xsh->getStageObjectTree()->getCurrentCameraId());
      newTextBG     = isActive ? viewer->getActiveCameraColor()
                               : viewer->getOtherCameraColor();
    } else if (id.isColumn() && (!xsh->isColumnEmpty(index))) {
      TXshColumn *colx = xsh->getColumn(index);
      if (!colx->canBeParent()) continue;

      QColor unused;
      viewer->getColumnColor(newTextBG, unused, id.getIndex(), xsh);
    } else
      continue;

    if (id == parentId) currentId = newTextID;
    if (newTextTr.length() > theLongestTxt.length()) theLongestTxt = newTextTr;
    if (id.isColumn()) {
      columnListID.append(newTextID);
      columnListTr.append(newTextTr);
      columnListColor.append(newTextBG);
    } else {
      pegbarListID.append(newTextID);
      pegbarListTr.append(newTextTr);
      pegbarListColor.append(newTextBG);
    }
  }
  for (i = 0; i < columnListID.size(); i++)
    addText(columnListID.at(i), columnListTr.at(i), columnListColor.at(i));
  for (i = 0; i < pegbarListID.size(); i++)
    addText(pegbarListID.at(i), pegbarListTr.at(i), pegbarListColor.at(i));

  m_width = fontMetrics().horizontalAdvance(theLongestTxt) + 32;
  selectCurrent(currentId);
}

//-----------------------------------------------------------------------------
QString ChangeObjectParent::getNameTr(const TStageObjectId id) {
  if (id.isTable()) return tr("Table");
  // return untranslated string for other types
  else
    return QString::fromStdString(id.toString());
}

//-----------------------------------------------------------------------------
void ChangeObjectParent::onItemSelected(QListWidgetItem *item) {
  assert(m_xsheetHandle);
  assert(m_objectHandle);

  QVariant data = item->data(Qt::UserRole);
  if (!data.isValid()) return;

  TStageObjectId newStageObjectId;
  newStageObjectId.setCode(data.toUInt());

  TXsheet *xsh                   = m_xsheetHandle->getXsheet();
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  TStageObjectId currentParentId =
      xsh->getStageObject(currentObjectId)->getParent();

  if (newStageObjectId == currentObjectId) return;

  if (newStageObjectId == currentParentId) {
    hide();
    return;
  }

  TStageObject *stageObject =
      m_xsheetHandle->getXsheet()->getStageObject(currentObjectId);
  TStageObjectCmd::setParent(currentObjectId, newStageObjectId, "B",
                             m_xsheetHandle);

  hide();
  m_objectHandle->notifyObjectIdChanged(false);
  m_xsheetHandle->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------
void ChangeObjectParent::selectCurrent(const TStageObjectId &id) {
  clearSelection();
  int numRows = count();
  for (int row = 0; row < numRows; row++) {
    QListWidgetItem *it = item(row);
    QVariant display    = it->data(Qt::UserRole);
    if (!display.isValid()) continue;
    if (id.getCode() == display.toUInt()) {
      setCurrentItem(it);
      return;
    }
  }
}

//=============================================================================
// ChangeObjectHandle
//-----------------------------------------------------------------------------

ChangeObjectHandle::ChangeObjectHandle(QWidget *parent)
    : ChangeObjectWidget(parent) {}

//-----------------------------------------------------------------------------
ChangeObjectHandle::~ChangeObjectHandle() {}

//-----------------------------------------------------------------------------
void ChangeObjectHandle::refresh() {
  clear();
  assert(m_xsheetHandle);
  assert(m_objectHandle);
  XsheetViewer* viewer = qobject_cast<XsheetViewer*>(parentWidget());
  if (!viewer) return;
  TXsheet *xsh         = m_xsheetHandle->getXsheet();
  assert(xsh);
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  TStageObject *stageObject      = xsh->getStageObject(currentObjectId);
  m_width                        = 28;

  int i;
  QString str;
  QColor colorHndHook = viewer->getPreviewFrameTextColor();
  QColor colorHndDef  = viewer->getSelectedColumnTextColor();
  QColor colorHndNone = viewer->getTextColor();
  if (stageObject->getParent().isColumn()) {
    for (i = 0; i < 20; i++) addText(str.number(20 - i), colorHndHook);
  }
  for (i = 0; i < 26; i++) {
    if (i == 1)
      addText(QString("B"), colorHndDef);
    else
      addText(QString(char('A' + i)), colorHndNone);
  }

  std::string handle = stageObject->getParentHandle();
  if (handle[0] == 'H' && handle.length() > 1) handle = handle.substr(1);

  selectCurrent(QString::fromStdString(handle));
}

//-----------------------------------------------------------------------------
void ChangeObjectHandle::onItemSelected(QListWidgetItem *item) {
  assert(m_xsheetHandle);
  assert(m_objectHandle);
  QString text                   = item->text();
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  QString handle                 = text;
  if (text.toInt() != 0) handle = QString("H") + handle;
  if (handle.isEmpty()) return;
  std::vector<TStageObjectId> ids;
  ids.push_back(currentObjectId);
  TStageObjectCmd::setParentHandle(ids, handle.toStdString(), m_xsheetHandle);
  hide();
  m_objectHandle->notifyObjectIdChanged(false);
  m_xsheetHandle->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------
void ChangeObjectHandle::selectCurrent(const QString &text) {
  clearSelection();
  int numRows = count();
  for (int row = 0; row < numRows; row++) {
    QListWidgetItem *it = item(row);
    if (text == it->text()) {
      setCurrentItem(it);
      return;
    }
  }
}

//=============================================================================
// RenameColumnField
//-----------------------------------------------------------------------------

RenameColumnField::RenameColumnField(QWidget* parent, XsheetViewer* viewer)
    : QLineEdit(parent), m_col(-1) {
  setFixedSize(20, 20);
  connect(this, SIGNAL(returnPressed()), SLOT(renameColumn()));
}

//-----------------------------------------------------------------------------
void RenameColumnField::show(const QRect &rect, int col) {
  move(rect.topLeft());
  setFixedSize(rect.size());
  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  font.setPixelSize(XSHEET_FONT_PX_SIZE);
  setFont(font);
  m_col = col;

  TXsheet *xsh    = m_xsheetHandle->getXsheet();
  int cameraIndex = xsh->getCameraColumnIndex();
  std::string name =
      col >= 0 ? xsh->getStageObject(TStageObjectId::ColumnId(col))->getName()
               : xsh->getStageObject(TStageObjectId::CameraId(cameraIndex))
                     ->getName();
  TXshColumn *column          = xsh->getColumn(col);
  TXshZeraryFxColumn *zColumn = dynamic_cast<TXshZeraryFxColumn *>(column);
  if (zColumn)
    name = ::to_string(zColumn->getZeraryColumnFx()->getZeraryFx()->getName());
  setText(QString(name.c_str()));
  selectAll();

  QWidget::show();
  raise();
  setFocus();
}

//-----------------------------------------------------------------------------
void RenameColumnField::renameColumn() {
  std::string newName     = text().toStdString();
  int cameraIndex         = m_xsheetHandle->getXsheet()->getCameraColumnIndex();
  TStageObjectId columnId = m_col >= 0 ? TStageObjectId::ColumnId(m_col)
                                       : TStageObjectId::CameraId(cameraIndex);
  TXshColumn *column =
      m_xsheetHandle->getXsheet()->getColumn(columnId.getIndex());
  if (!column && m_col >= 0) {
    m_xsheetHandle->getXsheet()->insertColumn(m_col);
    column = m_xsheetHandle->getXsheet()->getColumn(columnId.getIndex());
  }

  TXshZeraryFxColumn *zColumn = dynamic_cast<TXshZeraryFxColumn *>(column);
  if (zColumn)
    TFxCommand::renameFx(zColumn->getZeraryColumnFx(), ::to_wstring(newName),
                         m_xsheetHandle);
  else
    TStageObjectCmd::rename(columnId, newName, m_xsheetHandle);

  m_xsheetHandle->notifyXsheetChanged();
  m_col = -1;
  setText("");
  hide();
}

//-----------------------------------------------------------------------------
void RenameColumnField::focusOutEvent(QFocusEvent *e) {
  std::wstring newName = text().toStdWString();
  if (!newName.empty())
    renameColumn();
  else
    hide();

  QLineEdit::focusOutEvent(e);
}

//=============================================================================
// ColumnArea
//-----------------------------------------------------------------------------

void ColumnArea::onControlPressed(bool pressed) {
  if (m_isControlPressed == pressed) return;
  m_isControlPressed = pressed;
}

//-----------------------------------------------------------------------------
// ColumnArea
//-----------------------------------------------------------------------------
ColumnArea::ColumnArea(XsheetViewer* parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_viewer(parent)
    , m_pos(-1, -1)
    , m_tooltip(tr(""))
    , m_col(-1)
    , m_columnTransparencyPopup(0)
    , m_transparencyPopupTimer(0)
    , m_isPanning(false)
    , m_soundColumnPopup(0) {
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  TObjectHandle *objectHandle = TApp::instance()->getCurrentObject();
  m_changeObjectParent        = new ChangeObjectParent(m_viewer);
  m_changeObjectParent->setObjectHandle(objectHandle);
  m_changeObjectParent->setXsheetHandle(xsheetHandle);
  m_changeObjectParent->hide();

  m_changeObjectHandle = new ChangeObjectHandle(m_viewer);
  m_changeObjectHandle->setObjectHandle(objectHandle);
  m_changeObjectHandle->setXsheetHandle(xsheetHandle);
  m_changeObjectHandle->hide();


  m_renameColumnField = new RenameColumnField(this, m_viewer);
  m_renameColumnField->setXsheetHandle(xsheetHandle);
  m_renameColumnField->hide();

  QActionGroup *actionGroup = new QActionGroup(this);
  m_subsampling1            = new QAction(tr("&Subsampling 1"), actionGroup);
  m_subsampling2            = new QAction(tr("&Subsampling 2"), actionGroup);
  m_subsampling3            = new QAction(tr("&Subsampling 3"), actionGroup);
  m_subsampling4            = new QAction(tr("&Subsampling 4"), actionGroup);
  actionGroup->addAction(m_subsampling1);
  actionGroup->addAction(m_subsampling2);
  actionGroup->addAction(m_subsampling3);
  actionGroup->addAction(m_subsampling4);

  connect(actionGroup, SIGNAL(triggered(QAction *)), this,
          SLOT(onSubSampling(QAction *)));
  connect(xsheetHandle, SIGNAL(xsheetCameraChange(int)), this,
          SLOT(onXsheetCameraChange(int)));
  setMouseTracking(true);
}

//-----------------------------------------------------------------------------
ColumnArea::~ColumnArea() {}

void ColumnArea::resetInteraction() {
  if (m_transparencyPopupTimer) m_transparencyPopupTimer->stop();
  delete m_columnTransparencyPopup;
  m_columnTransparencyPopup = nullptr;
  delete m_soundColumnPopup;
  m_soundColumnPopup = nullptr;
  m_renameColumnField->clear();
  m_renameColumnField->hide();
  m_changeObjectParent->hide();
  m_changeObjectHandle->hide();
  m_doOnRelease = m_doOnMove = 0;
  m_isPanning = m_isControlPressed = false;
  m_col                            = -1;
  m_pos                            = QPoint(-1, -1);
  m_tooltip.clear();
}

DragTool *ColumnArea::getDragTool() const { return m_viewer->getDragTool(); }
void ColumnArea::setDragTool(DragTool *dragTool) {
  m_viewer->setDragTool(dragTool);
}

//-----------------------------------------------------------------------------
void ColumnArea::drawFoldedColumnHead(QPainter &p, int col) {
  const Orientation *o = m_viewer->orientation();

  QPoint orig = m_viewer->positionToXY(CellPosition(0, col));
  QRect rect  = o->rect(PredefinedRect::FOLDED_LAYER_HEADER).translated(orig);

  int x0, y0, x, y;

  if (o->isVerticalTimeline()) {
    x0 = rect.topLeft().x() + 1;
    y0 = 0;

    p.setPen(m_viewer->getDarkLineColor());
    p.fillRect(x0, y0 + 1, rect.width(), 18,
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0, y0 + 17, 2, rect.height() - 34,
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0 + 3, y0 + 20, 2, rect.height() - 36,
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0 + 6, y0 + 17, 2, rect.height() - 34,
               QBrush(m_viewer->getFoldedColumnBGColor()));

    p.setPen(m_viewer->getFoldedColumnLineColor());
    p.drawLine(x0 - 1, y0 + 17, x0 - 1, rect.height());
    p.setPen(m_viewer->getFoldedColumnLineColor());
    p.drawLine(x0 + 2, y0 + 17, x0 + 2, rect.height());
    p.drawLine(x0 + 5, y0 + 17, x0 + 5, rect.height());
    p.drawLine(x0, y0 + 17, x0 + 1, 17);
    p.drawLine(x0 + 3, y0 + 20, x0 + 4, 20);
    p.drawLine(x0 + 6, y0 + 17, x0 + 7, 17);

    // triangolini
    p.setPen(Qt::black);
    x = x0;
    y = 12;
    p.drawPoint(QPointF(x, y));
    x++;
    p.drawLine(x, y - 1, x, y + 1);
    x++;
    p.drawLine(x, y - 2, x, y + 2);
    x += 3;
    p.drawLine(x, y - 2, x, y + 2);
    x++;
    p.drawLine(x, y - 1, x, y + 1);
    x++;
    p.drawPoint(x, y);
  } else {
    x0 = 0;
    y0 = rect.topLeft().y() + 1;

    p.setPen(m_viewer->getFoldedColumnLineColor());
    p.fillRect(x0 + 1, y0, 18, rect.height(),
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0 + 17, y0, rect.width() - 34, 2,
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0 + 20, y0 + 3, rect.width() - 36, 2,
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0 + 17, y0 + 6, rect.width() - 34, 2,
               QBrush(m_viewer->getFoldedColumnBGColor()));

    p.setPen(m_viewer->getFoldedColumnLineColor());
    p.drawLine(x0 + 17, y0 - 1, rect.width(), y0 - 1);
    p.setPen(m_viewer->getFoldedColumnLineColor());
    p.drawLine(x0 + 17, y0 + 2, rect.width(), y0 + 2);
    p.drawLine(x0 + 17, y0 + 5, rect.width(), y0 + 5);
    p.drawLine(x0 + 17, y0, 17, y0 + 1);
    p.drawLine(x0 + 20, y0 + 3, 20, y0 + 4);
    p.drawLine(x0 + 17, y0 + 6, 17, y0 + 7);

    // triangolini
    p.setPen(Qt::black);
    x = 12;
    y = y0;
    p.drawPoint(QPointF(x, y));
    y++;
    p.drawLine(x - 1, y, x + 1, y);
    y++;
    p.drawLine(x - 2, y, x + 2, y);
    y += 3;
    p.drawLine(x - 2, y, x + 2, y);
    y++;
    p.drawLine(x - 1, y, x + 1, y);
    y++;
    p.drawPoint(x, y);
  }
}

void ColumnArea::prepareThumbnails() {
  const Orientation* layout = m_viewer->orientation();
  if (!layout->flag(PredefinedFlag::THUMBNAIL_AREA_VISIBLE) ||
      layout->rect(PredefinedRect::THUMBNAIL_AREA).isEmpty() ||
      layout->rect(PredefinedRect::THUMBNAIL).isEmpty() ||
      Preferences::instance()->getColumnIconLoadingPolicy() ==
          Preferences::LoadOnDemand)
    return;
  const QRect visible = visibleRegion().boundingRect();
  if (visible.isEmpty()) return;
  TXsheet* sheet = m_viewer->getXsheet();
  if (!sheet) return;
  const CellRange range = m_viewer->xyRectToRange(visible);
  for (int col = std::max(0, range.from().layer());
       col <= std::min(sheet->getColumnCount() - 1, range.to().layer());
       ++col) {
    if (!m_viewer->columnFan()->isActive(col)) continue;
    TXshColumn* column = sheet->getColumn(col);
    if (!column || column->isEmpty()) continue;
    if (auto* level = column->getLevelColumn()) level->setIconVisible(true);
    if (auto* mesh = column->getMeshColumn()) mesh->setIconVisible(true);
    if (auto* fx = dynamic_cast<TXshZeraryFxColumn*>(column))
      fx->setIconVisible(true);
  }
}

//-----------------------------------------------------------------------------
QPixmap ColumnArea::getColumnIcon(int columnIndex) {
  const Orientation *o = m_viewer->orientation();
  const bool camera         = columnIndex < 0;
  const PredefinedRect area = camera ? PredefinedRect::CAMERA_ICON_AREA
                                     : PredefinedRect::THUMBNAIL_AREA;
  const PredefinedRect image =
      camera ? PredefinedRect::CAMERA_ICON : PredefinedRect::THUMBNAIL;
  if (o->rect(area).isEmpty() || o->rect(image).isEmpty() ||
      !o->flag(camera ? PredefinedFlag::CAMERA_ICON_VISIBLE
                      : PredefinedFlag::THUMBNAIL_AREA_VISIBLE))
    return QPixmap();
  auto& resources  = m_viewer->headerResources();
  const QSize size = o->rect(image).size();
  const qreal dpr  = m_viewer->devicePixelRatioF();
  if (camera)
    return resources.svg(o->isVerticalTimeline() ? ":Resources/camera_small.svg"
                                                 : ":Resources/camera.svg",
                         size, Qt::transparent, dpr);
  TXsheet* sheet     = m_viewer->getXsheet();
  TXshColumn* column = sheet ? sheet->getColumn(columnIndex) : nullptr;
  if (!column || column->isEmpty() || column->getSoundColumn())
    return QPixmap();
  if (column->getPaletteColumn())
    return resources.svg(":Resources/palette_header.svg", size, Qt::transparent,
                         dpr);
  if (column->getSoundTextColumn())
    return resources.svg(getIconPath("notelevel"), size, Qt::transparent, dpr);
  int first, last;
  sheet->getCellRange(columnIndex, first, last);
  if (first > last) return QPixmap();
  const TXshCell cell = sheet->getCell(first, columnIndex);
  if (cell.isEmpty()) return QPixmap();
  auto* level        = column->getLevelColumn();
  auto* mesh         = column->getMeshColumn();
  auto* fx           = dynamic_cast<TXshZeraryFxColumn*>(column);
  const bool visible = (!level || level->isIconVisible()) &&
                       (!mesh || mesh->isIconVisible()) &&
                       (!fx || fx->isIconVisible());
  const bool loadOnDemand =
      Preferences::instance()->getColumnIconLoadingPolicy() ==
      Preferences::LoadOnDemand;
  const bool onDemand =
      loadOnDemand && m_viewer->getCurrentColumn() != columnIndex;
  if (loadOnDemand && !onDemand && !visible) return QPixmap();
  QPixmap icon =
      fx ? FxIconPixmapManager::instance()->getFxIconPm(
               fx->getZeraryColumnFx()->getZeraryFx()->getFxType())
         : IconGenerator::instance()->getIcon(cell.m_level.getPointer(),
                                              cell.m_frameId, false, onDemand);

  return loadOnDemand && !visible ? QPixmap() : resources.scaled(icon, size);
}

//-----------------------------------------------------------------------------
void ColumnArea::paintEvent(QPaintEvent *event) {  // AREA
  QRect toBeUpdated = event->rect();

  QPainter p(this);
  p.setClipRegion(event->region());

  TXsheet *xsh        = m_viewer->getXsheet();
  CellRange cellRange = m_viewer->xyRectToRange(toBeUpdated);
  int c0, c1;  // range of visible columns
  c0 = cellRange.from().layer();
  c1 = cellRange.to().layer();
  if (!m_viewer->orientation()->isVerticalTimeline()) {
    int colCount = std::max(1, xsh->getColumnCount());
    c1           = std::min(c1, colCount - 1);
  }

  ColumnFan* columnFan     = m_viewer->columnFan();
  auto* objectHandle       = TApp::instance()->getCurrentObject();
  const bool editingSpline = objectHandle->isSpline();
  const bool cameraCurrent =
      objectHandle->getObjectId() ==
      TStageObjectId::CameraId(xsh->getCameraColumnIndex());
  const int currentColumn = m_viewer->getCurrentColumn();
  int col;
  for (col = c0; col <= c1; col++) {
    const bool folded     = !columnFan->isActive(col);
    const auto headerRect = folded    ? PredefinedRect::FOLDED_LAYER_HEADER
                            : col < 0 ? PredefinedRect::CAMERA_LAYER_HEADER
                                      : PredefinedRect::LAYER_HEADER;
    if (!event->region().intersects(
            m_viewer->orientation()
                ->rect(headerRect)
                .translated(m_viewer->positionToXY(CellPosition(0, col)))))
      continue;
    // draw column fan (collapsed columns)
    if (!columnFan->isActive(col)) {
      drawFoldedColumnHead(p, col);
    } else {
      ColumnHeaderInteraction interaction;
      interaction.current = cameraCurrent ? col == -1 : currentColumn == col;
      interaction.selected =
          m_viewer->getColumnSelection()->isColumnSelected(col);
      interaction.editingSpline = editingSpline;
      ColumnHeaderPainter header(m_viewer, m_pos, p, col, interaction);
      header.paint(getColumnIcon(col));
    }
  }

  p.setPen(m_viewer->getVerticalLineHeadColor());
  p.setBrush(Qt::NoBrush);
  if (m_viewer->orientation()->isVerticalTimeline())
    p.drawRect(rect().adjusted(-1, 0, -1, -3));
  else
    p.drawRect(rect().adjusted(0, 0, -2, -1));

  if (getDragTool()) getDragTool()->drawColumnsArea(p);
}

//-----------------------------------------------------------------------------
using namespace DVGui;

ColumnTransparencyPopup::ColumnTransparencyPopup(XsheetViewer* viewer,
                                                 QWidget* parent)
    : QWidget(parent, Qt::Popup), m_viewer(viewer), m_lockBtn(nullptr) {
  setFixedWidth(8 + 78 + 8 + 100 + 8 + 8 + 8 + 7);

  m_slider = new QSlider(Qt::Horizontal, this);
  m_slider->setMinimum(1);
  m_slider->setMaximum(100);
  m_slider->setFixedHeight(14);
  m_slider->setFixedWidth(100);

  m_value = new DVGui::IntLineEdit(this, 1, 1, 100);
  /*m_value->setValidator(new QIntValidator (1, 100, m_value));
m_value->setFixedHeight(16);
m_value->setFixedWidth(30);
static QFont font("Helvetica", 7, QFont::Normal);
m_value->setFont(font);*/

  // contents of the combo box will be updated in setColumn
  m_filterColorCombo = new QComboBox(this);

  // Lock button is moved in the popup for Minimum layout
  QPushButton *lockExtraBtn = nullptr;
  if (m_viewer->getXsheetLayout() == "Minimum") {
    m_lockBtn = new QPushButton(tr("Lock Column"), this);
    m_lockBtn->setCheckable(true);
    m_lockBtn->setIcon(createLockIcon(m_viewer));
    lockExtraBtn = new QPushButton(this);
    QMenu *menu  = new QMenu();
    menu->setObjectName("xsheetColumnAreaMenu_Lock");
    CommandManager *cmdManager = CommandManager::instance();
    menu->addAction(cmdManager->getAction("MI_LockThisColumnOnly"));
    menu->addAction(cmdManager->getAction("MI_LockSelectedColumns"));
    menu->addAction(cmdManager->getAction("MI_LockAllColumns"));
    menu->addAction(cmdManager->getAction("MI_UnlockSelectedColumns"));
    menu->addAction(cmdManager->getAction("MI_UnlockAllColumns"));
    menu->addAction(cmdManager->getAction("MI_ToggleColumnLocks"));
    lockExtraBtn->setMenu(menu);
    lockExtraBtn->setFixedSize(20, 20);
  }

  QGridLayout *mainLayout = new QGridLayout();
  mainLayout->setContentsMargins(3, 3, 3, 3);
  mainLayout->setHorizontalSpacing(6);
  mainLayout->setVerticalSpacing(6);
  {
    mainLayout->addWidget(new QLabel(tr("Opacity:"), this), 0, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(3);
    {
      hlayout->addWidget(m_slider);
      hlayout->addWidget(m_value);
      hlayout->addWidget(new QLabel("%"));
    }
    mainLayout->addLayout(hlayout, 0, 1);

    mainLayout->addWidget(new QLabel(tr("Filter:"), this), 1, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_filterColorCombo, 1, 1,
                          Qt::AlignLeft | Qt::AlignVCenter);

    if (m_lockBtn) {
      QHBoxLayout *lockLay = new QHBoxLayout();
      lockLay->setContentsMargins(0, 0, 0, 0);
      lockLay->setSpacing(3);
      {
        lockLay->addWidget(m_lockBtn, 0);
        lockLay->addWidget(lockExtraBtn, 0);
      }
      mainLayout->addLayout(lockLay, 2, 1, Qt::AlignLeft | Qt::AlignVCenter);
    }
  }
  setLayout(mainLayout);

  bool ret = connect(m_slider, SIGNAL(sliderReleased()), this,
                     SLOT(onSliderReleased()));
  ret      = ret && connect(m_slider, SIGNAL(sliderMoved(int)), this,
                            SLOT(onSliderChange(int)));
  ret      = ret && connect(m_slider, SIGNAL(valueChanged(int)), this,
                            SLOT(onSliderValueChanged(int)));
  ret      = ret && connect(m_value, SIGNAL(textChanged(const QString &)), this,
                            SLOT(onValueChanged(const QString &)));

  ret = ret && connect(m_filterColorCombo, SIGNAL(activated(int)), this,
                       SLOT(onFilterColorChanged()));
  if (m_lockBtn)
    ret = ret && connect(m_lockBtn, SIGNAL(clicked(bool)), this,
                         SLOT(onLockButtonClicked(bool)));

  assert(ret);
}

//----------------------------------------------------------------
void ColumnTransparencyPopup::onSliderValueChanged(int val) {
  if (m_slider->isSliderDown()) return;
  m_value->setText(QString::number(val));
  onSliderReleased();
}

void ColumnTransparencyPopup::onSliderReleased() {
  m_column->setOpacity(troundp(255.0 * m_slider->value() / 100.0));
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//-----------------------------------------------------------------------
void ColumnTransparencyPopup::onSliderChange(int val) {
  disconnect(m_value, SIGNAL(textChanged(const QString &)), 0, 0);
  m_value->setText(QString::number(val));
  connect(m_value, SIGNAL(textChanged(const QString &)), this,
          SLOT(onValueChanged(const QString &)));
}

//----------------------------------------------------------------
void ColumnTransparencyPopup::onValueChanged(const QString &str) {
  int val = str.toInt();
  m_slider->setValue(val);
  m_column->setOpacity(troundp(255.0 * val / 100.0));

  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//----------------------------------------------------------------
// TODO : UNDO
void ColumnTransparencyPopup::onFilterColorChanged() {
  int id = m_filterColorCombo->currentData().toInt();
  if (m_column->getColorFilterId() == id) return;
  m_column->setColorFilterId(id);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//----------------------------------------------------------------
void ColumnTransparencyPopup::onLockButtonClicked(bool on) {
  assert(m_lockBtn);
  if (!m_lockBtn) return;
  m_column->lock(on);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//----------------------------------------------------------------
void ColumnTransparencyPopup::setColumn(TXshColumn *column) {
  m_column = column;
  assert(m_column);
  int val = (int)troundp(100.0 * m_column->getOpacity() / 255.0);
  m_slider->setValue(val);
  disconnect(m_value, SIGNAL(textChanged(const QString &)), 0, 0);
  m_value->setText(QString::number(val));
  connect(m_value, SIGNAL(textChanged(const QString &)), this,
          SLOT(onValueChanged(const QString &)));

  m_filterColorCombo->clear();
  // initialize color filter combo box
  QList<TSceneProperties::ColorFilter> filters = TApp::instance()
                                                     ->getCurrentScene()
                                                     ->getScene()
                                                     ->getProperties()
                                                     ->getColorFilters();

  for (int f = 0; f < filters.size(); f++) {
    TSceneProperties::ColorFilter filter = filters.at(f);
    if (f == 0)
      m_filterColorCombo->addItem(filter.name, f);
    else if (!filter.name.isEmpty())
      m_filterColorCombo->addItem(
          QIcon(m_viewer->headerResources().colorChip(QColor(
              filter.color.r, filter.color.g, filter.color.b, filter.color.m))),
          filter.name, f);
  }

  m_filterColorCombo->setCurrentIndex(
      m_filterColorCombo->findData(m_column->getColorFilterId()));

  if (m_lockBtn) m_lockBtn->setChecked(m_column->isLocked());
}

/*void ColumnTransparencyPopup::mouseMoveEvent ( QMouseEvent * e )
{
        int val = tcrop((e->pos().x()+10)/(this->width()/(99-1+1)), 1, 99);
        m_value->setText(QString::number(val));
        m_slider->setValue(val);
}*/

void ColumnTransparencyPopup::mouseReleaseEvent(QMouseEvent *e) {
  // hide();
}

//------------------------------------------------------------------------------
SoundColumnPopup::SoundColumnPopup(QWidget *parent)
    : QWidget(parent, Qt::Popup) {
  setFixedWidth(8 + 78 + 8 + 100 + 8 + 8 + 8 + 7);

  m_slider = new QSlider(Qt::Horizontal, this);
  m_slider->setMinimum(0);
  m_slider->setMaximum(100);
  m_slider->setFixedHeight(14);
  m_slider->setFixedWidth(100);

  m_value = new DVGui::IntLineEdit(this, 1, 1, 100);

  QLabel *sliderLabel = new QLabel(tr("Volume:"), this);

  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setContentsMargins(3, 3, 3, 3);
  mainLayout->setSpacing(3);
  {
    QHBoxLayout *hlayout = new QHBoxLayout;
    // hlayout->setContentsMargins(0, 3, 0, 3);
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(3);
    hlayout->addWidget(sliderLabel, 0);
    hlayout->addWidget(m_slider);
    hlayout->addWidget(m_value);
    hlayout->addWidget(new QLabel("%"));
    mainLayout->addLayout(hlayout, 0);
  }
  setLayout(mainLayout);

  bool ret = connect(m_slider, SIGNAL(sliderReleased()), this,
                     SLOT(onSliderReleased()));
  ret      = ret && connect(m_slider, SIGNAL(sliderMoved(int)), this,
                            SLOT(onSliderChange(int)));
  ret      = ret && connect(m_slider, SIGNAL(valueChanged(int)), this,
                            SLOT(onSliderValueChanged(int)));
  ret      = ret && connect(m_value, SIGNAL(textChanged(const QString &)), this,
                            SLOT(onValueChanged(const QString &)));
  assert(ret);
}

//----------------------------------------------------------------
void SoundColumnPopup::onSliderValueChanged(int val) {
  if (m_slider->isSliderDown()) return;
  m_value->setText(QString::number(val));
  onSliderReleased();
}

void SoundColumnPopup::onSliderReleased() {
  int val = m_slider->value();
  m_column->getSoundColumn()->setVolume(((double)val / 100.0));
  TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//-----------------------------------------------------------------------
void SoundColumnPopup::onSliderChange(int val) {
  disconnect(m_value, SIGNAL(textChanged(const QString &)), 0, 0);
  m_value->setText(QString::number(val));
  connect(m_value, SIGNAL(textChanged(const QString &)), this,
          SLOT(onValueChanged(const QString &)));
}

//----------------------------------------------------------------
void SoundColumnPopup::onValueChanged(const QString &str) {
  int val = str.toInt();
  m_slider->setValue(val);
  m_column->getSoundColumn()->setVolume(((double)val / 100.0));

  TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//----------------------------------------------------------------
void SoundColumnPopup::setColumn(TXshColumn *column) {
  m_column = column;
  assert(m_column);
  double volume = m_column->getSoundColumn()->getVolume();
  int val       = (int)troundp(100.0 * volume);
  m_slider->setValue(val);
  disconnect(m_value, SIGNAL(textChanged(const QString &)), 0, 0);
  m_value->setText(QString::number(val));
  connect(m_value, SIGNAL(textChanged(const QString &)), this,
          SLOT(onValueChanged(const QString &)));
}

void SoundColumnPopup::mouseReleaseEvent(QMouseEvent *e) {
  // hide();
}

//----------------------------------------------------------------
void ColumnArea::openTransparencyPopup() {
  if (m_transparencyPopupTimer) m_transparencyPopupTimer->stop();
  if (m_col < 0) return;
  TXshColumn *column = m_viewer->getXsheet()->getColumn(m_col);
  if (!column || column->isEmpty()) return;

  if (!column->isCamstandVisible()) {
    column->setCamstandVisible(true);
    TApp::instance()->getCurrentScene()->notifySceneChanged();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    update();
  }

  m_columnTransparencyPopup->setColumn(column);
  m_columnTransparencyPopup->show();
}

void ColumnArea::openSoundColumnPopup() {
  if (m_col < 0) return;
  TXshColumn *column = m_viewer->getXsheet()->getColumn(m_col);
  if (!column || column->isEmpty()) return;

  if (!column->isCamstandVisible()) {
    column->setCamstandVisible(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
    TApp::instance()->getCurrentScene()->notifySceneChanged();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    update();
  }

  m_soundColumnPopup->setColumn(column);
  m_soundColumnPopup->show();
}

void ColumnArea::openCameraColumnPopup(QPoint pos) {
  QMenu menu(this);

  TXsheet *xsh           = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObjectTree *tree = xsh->getStageObjectTree();
  int i, objCount = tree->getStageObjectCount();
  for (i = 0; i < objCount; i++) {
    TStageObject *obj = tree->getStageObject(i);
    if (!obj || !obj->getId().isCamera()) continue;
    TStageObjectId objId = obj->getId();
    std::string objName  = obj->getName();
    QAction *action      = new QAction(QString::fromStdString(objName), this);
    action->setData(objId.getIndex());
    connect(action, SIGNAL(triggered()), this,
            SLOT(onCameraColumnChangedTriggered()));
    menu.addAction(action);
  }
  // Lock button is moved in this menu for Minimum layout
  if (m_viewer->getXsheetLayout() == "Minimum") {
    menu.addSeparator();
    bool isLocked = m_viewer->getXsheet()->getColumn(-1)->isLocked();
    QAction *lockAction =
        new QAction((isLocked) ? tr("Unlock") : tr("Lock"), this);
    lockAction->setCheckable(true);
    lockAction->setChecked(isLocked);
    lockAction->setIcon(createLockIcon(m_viewer));
    menu.addAction(lockAction);
    connect(lockAction, SIGNAL(toggled(bool)), this,
            SLOT(onCameraColumnLockToggled(bool)));
  }

  menu.exec(pos);
}

void ColumnArea::onCameraColumnChangedTriggered() {
  int newIndex = qobject_cast<QAction *>(sender())->data().toInt();
  onXsheetCameraChange(newIndex);
}

void ColumnArea::onCameraColumnLockToggled(bool lock) {
  m_viewer->getXsheet()->getColumn(-1)->lock(lock);
}

//----------------------------------------------------------------
void ColumnArea::onXsheetCameraChange(int newIndex) {
  int oldIndex = m_viewer->getXsheet()->getCameraColumnIndex();
  if (newIndex == oldIndex) return;

  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();

  CameraColumnSwitchUndo *undo =
      new CameraColumnSwitchUndo(oldIndex, newIndex, xsheetHandle);
  undo->redo();
  TUndoManager::manager()->add(undo);
}

//----------------------------------------------------------------
void ColumnArea::startTransparencyPopupTimer(QMouseEvent *e) {  // AREA
  if (!m_columnTransparencyPopup)
    m_columnTransparencyPopup = new ColumnTransparencyPopup(m_viewer, this);

  m_columnTransparencyPopup->move(e->globalPos().x(), e->globalPos().y());

  if (!m_transparencyPopupTimer) {
    m_transparencyPopupTimer = new QTimer(this);
    bool ret = connect(m_transparencyPopupTimer, SIGNAL(timeout()), this,
                       SLOT(openTransparencyPopup()));
    assert(ret);
    m_transparencyPopupTimer->setSingleShot(true);
  }

  m_transparencyPopupTimer->start(300);
}

//----------------------------------------------------------------
void ColumnArea::mousePressEvent(QMouseEvent *event) {
  const Orientation *o = m_viewer->orientation();

  m_doOnRelease = 0;
  m_doOnMove    = 0;

  m_viewer->setQtModifiers(event->modifiers());
  assert(getDragTool() == 0);

  m_col = -1;  // new in 6.4

  // both left and right click can change the selection
  if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
    TXsheet *xsh   = m_viewer->getXsheet();
    ColumnFan* fan = m_viewer->columnFan();
    m_col          = m_viewer->xyToPosition(event->pos()).layer();
    // when clicking the column fan
    if (!fan->isActive(m_col))  // column Fan
    {
      m_viewer->unfoldColumns(m_col);
      return;
    }

    TXshColumn *column = xsh->getColumn(m_col);
    bool isEmpty       = !column || column->isEmpty();
    TApp::instance()->getCurrentObject()->setIsSpline(false);

    // get mouse position
    QPoint mouseInCell =
        event->pos() - m_viewer->positionToXY(CellPosition(0, m_col));
    // int x = event->pos().x() - m_viewer->columnToX(m_col);
    // int y = event->pos().y();
    // QPoint mouseInCell(x, y);
    int x = mouseInCell.x(), y = mouseInCell.y();

    // don't make column current when click on some of its toggle buttons
    bool needMakeColumnCurrent = true;
    if (o->rect(PredefinedRect::LOCK_AREA).contains(mouseInCell) ||
        o->rect(PredefinedRect::CONFIG_AREA).contains(mouseInCell))
      needMakeColumnCurrent = false;
    if (Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled()) {
      if (o->rect(PredefinedRect::UNIFIEDVIEW_LAYER_AREA).contains(mouseInCell))
        needMakeColumnCurrent = false;
    } else {
      if (o->rect(PredefinedRect::EYE_AREA).contains(mouseInCell) ||
          o->rect(PredefinedRect::PREVIEW_LAYER_AREA).contains(mouseInCell))
        needMakeColumnCurrent = false;
    }

    // set the clicked column to current
    if (needMakeColumnCurrent) m_viewer->setCurrentColumn(m_col);

    // clicking on the camera column
    if (m_col < 0) {
      // lock button
      if (o->rect(PredefinedRect::CAMERA_LOCK_AREA).contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        if (m_isControlPressed)
          m_doOnRelease = ToggleAllLock;
        else {
          m_doOnRelease = m_isControlPressed ? ToggleAllLock : 0;
          column->lock(!column->isLocked());
        }
      }
      // config button
      else if (o->rect(PredefinedRect::CAMERA_CONFIG_AREA)
                   .contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        m_doOnRelease = OpenSettings;
      }
      // clicking another area means column selection
      else {
        if (m_viewer->getColumnSelection()->isColumnSelected(m_col) &&
            event->button() == Qt::RightButton)
          return;
        setDragTool(XsheetGUI::DragTool::makeColumnSelectionTool(m_viewer));
      }
    }
    // clicking on the normal columns
    else if (!isEmpty) {
      // grabbing the left side of the column enables column move
      if (o->rect(PredefinedRect::DRAG_LAYER).contains(mouseInCell) ||
          (!o->flag(PredefinedFlag::DRAG_LAYER_VISIBLE)  // If dragbar hidden,
                                                         // layer name/number
                                                         // becomes dragbar
           && (o->rect(PredefinedRect::LAYER_NUMBER).contains(mouseInCell) ||
               o->rect(PredefinedRect::LAYER_NAME).contains(mouseInCell)))) {
        setDragTool(XsheetGUI::DragTool::makeColumnMoveTool(m_viewer));
      }
      // lock button
      else if (o->rect(PredefinedRect::LOCK_AREA).contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        if (m_isControlPressed)
          m_doOnRelease = ToggleAllLock;
        else {
          m_doOnMove = column->isLocked() ? ToggleOffLock : ToggleOnLock;
          column->lock(!column->isLocked());
        }
      }
      // unified view button
      else if (Preferences::instance()
                   ->isUnifyColumnVisibilityTogglesEnabled() &&
               o->rect(PredefinedRect::UNIFIEDVIEW_LAYER_AREA)
                   .contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        if (column->getSoundTextColumn()) {
          // do nothing
        } else {
          // sync eye button on release
          if (m_isControlPressed)
            m_doOnRelease = m_isControlPressed ? ToggleAllTransparency : 0;
          else {
            m_doOnMove = column->isCamstandVisible() ? ToggleOffTransparency
                                                     : ToggleOnTransparency;
            column->setCamstandVisible(!column->isCamstandVisible());
          }
          if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE) &&
              !column->getSoundColumn())
            startTransparencyPopupTimer(event);
        }
      }
      // separated view - preview button
      else if (!Preferences::instance()
                    ->isUnifyColumnVisibilityTogglesEnabled() &&
               o->rect(PredefinedRect::EYE_AREA).contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        if (column->getSoundTextColumn()) {
          // do nothing
        } else {
          if (m_isControlPressed)
            m_doOnRelease = ToggleAllPreviewVisible;
          else {
            m_doOnMove = column->isPreviewVisible() ? ToggleOffPreviewVisible
                                                    : ToggleOnPreviewVisible;
            column->setPreviewVisible(!column->isPreviewVisible());
          }
          if (column->getSoundColumn())
            TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
        }
      }
      // separated view - camstand button
      else if (!Preferences::instance()
                    ->isUnifyColumnVisibilityTogglesEnabled() &&
               o->rect(PredefinedRect::PREVIEW_LAYER_AREA)
                   .contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        if (column->getPaletteColumn() || column->getSoundTextColumn()) {
          // do nothing
        } else {
          if (m_isControlPressed)
            m_doOnRelease = ToggleAllTransparency;
          else {
            m_doOnMove = column->isCamstandVisible() ? ToggleOffTransparency
                                                     : ToggleOnTransparency;
            column->setCamstandVisible(!column->isCamstandVisible());
          }
          if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE) &&
              !column->getSoundColumn())
            startTransparencyPopupTimer(event);
        }
      }
      // config button
      else if (o->rect(PredefinedRect::CONFIG_AREA).contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        TXshZeraryFxColumn *zColumn =
            dynamic_cast<TXshZeraryFxColumn *>(column);

        if (column && (zColumn || column->getPaletteColumn() ||
                       column->getSoundTextColumn())) {
          // do nothing
        } else
          m_doOnRelease = OpenSettings;
      }
      // sound column
      else if (column && column->getSoundColumn()) {
        if (o->rect(PredefinedRect::SOUND_ICON).contains(mouseInCell)) {
          TXshSoundColumn *s = column->getSoundColumn();
          if (s) {
            if (s->isPlaying())
              s->stop();
            else {
              s->play();
              if (!s->isPlaying())
                s->stop();  // Serve per vista, quando le casse non sono
                            // attaccate
            }
            int interval = 0;
            if (s->isPlaying()) {
              TSoundTrackP sTrack = s->getCurrentPlaySoundTruck();
              interval            = sTrack->getDuration() * 1000 + 300;
            }
            if (s->isPlaying() && interval > 0) {
              QTimer::singleShot(interval, this, [this, s] {
                if (s && s->isPlaying()) s->stop();
                update();
              });
            }
          }
          update();
        } else if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE) &&
                   o->rect(PredefinedRect::VOLUME_AREA).contains(mouseInCell))
          setDragTool(XsheetGUI::DragTool::makeVolumeDragTool(m_viewer));
        else
          setDragTool(XsheetGUI::DragTool::makeColumnSelectionTool(m_viewer));
      }
      // clicking another area means column selection
      else {
        if (event->button() != Qt::LeftButton) return;
        if (xsh->getColumn(m_col)->getSoundTextColumn()) return;

        int y = Preferences::instance()->isShowXSheetToolbarEnabled() ? 30 : 0;
        if (o->rect(PredefinedRect::PEGBAR_NAME)
                .adjusted(0, 0, -20, 0)
                .contains(mouseInCell)) {
          m_changeObjectParent->refresh();
          m_changeObjectParent->show(
              QPoint(o->rect(PredefinedRect::PARENT_HANDLE_NAME).bottomLeft() +
                     QPoint(o->rect(PredefinedRect::CAMERA_CELL).width(), 0) +
                     m_viewer->positionToXY(CellPosition(0, m_col)) +
                     QPoint(-m_viewer->getColumnScrollValue(), y)));
          return;
        }
        if (o->rect(PredefinedRect::PARENT_HANDLE_NAME).contains(mouseInCell)) {
          m_changeObjectHandle->refresh();
          m_changeObjectHandle->show(
              QPoint(o->rect(PredefinedRect::PARENT_HANDLE_NAME).bottomLeft() +
                     m_viewer->positionToXY(CellPosition(0, m_col + 1)) +
                     QPoint(-m_viewer->getColumnScrollValue() + 2, y)));
          return;
        }

        setDragTool(XsheetGUI::DragTool::makeColumnSelectionTool(m_viewer));

        if (column) {
          // toggle columnIcon visibility with alt+click
          TXshLevelColumn *levelColumn = column->getLevelColumn();
          TXshMeshColumn *meshColumn   = column->getMeshColumn();
          TXshZeraryFxColumn *zColumn =
              dynamic_cast<TXshZeraryFxColumn *>(column);
          if (Preferences::instance()->getColumnIconLoadingPolicy() ==
                  Preferences::LoadOnDemand &&
              (event->modifiers() & Qt::AltModifier)) {
            if (levelColumn)
              levelColumn->setIconVisible(!levelColumn->isIconVisible());
            else if (meshColumn)
              meshColumn->setIconVisible(!meshColumn->isIconVisible());
            else if (zColumn)
              zColumn->setIconVisible(!zColumn->isIconVisible());
          }
        }
      }
      // update the current fx when zerary fx column is clicked
      if (column && column->getZeraryFxColumn()) {
        TFx *fx = column->getZeraryFxColumn()->getZeraryColumnFx();
        TApp::instance()->getCurrentFx()->setFx(fx);
      }
    } else {
      if (m_viewer->getColumnSelection()->isColumnSelected(m_col) &&
          event->button() == Qt::RightButton)
        return;
      setDragTool(XsheetGUI::DragTool::makeColumnSelectionTool(m_viewer));
    }

    m_viewer->dragToolClick(event);
    update();

  } else if (event->button() == Qt::MiddleButton) {
    m_pos       = event->pos();
    m_isPanning = true;
  }
}

//-----------------------------------------------------------------------------
void ColumnArea::mouseMoveEvent(QMouseEvent *event) {
  const Orientation *o = m_viewer->orientation();

  m_viewer->setQtModifiers(event->modifiers());
  QPoint pos = event->pos();
  const QPoint oldPos = m_pos;

  if (m_isPanning) {  // Pan tasto centrale
    QPoint delta = m_pos - pos;
    if (o->isVerticalTimeline())
      delta.setY(0);
    else
      delta.setX(0);
    m_viewer->scroll(delta);
    return;
  }

  int col = m_viewer->xyToPosition(pos).layer();
  if (col < -1) col = 0;
  TXsheet *xsh       = m_viewer->getXsheet();
  TXshColumn *column = xsh->getColumn(col);
  QPoint mouseInCell = pos - m_viewer->positionToXY(CellPosition(0, col));
  int x = mouseInCell.x(), y = mouseInCell.y();

  if ((event->buttons() & Qt::LeftButton) != 0 && m_doOnMove != 0 && column) {
    switch (m_doOnMove) {
    case ToggleOnTransparency:
      column->setCamstandVisible(true);
      // sync eye button
      if (Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled())
        column->setPreviewVisible(true);
      break;
    case ToggleOffTransparency:
      column->setCamstandVisible(false);
      // sync eye button
      if (Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled())
        column->setPreviewVisible(false);
      break;
    case ToggleOnPreviewVisible:
      column->setPreviewVisible(true);
      break;
    case ToggleOffPreviewVisible:
      column->setPreviewVisible(false);
      break;
    case ToggleOnLock:
      column->lock(true);
      break;
    case ToggleOffLock:
      column->lock(false);
      break;
    }
  }

  if ((event->buttons() & Qt::LeftButton) != 0 &&
      !visibleRegion().contains(pos)) {
    QRect bounds = visibleRegion().boundingRect();
    if (o->isVerticalTimeline())
      m_viewer->setAutoPanSpeed(bounds, QPoint(pos.x(), bounds.top()));
    else
      m_viewer->setAutoPanSpeed(bounds, QPoint(bounds.left(), pos.y()));
  } else
    m_viewer->stopAutoPan();

  m_pos = pos;

  if (event->buttons() && getDragTool()) {
    m_viewer->dragToolDrag(event);
    update();
    return;
  }

  // Setto i toolTip
  TStageObjectId columnId = m_viewer->getObjectId(col);
  TStageObjectId parentId = xsh->getStageObjectParent(columnId);

  if (col < 0)
    m_tooltip = tr("Click to select camera");
  else if (o->rect(PredefinedRect::DRAG_LAYER).contains(mouseInCell)) {
    m_tooltip = tr("Click to select column, drag to move it");
  } else if (o->rect(PredefinedRect::LAYER_NUMBER).contains(mouseInCell)) {
    if (o->isVerticalTimeline())
      m_tooltip = tr("Click to select column, drag to move it");
    else
      m_tooltip = tr("Click to select column");
  } else if (o->rect(PredefinedRect::LAYER_NAME).contains(mouseInCell)) {
    if (o->isVerticalTimeline())
      m_tooltip =
          tr("Click to select column, drag to move it, double-click to edit");
    else if (column && column->getSoundColumn()) {
      // sound column
      if (o->rect(PredefinedRect::SOUND_ICON).contains(mouseInCell))
        m_tooltip = tr("Click to play the soundtrack back");
      else if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE) &&
               o->rect(PredefinedRect::VOLUME_AREA).contains(mouseInCell))
        m_tooltip = tr("Set the volume of the soundtrack");
    } else
      m_tooltip = tr("Click to select column, double-click to edit");
  } else if (o->rect(PredefinedRect::LOCK_AREA).contains(mouseInCell)) {
    m_tooltip = tr("Lock Toggle");
  } else if (o->rect(PredefinedRect::CONFIG_AREA).contains(mouseInCell)) {
    m_tooltip = tr("Additional column settings");
  } else if (o->rect(PredefinedRect::EYE_AREA).contains(mouseInCell)) {
    m_tooltip = tr("Preview Visibility Toggle");
  } else if (o->rect(PredefinedRect::PREVIEW_LAYER_AREA)
                 .contains(mouseInCell)) {
    m_tooltip = tr("Camera Stand Visibility Toggle");
  } else if (o->rect(PredefinedRect::PARENT_HANDLE_NAME)
                 .contains(mouseInCell)) {
    m_tooltip = tr("Click to select parent handle");
  } else if (o->rect(PredefinedRect::PEGBAR_NAME).contains(mouseInCell)) {
    m_tooltip = tr("Click to select parent object");
  } else {
    if (column && column->getSoundColumn()) {
      // sound column
      if (o->rect(PredefinedRect::SOUND_ICON).contains(mouseInCell))
        m_tooltip = tr("Click to play the soundtrack back");
      else if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE) &&
               o->rect(PredefinedRect::VOLUME_AREA).contains(mouseInCell))
        m_tooltip = tr("Set the volume of the soundtrack");
    } else if (Preferences::instance()->getColumnIconLoadingPolicy() ==
               Preferences::LoadOnDemand)
      m_tooltip = tr("Alt + Click to Toggle Thumbnail");
    else
      m_tooltip = tr("");
  }
  if (event->buttons()) {
    update();
  } else {
    auto bounds = [this, o](const QPoint& position) {
      const int column = m_viewer->xyToPosition(position).layer();
      return o
          ->rect(column < 0 ? PredefinedRect::CAMERA_LAYER_HEADER
                            : PredefinedRect::LAYER_HEADER)
          .translated(m_viewer->positionToXY(CellPosition(0, column)));
    };
    const QRect oldHeader = bounds(oldPos), newHeader = bounds(pos);
    auto hoverPart = [o](const QRect& header, const QPoint& point) {
      const QPoint local = point - header.topLeft();
      return (o->rect(PredefinedRect::DRAG_LAYER).contains(local) ? 1 : 0) |
             (o->rect(PredefinedRect::LAYER_NAME).contains(local) ? 2 : 0);
    };
    if (oldHeader != newHeader ||
        hoverPart(oldHeader, oldPos) != hoverPart(newHeader, pos))
      update(QRegion(oldHeader).united(newHeader));
  }
}

//-----------------------------------------------------------------------------
bool ColumnArea::event(QEvent *event) {
  if (event->type() == QEvent::ToolTip) {
    if (!m_tooltip.isEmpty())
      QToolTip::showText(mapToGlobal(m_pos), m_tooltip);
    else
      QToolTip::hideText();
  }
  return QWidget::event(event);
}

//-----------------------------------------------------------------------------
void ColumnArea::mouseReleaseEvent(QMouseEvent *event) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = m_viewer->getXsheet();

  if (m_doOnRelease != 0) {
    TXshColumn *column = xsh->getColumn(m_col);
    if (m_doOnRelease == OpenSettings) {
      QPoint pos = event->pos();
      int col    = m_viewer->xyToPosition(pos).layer();
      // Align popup to be below to CONFIG button
      QRect configRect = m_viewer->orientation()->rect(
          (col < 0) ? PredefinedRect::CAMERA_CONFIG_AREA
                    : PredefinedRect::CONFIG_AREA);
      CellPosition cellPosition(0, col);
      QPoint topLeft     = m_viewer->positionToXY(cellPosition);
      QPoint mouseInCell = pos - topLeft;
      int x              = configRect.left() - mouseInCell.x() +
              1;  // distance from right edge of CONFIG button
      int y =
          mouseInCell.y() -
          configRect.bottom();  // distance from bottum edge of CONFIG button

      if (col < 0) {
        openCameraColumnPopup(
            QPoint(event->globalPos().x() + x, event->globalPos().y() - y));
      } else if (column->getSoundColumn()) {
        if (!m_soundColumnPopup)
          m_soundColumnPopup = new SoundColumnPopup(this);

        m_soundColumnPopup->move(event->globalPos().x() + x,
                                 event->globalPos().y() - y);

        openSoundColumnPopup();
      } else {
        if (!m_columnTransparencyPopup)
          m_columnTransparencyPopup =
              new ColumnTransparencyPopup(m_viewer, this);

        m_columnTransparencyPopup->move(event->globalPos().x() + x,
                                        event->globalPos().y() - y);

        // make sure the popup doesn't go off the screen to the right
        QDesktopWidget *desktop = qApp->desktop();
        QRect screenRect        = desktop->screenGeometry(app->getMainWindow());

        int popupLeft  = event->globalPos().x() + x;
        int popupRight = popupLeft + m_columnTransparencyPopup->width();

        // first condition checks if popup is on same monitor as main app;
        // if popup is on different monitor, leave as is
        if (popupLeft < screenRect.right() && popupRight > screenRect.right()) {
          int distance = popupRight - screenRect.right();
          m_columnTransparencyPopup->move(
              m_columnTransparencyPopup->x() - distance,
              m_columnTransparencyPopup->y());
        }

        openTransparencyPopup();
      }
    } else if (m_doOnRelease == ToggleAllPreviewVisible ||
               m_doOnRelease == ToggleAllTransparency ||
               m_doOnRelease == ToggleAllLock) {
      const auto toggle = m_doOnRelease == ToggleAllLock
                              ? XsheetEngine::ColumnToggle::Lock
                          : m_doOnRelease == ToggleAllTransparency
                              ? XsheetEngine::ColumnToggle::Camstand
                              : XsheetEngine::ColumnToggle::Preview;
      const bool lock   = toggle == XsheetEngine::ColumnToggle::Lock;
      const auto result = XsheetEngine::toggleColumns(
          xsh, toggle,
          lock && Preferences::instance()->isXsheetCameraColumnVisible(), !lock,
          Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled());
      if (result.soundChanged)
        app->getCurrentXsheet()->notifyXsheetSoundChanged();
    } else
      assert(false);
  }
  if (m_doOnRelease != 0 || m_doOnMove != 0) {
    app->getCurrentScene()->notifySceneChanged();
    // signal XsheetChanged will invoke PreviewFxManager to all rendered frames,
    // if necessary. it causes slowness when opening preview flipbook of large
    // scene.
    // KNOWN BUG: Side effect, transparency doesn't sync in schematic if false.
    bool isTransparencyRendered =
        app->getCurrentScene()
            ->getScene()
            ->getProperties()
            ->isColumnColorFilterOnRenderEnabled() ||
        Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled();
    bool isStateChanged = m_doOnMove == ToggleOnPreviewVisible ||
                          m_doOnMove == ToggleOffPreviewVisible ||
                          m_doOnRelease == ToggleAllPreviewVisible ||
                          m_doOnMove == ToggleOnLock ||
                          m_doOnMove == ToggleOffLock ||
                          m_doOnRelease == ToggleAllLock;
    if (isStateChanged ||
        (isTransparencyRendered && (m_doOnMove == ToggleOnTransparency ||
                                    m_doOnMove == ToggleOffTransparency ||
                                    m_doOnRelease == ToggleAllTransparency ||
                                    m_doOnRelease == OpenSettings))) {
      app->getCurrentXsheet()->notifyXsheetChanged();
    }
    update();
    m_doOnRelease = 0;
    m_doOnMove    = 0;
  }

  if (m_transparencyPopupTimer) m_transparencyPopupTimer->stop();

  m_viewer->setQtModifiers(Qt::KeyboardModifiers());
  m_viewer->dragToolRelease(event);
  m_isPanning = false;
  m_viewer->stopAutoPan();
}

//-----------------------------------------------------------------------------
void ColumnArea::mouseDoubleClickEvent(QMouseEvent *event) {
  const Orientation *o = m_viewer->orientation();

  QPoint pos = event->pos();
  int col    = m_viewer->xyToPosition(pos).layer();
  CellPosition cellPosition(0, col);
  QPoint topLeft     = m_viewer->positionToXY(cellPosition);
  QPoint mouseInCell = pos - topLeft;

  QRect nameRect = o->rect((col < 0) ? PredefinedRect::CAMERA_LAYER_NAME
                                     : PredefinedRect::LAYER_NAME);
  if (!nameRect.contains(mouseInCell)) return;

  TXsheet *xsh = m_viewer->getXsheet();
  // enable to rename empty column when the column name is linked to the level
  if (!Preferences::instance()->isLinkColumnNameWithLevelEnabled() &&
      col >= 0 && xsh->isColumnEmpty(col))
    return;

  QPoint fieldPos =
      (col < 0 && o->isVerticalTimeline()) ? nameRect.topLeft() : topLeft;
  QRect renameRect =
      o->rect(PredefinedRect::RENAME_COLUMN).translated(fieldPos);
  m_renameColumnField->show(renameRect, col);
}

//-----------------------------------------------------------------------------
void ColumnArea::contextMenuEvent(QContextMenuEvent *event) {
#ifndef _WIN32
  /* On windows the widget receive the release event before the menu
     is shown, on linux and osx the release event is lost, never
     received by the widget */
  QMouseEvent fakeRelease(QEvent::MouseButtonRelease, event->pos(),
                          Qt::RightButton, Qt::NoButton, Qt::NoModifier);

  QApplication::instance()->sendEvent(this, &fakeRelease);
#endif
  const Orientation *o = m_viewer->orientation();

  int col = m_viewer->xyToPosition(event->pos()).layer();

  bool isCamera = col < 0;

  m_viewer->setCurrentColumn(col);
  TXsheet *xsh       = m_viewer->getXsheet();
  QPoint topLeft     = m_viewer->positionToXY(CellPosition(0, col));
  QPoint mouseInCell = event->pos() - topLeft;

  QMenu menu(this);
  CommandManager *cmdManager = CommandManager::instance();

  //---- Unified
  if (((isCamera && !o->isVerticalTimeline()) || !xsh->isColumnEmpty(col)) &&
      o->rect(PredefinedRect::UNIFIEDVIEW_LAYER_AREA).contains(mouseInCell) &&
      Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled()) {
    menu.setObjectName("xsheetColumnAreaMenu_Preview");

    menu.addAction(cmdManager->getAction("MI_EnableThisColumnOnly"));
    menu.addAction(cmdManager->getAction("MI_EnableSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_EnableAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DisableAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DisableSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_SwapEnabledColumns"));
  }
  //---- Preview
  else if (((isCamera && !o->isVerticalTimeline()) ||
            !xsh->isColumnEmpty(col)) &&
           o->rect(PredefinedRect::EYE_AREA).contains(mouseInCell) &&
           !Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled()) {
    menu.setObjectName("xsheetColumnAreaMenu_Preview");

    menu.addAction(cmdManager->getAction("MI_EnableThisColumnOnly"));
    menu.addAction(cmdManager->getAction("MI_EnableSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_EnableAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DisableAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DisableSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_SwapEnabledColumns"));
  }
  //---- Camstand
  else if (((isCamera && !o->isVerticalTimeline()) ||
            !xsh->isColumnEmpty(col)) &&
           o->rect(PredefinedRect::PREVIEW_LAYER_AREA).contains(mouseInCell) &&
           !Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled()) {
    menu.setObjectName("xsheetColumnAreaMenu_Camstand");

    menu.addAction(cmdManager->getAction("MI_ActivateThisColumnOnly"));
    menu.addAction(cmdManager->getAction("MI_ActivateSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_ActivateAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DeactivateAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DeactivateSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_ToggleColumnsActivation"));
    // hide all columns placed on the left
    menu.addAction(cmdManager->getAction("MI_DeactivateUpperColumns"));
  }
  //---- Lock
  else if ((isCamera || !xsh->isColumnEmpty(col)) &&
           o->rect((isCamera) ? PredefinedRect::CAMERA_LOCK_AREA
                              : PredefinedRect::LOCK_AREA)
               .contains(mouseInCell)) {
    menu.setObjectName("xsheetColumnAreaMenu_Lock");

    menu.addAction(cmdManager->getAction("MI_LockThisColumnOnly"));
    menu.addAction(cmdManager->getAction("MI_LockSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_LockAllColumns"));
    menu.addAction(cmdManager->getAction("MI_UnlockSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_UnlockAllColumns"));
    menu.addAction(cmdManager->getAction("MI_ToggleColumnLocks"));
  }
  // right clicking another area / right clicking empty column head
  else {
    if (!isCamera) {
      int r0, r1;
      xsh->getCellRange(col, r0, r1);
      TXshCell cell = xsh->getCell(r0, col);
      menu.addAction(cmdManager->getAction(MI_Cut));
      menu.addAction(cmdManager->getAction(MI_Copy));
      menu.addAction(cmdManager->getAction(MI_Paste));
      menu.addAction(cmdManager->getAction(MI_PasteAbove));
      menu.addAction(cmdManager->getAction(MI_Clear));
      menu.addAction(cmdManager->getAction(MI_Insert));
      menu.addAction(cmdManager->getAction(MI_InsertAbove));
      menu.addSeparator();
      menu.addAction(cmdManager->getAction(MI_InsertFx));
      menu.addAction(cmdManager->getAction(MI_NewNoteLevel));
      menu.addAction(cmdManager->getAction(MI_RemoveEmptyColumns));
      menu.addSeparator();
      if (m_viewer->getXsheet()->isColumnEmpty(col) ||
          (cell.m_level && cell.m_level->getChildLevel()))
        menu.addAction(cmdManager->getAction(MI_OpenChild));

      // Close sub xsheet and move to parent sheet
      ToonzScene *scene      = TApp::instance()->getCurrentScene()->getScene();
      ChildStack *childStack = scene->getChildStack();
      if (childStack && childStack->getAncestorCount() > 0) {
        menu.addAction(cmdManager->getAction(MI_CloseChild));
      }

      menu.addAction(cmdManager->getAction(MI_Collapse));
      if (cell.m_level && cell.m_level->getChildLevel()) {
        menu.addAction(cmdManager->getAction(MI_Resequence));
        menu.addAction(cmdManager->getAction(MI_CloneChild));
        menu.addAction(cmdManager->getAction(MI_ExplodeChild));
      }
      menu.addSeparator();
    }
    menu.addAction(cmdManager->getAction(MI_FoldColumns));
    if (Preferences::instance()->isShowKeyframesOnXsheetCellAreaEnabled()) {
      QAction *cameraToggle =
          cmdManager->getAction(MI_ToggleXsheetCameraColumn);
      bool cameraVisible =
          Preferences::instance()->isXsheetCameraColumnVisible();
      if (cameraVisible)
        cameraToggle->setText(tr("Hide Camera Column"));
      else
        cameraToggle->setText(tr("Show Camera Column"));
      menu.addAction(cameraToggle);
    }
    menu.addSeparator();
    menu.addAction(cmdManager->getAction(MI_ToggleXSheetToolbar));
    menu.addAction(cmdManager->getAction(MI_ToggleXsheetBreadcrumbs));

    if (isCamera) {
      menu.exec(event->globalPos());
      return;
    }

    // force the selected cells placed in n-steps
    if (!xsh->isColumnEmpty(col)) {
      menu.addSeparator();
      QMenu *reframeSubMenu = new QMenu(tr("Reframe"), this);
      {
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe1));
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe2));
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe3));
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe4));
        reframeSubMenu->addAction(
            cmdManager->getAction(MI_ReframeWithEmptyInbetweens));
      }
      menu.addMenu(reframeSubMenu);
      menu.addAction(cmdManager->getAction(MI_AutoInputCellNumber));
    }

    if (containsRasterLevel(m_viewer->getColumnSelection())) {
      QMenu *subsampleSubMenu = new QMenu(tr("Subsampling"), this);
      {
        subsampleSubMenu->addAction(m_subsampling1);
        subsampleSubMenu->addAction(m_subsampling2);
        subsampleSubMenu->addAction(m_subsampling3);
        subsampleSubMenu->addAction(m_subsampling4);
      }
      menu.addMenu(subsampleSubMenu);
    }

    if (!xsh->isColumnEmpty(col)) {
      menu.addAction(cmdManager->getAction(MI_ReplaceLevel));
      menu.addAction(cmdManager->getAction(MI_ReplaceParentDirectory));
    }

    if (o->isVerticalTimeline()) {
      menu.addSeparator();
      QAction *showParentColors =
          new QAction(tr("Show Column Parent Colors"), this);
      showParentColors->setCheckable(true);
      showParentColors->setChecked(
          Preferences::instance()->isParentColorsInXsheetColumnEnabled());
      showParentColors->setToolTip(
          tr("Show the column parent's color in the Xsheet"));

      connect(showParentColors, &QAction::toggled, [=]() {
        Preferences::instance()->setValue(
            parentColorsInXsheetColumn,
            showParentColors->isChecked() ? true : false);
      });
      menu.addAction(showParentColors);
   
    }
  }

  QAction *act  = cmdManager->getAction(MI_Insert),
          *act2 = cmdManager->getAction(MI_InsertAbove),
          *act3 = cmdManager->getAction(MI_Paste),
          *act4 = cmdManager->getAction(MI_PasteAbove);

  QString actText = act->text(), act2Text = act2->text(),
          act3Text = act3->text(), act4Text = act4->text();

  if (o->isVerticalTimeline()) {
    act->setText(tr("&Insert Before"));
    act2->setText(tr("&Insert After"));
    act3->setText(tr("&Paste Insert Before"));
    act4->setText(tr("&Paste Insert After"));
  } else {
    act->setText(tr("&Insert Below"));
    act2->setText(tr("&Insert Above"));
    act3->setText(tr("&Paste Insert Below"));
    act4->setText(tr("&Paste Insert Above"));
  }

  menu.exec(event->globalPos());

  act->setText(actText);
  act2->setText(act2Text);
  act3->setText(act3Text);
  act4->setText(act4Text);
}

//-----------------------------------------------------------------------------
void ColumnArea::onSetMask(int col) {
  TXshColumn *column = m_viewer->getXsheet()->getColumn(m_col);

  std::string name = m_viewer->getXsheet()
                         ->getStageObject(TStageObjectId::ColumnId(col))
                         ->getName();
  ColumnMaskUndo *undo = new ColumnMaskUndo(col, column->isMask(), name);
  undo->redo();
  TUndoManager::manager()->add(undo);
  update();
}

//-----------------------------------------------------------------------------
void ColumnArea::onSubSampling(QAction *action) {
  int subsampling;
  if (action == m_subsampling1)
    subsampling = 1;
  else if (action == m_subsampling2)
    subsampling = 2;
  else if (action == m_subsampling3)
    subsampling = 3;
  else if (action == m_subsampling4)
    subsampling = 4;

  TColumnSelection *selection = m_viewer->getColumnSelection();
  TXsheet *xsh                = m_viewer->getXsheet();
  assert(selection && xsh);
  const std::set<int> indexes = selection->getIndices();
  for (auto const &e : indexes) {
    if (e < 0) continue;  // Ignore camera column
    TXshColumn *column          = xsh->getColumn(e);
    TXshColumn::ColumnType type = column->getColumnType();
    if (type != TXshColumn::eLevelType) continue;
    const QSet<TXshLevel *> levels = getLevels(column);
    QSet<TXshLevel *>::const_iterator it2;
    for (it2 = levels.begin(); it2 != levels.end(); it2++) {
      TXshSimpleLevel *sl = (*it2)->getSimpleLevel();
      if (!sl || sl->getProperties()->getDirtyFlag()) continue;
      int type = sl->getType();
      if (type == TZI_XSHLEVEL || type == TZP_XSHLEVEL ||
          type == OVL_XSHLEVEL) {
        sl->getProperties()->setSubsampling(subsampling);
        sl->invalidateFrames();
      }
    }
  }
  TApp::instance()
      ->getCurrentXsheet()
      ->getXsheet()
      ->getStageObjectTree()
      ->invalidateAll();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}
}  // namespace XsheetGUI
