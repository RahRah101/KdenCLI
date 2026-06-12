#include "TitleBuilder.h"
#include "tinyxml2.h"
#include <sstream>
#include "TextMetrics.h"

namespace TitleBuilder {

static void resolveAnchor(const std::string &anchor, int margin,
                          int profile_w, int profile_h,
                          int text_w, int box_h,
                          int &out_x, int &out_y, int &out_align, int &out_box_w) {
    std::string vert = "center", horiz = "center";
    auto dash = anchor.find('-');
    if (dash == std::string::npos) {
        // single word: "center" => center-center, else treat as vertical
        if (anchor != "center") vert = anchor;
    } else {
        vert  = anchor.substr(0, dash);
        horiz = anchor.substr(dash + 1);
    }

    out_box_w = text_w;

    if (horiz == "left")       { out_x = margin; out_align = 0; }
    else if (horiz == "right") { out_x = profile_w - text_w - margin; out_align = 2; }
    else                       { out_x = (profile_w - text_w) / 2; out_align = 1; }

    if (vert == "top")         out_y = margin;
    else if (vert == "bottom") out_y = profile_h - box_h - margin;
    else                       out_y = (profile_h - box_h) / 2;
}

std::string Build(const TitleParams &p) {
    int box_h = (p.box_h > 0) ? p.box_h : static_cast<int>(p.font_size * 1.3);
    int text_w = (p.box_w > 0) ? p.box_w
           : TextMetrics::textWidth(p.text, p.font, p.font_size,
                                    p.bold, p.italic, p.font_file);

    int x = p.x, y = p.y, alignment = 1;
    int box_w = text_w;
    if (p.use_anchor)
        resolveAnchor(p.anchor, p.margin, p.profile_w, p.profile_h,
                      text_w, box_h, x, y, alignment, box_w);
    int out_frame = p.length_frames - 1;

    using namespace tinyxml2;
    XMLDocument doc;

    XMLElement* root = doc.NewElement("kdenlivetitle");
    root->SetAttribute("LC_NUMERIC", "C");
    root->SetAttribute("width",    p.profile_w);
    root->SetAttribute("height",   p.profile_h);
    root->SetAttribute("duration", p.length_frames);
    root->SetAttribute("out",      out_frame);
    doc.InsertEndChild(root);

    XMLElement* item = doc.NewElement("item");
    item->SetAttribute("type", "QGraphicsTextItem");
    item->SetAttribute("z-index", "0");
    root->InsertEndChild(item);

    XMLElement* pos = doc.NewElement("position");
    pos->SetAttribute("x", x);
    pos->SetAttribute("y", y);
    item->InsertEndChild(pos);

    XMLElement* transform = doc.NewElement("transform");
    transform->SetText("1,0,0,0,1,0,0,0,1");
    pos->InsertEndChild(transform);

    XMLElement* content = doc.NewElement("content");
    content->SetAttribute("font", p.font.c_str());
    content->SetAttribute("font-color", p.color.c_str());
    content->SetAttribute("font-outline-color", p.outline_color.c_str());
    content->SetAttribute("font-outline", p.outline.c_str());
    content->SetAttribute("font-pixel-size", p.font_size);
    content->SetAttribute("font-weight", "700");
    content->SetAttribute("font-italic", "0");
    content->SetAttribute("font-underline", "0");
    content->SetAttribute("alignment", alignment);
    content->SetAttribute("box-width", box_w);
    content->SetAttribute("box-height", box_h);
    content->SetAttribute("font-weight", p.bold ? "700" : "400");
    content->SetAttribute("font-italic", p.italic ? "1" : "0");
    content->SetAttribute("font-underline", p.underline ? "1" : "0");
    content->SetAttribute("letter-spacing", p.letter_spacing);
    content->SetAttribute("line-spacing", p.line_spacing);
    if (!p.shadow.empty())   content->SetAttribute("shadow", p.shadow.c_str());
    if (!p.gradient.empty()) content->SetAttribute("gradient", p.gradient.c_str());

    content->SetText(p.text.c_str());

    item->InsertEndChild(content);

    XMLElement* startvp = doc.NewElement("startviewport");
    startvp->SetAttribute("rect", ("0,0," + std::to_string(p.profile_w) + "," + std::to_string(p.profile_h)).c_str());
    root->InsertEndChild(startvp);

    XMLElement* endvp = doc.NewElement("endviewport");
    endvp->SetAttribute("rect", ("0,0," + std::to_string(p.profile_w) + "," + std::to_string(p.profile_h)).c_str());
    root->InsertEndChild(endvp);

    XMLElement* bg = doc.NewElement("background");
    bg->SetAttribute("color", "0,0,0,0");
    root->InsertEndChild(bg);

    XMLPrinter printer;
    doc.Print(&printer);
    return printer.CStr();
}

}