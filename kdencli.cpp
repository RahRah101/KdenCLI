#include <iostream>
#include <string>
#include <vector>
#include "lib/KdenCLIProject.h"
#include "lib/CLI11.hpp"

using namespace std;


/** COMPILE:
 *  g++ *.cpp lib/*.cpp -g -o kdencli
 *  
 *  RUN:
 *  ./kdencli
 */

#include <fstream>
#include <string>
#include <map>





//Loads local config file
std::map<std::string, std::string> loadConfig(const std::string &path = "config.local") {
    std::map<std::string, std::string> config;
    std::ifstream file(path);
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        config[line.substr(0, eq)] = line.substr(eq + 1);
    }

    return config;
}






int main(int argc, char** argv){
    auto config = loadConfig();
    
    //Just a dumb ass test to see if KdenCLIProject works
    std::cout << "1. Creating project...\n";
    KdenCLIProject proj;
    
    std::cout << "2. Setting profile...\n";
    proj.SetProfile(60, 1920, 1080);
    
    std::cout << "3. Importing video...\n";
    std::string video_path = config["MEDIA_FOLDER_PATH"] + "great_expanse.mp4";
    std::cout << "   Path: " << video_path << "\n";
    ClipId video = proj.ImportClip(video_path);
    
    std::cout << "4. Importing audio...\n";
    ClipId audio = proj.ImportClip(config["MEDIA_FOLDER_PATH"] + "Free_Test_Data_500KB_MP3.mp3");
    
    std::cout << "5. Adding video track...\n";
    TrackId vtrack = proj.AddVideoTrack();
    
    std::cout << "6. Placing clip...\n";
    TrackEntryId entry = proj.PlaceClip(vtrack, video, 0, 10);
    
    std::cout << "7. Fading...\n";
    proj.FadeClip(vtrack, entry, 1.0, 0.5);
    
    std::cout << "8. Auto-placing audio...\n";
    auto [track, eid] = proj.PlaceOnAudioTrack(audio, 0, 20);
    
    std::cout << "9. Saving...\n";
    proj.Save(config["OUTPUT_FOLDER_PATH"] + "output_project.kdenlive");
    
    std::cout << "Done.\n";
    return 0;
}