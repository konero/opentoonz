#pragma once

#ifndef MODIFIERFREEHAND_INCLUDED
#define MODIFIERFREEHAND_INCLUDED

#include <tools/inputmanager.h>

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// Reconstructs the shared freehand path from sparse input with a one-sample
// look-ahead. The last segment remains provisional until another sample (or
// release) arrives.
class DVAPI TModifierFreehand : public TInputModifier {
public:
  typedef TSubTrackHandler Handler;

  class DVAPI Interpolator : public TTrackInterpolator {
  public:
    TTrackTangentList tangents;
    using TTrackInterpolator::TTrackInterpolator;
    TTrackPoint interpolate(double index) override;
  };

  static TTrackTangent calcTangent(const TTrack &track, int index);

  void modifyTrack(const TTrack &track, TTrackList &outTracks) override;
};

#endif
