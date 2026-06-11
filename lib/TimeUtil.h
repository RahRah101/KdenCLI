#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include <string>

namespace TimeUtil {
    // "H:M:S" timecode -> seconds.
    float parseTimecode(const char* tc);

    // Flexible timestamp -> seconds. Accepts "H:M:S", "M:S", or bare seconds.
    float parseTimestamp(const std::string &input);

    // frame count -> "HH:MM:SS:FF" (kdenlive:duration format)
    std::string framesToTimecode(int frames, int fps);
}

#endif