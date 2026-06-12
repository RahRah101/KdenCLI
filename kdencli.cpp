#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <map>
#include "lib/KdenCLIProject.h"
#include "lib/CLI11.hpp"
#include "lib/MediaProbe.h"
#include "lib/types.h"
#include "lib/TimeUtil.h"
#include "lib/TitleBuilder.h"

using namespace std;


//TODO: Make freetype2 dependency for the text metrics optional.
/** COMPILE:
 *  g++ -std=c++17 kdencli.cpp lib/*.cpp -g -o kdencli $(pkg-config --cflags --libs freetype2)
 *  
 *  RUN:
 *  ./kdencli
 */

//-- HELPERS --
//TODO: Lowkey what a weird, hacky type.
//Maybe murder it and pass the arguments directly in the functions needed.
struct PlaceTiming {
    float length;
    float offset;
};

PlaceTiming resolveTiming(float base_length, float base_offset,
                          const string &ss, const string &to) {
    if (!ss.empty() && !to.empty()) {
        float offset = TimeUtil::parseTimestamp(ss);
        float length = TimeUtil::parseTimestamp(to) - offset;
        return {length, offset};
    }
    if (!ss.empty()) {
        return {-1, TimeUtil::parseTimestamp(ss)};
    }
    return {base_length, base_offset};
}

struct PlaceResult {
    Placement primary;
    bool av_split_attempted = false;
    bool av_split_succeeded = false;
    Placement paired;
};

//-- COMMAND PARSING FUNCTION --
void cmdCreate(const string &output, int fps, int width, int height) {
    KdenCLIProject proj;
    proj.SetProfile(fps, width, height);
    proj.AddVideoTrack();
    proj.AddAudioTrack();
    proj.Save(output);
    cout << "Created project: " << output << "\n";
    cout << "  Profile: " << width << "x" << height << " @ " << fps << " fps\n";
}

void cmdImport(const string &project, const string &filepath) {
    KdenCLIProject proj;
    proj.Open(project);
    ClipId id = proj.ImportClip(filepath);
    proj.Save(project);
    cout << "Imported clip " << id << ": " << filepath << "\n";
}

void cmdAddTrack(const string &project, const string &type) {
    KdenCLIProject proj;
    proj.Open(project);
    TrackId id = (type == "audio") ? proj.AddAudioTrack() : proj.AddVideoTrack();
    proj.Save(project);
    cout << "Added " << type << " track " << id << "\n";
}

void cmdTitle(const string &project, int track, float at, float duration,
              const TitleBuilder::TitleParams &params) {
    KdenCLIProject proj;
    proj.Open(project);
    Placement pl = proj.AddTitle(params, track, at, duration);
    proj.Save(project);
    cout << "Placed title on track " << pl.track << " -> entry " << pl.entry << "\n";
}

PlaceResult place(KdenCLIProject &proj, const string &filepath, int track,
                  float at, float length, float offset,
                  const string &ss, const string &to,
                  PlaceMode mode) {
    auto timing = resolveTiming(length, offset, ss, to);
    PlaceResult result;
    result.primary.track = track;

    if (timing.length < 0)
        result.primary.entry = proj.PlaceFullClip(filepath, track, at, timing.offset);
    else
        result.primary.entry = proj.PlaceClipByFilename(filepath, track, at, timing.length, timing.offset);

    if (mode == PlaceMode::AUTO) {
        auto streams = MediaProbe::GetStreams(filepath);
        if (streams.has_video && streams.has_audio) {
            result.av_split_attempted = true;
            TrackId paired = proj.FindPairedTrack(track);
            if (paired >= 0) {
                result.paired.track = paired;
                if (timing.length < 0)
                    result.paired.entry = proj.PlaceFullClip(filepath, paired, at, timing.offset);
                else
                    result.paired.entry = proj.PlaceClipByFilename(filepath, paired, at, timing.length, timing.offset);
                result.av_split_succeeded = true;
            }
        }
    }
    return result;
}

void printPlaceResult(const PlaceResult &r, const string &filepath) {
    cout << "Placed " << filepath << " on track " << r.primary.track 
         << " -> entry " << r.primary.entry << "\n";
    
    if (r.av_split_attempted) {
        if (r.av_split_succeeded) {
            cout << "A/V split: also placed on track " << r.paired.track 
                 << " -> entry " << r.paired.entry << "\n";
        } else {
            cout << "No paired track found for A/V split\n";
        }
    }
}

//CLI wrapper for place command.
//Filepath overload
void cmdPlace(KdenCLIProject &proj, const string &filepath, int track,
              float at, float length, float offset,
              const string &ss, const string &to,
              PlaceMode mode = PlaceMode::AUTO) {
    PlaceResult r = place(proj, filepath, track, at, length, offset, ss, to, mode);
    printPlaceResult(r, filepath);
}

//CLI wrapper for place command.
//clip ID overload
void cmdPlace(KdenCLIProject &proj, int clip_id, int track,
              float at, float length, float offset,
              const string &ss, const string &to,
              PlaceMode mode = PlaceMode::AUTO) {
    string path = proj.GetClipResource(clip_id);
    cout << "Resolved clip " << clip_id << " -> " << path << "\n";
    PlaceResult r = place(proj, path, track, at, length, offset, ss, to, mode);
    printPlaceResult(r, path);
}
void cmdFade(const string &project, int track, int entry_id,
             float in_start, float in_end,
             float out_start, float out_end) {
    KdenCLIProject proj;
    proj.Open(project);

    if (in_end > in_start) {
        EffectContext ctx;
        ctx.in_time  = in_start;
        ctx.out_time = in_end;
        proj.ApplyEffect(track, entry_id, "fade_from_black", ctx);
    }

    if (out_end > out_start) {
        EffectContext ctx;
        ctx.in_time  = out_start;
        ctx.out_time = out_end;
        proj.ApplyEffect(track, entry_id, "fade_to_black", ctx);
    }

    proj.Save(project);
    cout << "Applied fade to track " << track << " entry " << entry_id << "\n";
}

void cmdListEffects(const EffectCatalog &catalog) {
    for (const auto &id : catalog.list_ids())
        cout << id << "\n";
}

void cmdDescribeEffect(const EffectCatalog &catalog, const string &id) {
    const EffectDefinition* def = catalog.get(id);
    if (!def)
        throw runtime_error("Effect not found: " + id);

    cout << def->id << " (" << def->tag << ")\n";
    cout << "  type: " << (def->type.empty() ? "any" : def->type) << "\n";
    cout << "  parameters:\n";
    for (const auto &p : def->parameters) {
        cout << "    --param " << p.name 
             << "=" << p.default_value
             << "\n";
    }
}

void cmdApplyEffect(KdenCLIProject &proj, const string &id, int track, int entry, 
                    float effect_in, float effect_out, 
                    const vector<string> &effect_params) {
    EffectContext ctx;
    ctx.in_time  = effect_in;
    ctx.out_time = effect_out;

    for (const auto &p : effect_params) {
        auto eq = p.find('=');
        if (eq == string::npos)
            throw runtime_error("Invalid param format (expected key=value): " + p);
                ctx.overrides[p.substr(0, eq)] = p.substr(eq + 1);
    }
    proj.ApplyEffect(track, entry, id, ctx);
}

void cmdInfo(const string &project) {
    KdenCLIProject proj;
    proj.Open(project);
    proj.PrintInfo();
}



int main(int argc, char** argv){
    CLI::App app{"KdenCLI, a CLI wrapper for KdenCode(and KdenLive, I guess...)"};

    app.require_subcommand(1);

    // --- create ---
    string create_output;
    int create_fps = 30, create_width = 1920, create_height = 1080;

    auto *create = app.add_subcommand("create", "Create a new .kdenlive project");
    create->add_option("output", create_output, "Output .kdenlive file path")->required();
    create->add_option("--fps,-f", create_fps, "Framerate (default: 30)");
    create->add_option("--width,-w", create_width, "Frame width (default: 1920)");
    create->add_option("--height", create_height, "Frame height (default: 1080)");

    // --- import ---
    string import_project, import_filepath;

    auto *import_cmd = app.add_subcommand("import", "Import a media file into the project bin");
    import_cmd->add_option("project", import_project, "Project file")->required();
    import_cmd->add_option("filepath", import_filepath, "Path to media file")->required();

    // --- add-track ---
    string track_project, track_type = "video";

    auto *add_track = app.add_subcommand("add-track", "Add a video or audio track");
    add_track->add_option("project", track_project, "Project file")->required();
    add_track->add_option("--type,-t", track_type, "Track type: video or audio (default: video)");

    // --- place ---
    string place_project, place_clip_file;
    int place_track = -1, place_clip_id = -1;
    float place_at = 0, place_length = -1, place_offset = 0;
    string place_cut_start, place_cut_end;
    bool place_video_only = false, place_audio_only = false;

    auto *place = app.add_subcommand("place", "Place a clip on a track");
    place->add_option("project", place_project, "Project file")->required();
    place->add_option("--track,-t", place_track, "Track ID")->required();
    auto *place_clip_group = place->add_option_group("clip", "Clip to place (pick one)");
    place_clip_group->add_option("--clipid,-c", place_clip_id, "Clip ID");
    place_clip_group->add_option("--file,-f", place_clip_file, "Clip filepath");
    place_clip_group->require_option(1);
    place->add_option("--at,-a", place_at, "Timestamp in seconds (default: 0)");
    place->add_option("--length,-l", place_length, "Clip length in seconds");
    place->add_option("--offset,-o", place_offset, "Start offset within clip (default: 0)");
    place->add_option("--ss", place_cut_start, "Cut start (seconds, MM:SS, or HH:MM:SS)");
    place->add_option("--to", place_cut_end, "Cut end (seconds, MM:SS, or HH:MM:SS)");
    place->add_flag("--video-only", place_video_only, "Place on specified track only (skip audio)");
    place->add_flag("--audio-only", place_audio_only, "Place on specified track only (skip video)");

    // --- title ---
    string title_project, title_text, title_anchor = "bottom-center",
        title_font = "Liberation Sans", title_color = "255,255,255,255", 
        title_outline_color = "0,0,0,255", title_outline="2";
    int title_track = -1, title_margin = 80, 
        title_x = 0, title_y = 0, 
        title_font_size = 50, title_box_h = 0, title_box_w = 0;
    float title_at = 0, title_duration = 5;
    bool title_use_xy = false;
    bool title_bold = false, title_italic = false, title_underline = false;
    int title_letter_spacing = 0, title_line_spacing = 0;
    string title_font_file, title_shadow, title_gradient;

    auto *title = app.add_subcommand("title", "Add a text title / overlay");
    title->add_option("project", title_project, "Project file")->required();
    title->add_option("--track,-t", title_track, "Track ID (default: auto-pick video track)");
    title->add_option("--text", title_text, "Title text")->required();
    title->add_option("--at,-a", title_at, "Timestamp in seconds (default 0)");
    title->add_option("--duration,-d", title_duration, "Duration in seconds (default 5)");
    title->add_option("--anchor", title_anchor,
                      "Anchor: top-left|top-center|top-right|center-left|center|"
                      "center-right|bottom-left|bottom-center|bottom-right");
    title->add_option("--margin", title_margin, "Margin from edge, px (default 80)");
    title->add_option("--x", title_x, "Explicit x (requires --xy)");
    title->add_option("--y", title_y, "Explicit y (requires --xy)");
    title->add_flag("--xy", title_use_xy, "Use explicit --x/--y instead of --anchor");
    title->add_option("--font", title_font, "Font family (default Arial)");
    title->add_option("--font-size", title_font_size, "Font pixel size (default 50)");
    title->add_option("--color", title_color, "Font color R,G,B,A (default white)");
    title->add_option("--outline-color", title_outline_color, "Outline color R,G,B,A");
    title->add_option("--outline", title_outline, "Outline width px (default 2)");
    title->add_option("--box-width", title_box_w, "Text box width (0 = full frame width)");
    title->add_option("--box-height", title_box_h, "Text box height (default 0)");

    title->add_flag("--bold", title_bold, "Bold text");
    title->add_flag("--italic", title_italic, "Italic text");
    title->add_flag("--underline", title_underline, "Underline text");
    title->add_option("--letter-spacing", title_letter_spacing, "Letter spacing px");
    title->add_option("--line-spacing", title_line_spacing, "Line spacing px");
    title->add_option("--font-file", title_font_file, "Explicit .ttf path (cross-platform; bypasses fc-match)");
    title->add_option("--shadow", title_shadow, "Shadow: enabled;#AARRGGBB;blur;offX;offY");
    title->add_option("--gradient", title_gradient, "Gradient: #AARRGGBB;#AARRGGBB;0;100;angle");

    // --- fade ---
    string fade_project;
    int fade_track = -1, fade_entry = -1;
    float fade_in_start = 0, fade_in_end = 0;
    float fade_out_start = 0, fade_out_end = 0;

    auto *fade = app.add_subcommand("fade", "Apply fade to a placed clip");
    fade->add_option("project", fade_project, "Project file")->required();
    fade->add_option("--track,-t", fade_track, "Track ID")->required();
    fade->add_option("--entry,-e", fade_entry, "Entry ID")->required();
    fade->add_option("--in-start", fade_in_start, "Fade in start time (seconds)");
    fade->add_option("--in-end", fade_in_end, "Fade in end time (seconds)");
    fade->add_option("--out-start", fade_out_start, "Fade out start time (seconds)");
    fade->add_option("--out-end", fade_out_end, "Fade out end time (seconds)");

    // --- effect ---
    string effect_id;
    float effect_in = 0, effect_out = 0;
    vector<string> effect_params;
    string effect_project;
    int effect_track = -1, effect_entry = -1;
    string effect_describe;
    bool effect_list = false;

    auto *eff = app.add_subcommand("effect", "Apply any effect from the catalog");
 
    eff->add_option("project", effect_project, "Project file");
    eff->add_option("--track,-t", effect_track, "Track ID");
    eff->add_option("--entry,-e", effect_entry, "Entry ID");
    eff->add_option("--id", effect_id, "Effect ID (e.g. fade_from_black, volume, reverb)");
    eff->add_option("--in-start", effect_in, "Filter start time (seconds)");
    eff->add_option("--in-end", effect_out, "Filter end time (seconds)");
    eff->add_option("--param,-p", effect_params, "Parameter override: key=value (repeatable)");
    eff->add_flag("--list,-l", effect_list, "List all available effect IDs");
    eff->add_option("--describe,-d", effect_describe, "Describe parameters for an effect ID");

    // --- info ---
    string info_project;

    auto *info = app.add_subcommand("info", "Print project info");
    info->add_option("project", info_project, "Project file")->required();

    CLI11_PARSE(app, argc, argv);

    PlaceMode mode = place_video_only ? PlaceMode::VIDEO_ONLY
               : place_audio_only ? PlaceMode::AUDIO_ONLY
               : PlaceMode::AUTO;

    try {
        if (create->parsed())
            cmdCreate(create_output, create_fps, create_width, create_height);

        else if (import_cmd->parsed())
            cmdImport(import_project, import_filepath);

        else if (add_track->parsed())
            cmdAddTrack(track_project, track_type);

        else if (place->parsed()) {
            KdenCLIProject proj;
            proj.Open(place_project);

            if (!place_clip_file.empty())
                cmdPlace(proj, place_clip_file, place_track, place_at,
                    place_length, place_offset, place_cut_start, place_cut_end, mode);
            else
                cmdPlace(proj, place_clip_id, place_track, place_at,
                         place_length, place_offset, place_cut_start, place_cut_end, mode);

            proj.Save(place_project);
        }

        else if (title->parsed()) {
            TitleBuilder::TitleParams tp;
            tp.text = title_text;
            tp.use_anchor = !title_use_xy;
            tp.anchor = title_anchor;
            tp.margin = title_margin;
            tp.x = title_x;
            tp.y = title_y;
            tp.font = title_font;
            tp.font_size = title_font_size;
            tp.color = title_color;
            tp.outline_color = title_outline_color;
            tp.outline = title_outline;
            tp.box_w = title_box_w;
            tp.box_h = title_box_h;
            tp.bold = title_bold;
            tp.italic = title_italic;
            tp.font_file = title_font_file;
            tp.letter_spacing = title_letter_spacing;
            tp.line_spacing = title_line_spacing;
            tp.gradient = title_gradient;
            tp.underline = title_underline;
            tp.shadow = title_shadow;
            // profile_w/h and length_frames filled by AddTitle from the project
            cmdTitle(title_project, title_track, title_at, title_duration, tp);
        }

        else if (fade->parsed())
            cmdFade(fade_project, fade_track, fade_entry, fade_in_start, fade_in_end, fade_out_start, fade_out_end);

        else if (eff->parsed()) {
            if (effect_list) {
                KdenCLIProject proj;
                proj.LoadCatalog();
                cmdListEffects(proj.GetCatalog());
            }
            else if (!effect_describe.empty()) {
                KdenCLIProject proj;
                proj.LoadCatalog();
                cmdDescribeEffect(proj.GetCatalog(), effect_describe);
            }
            else {
                if (effect_project.empty() || effect_id.empty() || effect_track < 0 || effect_entry < 0)
                    throw runtime_error("effect requires project, --track, --entry, and --id");
                
                KdenCLIProject proj;
                proj.Open(effect_project);
                cmdApplyEffect(proj, effect_id, effect_track, effect_entry, effect_in, effect_out, effect_params);
                proj.Save(effect_project);
                cout << "Applied effect '" << effect_id << "' to track " << effect_track 
                    << " entry " << effect_entry << "\n";
            }
    }
            
        else if (info->parsed())
            cmdInfo(info_project);
        

    } catch (const exception &e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}