#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "KdenCLIProject.h"
#include "MediaProbe.h"

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
    TrackId id = file.AddTrack(KdenliveFile::VIDEO);
    //video_tracks.push_back(id);
    return id;
}

TrackId KdenCLIProject::AddAudioTrack() {
    TrackId id = file.AddTrack(KdenliveFile::AUDIO);
    return id;
}

//TODO: Document better
//if length < -1, full clip
TrackEntryId KdenCLIProject::PlaceClipById(TrackId track, ClipId clip,
                                        float timestamp, float length,
                                        float start_offset) {
    float track_length = file.GetTrackLength(track);
    float gap = timestamp - track_length;

    if (gap > 0.001f) {
        file.AddBlankToTrack(track, gap);
    }

    return file.AddClipToTrack(track, clip, length, start_offset);
}


TrackEntryId KdenCLIProject::PlaceClipByFilename(const std::string &filepath, TrackId track,
                                        float timestamp, float length,
                                        float start_offset) {
    ClipId clip = file.FindClipByResource(filepath);
    if (clip < 0) {
        throw std::runtime_error("Clip not in bin: " + filepath + "\nRun import first.");
    }

    //If length is < 0, use full clip
    if (length < 0) {
        length = MediaProbe::GetDuration(filepath) - start_offset;
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

TrackId KdenCLIProject::FindOrCreateTrack(KdenliveFile::TrackType type,
                                           float timestamp) {
    for (const auto &track : file.GetTracks()) {
        if (track.type == type && track.length <= timestamp) {
            return track.id;
        }
    }
    return file.AddTrack(type);
}

KdenCLIProject::Placement KdenCLIProject::PlaceOnVideoTrack(
        ClipId clip, float timestamp, float length, float start_offset) {
    TrackId track = FindOrCreateTrack(KdenliveFile::VIDEO, timestamp);
    TrackEntryId entry = PlaceClipById(track, clip, timestamp, length, start_offset);
    return {track, entry};
}

KdenCLIProject::Placement KdenCLIProject::PlaceOnAudioTrack(
        ClipId clip, float timestamp, float length, float start_offset) {
    TrackId track = FindOrCreateTrack(KdenliveFile::AUDIO, timestamp);
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
        std::string type = (t.type == KdenliveFile::AUDIO) ? "audio" : "video";
        std::cout << "  [" << t.id << "] " << type << " - " << t.length << "s\n";
    }

    std::cout << "Clips: " << clips.size() << "\n";
    for (const auto &c : clips) {
        std::cout << "  [" << c.id << "] " << c.resource << "\n";
    }
}

void KdenCLIProject::Save(const std::string &filepath) {
    // extract directory and filename
    fs::path p(filepath);
    std::string dir = p.parent_path().string();
    std::string name = p.stem().string();

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