#ifndef EFFECT_H
#define EFFECT_H

#include <string>
#include <vector>
#include <map>
#include "types.h"

//General effect interface

enum class ParamType {
    FIXED,
    CONSTANT,
    POSITION,
    KEYFRAME,
    ANIMATED,
    MULTISWITCH,
    BOOL,
    LIST,
    URL,
    DOUBLE,
    BEZIER_SPLINE,
    UNKNOWN
};

struct EffectParameter {
    std::string name;
    ParamType type;
    std::string default_value;
};

struct EffectDefinition {
    std::string tag;       // => mlt_service (TODO: might want to make this more explicit?)
    std::string id;        // => kdenlive_id
    std::string type;      // "audio", "video", ""
    bool unique = false;
    std::vector<EffectParameter> parameters;
};

struct EffectContext {
    float in_time  = 0;
    float out_time = 0;
    std::map<std::string, std::string> overrides;
};

class EffectCatalog {
public:
    void register_effect(const EffectDefinition &def);
    void load_from_directory(const std::string &path);
    const EffectDefinition* get(const std::string &id) const;
    std::vector<std::string> list_ids() const;
private:
    EffectDefinition parse_effect_file(const std::string &filepath);

    std::map<std::string, EffectDefinition> effects;
};

#endif