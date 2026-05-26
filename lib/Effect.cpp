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
    if (!(fs::exists(path) || fs::is_directory(path))) {
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
    return ParamType::UNKNOWN;
}

EffectDefinition EffectCatalog::parse_effect_file(const std::string &filepath) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filepath.c_str() ) != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("Failed to load effect file: " + filepath);
    }

    tinyxml2::XMLElement* root = doc.RootElement();
    if (root == nullptr) {
        throw std::runtime_error("Effect file has no root element: " + filepath);
    }
    
    EffectDefinition definition;

    //Little lambda helper to handle cases where attribute/param shit returns nullptr 
    //and we assign it to a string and get undefined behaviors like complete retards
    //You could also use the ternary operator for each calls but using that lambda costs nothing and is more readable (arguably???)
    auto str_or_empty = [](const char* val) -> std::string {
        return val ? val : "";
    };

    definition.tag  = str_or_empty(root->Attribute("tag")); // => mlt_service (dumb reminder!!!)
    definition.id   = str_or_empty(root->Attribute("id"));
    definition.type = str_or_empty(root->Attribute("type"));

    if (definition.tag.empty() || definition.id.empty()) {
        throw std::runtime_error("Not a valid effect definition: " + filepath);
    }

    const char* unique_attr = root->Attribute("unique");
    definition.unique = (unique_attr != nullptr && strcmp(unique_attr, "1") == 0);
    

    tinyxml2::XMLElement* param = root->FirstChildElement("parameter");
    
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

