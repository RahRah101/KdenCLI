#include <iostream>
#include <string>
#include <vector>
#include "lib/KdenliveProject.h"

using namespace std;


/** COMPILE:
 *  g++ *.cpp lib/*.cpp -g -o example.exe
 *  
 *  RUN:
 *  example.exe
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
    //EDIT CONFIG FILE ON YOUR COMPUTER TO SET PATHS
    /*
    Example config file:
    MEDIA_FOLDER_PATH=[PATH TO YOUR MEDIA FOLDER]
    OUTPUT_FOLDER_PATH=[PATH TO YOUR OUTPUT FOLDER]
    */
    auto config = loadConfig();

    //TODO: Add command line arguments
    //kdencli --framerate 60 --width 1920 --height 1080 [filename]
    //kdencli -f 60 -w 1920 -h 1080 [filename]
    

    KdenliveProject proj;
    proj.SetProfile( 60, 1920, 1080 );


    //kdencli --clip filename
    // Add a video to the video track.
    proj.CreateClipOnVideoTrack( 0, "great_expanse.mp4", 10 );

    // Add some audio to the audio track.
    proj.CreateClipOnAudioTrack( 0, "Free_Test_Data_500KB_MP3.mp3", 20 );

    // Add a clip to both the audio and video tracks
    Clip* clip_1 = proj.CreateClip("cavern_clinger_boss.mp4", 10);
    proj.AddClipToVideoTrack(9, clip_1);
    proj.AddClipToAudioTrack(9, clip_1);

    // You can set the length and fade of the clip after adding it
    clip_1->SetBounds(15);
    clip_1->SetFadeOffsets(1, 0);

    // Generate the .kdenlive file. The resulting file should open in Kdenlive.
    vector<string> media_paths = {config["MEDIA_FOLDER_PATH"]};
    proj.SaveToFile(media_paths, "example_generated_project", config["OUTPUT_FOLDER_PATH"]);

    return 0;
}
