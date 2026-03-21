#ifndef KDENCLIPROJECT_H
#define KDENCLIPROJECT_H

#include <string>
#include <vector>
#include <map>
#include "KdenliveFile.h"

class KdenCLIProject {
public:
    // Create a new empty project
    KdenCLIProject();

    // Set the video profile
    void SetProfile(float framerate, int width, int height);

    // Import a clip into the project bin. Takes a full filepath.
    // Returns a ClipId for use with PlaceClip.
    ClipId ImportClip(const std::string &filepath);

    // Add a track. Returns TrackId.
    TrackId AddVideoTrack();
    TrackId AddAudioTrack();

    // Place a clip on a specific track at a timestamp.
    // Handles blank insertion automatically.
    // Returns the TrackEntryId for applying effects later.
    TrackEntryId PlaceClip(TrackId track, ClipId clip, float timestamp,
                           float length, float start_offset = 0);

    // Apply fade effect to a placed clip
    void FadeClip(TrackId track, TrackEntryId entry,
                  float fade_in, float fade_out);

    // Place a clip, auto-selecting or creating a track with room.
    // Returns the TrackId used and TrackEntryId.
    struct Placement {
        TrackId track;
        TrackEntryId entry;
    };
    Placement PlaceOnVideoTrack(ClipId clip, float timestamp,
                                float length, float start_offset = 0);
    Placement PlaceOnAudioTrack(ClipId clip, float timestamp,
                                float length, float start_offset = 0);

    // Query
    float GetTrackLength(TrackId track);
    void PrintInfo();

    // Save
    void Save(const std::string &filepath);
    std::string ToString();

private:
    TrackId FindOrCreateTrack(std::vector<TrackId> &tracks,
                              KdenliveFile::TrackType type,
                              float timestamp);

    KdenliveFile file;
    std::vector<TrackId> video_tracks;
    std::vector<TrackId> audio_tracks;
    std::map<std::string, ClipId> imported_clips; // path -> ClipId dedup
};

#endif