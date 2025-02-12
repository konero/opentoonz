// Toonz
#include "dragcommandhandler.h"
#include "menubarcommandids.h"

// Qt
#include <QKeyEvent>
#include <QDebug>

// ToonzQt
#include "toonzqt/menubarcommand.h"

DragCommandHandler::DragCommandHandler(QObject* parent)
    : QObject(parent)
    , m_isDragging(false)
    , m_accumulatedDelta(0)
    , m_activeMapping(nullptr) {
  initializeDefaultMappings();
  qDebug() << "DragCommandHandler initialized";
}

DragCommandHandler::~DragCommandHandler() {}

void DragCommandHandler::initializeDefaultMappings() {
  CommandMapping frameNav;
  frameNav.triggerKey                   = Qt::Key_NumberSign;
  frameNav.leftCommand                  = "MI_PrevFrame";
  frameNav.rightCommand                 = "MI_NextFrame";
  frameNav.dragThreshold                = 20;  // Reduced from 50
  frameNav.repeatThreshold              = 15;  // Reduced from 30
  m_commandMappings[Qt::Key_NumberSign] = frameNav;
  qDebug() << "Default mappings initialized with # key for frame navigation";
}

void DragCommandHandler::handleKeyPress(QKeyEvent* event) {
  qDebug() << "Key press event:" << event->key()
           << "Modifiers:" << event->modifiers();

  auto it = m_commandMappings.find((Qt::Key)event->key());
  if (it != m_commandMappings.end()) {
    m_activeMapping    = &it.value();
    m_isDragging       = false;
    m_startPos         = QCursor::pos();
    m_lastPos          = m_startPos;
    m_accumulatedDelta = 0;
    qDebug() << "Drag mode activated with key:" << event->key();
    event->accept();
  }
}

void DragCommandHandler::handleKeyRelease(QKeyEvent* event) {
  qDebug() << "Key release event:" << event->key();

  if (m_activeMapping && event->key() == m_activeMapping->triggerKey) {
    qDebug() << "Drag mode deactivated";
    m_activeMapping = nullptr;
    m_isDragging    = false;
    event->accept();
  }
}

void DragCommandHandler::handleMouseMove(const QPoint& globalPos) {
  if (!m_activeMapping) return;

  if (!m_isDragging) {
    int delta = globalPos.x() - m_startPos.x();
    qDebug() << "Pre-drag delta:" << delta
             << "Threshold:" << m_activeMapping->dragThreshold;

    if (abs(delta) >= m_activeMapping->dragThreshold) {
      m_isDragging = true;
      m_lastPos    = globalPos;
      qDebug() << "Drag initiated, executing command";
      executeCommand(delta > 0);
    }
  } else {
    int delta = globalPos.x() - m_lastPos.x();
    m_accumulatedDelta += delta;
    qDebug() << "Dragging - Accumulated delta:" << m_accumulatedDelta
             << "Repeat threshold:" << m_activeMapping->repeatThreshold;

    if (abs(m_accumulatedDelta) >= m_activeMapping->repeatThreshold) {
      qDebug() << "Executing repeat command";
      executeCommand(m_accumulatedDelta > 0);
      m_accumulatedDelta = 0;
      m_lastPos          = globalPos;
    }
  }
}

void DragCommandHandler::executeCommand(bool rightDirection) {
  if (!m_activeMapping) return;

  const char* command = rightDirection ? m_activeMapping->rightCommand
                                       : m_activeMapping->leftCommand;

  qDebug() << "Executing command:" << command
           << (rightDirection ? "(right)" : "(left)");
  CommandManager::instance()->execute(command);
}

bool DragCommandHandler::isDragging() const { return m_isDragging; }

void DragCommandHandler::addCommandMapping(const CommandMapping& mapping) {
  m_commandMappings[mapping.triggerKey] = mapping;
  qDebug() << "Added new command mapping for key:" << mapping.triggerKey;
}