#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "KdenCLIProject.h"
#include "MediaProbe.h"
#include "Effect.h"
#include <cstdlib>
#include "Config.h"
#include <climits>

namespace fs = std::filesystem;

KdenCLIProject::KdenCLIProject() {
    //file.SetProfile(30, 1920, 1080);
}


void KdenCLIProject::SetProfile(float framerate, int width, int height) {
    file.SetProfile(static_cast<int>(framerate), width, height);
}

void KdenCLIProject::Open(const std::string &filepath) {
    file.LoadFromFile(filepath);
    project_path = filepath;

}

ClipId KdenCLIProject::ImportClip(const std::string &filepath) {
    // check if already imported
    ClipId existing = file.FindClipByResource(filepath);
    if (existing >= 0) {
        return existing; 
    }
    // verify file exists
    if (!fs::exists(filepath)) {
        throw std::runtime_error("File not found: " + filepath);
    }

    ClipId id = file.AddClipToBin(filepath);
    return id;
}

TrackId KdenCLIProject::AddVideoTrack() {
    TrackId id = file.AddTrack(TrackType::VIDEO);
    //video_tracks.push_back(id);
    return id;
}

TrackId KdenCLIProject::AddAudioTrack() {
    TrackId id = file.AddTrack(TrackType::AUDIO);
    return id;
}

TrackEntryId KdenCLIProject::PlaceClipById(TrackId track, ClipId clip,
                                            float timestamp, float length,
                                            float start_offset) {
    return file.InsertClipAtPosition(track, clip, timestamp, length, start_offset);
}


TrackEntryId KdenCLIProject::PlaceClipByFilename(const std::string &filepath, TrackId track,
                                        float timestamp, float length,
                                        float start_offset) {
    ClipId clip = file.FindClipByResource(filepath);
    if (clip < 0) {
        clip = ImportClip(filepath);
    }
    return PlaceClipById(track, clip, timestamp, length, start_offset);
}
TrackEntryId KdenCLIProject::PlaceFullClip(const std::string &filepath, TrackId track, 
    float timestamp, float start_offset) {
    float length = MediaProbe::GetDuration(filepath) - start_offset;
    return PlaceClipByFilename(filepath, track, timestamp, length, start_offset);
}

void KdenCLIProject::FadeClip(TrackId track, TrackEntryId entry,
                               float fade_in, float fade_out) {
    file.FadeClip(track, entry, fade_in, fade_out);
}

void KdenCLIProject::ApplyEffect(TrackId track, TrackEntryId entry,
                                  const std::string &effect_id,
                                  EffectContext ctx) {
    catalog.load_from_directory(KdenConfig::share_path() + "effects/");
    const EffectDefinition* def = catalog.get(effect_id);
    if (!def)
        throw std::runtime_error("Effect not found in catalog: " + effect_id);
    file.ApplyEffect(track, entry, *def, ctx);
}

TrackId KdenCLIProject::FindOrCreateTrack(TrackType type,
                                           float timestamp) {
    for (const auto &track : file.GetTracks()) {
        if (track.type == type && track.length <= timestamp) {
            return track.id;
        }
    }
    return file.AddTrack(type);
}

Placement KdenCLIProject::PlaceOnVideoTrack(
        ClipId clip, float timestamp, float length, float start_offset) {
    TrackId track = FindOrCreateTrack(TrackType::VIDEO, timestamp);
    TrackEntryId entry = PlaceClipById(track, clip, timestamp, length, start_offset);
    return {track, entry};
}

Placement KdenCLIProject::PlaceOnAudioTrack(
        ClipId clip, float timestamp, float length, float start_offset) {
    TrackId track = FindOrCreateTrack(TrackType::AUDIO, timestamp);
    TrackEntryId entry = PlaceClipById(track, clip, timestamp, length, start_offset);
    return {track, entry};
}

float KdenCLIProject::GetTrackLength(TrackId track) {
    return file.GetTrackLength(track);
}

void KdenCLIProject::PrintInfo() {
    auto tracks = file.GetTracks();
    auto clips = file.GetClips();

    std::cout << "Tracks: " << tracks.size() << "\n";
    for (const auto &t : tracks) {
        std::string type = (t.type == TrackType::AUDIO) ? "audio" : "video";
        std::cout << "  [" << t.id << "] " << type << " - " << t.length << "s\n";
    }

    std::cout << "Clips: " << clips.size() << "\n";
    for (const auto &c : clips) {
        std::cout << "  [" << c.id << "] " << c.resource << "\n";
    }
}

void KdenCLIProject::Save(const std::string &filepath) {
    fs::path p(filepath);
    std::string dir = p.parent_path().string();
    std::string name = p.filename().string();

    if (dir.empty()) {
        file.SaveToFile(name);
    } else {
        file.SaveToFile(name, dir);
    }
}

std::string KdenCLIProject::ToString() {
    return file.ToString();
}

std::string KdenCLIProject::GetClipResource(ClipId id) {
    for (const auto &c : file.GetClips()) {
        if (c.id == id) return c.resource;
    }
    throw std::runtime_error("Clip ID not found: " + std::to_string(id));
}

std::vector<KdenliveFile::TrackInfo> KdenCLIProject::GetTracks() {
    //This is so dumb dawg, but whatever
    return file.GetTracks();
}

TrackId KdenCLIProject::FindPairedTrack(TrackId track) {
    auto tracks = file.GetTracks();

    TrackType src_type = TrackType::VIDEO;
    for (const auto &t : tracks) {
        if (t.id == track) { src_type = t.type; break; }
    }

    TrackType target = (src_type == TrackType::VIDEO)
        ? TrackType::AUDIO : TrackType::VIDEO;

    //Identify the best audio/video track to pair with its "opposite"(not really but you get me) in this case
    TrackId best = -1;
    int best_dist = INT_MAX;
    for (const auto &t : tracks) {
        if (t.type == target && abs(t.id - track) < best_dist) {
            best = t.id;
            best_dist = abs(t.id - track);
        }
    }
    return best;
}