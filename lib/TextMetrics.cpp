#include "TextMetrics.h"
#include <array>
#include <cstdio>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace TextMetrics {

// fallback when FreeType/fontconfig unavailable
static int estimateWidth(const std::string &text, int pixel_size) {
    return static_cast<int>(text.size() * pixel_size * 0.5);
}

std::string resolveFontFile(const std::string &family, bool bold, bool italic,
                            const std::string &explicit_path) {
    if (!explicit_path.empty()) return explicit_path;

    std::string query = family;
    if (bold)   query += ":bold";
    if (italic) query += ":italic";

    std::string result = "";

#ifdef __linux__
    std::string cmd = "fc-match -f '%{file}' \"" + query + "\"";

    std::array<char, 512> buffer;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        result += buffer.data();
    pclose(pipe);

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'
                               || result.back() == ' '))
        result.pop_back();
#endif
    //TODO: Handle Windows and MacOS cases that need something other than fc-match
    //(from my research, it would be CoreText on MacOS and DirectWrite on Windows)
    /*
#ifdef _WIN32
    ... whatever yall do over there ...
#endif
#ifdef __APPLE__
   ... whatever yall do over there ...
#endif
    
    */
    return result;
}

int textWidth(const std::string &text, const std::string &family, int pixel_size,
              bool bold, bool italic, const std::string &font_file) {
    std::string font_path = resolveFontFile(family, bold, italic, font_file);

    if (font_path.empty())
        return estimateWidth(text, pixel_size);

    FT_Library lib;
    if (FT_Init_FreeType(&lib))
        return estimateWidth(text, pixel_size);

    FT_Face face;
    if (FT_New_Face(lib, font_path.c_str(), 0, &face)) {
        FT_Done_FreeType(lib);
        return estimateWidth(text, pixel_size);
    }

    FT_Set_Pixel_Sizes(face, 0, pixel_size);

    long width_26_6 = 0;
    for (unsigned char c : text) {
        if (FT_Load_Char(face, c, FT_LOAD_DEFAULT)) continue;
        width_26_6 += face->glyph->advance.x;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(lib);
    return static_cast<int>(width_26_6 >> 6);
}

}