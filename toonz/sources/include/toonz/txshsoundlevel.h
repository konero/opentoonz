#pragma once

#ifndef TXSHSOUNDLEVEL_INCLUDED
#define TXSHSOUNDLEVEL_INCLUDED

#include "toonz/txshlevel.h"
#include "tsound.h"

#include <QList>

#include "tpersist.h"
#include <map>
#include <vector>
#include <mutex>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TXshSoundLevel final : public TXshLevel {
  PERSIST_DECLARATION(TXshSoundLevel)

  TSoundTrackP m_soundTrack;

  double m_duration;  // overall soundtrack duration in seconds
  double m_samplePerFrame;
  int m_frameSoundCount;
  double m_fps;
  // Normalized min/max time blocks keyed by sampling resolution and block,
  // never panel ID. Only visible/requested portions of a clip are computed.
  mutable std::map<std::pair<int, int>, std::vector<DoublePair>> m_values;
  mutable std::mutex m_valuesMutex;
  double m_pressureScale = 0;

  TFilePath m_path;

  DECLARE_CLASS_CODE
public:
  TXshSoundLevel(std::wstring name = L"", int startOffset = 0,
                 int endOffset = 0);
  ~TXshSoundLevel();

  TXshSoundLevel *clone() const;

  void setScene(ToonzScene *scene) override;

  //! Overridden from TXshLevel
  TXshSoundLevel *getSoundLevel() override { return this; }

  void loadSoundTrack();
  void loadSoundTrack(const TFilePath &fileName);

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  void load() override;
  void save() override;
  void save(const TFilePath &path);

  void computeValues();

  // Normalized pressure envelope; display amplitude is applied by the caller.
  void getValueAtPixel(int pixelsPerFrame, int pixel, DoublePair& values) const;

  /*! Set frame rate to \b fps. \sa getSamplePerFrame() */
  void setFrameRate(double fps);
  double getFrameRate() const { return m_fps; }

  void setPath(const TFilePath &path) { m_path = path; }
  TFilePath getPath() const override { return m_path; }

  void setSoundTrack(TSoundTrackP st);
  TSoundTrackP getSoundTrack() { return m_soundTrack; }

  //! Pay Attention this is the sound frame !!
  int getFrameSoundCount() const { return m_frameSoundCount; }

  int getSamplePerFrame() const { return m_samplePerFrame; }

  int getFrameCount() const override;

  void getFids(std::vector<TFrameId> &fids) const override;

private:
  const std::vector<DoublePair>& valuesFor(
      int pixelsPerFrame, int block) const;  // m_valuesMutex held
  void resetValues();                        // m_valuesMutex held

  // not implemented
  TXshSoundLevel(const TXshSoundLevel &);
  TXshSoundLevel &operator=(const TXshSoundLevel &);
};

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshSoundLevel>;
#endif
typedef TSmartPointerT<TXshSoundLevel> TXshSoundLevelP;

#endif  // TXSHSOUNDLEVEL_INCLUDED
