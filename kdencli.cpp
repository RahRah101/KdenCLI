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

    auto *place = app.add_subcommand("place", "Place a clip on a track");
    place->add_option("project", place_project, "Project file")->required();
    place->add_option("--track,-t", place_track, "Track ID")->required();
    auto *place_clip_group = place->add_option_group("clip", "Clip to place (pick one)");
    place_clip_group->add_option("--clipid,-c", place_clip_id, "Clip ID");
    place_clip_group->add_option("--file,-f", place_clip_file, "Clip filepath");
    place_clip_group->require_option(1);
    place->add_option("--at,-a", place_at, "Timestamp in seconds (default: 0)");
    place->add_option("--length,-l", place_length, "Clip length in seconds")->required();
    place->add_option("--offset,-o", place_offset, "Start offset within clip (default: 0)");

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


    //Load config
    /*
    Example config file:
    KDENLIVE_SHARE_ROOT=/usr/share/kdenlive/
    */
    auto config = loadConfig();
    
    
    //This is ugly as hell
    //TODO : Clean up with function calls later
    try {
        if (create->parsed()) {
        KdenCLIProject proj;
        proj.SetProfile(create_fps, create_width, create_height);
        proj.AddVideoTrack();
        proj.AddAudioTrack();
        proj.Save(create_output);
        cout << "Created project: " << create_output << "\n";
        cout << "  Profile: " << create_width << "x" << create_height
                 << " @ " << create_fps << " fps\n";
        }

        else if (import_cmd->parsed()) {
            KdenCLIProject proj;
            proj.Open(import_project);
            ClipId id = proj.ImportClip(import_filepath);
            proj.Save(import_project);
            cout << "Imported clip " << id << ": " << import_filepath << "\n";
        }

        else if (add_track->parsed()) {
            KdenCLIProject proj;
            proj.Open(track_project);
            TrackId id;
            if (track_type == "audio") {
                id = proj.AddAudioTrack();
            } else {
                id = proj.AddVideoTrack();
            }
            proj.Save(track_project);
            cout << "Added " << track_type << " track " << id << "\n";
        }

        else if (place->parsed()) {
            KdenCLIProject proj;
            proj.Open(place_project);

            TrackEntryId entry;
            if (!place_clip_file.empty()) {
                entry = proj.PlaceClipByFilename(place_clip_file, place_track,
                                                  place_at, place_length, place_offset);
            } else {
                entry = proj.PlaceClipById(place_track, place_clip_id,
                                            place_at, place_length, place_offset);
            }

            proj.Save(place_project);
            cout << "Placed on track " << place_track << " -> entry " << entry << "\n";
        }

        else if (fade->parsed()) {
            KdenCLIProject proj;
            proj.Open(fade_project);
            proj.FadeClip(fade_track, fade_entry, fade_in, fade_out);
            proj.Save(fade_project);
            cout << "Applied fade to track " << fade_track
                 << " entry " << fade_entry << "\n";
        }

        else if (info->parsed()) {
            KdenCLIProject proj;
            proj.Open(info_project);
            proj.PrintInfo();
        }

    } catch (const exception &e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
