#include "TimeUtil.h"
#include <cstdio>

namespace TimeUtil {

float parseTimecode(const char* tc) {
    if (!tc) return 0;
    int h, m;
    float s;
    if (sscanf(tc, "%d:%d:%f", &h, &m, &s) == 3)
        return h * 3600.0f + m * 60.0f + s;
    return 0;
}

float parseTimestamp(const std::string &input) {
    int h, m;
    float s;
    if (sscanf(input.c_str(), "%d:%d:%f", &h, &m, &s) == 3)
        return h * 3600.0f + m * 60.0f + s;
    if (sscanf(input.c_str(), "%d:%f", &m, &s) == 2)
        return m * 60.0f + s;
    return std::stof(input);
}

std::string framesToTimecode(int frames, int fps) {
    if (fps <= 0) fps = 1;            // guard div-by-zero
    int total_secs = frames / fps;
    int ff = frames % fps;
    int ss = total_secs % 60;
    int mm = (total_secs / 60) % 60;
    int hh = total_secs / 3600;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d:%02d", hh, mm, ss, ff);
    return std::string(buf);
}

}  // namespace TimeUtil