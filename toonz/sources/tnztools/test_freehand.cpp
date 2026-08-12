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

double length2(const TPointD &p) { return p.x * p.x + p.y * p.y; }

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
  for (int i = 0; i < degenerateOutput.front()->size(); ++i) {
    assertFinite(degenerateOutput.front()->point(i).position);
    assertFinite(TModifierFreehand::calcTangent(degenerate, i).position);
  }
}

void testUnevenSpacingAndCuspBoundary() {
  TTrack uneven;
  fillTrack(uneven, {TPointD(0, 0), TPointD(0.01, 0.02),
                     TPointD(20, 0.5), TPointD(20.1, 8)});
  for (int i = 0; i < uneven.size(); ++i)
    assertFinite(TModifierFreehand::calcTangent(uneven, i).position);

  constexpr double pi = 3.14159265358979323846;
  const auto tangentAtAngle = [=](double degrees) {
    const double radians = degrees * pi / 180.0;
    TTrack track;
    fillTrack(track,
              {TPointD(0, 0), TPointD(1, 0),
               TPointD(1 + std::cos(radians), std::sin(radians))});
    return TModifierFreehand::calcTangent(track, 1).position;
  };

  assert(length2(tangentAtAngle(79.999)) > TConsts::epsilon);
  assert(length2(tangentAtAngle(80.001)) <= TConsts::epsilon);
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

}  // namespace

int main() {
  testCircle(6);
  testCircle(8);
  testCircle(12);
  testCornersAndDegenerateSamples();
  testUnevenSpacingAndCuspBoundary();
  testPressureAndLookahead();
  return 0;
}
