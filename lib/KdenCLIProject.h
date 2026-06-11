#ifndef KDENCLIPROJECT_H
#define KDENCLIPROJECT_H

#include <string>
#include <vector>
#include <map>
#include "KdenliveFile.h"
#include "types.h"
#include "Effect.h"
#include "TitleBuilder.h"

class KdenCLIProject {
public:
    // Create a new empty project
    KdenCLIProject();

    // Open an existing project
    void Open(const std::string &filepath);
    void LoadCatalog();
    const EffectCatalog& GetCatalog() const { return catalog; }    
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
    TrackEntryId PlaceClipById(TrackId track, ClipId clip, float timestamp,
                           float length, float start_offset = 0);
    TrackEntryId PlaceClipByFilename(const std::string &filepath, TrackId track,
                                        float timestamp, float length, float start_offset = 0);
    TrackEntryId PlaceFullClip(const std::string &filepath, TrackId track, 
                                        float timestamp, float start_offset = 0);


    // Apply fade effect to a placed clip
    void FadeClip(TrackId track, TrackEntryId entry,
                  float fade_in, float fade_out);

    // Apply an effect to a placed clip
    void ApplyEffect(TrackId track, TrackEntryId entry, const std::string &effect_id, EffectContext ctx);

    // Place a clip, auto-selecting or creating a track with room.
    // Returns the TrackId used and TrackEntryId.
    Placement PlaceOnVideoTrack(ClipId clip, float timestamp,
                                float length, float start_offset = 0);
    Placement PlaceOnAudioTrack(ClipId clip, float timestamp,
                                float length, float start_offset = 0);
    
    Placement AddTitle(TitleBuilder::TitleParams params,
                        int track, float timestamp, float duration_s);

    // Query
    float GetTrackLength(TrackId track);
    void PrintInfo();

    // Save
    void Save(const std::string &filepath);
    std::string ToString();

    // Getters
    std::string GetClipResource(ClipId id);
    std::string GetProjectPath() { return project_path; };
    std::vector<KdenliveFile::TrackInfo> GetTracks();
    TrackId FindPairedTrack(TrackId track);
    KdenliveFile::ProfileInfo GetProfile() { return file.GetProfile(); }

private:
    TrackId FindOrCreateTrack(TrackType type,
                              float timestamp);

    KdenliveFile file;
    std::string project_path;
    EffectCatalog catalog;
};

#endif