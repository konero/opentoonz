
#include <tools/modifiers/modifierfreehand.h>

#include <cmath>

namespace {

constexpr double kCuspCosine = 0.17364817766693033;  // cos(80 degrees)

double monotonePressureTangent(const TTrack &track, int index) {
  if (track.size() < 2) return 0.0;
  if (index <= 0) return track[1].pressure - track[0].pressure;
  if (index >= track.size() - 1)
    return track[index].pressure - track[index - 1].pressure;

  const double left  = track[index].pressure - track[index - 1].pressure;
  const double right = track[index + 1].pressure - track[index].pressure;
  if (left * right <= 0.0) return 0.0;

  // Fritsch-Carlson's local limiter: the derivative cannot create a new
  // extremum between either pair of pressure samples.
  const double sign = left < 0.0 ? -1.0 : 1.0;
  const double limit = 3.0 * std::min(std::abs(left), std::abs(right));
  return sign * std::min(0.5 * (std::abs(left) + std::abs(right)), limit);
}

double monotonePressure(double p0, double p1, double m0, double m1,
                        double u) {
  const double h00 = 2.0 * u * u * u - 3.0 * u * u + 1.0;
  const double h10 = u * u * u - 2.0 * u * u + u;
  const double h01 = -2.0 * u * u * u + 3.0 * u * u;
  const double h11 = u * u * u - u * u;
  const double value = h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1;
  return tcrop(value, std::min(p0, p1), std::max(p0, p1));
}

}  // namespace

TTrackPoint TModifierFreehand::Interpolator::interpolate(double index) {
  double frac;
  int i0 = track.floorIndex(index, &frac);
  int i1 = i0 + 1;

  const TTrackPoint &p0 = track[i0];
  const TTrackPoint &p1 = track[i1];
  TTrackTangent t0 = i0 >= 0 && i0 < (int)tangents.size() ? tangents[i0]
                                                            : TTrackTangent();
  TTrackTangent t1 = i1 >= 0 && i1 < (int)tangents.size() ? tangents[i1]
                                                            : TTrackTangent();

  // Tangents are expressed in the normalized parameter of this interval.
  // Do not multiply them by the geometric chord a second time: that magnifies
  // the gaps produced by fast input and recreates angular, rounded polylines.
  TTrackPoint result = TTrack::interpolationSpline(p0, p1, t0, t1, frac);
  result.pressure = monotonePressure(
      tcrop(p0.pressure, 0.0, 1.0), tcrop(p1.pressure, 0.0, 1.0),
      t0.pressure, t1.pressure, frac);
  result.pressure = tcrop(result.pressure, 0.0, 1.0);
  result.tilt.x   = tcrop(result.tilt.x, -1.0, 1.0);
  result.tilt.y   = tcrop(result.tilt.y, -1.0, 1.0);
  return result;
}

TTrackTangent TModifierFreehand::calcTangent(const TTrack &track, int index) {
  if (track.size() < 2 || index < 0 || index >= track.size())
    return TTrackTangent();

  // Use a one-sided quadratic derivative at the ends so the first and last
  // visible segments are curved immediately instead of starting as a hard
  // chord.  The parameter intervals retain the centripetal (sqrt chord)
  // spacing used by the interior tangent.
  if (index == 0 || index == track.size() - 1) {
    const double pressure = monotonePressureTangent(track, index);
    if (index == 0) {
      const TPointD &p0 = track[0].position;
      const TPointD &p1 = track[1].position;
      if (track.size() < 3) return TTrackTangent(p1 - p0, pressure);
      const TPointD &p2 = track[2].position;
      const double l0   = track[1].length - track[0].length;
      const double l1   = track[2].length - track[1].length;
      if (l0 <= TConsts::epsilon)
        return TTrackTangent(TPointD(), pressure);
      const double a = sqrt(l0);
      const double b = sqrt(std::max(l1, TConsts::epsilon));
      return TTrackTangent(
          p0 * (-(2.0 * a + b) / (a + b)) +
          p1 * ((a + b) / b) - p2 * (a * a / (b * (a + b))), pressure);
    }
    const TPointD &pm = track[track.size() - 2].position;
    const TPointD &p2 = track[track.size() - 1].position;
    if (track.size() < 3) return TTrackTangent(p2 - pm, pressure);
    const double l0 = track[track.size() - 2].length -
                      track[track.size() - 3].length;
    const double l1 = track[track.size() - 1].length -
                      track[track.size() - 2].length;
    if (l1 <= TConsts::epsilon)
      return TTrackTangent(TPointD(), pressure);
    const TPointD &pp = track[track.size() - 3].position;
    const double a = sqrt(std::max(l0, TConsts::epsilon));
    const double b = sqrt(l1);
    return TTrackTangent(
        pp * (b * b / (a * (a + b))) -
        pm * ((a + b) / a) + p2 * ((a + 2.0 * b) / (a + b)), pressure);
  }

  const TTrackPoint &p0 = track[index - 1];
  const TTrackPoint &p1 = track[index];
  const TTrackPoint &p2 = track[index + 1];

  TPointD d0 = p1.position - p0.position;
  TPointD d1 = p2.position - p1.position;
  const double l0 = p1.length - p0.length;
  const double l1 = p2.length - p1.length;
  const double pressure = monotonePressureTangent(track, index);
  if (l0 <= TConsts::epsilon || l1 <= TConsts::epsilon)
    return TTrackTangent(TPointD(), pressure);

  // Centripetal Catmull-Rom parameterization (alpha = 0.5).  The square-root
  // chord parameter prevents loops and overshoot when input samples are sparse.
  double t0 = sqrt(l0);
  double t1 = sqrt(l1);
  TPointD incoming = d0 * (1.0 / t0);
  TPointD outgoing = d1 * (1.0 / t1);
  TPointD across   = (p2.position - p0.position) * (1.0 / (t0 + t1));

  // Preserve intentional cusps while smoothing ordinary circular turns.
  double cosine = (d0.x * d1.x + d0.y * d1.y) / (l0 * l1);
  if (cosine <= kCuspCosine)
    return TTrackTangent(TPointD(), pressure);

  // Non-uniform Catmull-Rom/Hermite derivative for alpha = 0.5.
  return TTrackTangent((incoming + outgoing - across) * t1, pressure);
}

void TModifierFreehand::modifyTrack(const TTrack &track, TTrackList &outTracks) {
  if (!track.handler) {
    Handler *handler = new Handler();
    track.handler = handler;
    handler->track = new TTrack(track);
    new Interpolator(*handler->track);
  }

  Handler *handler = dynamic_cast<Handler *>(track.handler.getPointer());
  if (!handler) return;

  outTracks.push_back(handler->track);
  TTrack &subTrack = *handler->track;
  Interpolator *intr =
      dynamic_cast<Interpolator *>(subTrack.getInterpolator().getPointer());
  if (!intr) return;
  if (!track.changed()) return;

  int start = track.size() - track.pointsAdded;
  if (start < 0) start = 0;

  int tangentStart = start - 1;
  if (tangentStart < 0) tangentStart = 0;
  intr->tangents.resize(tangentStart);
  for (int i = tangentStart; i < track.size(); ++i)
    intr->tangents.push_back(calcTangent(track, i));

  subTrack.truncate(start);
  for (int i = start; i < track.size(); ++i)
    subTrack.push_back(subTrack.pointFromOriginal(i), false);

  // Keep only the newest segment provisional.  It becomes immutable as soon
  // as the next sample supplies the one-point look-ahead; release fixes all.
  if (track.fixedFinished())
    subTrack.fix_all();
  else if (track.fixedSize())
    subTrack.fix_to(std::max(track.fixedSize() - 1, 1));

  track.resetChanges();
}
