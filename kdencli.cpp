#include <iostream>
#include <string>
#include <vector>
#include "lib/KdenCLIProject.h"
#include "lib/CLI11.hpp"

using namespace std;


/** COMPILE:
 *  g++ *.cpp lib/*.cpp -g -o kdencli
 *  
 *  RUN:
 *  ./kdencli
 */

#include <fstream>
#include <string>
#include <map>


//-- HELPERS --
float parseTimestamp(const std::string &input) {
    int h, m;
    float s;
    if (sscanf(input.c_str(), "%d:%d:%f", &h, &m, &s) == 3)
        return h * 3600.0f + m * 60.0f + s;
    if (sscanf(input.c_str(), "%d:%f", &m, &s) == 2)
        return m * 60.0f + s;
    return std::stof(input);
}

struct PlaceTiming {
    float length;
    float offset;
};

PlaceTiming resolveTiming(float base_length, float base_offset,
                          const string &ss, const string &to) {
    if (!ss.empty() && !to.empty()) {
        float offset = parseTimestamp(ss);
        float length = parseTimestamp(to) - offset;
        return {length, offset};
    }
    if (!ss.empty()) {
        return {-1, parseTimestamp(ss)};
    }
    return {base_length, base_offset};
}


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

void cmdPlace(KdenCLIProject &proj, const string &filepath, int track,
              float at, float length, float offset,
              const string &ss, const string &to) {
    auto timing = resolveTiming(length, offset, ss, to);
    TrackEntryId entry;
    if (timing.length < 0) {
        entry = proj.PlaceFullClip(filepath, track, at, timing.offset);
    } else {
        entry = proj.PlaceClipByFilename(filepath, track, at, timing.length, timing.offset);
    }
    cout << "Placed " << filepath << " on track " << track << " -> entry " << entry << "\n";
}

void cmdPlace(KdenCLIProject &proj, int clip_id, int track,
              float at, float length, float offset,
              const string &ss, const string &to) {
    auto timing = resolveTiming(length, offset, ss, to);
    TrackEntryId entry;
    if (timing.length < 0) {
        string path = proj.GetClipResource(clip_id);
        entry = proj.PlaceFullClip(path, track, at, timing.offset);
    } else {
        entry = proj.PlaceClipById(track, clip_id, at, timing.length, timing.offset);
    }
    cout << "Placed clip " << clip_id << " on track " << track << " -> entry " << entry << "\n";
}

void cmdFade(const string &project, int track, int entry_id,
             float fade_in, float fade_out) {
    KdenCLIProject proj;
    proj.Open(project);
    proj.FadeClip(track, entry_id, fade_in, fade_out);
    proj.Save(project);
    cout << "Applied fade to track " << track << " entry " << entry_id << "\n";
}

void cmdInfo(const string &project) {
    KdenCLIProject proj;
    proj.Open(project);
    proj.PrintInfo();
}


//Loads local config file
std::map<std::string, std::string> loadConfig(const std::string &path = "config.local") {
    std::map<std::string, std::string> config;
    std::ifstream file(path);
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        config[line.substr(0, eq)] = line.substr(eq + 1);
    }

    return config;
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
    place->add_option("-ss", place_cut_start, "Cut start (seconds, MM:SS, or HH:MM:SS)");
    place->add_option("-to", place_cut_end, "Cut end (seconds, MM:SS, or HH:MM:SS)");
    
    // --- fade ---
    //TODO: Create a general "effects" command that can call arbitrary effects of which fade is part of
    //Once you figure out how to set up mlt_services for different effects
    string fade_project;
    int fade_track = -1, fade_entry = -1;
    float fade_in = 0, fade_out = 0;

    auto *fade = app.add_subcommand("fade", "Apply fade to a placed clip");
    fade->add_option("project", fade_project, "Project file")->required();
    fade->add_option("--track,-t", fade_track, "Track ID")->required();
    fade->add_option("--entry,-e", fade_entry, "Entry ID (from place command)")->required();
    fade->add_option("--in,-i", fade_in, "Fade in duration in seconds");
    fade->add_option("--out,-o", fade_out, "Fade out duration in seconds");

    // --- info ---
    string info_project;

    auto *info = app.add_subcommand("info", "Print project info");
    info->add_option("project", info_project, "Project file")->required();

    CLI11_PARSE(app, argc, argv);

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
                         place_length, place_offset, place_cut_start, place_cut_end);
            else
                cmdPlace(proj, place_clip_id, place_track, place_at,
                         place_length, place_offset, place_cut_start, place_cut_end);

            proj.Save(place_project);
        }

        else if (fade->parsed())
            cmdFade(fade_project, fade_track, fade_entry, fade_in, fade_out);

        else if (info->parsed())
            cmdInfo(info_project);

    } catch (const exception &e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}