#ifndef KDENCLI_TYPES_H
#define KDENCLI_TYPES_H

typedef int ClipId;
typedef int TrackId;
typedef int TrackEntryId;

// Data types that I want cluttering the global namespace (Sorry Josh!)
enum class TrackType { VIDEO, AUDIO };

// CLI place behavior
enum class PlaceMode { AUTO, VIDEO_ONLY, AUDIO_ONLY };

struct Placement {
    TrackId track;
    TrackEntryId entry;
};


#endif