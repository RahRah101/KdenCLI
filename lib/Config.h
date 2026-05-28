#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace KdenConfig {
    const std::string& share_path();

    void set_share_path(const std::string &path);
}

#endif