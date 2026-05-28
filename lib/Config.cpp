#include "Config.h"
#include <cstdlib>
#include <vector>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {
    std::string g_override;

    std::vector<std::string> default_share_paths() {
    // TODO: verify these on an actual Windows install
    #if defined(_WIN32)
        return {
            "C:/Program Files/kdenlive/data/",
            "C:/Program Files (x86)/kdenlive/data/",
        };
    #elif defined(__APPLE__)
        // TODO: verify these on an actual macOS install
        return {
            "/Applications/kdenlive.app/Contents/Resources/",
            "/usr/local/share/kdenlive/",
        };
    #else
        return {
            "/usr/share/kdenlive/",
            "/usr/local/share/kdenlive/",
            "/var/lib/flatpak/app/org.kde.kdenlive/current/active/files/share/kdenlive/",
        };
    #endif
    }

    std::string resolve() {
        // 1. Explicit override
        if (!g_override.empty())
            return g_override;

        // 2. Environment variable
        const char* env = std::getenv("KDENLIVE_SHARE_PATH");
        if (env != nullptr && env[0] != '\0')
            return env;

        // 3. Platform defaults — first one that actually has effects/
        for (const auto &candidate : default_share_paths()) {
            if (fs::exists(candidate + "effects/"))
                return candidate;
        }

        throw std::runtime_error(
            "Could not locate Kdenlive share directory. "
            "Set KDENLIVE_SHARE_PATH");
    }
}

namespace KdenConfig {
    void set_share_path(const std::string &path) {
        g_override = path;
    }

    const std::string& share_path() {
        static const std::string cached = resolve();
        return cached;
    }
}