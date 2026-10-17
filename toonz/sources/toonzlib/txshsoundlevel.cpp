

#include "toonz/txshsoundlevel.h"
#include "tsound_io.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshleveltypes.h"

#include "tstream.h"
#include "toutputproperties.h"
#include "tconvert.h"

//-----------------------------------------------------------------------------

namespace {
constexpr int WaveformBlockSize = 256;
constexpr int WaveformCacheBlocks =
    128;  // At most 512 KiB of envelope samples.
}  // namespace

DEFINE_CLASS_CODE(TXshSoundLevel, 53)

PERSIST_IDENTIFIER(TXshSoundLevel, "soundLevel")

//=============================================================================

TXshSoundLevel::TXshSoundLevel(std::wstring name, int startOffset,
                               int endOffset)
    : TXshLevel(m_classCode, name)
    , m_soundTrack(0)
    , m_duration(0)
    , m_samplePerFrame(0)
    , m_frameSoundCount(0)
    , m_fps(12)
    , m_path() {}

//-----------------------------------------------------------------------------

TXshSoundLevel::~TXshSoundLevel() {}

//-----------------------------------------------------------------------------

TXshSoundLevel *TXshSoundLevel::clone() const {
  std::lock_guard<std::mutex> lock(m_valuesMutex);
  TXshSoundLevel *sound = new TXshSoundLevel();
  sound->setFrameRate(m_fps);
  sound->setSoundTrack(m_soundTrack ? m_soundTrack->clone() : TSoundTrackP());
  sound->m_duration = m_duration;
  sound->m_path     = m_path;
  return sound;
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::setScene(ToonzScene *scene) {
  assert(scene);
  TXshLevel::setScene(scene);
  TOutputProperties *properties = scene->getProperties()->getOutputProperties();
  assert(properties);
  setFrameRate(properties->getFrameRate());
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::loadSoundTrack() {
  assert(getScene());

  TSceneProperties *properties = getScene()->getProperties();
  if (properties) {
    TOutputProperties *outputProperties = properties->getOutputProperties();
    if (outputProperties) m_fps         = outputProperties->getFrameRate();
  }
  TFilePath path = getScene()->decodeFilePath(m_path);
  try {
    loadSoundTrack(path);
  } catch (TException &e) {
    throw TException(e.getMessage());
  }
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::loadSoundTrack(const TFilePath &fileName) {
  try {
    TSoundTrackP st;
    TFilePath path(fileName);
    bool ret = TSoundTrackReader::load(path, st);
    if (ret) {
      m_duration = st->getDuration();
      setName(fileName.getWideName());
      setSoundTrack(st);
    }
  } catch (TException &) {
    return;
  }
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::load() { loadSoundTrack(); }

//-----------------------------------------------------------------------------

void TXshSoundLevel::save() { save(m_path); }

//-----------------------------------------------------------------------------

void TXshSoundLevel::save(const TFilePath &path) {
  TSoundTrackWriter::save(path, m_soundTrack);
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::loadData(TIStream &is) {
  is >> m_name;
  setName(m_name);

  std::string tagName;
  bool flag = false;

  int type = UNKNOWN_XSHLEVEL;

  for (;;) {
    if (is.matchTag(tagName)) {
      if (tagName == "path") {
        is >> m_path;
        is.matchEndTag();
      } else if (tagName == "type") {
        std::string v;
        is >> v;
        if (v == "sound") type = SND_XSHLEVEL;
        is.matchEndTag();
      } else
        throw TException("unexpected tag " + tagName);
    } else
      break;
  }
  setType(type);
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::saveData(TOStream &os) {
  os << m_name;

  std::map<std::string, std::string> attr;
  os.child("type") << L"sound";
  os.child("path") << m_path;
}

//-----------------------------------------------------------------------------

const std::vector<DoublePair>& TXshSoundLevel::valuesFor(int pixelsPerFrame,
                                                         int block) const {
  const auto key = std::make_pair(pixelsPerFrame, block);
  auto found     = m_values.find(key);
  if (found != m_values.end()) return found->second;
  if (m_values.size() >= WaveformCacheBlocks) m_values.erase(m_values.begin());
  auto& values = m_values[key];
  if (!m_soundTrack || m_pressureScale == 0 || m_frameSoundCount <= 0)
    return values;
  const qint64 first    = qint64(block) * WaveformBlockSize;
  const qint64 endPixel = std::min(first + WaveformBlockSize,
                                   qint64(m_frameSoundCount) * pixelsPerFrame);
  if (first >= endPixel) return values;
  values.resize(size_t(endPixel - first), DoublePair(0, 0));
  const int sampleCount       = m_soundTrack->getSampleCount();
  const double samplePerPixel = m_samplePerFrame / pixelsPerFrame;
  for (qint64 index = first; index < endPixel; ++index) {
    const qint64 frame = index / pixelsPerFrame;
    const int pixel    = int(index % pixelsPerFrame);
    const int begin    = int(frame * m_samplePerFrame + pixel * samplePerPixel);
    const double endSample =
        pixel + 1 == pixelsPerFrame
            ? (frame + 1) * m_samplePerFrame - 1
            : frame * m_samplePerFrame + (pixel + 1) * samplePerPixel - 1;
    const int end = std::min(sampleCount - 1, int(endSample));
    if (begin >= sampleCount || end < begin) continue;
    double minimum = 0, maximum = 0;
    m_soundTrack->getMinMaxPressure(begin, end, TSound::MONO, minimum, maximum);
    values[size_t(index - first)] =
        DoublePair(minimum * m_pressureScale, maximum * m_pressureScale);
  }
  return values;
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::computeValues() {
  std::lock_guard<std::mutex> lock(m_valuesMutex);
  resetValues();
}

void TXshSoundLevel::setSoundTrack(TSoundTrackP track) {
  std::lock_guard<std::mutex> lock(m_valuesMutex);
  m_soundTrack = track;
  resetValues();
}

void TXshSoundLevel::resetValues() {
  m_values.clear();
  m_pressureScale   = 0;
  m_samplePerFrame  = 0;
  m_frameSoundCount = 0;
  if (!m_soundTrack || m_fps <= 0) return;
  m_samplePerFrame      = m_soundTrack->getSampleRate() / m_fps;
  const int sampleCount = m_soundTrack->getSampleCount();
  if (sampleCount <= 0 || m_samplePerFrame <= 0) return;
  m_frameSoundCount = tceil(sampleCount / m_samplePerFrame);
  double minimum = 0, maximum = 0;
  m_soundTrack->getMinMaxPressure(0, sampleCount, TSound::LEFT, minimum,
                                  maximum);
  const double pressure = std::max(fabs(minimum), fabs(maximum));
  if (pressure > 0) m_pressureScale = 1.0 / pressure;
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::getValueAtPixel(int pixelsPerFrame, int pixel,
                                     DoublePair& values) const {
  std::lock_guard<std::mutex> lock(m_valuesMutex);
  values = DoublePair(0, 0);
  if (pixelsPerFrame <= 0 || pixel < 0 ||
      qint64(pixel) >= qint64(m_frameSoundCount) * pixelsPerFrame)
    return;
  const auto& block   = valuesFor(pixelsPerFrame, pixel / WaveformBlockSize);
  const size_t offset = pixel % WaveformBlockSize;
  if (offset < block.size()) values = block[offset];
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::setFrameRate(double fps) {
  std::lock_guard<std::mutex> lock(m_valuesMutex);
  if (m_fps != fps) {
    m_fps = fps;
    resetValues();
  }
}

//-----------------------------------------------------------------------------

int TXshSoundLevel::getFrameCount() const {
  int frameCount = m_duration * m_fps;
  return (frameCount == 0) ? 1 : frameCount;
}

//-----------------------------------------------------------------------------
// Implementato per utilita'
void TXshSoundLevel::getFids(std::vector<TFrameId> &fids) const {
  int i;
  for (i = 0; i < getFrameCount(); i++) fids.push_back(TFrameId(i));
}
