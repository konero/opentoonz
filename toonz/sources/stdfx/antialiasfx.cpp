#include "texception.h"
#include "tfxparam.h"
#include "trop.h"
#include "stdfx.h"
#include "trasterfx.h"

//-------------------------------------------------------------------

class AntialiasFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(AntialiasFx)

  TRasterFxPort m_input;
  TDoubleParamP m_threshold;
  TDoubleParamP m_softness;

public:
  AntialiasFx() : m_threshold(10), m_softness(100) {
    // threshold parameter (0-256)
    bindParam(this, "threshold", m_threshold);
    m_threshold->setValueRange(0, 256);

    // softness parameter (0-100)
    bindParam(this, "softness", m_softness);
    m_softness->setValueRange(0, 100);

    // input port
    addInputPort("Source", m_input);
  }

  ~AntialiasFx() {};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    // Antialias doesn't change the bounding box...
    if (m_input.isConnected()) {
      return m_input->doGetBBox(frame, bBox, info);
    } else {
      bBox = TRectD();
      return false;
    }
  }

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override {
    // Antialias doesn't transform the coordinates...
    infoOnInput = infoOnOutput;
    rectOnInput = rectOnOutput;
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    if (!m_input.isConnected()) return;

    // Get parameter values
    int threshold = (int)m_threshold->getValue(frame);
    int softness  = (int)m_softness->getValue(frame);

    // Skip if no softness
    if (softness == 0) {
      m_input->compute(tile, frame, ri);
      return;
    }

    // Compute the input into our tile
    m_input->compute(tile, frame, ri);

    // Skip antialiasing for float rasters as per the menu implementation
    TRasterP ras = tile.getRaster();
    if (TRasterFP rasF = ras) {
      return;
    }

    // Create a new raster for the output to avoid in-place modification
    TRasterP outRas = ras->create(ras->getLx(), ras->getLy());

    // Apply antialias effect using TRop
    TRop::antialias(ras, outRas, threshold, softness);

    // Copy result back to tile
    ras->copy(outRas);
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    // TODO: Is this needed?
    return true;
  }
};

// Register the FX
FX_PLUGIN_IDENTIFIER(AntialiasFx, "antialiasFx")
