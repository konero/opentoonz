#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QPoint>

class QKeyEvent;

class DragCommandHandler : public QObject {
  Q_OBJECT

public:
  struct CommandMapping {
    Qt::Key triggerKey;
    const char* leftCommand;
    const char* rightCommand;
    int dragThreshold;
    int repeatThreshold;
  };

  DragCommandHandler(QObject* parent = nullptr);
  ~DragCommandHandler();

  void handleKeyPress(QKeyEvent* event);
  void handleKeyRelease(QKeyEvent* event);
  void handleMouseMove(const QPoint& globalPos);

  bool isDragging() const;
  void addCommandMapping(const CommandMapping& mapping);

private:
  void executeCommand(bool rightDirection);
  void initializeDefaultMappings();

private:
  bool m_isDragging;
  QPoint m_startPos;
  QPoint m_lastPos;
  int m_accumulatedDelta;
  CommandMapping* m_activeMapping;
  QHash<Qt::Key, CommandMapping> m_commandMappings;
};
