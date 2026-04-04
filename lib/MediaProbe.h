// MediaProbe.h
#ifndef MEDIAPROBE_H
#define MEDIAPROBE_H

#include <string>

namespace MediaProbe {
    float GetDuration(const std::string &filepath);
    //TODO: GetResolution, GetFPS, GetCodec, HasAudio, etc.
}

#endif