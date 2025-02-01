#include "toonz/scriptbinding_toonz_raster_converter.h"
#include "toonz/scriptbinding_level.h"
#include "tropcm.h"
#include "convert2tlv.h"

namespace TScriptBinding {

ToonzRasterConverter::ToonzRasterConverter() : m_flatSource(false) {}

ToonzRasterConverter::~ToonzRasterConverter() {}

QScriptValue ToonzRasterConverter::ctor(QScriptContext *context,
                                        QScriptEngine *engine) {
  return engine->newQObject(new ToonzRasterConverter(),
                            QScriptEngine::AutoOwnership);
}

QScriptValue ToonzRasterConverter::toString() { return "ToonzRasterConverter"; }

QScriptValue ToonzRasterConverter::convert(QScriptContext *context,
                                           QScriptEngine *engine) {
  if (context->argumentCount() != 1)
    return context->throwError(
        "Expected one argument (a raster Level or a raster Image)");

  QScriptValue arg = context->argument(0);
  Image *img       = qscriptvalue_cast<Image *>(arg);

  if (!img) {
    return context->throwError(tr("Bad argument: should be a raster Image"));
  }

  // Check the image type
  QString type = img->getType();
  if (type != "Raster") {
    return context->throwError(tr("Can't convert a %1 image").arg(type));
  }

  // Perform the conversion
  RasterToToonzRasterConverter converter;
  TRasterImageP ri  = img->getImg();
  TRasterP raster   = ri->getRaster();  // Extract the raster from TRasterImageP
  TRasterCM32P rout = converter.convert(raster);  // Convert to TRasterCM32P

  // Wrap the TRasterCM32P in a TToonzImageP
  TToonzImageP ti = TToonzImageP(rout, rout->getBounds());
  ti->setPalette(converter.getPalette().getPointer());  // Assign the palette

  // Return the result as a new Image object
  return create(engine, new Image(ti));
}

}  // namespace TScriptBinding
