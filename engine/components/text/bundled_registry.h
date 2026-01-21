#pragma once
#include <string>
#include "../../config.h"

#ifndef DISABLE_BUNDLE_FONT
  #include "components/text/iosevka/bundled_fonts_decl.h"
#endif

struct FontBlob {
  unsigned char* data;
  unsigned int len;
};

inline FontBlob GetBundledIosevka(const std::string& weight) {

#ifdef DISABLE_BUNDLE_FONT

  return {nullptr, 0};

#else
  if (weight == "thin")       return {sgr_iosevkaslab_thin_ttc, sgr_iosevkaslab_thin_ttc_len};
  if (weight == "extralight") return {sgr_iosevkaslab_extraLight_ttc, sgr_iosevkaslab_extraLight_ttc_len};
  if (weight == "light")      return {sgr_iosevkaslab_light_ttc, sgr_iosevkaslab_light_ttc_len};
  if (weight == "medium")     return {sgr_iosevkaslab_medium_ttc, sgr_iosevkaslab_medium_ttc_len};
  if (weight == "semibold")   return {sgr_iosevkaslab_semiBold_ttc, sgr_iosevkaslab_semiBold_ttc_len};
  if (weight == "bold")       return {sgr_iosevkaslab_bold_ttc, sgr_iosevkaslab_bold_ttc_len};
  if (weight == "heavy")      return {sgr_iosevkaslab_heavy_ttc, sgr_iosevkaslab_heavy_ttc_len};
  if (weight == "extrabold")  return {sgr_iosevkaslab_extraBold_ttc, sgr_iosevkaslab_extraBold_ttc_len};

  return {sgr_iosevkaslab_regular_ttc, sgr_iosevkaslab_regular_ttc_len};

#endif
}
