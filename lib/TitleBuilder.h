#ifndef TITLEBUILDER_H
#define TITLEBUILDER_H

#include <string>

namespace TitleBuilder {

    struct TitleParams {
        std::string text;
        // explicit position; if use_anchor is true these get computed instead
        int x = 0;
        int y = 0;
        bool use_anchor = false;
        std::string anchor = "bottom-center";// e.g. "top-left", "center"
        int margin = 80;

        // box (for anchored text the box usually spans frame width)
        int box_w = 0;// 0 => default to profile width
        int box_h = 0;

        std::string font = "Liberation Sans";
        int font_size = 50;
        std::string color = "255,255,255,255";
        std::string outline_color = "0,0,0,255";
        std::string outline = "2";

        int profile_w = 1920;
        int profile_h = 1080;
        int length_frames = 150;

        bool bold = false;
        bool italic = false;
        bool underline = false;
        int letter_spacing = 0;
        int line_spacing = 0;
        std::string font_file = "";          // explicit .ttf override (cross-platform)
        // advanced pass-through (raw kdenlivetitle format), empty = omit:
        std::string shadow = "";             // e.g : "1;#ff000000;31;15;0"
        std::string gradient = "";           // e.g "#ffffffff;#ff000000;0;100;90"
    };

    // params -> the raw <kdenlivetitle> document string (NOT yet xml-escaped;
    // the caller passes it to AddTitle, where tinyxml2 SetText escapes it).
    std::string Build(const TitleParams &p);
}

#endif