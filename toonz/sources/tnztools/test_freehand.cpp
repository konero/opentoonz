#include <tools/modifiers/modifierfreehand.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <vector>

namespace {

void fillTrack(TTrack &track, const std::vector<TPointD> &positions,
               bool final = false) {
  for (int i = 0; i < (int)positions.size(); ++i)
    track.push_back(TTrackPoint(positions[i], 0.15 + 0.1 * (i % 7),
                                TPointD(0.1 * i, -0.1 * i), i, i * 100.0,
                                0.0, final && i + 1 == (int)positions.size()),
                    true);
}

void assertFinite(const TPointD &p) {
  assert(std::isfinite(p.x));
  assert(std::isfinite(p.y));
}

void testCircle(int count) {
  constexpr double pi = 3.14159265358979323846;
  std::vector<TPointD> points;
  for (int i = 0; i < count; ++i) {
    double angle = 2.0 * pi * i / count;
    points.push_back(TPointD(std::cos(angle), std::sin(angle)));
  }

  TTrack raw;
  fillTrack(raw, points);
  TTrackList output;
  TModifierFreehand modifier;
  modifier.modifyTrack(raw, output);
  assert(output.size() == 1);

  const TTrack &track = *output.front();
  for (int i = 0; i + 1 < track.size(); ++i)
    for (int j = 1; j < 20; ++j) {
      TPointD p = track.calcPoint(i + j / 20.0).position;
      assertFinite(p);
      assert(std::abs(std::sqrt(p.x * p.x + p.y * p.y) - 1.0) < 0.04);
    }
}

void testCornersAndDegenerateSamples() {
  TTrack cusp;
  fillTrack(cusp, {TPointD(0, 0), TPointD(1, 0), TPointD(1, 1)});
  TTrackTangent cuspTangent = TModifierFreehand::calcTangent(cusp, 1);
  assert(std::abs(cuspTangent.position.x) < 1e-9);
  assert(std::abs(cuspTangent.position.y) < 1e-9);

  TTrack line;
  fillTrack(line, {TPointD(0, 0), TPointD(1, 0), TPointD(2, 0),
                   TPointD(3, 0), TPointD(4, 0)});
  TTrackList lineOutput;
  TModifierFreehand modifier;
  modifier.modifyTrack(line, lineOutput);
  for (int i = 0; i + 1 < lineOutput.front()->size(); ++i)
    for (int j = 1; j < 10; ++j) {
      TPointD p = lineOutput.front()->calcPoint(i + j / 10.0).position;
      assertFinite(p);
      assert(std::abs(p.y) < 1e-9);
    }

  TTrack degenerate;
  fillTrack(degenerate, {TPointD(0, 0), TPointD(0, 0), TPointD(1, 0),
                         TPointD(0, 0), TPointD(0, 0)});
  TTrackList degenerateOutput;
  modifier.modifyTrack(degenerate, degenerateOutput);
  for (int i = 0; i < degenerateOutput.front()->size(); ++i)
    assertFinite(degenerateOutput.front()->point(i).position);
}

void testPressureAndLookahead() {
  TTrack raw;
  fillTrack(raw, {TPointD(0, 0), TPointD(1, 0), TPointD(2, 0),
                  TPointD(3, 0)});
  TTrackList output;
  TModifierFreehand modifier;
  modifier.modifyTrack(raw, output);
  assert(output.front()->fixedSize() == raw.size() - 1);
  assert(output.front()->previewSize() == 1);
  for (int i = 0; i + 1 < output.front()->size(); ++i) {
    const double p0 = output.front()->point(i).pressure;
    const double p1 = output.front()->point(i + 1).pressure;
    for (int j = 1; j < 10; ++j) {
      const double pressure = output.front()->calcPoint(i + j / 10.0).pressure;
      assert(pressure >= 0.0 && pressure <= 1.0);
      assert(pressure >= std::min(p0, p1) - 1e-9 &&
             pressure <= std::max(p0, p1) + 1e-9);
    }
  }

  TTrack released;
  fillTrack(released, {TPointD(0, 0), TPointD(1, 0), TPointD(2, 0)}, true);
  TTrackList releasedOutput;
  modifier.modifyTrack(released, releasedOutput);
  assert(releasedOutput.front()->fixedSize() == released.size());
  assert(releasedOutput.front()->previewSize() == 0);

  TTrack one;
  fillTrack(one, {TPointD(4, 2)});
  TTrackList oneOutput;
  modifier.modifyTrack(one, oneOutput);
  assert(oneOutput.front()->size() == 1);
  assertFinite(oneOutput.front()->calcPoint(0).position);

  TTrack two;
  fillTrack(two, {TPointD(4, 2), TPointD(5, 3)}, true);
  TTrackList twoOutput;
  modifier.modifyTrack(two, twoOutput);
  assert(twoOutput.front()->fixedSize() == 2);
  assert(twoOutput.front()->previewSize() == 0);
}

struct CaptureHandler : TInputHandler {
  std::vector<TPointD> committed;

  void inputPaintTrackPoint(const TTrackPoint &point, const TTrack &, bool,
                            bool preview) override {
    if (!preview) committed.push_back(point.position);
  }
};

std::vector<TPointD> runInputSequence(const std::vector<TPointD> &positions,
                                      bool batched) {
  TInputManager manager;
  CaptureHandler handler;
  manager.setHandler(&handler);
  manager.addModifier(new TModifierFreehand());

  const TTimerTicks start = TToolTimer::ticks();
  std::vector<TToolInputSample> samples;
  for (int i = 0; i < (int)positions.size(); ++i) {
    TToolInputSample sample;
    sample.position  = positions[i];
    sample.timestamp = start + i + 1;
    samples.push_back(sample);
  }

  if (batched) {
    manager.trackEvents(0, 0, samples.data(), (int)samples.size());
    manager.processTracks();
  } else {
    for (const TToolInputSample &sample : samples) {
      manager.trackEvent(0, 0, sample.position, sample.pressure, sample.tilt,
                         false, false, false, sample.timestamp);
      manager.processTracks();
    }
  }

  // Release supplies the final point and flushes the provisional segment.
  const TToolInputSample &last = samples.back();
  manager.trackEvent(0, 0, last.position, last.pressure, last.tilt, false,
                    false, true, last.timestamp + 1);
  manager.processTracks();
  return handler.committed;
}

void testBatchedAndIndividualInput() {
  const std::vector<TPointD> positions = {
      TPointD(0, 0), TPointD(1, 0.5), TPointD(2, 1.0), TPointD(3, 0.25)};
  std::vector<TPointD> individual = runInputSequence(positions, false);
  std::vector<TPointD> batched    = runInputSequence(positions, true);
  assert(individual.size() == batched.size());
  for (int i = 0; i < (int)individual.size(); ++i) {
    assert(std::abs(individual[i].x - batched[i].x) < 1e-9);
    assert(std::abs(individual[i].y - batched[i].y) < 1e-9);
  }
}

}  // namespace

int main() {
  testCircle(6);
  testCircle(8);
  testCircle(12);
  testCornersAndDegenerateSamples();
  testPressureAndLookahead();
  testBatchedAndIndividualInput();
  return 0;
}
