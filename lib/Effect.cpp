#include <string>
#include <vector>
#include <map>
#include "types.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "tinyxml2.h"
#include "Effect.h"

namespace fs = std::filesystem;


void EffectCatalog::register_effect(const EffectDefinition &def) {
    //TODO: Implement register_effect
    effects[def.id] = def;
}

const EffectDefinition* EffectCatalog::get(const std::string &id) const {
    auto it = effects.find(id);
    if (it == effects.end()) {
        return nullptr;
    }
    return &it->second;
}


void EffectCatalog::load_from_directory(const std::string &path){
    if (!fs::exists(path) || !fs::is_directory(path)) {
        throw std::runtime_error("Effect directory not found: " + path);
    }

    int loaded = 0;
    int skipped = 0;

    for (const auto &entry : fs::directory_iterator(path)) {
        if (entry.path().extension() != ".xml") {
            continue;
        }
        try {
            EffectDefinition def = parse_effect_file(entry.path().string());
            register_effect(def);
            loaded++;
        } catch (const std::exception &e) {
            std::cerr << "Failed to load effect " << entry.path().string() << ": " << e.what() << std::endl;
            skipped++;
        }
    }
    //TODO: Delete this ugly logging thing
    std::cout << "Loaded : " << loaded << " effects\nSkipped :" << skipped << std::endl;
}

ParamType parse_param_type(const std::string &type_str) {
    if (type_str == "fixed")       return ParamType::FIXED;
    if (type_str == "constant")    return ParamType::CONSTANT;
    if (type_str == "position")    return ParamType::POSITION;
    if (type_str == "keyframe")    return ParamType::KEYFRAME;
    if (type_str == "animated")    return ParamType::ANIMATED;
    if (type_str == "multiswitch") return ParamType::MULTISWITCH;
    if (type_str == "bool")        return ParamType::BOOL;
    if (type_str == "list")          return ParamType::LIST;
    if (type_str == "url")           return ParamType::URL;
    if (type_str == "double")        return ParamType::DOUBLE;
    if (type_str == "bezier_spline") return ParamType::BEZIER_SPLINE;
    return ParamType::UNKNOWN;
}

std::vector<std::string> EffectCatalog::list_ids() const {
    std::vector<std::string> ids;
    for (const auto &pair : effects)
        ids.push_back(pair.first);
    return ids;
}

EffectDefinition EffectCatalog::parse_effect_file(const std::string &filepath) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filepath.c_str() ) != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("Failed to load effect file: " + filepath);
    }

    tinyxml2::XMLElement* root = doc.RootElement();
    tinyxml2::XMLElement* effect_el = root;
    if (strcmp(root->Name(), "effect") != 0)
        effect_el = root->FirstChildElement("effect");

    if (effect_el == nullptr)
        throw std::runtime_error("No effect element found: " + filepath);
    
    EffectDefinition definition;

    //Little lambda helper to handle cases where attribute/param shit returns nullptr 
    //and we assign it to a string and get undefined behaviors like complete retards
    //You could also use the ternary operator for each calls but using that lambda costs nothing and is more readable (arguably???)
    auto str_or_empty = [](const char* val) -> std::string {
        return val ? val : "";
    };

    definition.tag  = str_or_empty(effect_el->Attribute("tag")); // => mlt_service (dumb reminder!!!)
    definition.id   = str_or_empty(effect_el->Attribute("id"));
    definition.type = str_or_empty(effect_el->Attribute("type"));

    if (definition.tag.empty())
        throw std::runtime_error("Not a valid effect definition: " + filepath);

    if (definition.id.empty())
        definition.id = definition.tag;

    const char* unique_attr = effect_el->Attribute("unique");
    definition.unique = (unique_attr != nullptr && strcmp(unique_attr, "1") == 0);
    definition.ladspa_lib  = str_or_empty(effect_el->Attribute("library"));
    definition.ladspaid = str_or_empty(effect_el->Attribute("ladspaid"));

    tinyxml2::XMLElement* param = effect_el->FirstChildElement("parameter");
    
    while (param != nullptr) {
        EffectParameter parameter;
        
        const char* type_str = param->Attribute("type");
        const char* name_str = param->Attribute("name");
        const char* default_str = param->Attribute("default");
        
        if (!type_str || !name_str) {
            param = param->NextSiblingElement("parameter");
            continue;
        }
        
        parameter.type = parse_param_type(type_str);
        parameter.name = name_str;
        parameter.default_value = str_or_empty(default_str);
        
        definition.parameters.push_back(parameter);
        param = param->NextSiblingElement("parameter");
    }
    
    return definition;
}

