#ifndef TEXTMETRICS_H
#define TEXTMETRICS_H

#include <string>

namespace TextMetrics {
    // Resolve a font family (+ style) to a .ttf path via fontconfig (fc-match, a wild dependency just appeared!!!)
    std::string resolveFontFile(const std::string &family,
                                bool bold = false, bool italic = false,
                                const std::string &explicit_path = "");

    // Pixel width of `text` rendered in the given font at pixel_size, measured
    // with FreeType (sum of glyph advances). Falls back to a crude estimate
    // (len * size * 0.5) if the font can't be resolved/loaded.
    int textWidth(const std::string &text, const std::string &family,
                  int pixel_size, bool bold = false, bool italic = false,
                  const std::string &font_file = "");}

#endif