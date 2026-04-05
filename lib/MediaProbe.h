#ifndef MEDIAPROBE_H
#define MEDIAPROBE_H

#include <string>

namespace MediaProbe {
    float GetDuration(const std::string &filepath);
    struct StreamInfo {
        bool has_video;
        bool has_audio;
    };
    StreamInfo GetStreams(const std::string &filepath);
    //TODO: GetResolution, GetFPS, GetCodec, HasAudio, etc.
}

#endif