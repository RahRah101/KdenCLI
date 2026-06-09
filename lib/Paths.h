#ifndef PATHS_H
#define PATHS_H

#include <string>

namespace KdenPaths {
    // Kdenlive's installed share dir
    const std::string& kdenlive_share();
    void set_kdenlive_share(const std::string &path);

    // KdenCLI's own bundled data dir
    const std::string& data_dir();
    void set_data_dir(const std::string &path);

    // Convenience: full path to the empty project template
    std::string empty_project();
}

#endif