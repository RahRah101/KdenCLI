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
        int box_h = 200;

        std::string font = "Liberation Sans";
        int font_size = 50;
        std::string color = "255,255,255,255";
        std::string outline_color = "0,0,0,255";
        std::string outline = "2";

        int profile_w = 1920;
        int profile_h = 1080;
        int length_frames = 150;
    };

    // params -> the raw <kdenlivetitle> document string (NOT yet xml-escaped;
    // the caller passes it to AddTitle, where tinyxml2 SetText escapes it).
    std::string Build(const TitleParams &p);
}

#endif