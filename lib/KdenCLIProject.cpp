#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "KdenCLIProject.h"

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
    //TODO: It's not enough to just load the file - we need to reconstruct the project.
    //PrintInfo fails completely because of this.
    //It's a bit dirty to constantly sync the project and KdenliveFile ngl.
    //Might have to rethink architecture.
    //Maybe ditch KdenCLIProject and just use KdenliveFile directly.
}

ClipId KdenCLIProject::ImportClip(const std::string &filepath) {
    // check if already imported
    auto it = imported_clips.find(filepath);
    if (it != imported_clips.end()) {
        return it->second;
    }

    // verify file exists
    if (!fs::exists(filepath)) {
        throw std::runtime_error("File not found: " + filepath);
    }

    ClipId id = file.AddClipToBin(filepath);
    imported_clips[filepath] = id;
    return id;
}

TrackId KdenCLIProject::AddVideoTrack() {
    TrackId id = file.AddTrack(KdenliveFile::VIDEO);
    video_tracks.push_back(id);
    return id;
}

TrackId KdenCLIProject::AddAudioTrack() {
    TrackId id = file.AddTrack(KdenliveFile::AUDIO);
    audio_tracks.push_back(id);
    return id;
}

TrackEntryId KdenCLIProject::PlaceClip(TrackId track, ClipId clip,
                                        float timestamp, float length,
                                        float start_offset) {
    float track_length = file.GetTrackLength(track);
    float gap = timestamp - track_length;

    if (gap > 0.001f) {
        file.AddBlankToTrack(track, gap);
    }

    return file.AddClipToTrack(track, clip, length, start_offset);
}

void KdenCLIProject::FadeClip(TrackId track, TrackEntryId entry,
                               float fade_in, float fade_out) {
    file.FadeClip(track, entry, fade_in, fade_out);
}

TrackId KdenCLIProject::FindOrCreateTrack(std::vector<TrackId> &tracks,
                                           KdenliveFile::TrackType type,
                                           float timestamp) {
    for (auto id : tracks) {
        if (file.GetTrackLength(id) <= timestamp) {
            return id;
        }
    }

    TrackId new_track = file.AddTrack(type);
    tracks.push_back(new_track);
    return new_track;
}

KdenCLIProject::Placement KdenCLIProject::PlaceOnVideoTrack(
        ClipId clip, float timestamp, float length, float start_offset) {
    TrackId track = FindOrCreateTrack(video_tracks, KdenliveFile::VIDEO, timestamp);
    TrackEntryId entry = PlaceClip(track, clip, timestamp, length, start_offset);
    return {track, entry};
}

KdenCLIProject::Placement KdenCLIProject::PlaceOnAudioTrack(
        ClipId clip, float timestamp, float length, float start_offset) {
    TrackId track = FindOrCreateTrack(audio_tracks, KdenliveFile::AUDIO, timestamp);
    TrackEntryId entry = PlaceClip(track, clip, timestamp, length, start_offset);
    return {track, entry};
}

float KdenCLIProject::GetTrackLength(TrackId track) {
    return file.GetTrackLength(track);
}

void KdenCLIProject::PrintInfo() {
    std::cout << "Video tracks: " << video_tracks.size() << "\n";
    for (auto id : video_tracks) {
        std::cout << "  Track " << id << " length: " << file.GetTrackLength(id) << "s\n";
    }
    std::cout << "Audio tracks: " << audio_tracks.size() << "\n";
    for (auto id : audio_tracks) {
        std::cout << "  Track " << id << " length: " << file.GetTrackLength(id) << "s\n";
    }
    std::cout << "Imported clips: " << imported_clips.size() << "\n";
    for (const auto &[path, id] : imported_clips) {
        std::cout << "  [" << id << "] " << path << "\n";
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