#include "Paths.h"
#include <cstdlib>
#include <vector>
#include <filesystem>
#include <stdexcept>

#if defined(_WIN32)
  #include <windows.h>
#elif defined(__APPLE__)
  #include <mach-o/dyld.h>
  #include <climits>
#else
  #include <unistd.h>
  #include <limits.h>
#endif

namespace fs = std::filesystem;

namespace {
    std::string g_share_override;
    std::string g_data_override;

    std::vector<std::string> default_share_paths() {
    #if defined(_WIN32)
        // TODO: verify these on an actual Windows install
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

    std::string resolve_share() {
        if (!g_share_override.empty())
            return g_share_override;

        const char* env = std::getenv("KDENLIVE_SHARE_PATH");
        if (env != nullptr && env[0] != '\0')
            return env;

        for (const auto &candidate : default_share_paths()) {
            if (fs::exists(candidate + "effects/"))
                return candidate;
        }

        throw std::runtime_error(
            "Could not locate Kdenlive share directory. "
            "Set KDENLIVE_SHARE_PATH");
    }

    fs::path executable_dir() {
    #if defined(_WIN32)
        wchar_t buf[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (len == 0 || len == MAX_PATH)
            return fs::current_path();
        return fs::path(buf).parent_path();
    #elif defined(__APPLE__)
        char buf[PATH_MAX];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) != 0)
            return fs::current_path();
        std::error_code ec;
        fs::path resolved = fs::canonical(buf, ec);
        if (ec) resolved = fs::path(buf);
        return resolved.parent_path();
    #else
        char buf[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len == -1)
            return fs::current_path();
        buf[len] = '\0';
        return fs::path(buf).parent_path();
    #endif
    }

    std::string resolve_data() {
        if (!g_data_override.empty())
            return g_data_override;

        const char* env = std::getenv("KDENCLI_DATA_DIR");
        if (env != nullptr && env[0] != '\0')
            return env;

        fs::path exe = executable_dir();

        std::vector<fs::path> candidates = {
            exe,
            exe.parent_path() / "share" / "kdencli",
        #if !defined(_WIN32) && !defined(__APPLE__)
            fs::path("/usr/share/kdencli"),
            fs::path("/usr/local/share/kdencli"),
        #endif
        };

        for (const auto& c : candidates) {
            if (fs::exists(c / "dependencies" / "empty_project.kdenlive"))
                return c.string();
        }

        throw std::runtime_error(
            "Could not locate KdenCLI data dir (dependencies/). "
            "Set KDENCLI_DATA_DIR.");
    }
}

namespace KdenPaths {
    void set_kdenlive_share(const std::string &path) {
        g_share_override = path;
    }

    const std::string& kdenlive_share() {
        static const std::string cached = resolve_share();
        return cached;
    }

    void set_data_dir(const std::string &path) {
        g_data_override = path;
    }

    const std::string& data_dir() {
        static const std::string cached = resolve_data();
        return cached;
    }

    std::string empty_project() {
        return (fs::path(data_dir()) / "dependencies" / "empty_project.kdenlive").string();
    }
}