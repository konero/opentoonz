#include "../xsheetengine.h"
#include "../sheetviewstate.h"
#include "../xsheetviewgeometry.h"
#include "../timelineviewgeometry.h"
#include "../columnheaderpainter.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonzqt/gutil.h"
#include "toonzqt/spreadsheetviewer.h"
#include "tstream.h"
#include "tconvert.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QScrollBar>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>

namespace {
int checks = 0;
void require(bool condition, const char* message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}

void frameScrolling() {
  QScrollArea xsheet, functions, secondFunctions, timeline;
  Spreadsheet::FrameScroller sheetScroller, functionScroller, secondScroller;
  int sheetZoom = 50, functionZoom = 100;
  auto attach = [](Spreadsheet::FrameScroller& scroller, QScrollArea& area,
                   const int& zoom) {
    area.verticalScrollBar()->setRange(0, 1000);
    area.horizontalScrollBar()->setRange(0, 1000);
    scroller.setFrameScrollArea(&area);
    QObject::connect(&scroller, &Spreadsheet::FrameScroller::zoomScrollAdjust,
                     &area, [&zoom](QPointF& offset, bool toZoom) {
                       offset.setY(toZoom ? offset.y() * zoom / 100.0
                                          : offset.y() * 100.0 / zoom);
                     });
    scroller.registerFrameScroller();
  };
  attach(sheetScroller, xsheet, sheetZoom);
  attach(functionScroller, functions, functionZoom);
  attach(secondScroller, secondFunctions, functionZoom);
  auto sheetBar    = xsheet.verticalScrollBar();
  auto functionBar = functions.verticalScrollBar();
  auto secondBar   = secondFunctions.verticalScrollBar();
  sheetBar->setValue(100);
  require(functionBar->value() == 200 && secondBar->value() == 200,
          "scroll reaches every peer once, even at different zooms");
  require(sheetBar->value() == 100, "scroll does not echo to sender");
  functionBar->setValue(240);
  require(sheetBar->value() == 120 && secondBar->value() == 240,
          "Function Editor scrollbar scrolls XSheet in frame units");
  sheetBar->setValue(80);
  require(functionBar->value() == 160 && secondBar->value() == 160,
          "reverse scrolling preserves frame alignment");
  xsheet.horizontalScrollBar()->setValue(50);
  require(functionBar->value() == 160 &&
              functions.horizontalScrollBar()->value() == 0,
          "column scrolling stays local");
  timeline.horizontalScrollBar()->setRange(0, 1000);
  timeline.horizontalScrollBar()->setValue(300);
  timeline.verticalScrollBar()->setRange(0, 1000);
  timeline.verticalScrollBar()->setValue(300);
  require(sheetBar->value() == 80 && functionBar->value() == 160,
          "Timeline scrollbars do not participate");

  // Model the local scrollbar movement while zooming about the current frame.
  sheetScroller.setSyncing(true);
  sheetZoom = 100;
  sheetBar->setValue(160);
  sheetScroller.setSyncing(false);
  require(functionBar->value() == 160, "zoom anchoring does not scroll peers");
  sheetBar->setValue(180);
  require(functionBar->value() == 180 && secondBar->value() == 180,
          "equal scale and range align absolute scrollbar positions");
  functionBar->setMaximum(2000);
  sheetScroller.setSyncing(true);
  sheetBar->setValue(200);
  sheetScroller.setSyncing(false);
  sheetBar->setValue(210);
  require(functionBar->value() == 190,
          "suppressed zoom movement updates the next scroll baseline");
  functionBar->setValue(220);
  require(sheetBar->value() == 240 && secondBar->value() == 240,
          "unequal scrollbar ranges synchronize without recursive echoes");
  functionScroller.unregisterFrameScroller();
  sheetBar->setValue(260);
  require(functionBar->value() == 220,
          "hidden Function Editor stops receiving");
  functionBar->setValue(300);
  require(sheetBar->value() == 260, "hidden Function Editor stops sending");
  functionScroller.registerFrameScroller();
  functionBar->setValue(310);
  require(sheetBar->value() == 270, "reopened editor uses current baseline");
  {
    QScrollArea temporary;
    Spreadsheet::FrameScroller temporaryScroller;
    attach(temporaryScroller, temporary, sheetZoom);
  }
  sheetBar->setValue(280);
  require(functionBar->value() == 320,
          "destroyed editor unregisters without a hide event");
}

void geometry() {
  ColumnFan vertical, horizontal;
  vertical.setDimensions(74, 22);
  horizontal.setDimensions(24, 24);
  vertical.setCameraVisible(true);
  horizontal.setCameraVisible(true);
  for (int zoom : {20, 37, 53, 77, 100}) {
    for (int layer = -1; layer < 8; ++layer) {
      for (int frame : {0, 1, 2, 19, 10000}) {
        const CellPosition cell(frame, layer);
        const QPoint v = XsheetViewGeometry::pointAt(cell, vertical, 20, zoom);
        const auto vc  = XsheetViewGeometry::cellAt(v, vertical, 20, zoom);
        require(vc.frame() == frame && vc.layer() == layer,
                "vertical top-left boundary");
        const QPoint h =
            TimelineViewGeometry::pointAt(cell, horizontal, 50, 24, 9, 8, zoom);
        const auto hc =
            TimelineViewGeometry::cellAt(h, horizontal, 50, 24, 8, zoom);
        require(hc.frame() == frame && hc.layer() == layer,
                "timeline top-left boundary");
        const QPoint next = TimelineViewGeometry::pointAt(
            CellPosition(frame + 1, layer), horizontal, 50, 24, 9, 8, zoom);
        const auto end = TimelineViewGeometry::cellAt(
            QPoint(next.x() - 1, h.y() + 23), horizontal, 50, 24, 8, zoom);
        require(end.frame() == frame && end.layer() == layer,
                "timeline bottom-right boundary");
      }
    }
  }
  horizontal.deactivate(2);
  horizontal.deactivate(3);
  const QPoint folded = TimelineViewGeometry::pointAt(
      CellPosition(0, 3), horizontal, 50, 24, 9, 8, 100);
  require(TimelineViewGeometry::cellAt(folded, horizontal, 50, 24, 8, 100)
                  .layer() == 3,
          "folded group boundary");
  horizontal.setCameraVisible(false);
  require(TimelineViewGeometry::cellAt(QPoint(0, 0), horizontal, 50, 24, 8, 100)
                  .layer() == 7,
          "hidden camera preserves top layer");
}

void folding(const QString& directory) {
  TXsheetP sheet = new TXsheet;
  for (int i = 0; i < 5; ++i) sheet->insertColumn(i);
  ColumnFoldState initial;
  initial.setFolded(1, true);
  sheet->setColumnFoldState(initial);

  int firstUpdates  = 0;
  int secondUpdates = 0;
  SheetViewState first(sheet.getPointer(), 74, 22, [&] { ++firstUpdates; }),
      second(sheet.getPointer(), 24, 24, [&] { ++secondUpdates; });
  require(!first.columns()->isActive(1) && !second.columns()->isActive(1),
          "shared document folding seeds both panel projections");
  require(first.columns()->colToLayerAxis(1) !=
              second.columns()->colToLayerAxis(1),
          "panel projections retain orientation-specific dimensions");

  ColumnFoldState folded = sheet->getColumnFoldState();
  folded.setFolded(1, false);
  folded.setFolded(3, true);
  sheet->setColumnFoldState(folded);
  require(first.columns()->isActive(1) && second.columns()->isActive(1) &&
              !first.columns()->isActive(3) && !second.columns()->isActive(3),
          "fold changes update both panel projections");
  require(first.columns()->colToLayerAxis(2) == 148 &&
              second.columns()->colToLayerAxis(2) == 48,
          "fold changes update each panel's geometry projection");
  require(firstUpdates == 1 && secondUpdates == 1,
          "fold changes invalidate both panel projections");

  sheet->insertColumn(2);
  require(!first.columns()->isActive(4) && !second.columns()->isActive(4),
          "insert shifts shared folding");
  sheet->moveColumn(4, 0);
  require(!first.columns()->isActive(0) && !second.columns()->isActive(0) &&
              first.columns()->isActive(4) && second.columns()->isActive(4),
          "move shifts shared folding");
  sheet->removeColumn(0);
  require(first.columns()->isActive(0) && second.columns()->isActive(0),
          "remove drops the shared fold for the removed column");

  ColumnFoldState saved;
  saved.setFolded(-1, true);
  saved.setFolded(1, true);
  saved.setFolded(2, true);
  saved.setFolded(8, true);
  const TFilePath path((directory + "/folding.tnz").toStdWString());
  {
    TOStream stream(path);
    stream.openChild("columnFan");
    saved.saveData(stream);
    stream.closeChild();
  }
  ColumnFoldState loaded;
  TIStream stream(path);
  std::string tag;
  require(stream.matchTag(tag) && tag == "columnFan", "legacy folding tag");
  loaded.loadData(stream);
  require(loaded.isFolded(1) && loaded.isFolded(2) && loaded.isFolded(8) &&
              !loaded.isFolded(3),
          "legacy folding range round-trip");
  require(!loaded.isFolded(-1), "camera folding remains in camera metadata");
}

void queriesAndEdits() {
  TXsheetP sheet     = new TXsheet;
  TXshSimpleLevelP a = new TXshSimpleLevel(L"a");
  TXshSimpleLevelP b = new TXshSimpleLevel(L"b");
  a->setType(PLI_XSHLEVEL);
  b->setType(PLI_XSHLEVEL);
  sheet->setCell(0, 0, TXshCell(a.getPointer(), TFrameId(1)));
  sheet->setCell(9999, 0, TXshCell(a.getPointer(), TFrameId(2)));
  XsheetEngine engine;
  require(engine.singleLevel(sheet.getPointer(), 0) == a.getPointer(),
          "single-level summary");
  const auto header = engine.columnHeader(sheet.getPointer(), 0, false, true);
  require(!header.empty && header.name == "a",
          "header snapshot resolves level display name");
  sheet->setCell(5000, 0, TXshCell(b.getPointer(), TFrameId(1)));
  engine.invalidate();
  require(engine.singleLevel(sheet.getPointer(), 0) == nullptr,
          "summary invalidates after edit");
  sheet->clearCells(5000, 0, 1);
  engine.invalidate();
  require(engine.singleLevel(sheet.getPointer(), 0) == a.getPointer(),
          "summary recovers after undo-like edit");

  TXshPaletteLevelP palette = new TXshPaletteLevel(L"palette");
  sheet->setCell(0, 1, TXshCell(palette.getPointer(), TFrameId(1)));
  auto* normal  = sheet->getColumn(0);
  auto* special = sheet->getColumn(1);
  normal->setPreviewVisible(true);
  special->setPreviewVisible(true);
  const auto result = XsheetEngine::toggleColumns(
      sheet.getPointer(), XsheetEngine::ColumnToggle::Preview, false, true,
      false);
  require(result.changed && !normal->isPreviewVisible(),
          "bulk preview changes normal columns");
  require(special->isPreviewVisible(),
          "header bulk preview excludes palette columns");
  normal->setCamstandVisible(true);
  normal->setPreviewVisible(true);
  XsheetEngine::toggleColumns(sheet.getPointer(),
                              XsheetEngine::ColumnToggle::Camstand, false, true,
                              true);
  require(!normal->isCamstandVisible() && !normal->isPreviewVisible(),
          "unified visibility stays synchronized");
  sheet->insertColumn(2);
  sheet->getColumn(2)->lock(false);
  XsheetEngine::toggleColumns(sheet.getPointer(),
                              XsheetEngine::ColumnToggle::Lock, false, false,
                              false);
  require(!sheet->getColumn(2)->isLocked(),
          "header bulk lock excludes empty columns");
  XsheetEngine::toggleColumns(sheet.getPointer(),
                              XsheetEngine::ColumnToggle::Lock, true, false,
                              false, XsheetEngine::ColumnTargets::Existing);
  require(sheet->getColumn(2)->isLocked(),
          "layer-panel bulk lock preserves existing empty-column policy");
  TXsheetP other = new TXsheet;
  other->setCell(0, 0, TXshCell(b.getPointer(), TFrameId(1)));
  require(engine.singleLevel(other.getPointer(), 0) == b.getPointer(),
          "sheet switch invalidates summary");
}

void waveform() {
  TXshSoundLevelP level = new TXshSoundLevel;
  level->setFrameRate(25);
  TSoundTrackP track = TSoundTrack::create(48000, 16, 1, 4800, TSound::INT);
  auto* samples =
      reinterpret_cast<short*>(const_cast<UCHAR*>(track->getRawData()));
  for (int i = 0; i < track->getSampleCount(); ++i)
    samples[i] = i % 2 ? 1000 : -1000;
  level->setSoundTrack(track);
  for (int resolution : {20, 50, 25, 60}) {
    DoublePair value;
    level->getValueAtPixel(resolution, 0, value);
    require(
        std::abs(value.first + 1) < 1e-8 && std::abs(value.second - 1) < 1e-8,
        "resolution-independent normalized envelope");
    level->getValueAtPixel(resolution, 100000, value);
    require(value.first == 0 && value.second == 0,
            "out-of-range waveform resets output");
  }
  for (int pixel : {0, 255, 256, 499}) {
    DoublePair value;
    level->getValueAtPixel(200, pixel, value);
    require(
        std::abs(value.first + 1) < 1e-8 && std::abs(value.second - 1) < 1e-8,
        "time-block boundary keeps the expected sample interval");
  }
  DoublePair tail(1, 1);
  level->getValueAtPixel(200, 500, tail);
  require(tail.first == 0 && tail.second == 0,
          "partial final frame has no repeated tail sample");
  std::atomic<bool> consistent{true};
  std::vector<std::thread> readers;
  for (int reader = 0; reader < 4; ++reader)
    readers.emplace_back([&, reader] {
      for (int i = 0; i < 50; ++i) {
        DoublePair value;
        level->getValueAtPixel(20 + reader * 50 + i, 0, value);
        if (std::abs(value.first + 1) > 1e-8 ||
            std::abs(value.second - 1) > 1e-8)
          consistent = false;
      }
    });
  for (auto& reader : readers) reader.join();
  require(consistent, "concurrent envelope readers survive cache eviction");
  level->setFrameRate(50);
  require(level->getFrameSoundCount() == 5,
          "frame-rate change invalidates envelope");
  for (int i = 0; i < track->getSampleCount(); ++i) samples[i] = 0;
  level->setSoundTrack(track);
  DoublePair value(1, 1);
  level->getValueAtPixel(20, 0, value);
  require(value.first == 0 && value.second == 0,
          "silent replacement invalidates envelope");
}

void resources(const QString& directory) {
  const QString path = directory + "/button.svg";
  QFile file(path);
  require(file.open(QIODevice::WriteOnly), "create SVG fixture");
  file.write(
      "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" "
      "height=\"16\"><rect x=\"2\" y=\"2\" width=\"12\" height=\"12\" "
      "fill=\"red\"/></svg>");
  file.close();
  XsheetGUI::ColumnHeaderResources cache;
  const QPixmap first  = cache.svg(path, QSize(16, 16), Qt::transparent, 1);
  const QPixmap second = cache.svg(path, QSize(16, 16), Qt::transparent, 1);
  require(!first.isNull() && first.cacheKey() == second.cacheKey(),
          "warm SVG reuses rasterization");
  require(first.toImage() ==
              svgToPixmap(path, QSize(16, 16), Qt::KeepAspectRatio).toImage(),
          "cached SVG preserves existing pixels");
  const QPixmap highDpi = cache.svg(path, QSize(16, 16), Qt::transparent, 2);
  require(highDpi.size() == QSize(32, 32) && highDpi.devicePixelRatioF() == 2,
          "DPR participates in cache identity");
  require(cache.svg(path, QSize(16, 16), Qt::white, 1).cacheKey() !=
              first.cacheKey(),
          "background participates in cache identity");
  cache.clear();
  require(cache.svg(path, QSize(16, 16), Qt::transparent, 1).cacheKey() !=
              first.cacheKey(),
          "theme invalidation drops old resources");
  QElapsedTimer timer;
  timer.start();
  for (int i = 0; i < 500; ++i)
    svgToPixmap(path, QSize(16, 16), Qt::KeepAspectRatio);
  const qint64 uncached = timer.nsecsElapsed();
  timer.restart();
  for (int i = 0; i < 500; ++i)
    cache.svg(path, QSize(16, 16), Qt::transparent, 1);
  std::cout << "SVG microbenchmark (500 requests), uncached ns=" << uncached
            << ", cached ns=" << timer.nsecsElapsed() << '\n';
}
}  // namespace

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QTemporaryDir directory;
  try {
    require(directory.isValid(), "temporary fixture directory");
    geometry();
    frameScrolling();
    folding(directory.path());
    queriesAndEdits();
    waveform();
    resources(directory.path());
    std::cout << checks << " checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAILED: " << error.what() << '\n';
  } catch (const TException& error) {
    std::cerr << "FAILED: " << to_string(error.getMessage()) << '\n';
  }
  return 1;
}
