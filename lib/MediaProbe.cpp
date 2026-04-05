#include "MediaProbe.h"
#include <cstdio>
#include <array>
#include <stdexcept>

namespace MediaProbe {

    float GetDuration(const std::string &filepath) {
        std::string cmd = "ffprobe -v error -show_entries format=duration "
                        "-of default=nw=1:nk=1 \"" + filepath + "\"";

        std::array<char, 128> buffer;
        std::string result;

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            throw std::runtime_error("ffprobe not found. Install ffmpeg or add it to PATH.");
        }

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }

        int status = pclose(pipe);
        if (status != 0 || result.empty()) {
            throw std::runtime_error("ffprobe failed for: " + filepath);
        }

        return std::stof(result);
    }

    StreamInfo GetStreams(const std::string &filepath) {
    std::string cmd = "ffprobe -v error -show_entries stream=codec_type "
                      "-of csv=p=0 \"" + filepath + "\"";

    std::array<char, 128> buffer;
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        throw std::runtime_error("ffprobe not found");

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        result += buffer.data();

    pclose(pipe);

    return {
        result.find("video") != std::string::npos,
        result.find("audio") != std::string::npos
    };
}

}