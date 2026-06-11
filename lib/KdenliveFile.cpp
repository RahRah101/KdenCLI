#include <iomanip>
#include <sstream>
#include <cmath>
#include <iostream>
#include "KdenliveFile.h"
#include <filesystem>
#include <unistd.h>
#include <limits.h>
#include "Paths.h"
#include "TimeUtil.h"
#include <cstdio>


using namespace std;
using namespace tinyxml2;
namespace fs = std::filesystem;


ifstream openInputFile(const string &file_path){
	//open file
	ifstream input_file(file_path);

	//check if file was opened successfully
	if(!input_file.good()){
		cerr << "File '" << file_path << "' not found";
		exit(-1);
	}

	return input_file;
}

ofstream openOutputFile(const string &filePath){
	//open file
	ofstream output_file(filePath);

	return output_file;
}

static std::vector<std::string> split_lines(const std::string &s) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string line;
    while (std::getline(ss, line))
        result.push_back(line);
    return result;
}

void KdenliveFile::ApplyEffect(TrackId track_id, TrackEntryId entry_id,
                                const EffectDefinition &def,
                                const EffectContext &ctx) {
    // Get this TrackEntry
    const TrackEntry &this_entry = track_entries[track_id][entry_id];
    // Don't allow a filter to be applied to Blank entries
    if (this_entry.entry_type == EntryType::BLANK)
        return;

    // Get the entry in the doc
    const int playlist_index = track_id * 2;
    const string playlist_id = "playlist" + to_string(playlist_index);
    XMLElement* entry = FindPlaylistEntry(playlist_id.c_str(), entry_id);

    // Create the filter element with its in/out MLT attributes
    const string filter_id = "filter" + to_string(filter_count);
    XMLElement* filter = CreateFilterElement(filter_id.c_str(), ctx.in_time, ctx.out_time);

    std::string service = def.tag;
    std::string kdenlive_id = def.id;
    //Workaround for LADSPA effects. This is disgusting.
    //But I noticed in the xml they are actually written as "ladspa.<plugin_id>"
    //For whatever reason
    //TODO : Fix LADSAP effects not working properly.
    if (def.tag == "ladspa" && !def.ladspaid.empty()) {
        service = "ladspa." + def.ladspaid;
        kdenlive_id = service;
    }

    AddPropertyElement(filter, "mlt_service", service.c_str());
    AddPropertyElement(filter, "kdenlive_id", kdenlive_id.c_str());


    // Walk the effect's parameter definitions and emit properties
    for (const auto &param : def.parameters) {
        if (param.type == ParamType::UNKNOWN)
            continue;

        //use caller override from the Effect Context if provided, else fall back to default
        std::string value = param.default_value;
        auto override_it = ctx.overrides.find(param.name);
        if (override_it != ctx.overrides.end())
            value = override_it->second;

        if (param.type == ParamType::MULTISWITCH) {
            // MULTISWITCH encodes multiple property name/value pairs
            // separated by newline (&#10; in XML). Emit one property per pair.
            // e.g. name="level\nalpha", value="1\n0=0;-1=1"
            // emits: level=1 and alpha=0=0;-1=1
            auto names  = split_lines(param.name);
            auto values = split_lines(value);

            //We can safely assume that names.size() == values.size(),
            //as multi-switch encodes name/value pairs so we traverse both
            //As if that was a map...
            //This should be consistent in the XML and for now I cba to write 
            //a failsafe/exception
            for (size_t i = 0; i < names.size() && i < values.size(); i++) {
                AddPropertyElement(filter, names[i].c_str(), values[i].c_str());
            }
        } else {
            // All other types: one property, one value
            AddPropertyElement(filter, param.name.c_str(), value.c_str());
        }
    }

    // Attach filter to the entry and update internal counter
    entry->InsertEndChild(filter);
    filter_count++;
}

void KdenliveFile::LoadFromFile(const std::string &filepath) {
    xml_doc.Clear();
    ifstream input_file = openInputFile(filepath);
    string content = readEntireFile(input_file);
    input_file.close();
    xml_doc.Parse(content.c_str());
    InitFromXML();
    ReconstructState();
}

void KdenliveFile::InitFromXML() {
    root = xml_doc.RootElement();
    profile = root->FirstChildElement();
    main_producer = profile->NextSiblingElement();
    main_bin = FindPlaylistElement("main_bin");

    string timeline_tractor_id = FindDocUUID();
    timeline_tractor = FindTractorElement(timeline_tractor_id.c_str());

    final_tractor = main_bin->NextSiblingElement();
    last_added_root_element = main_producer;

    // Walk forward from main_producer to find the last element before main_bin
    // so that AddElementToRoot inserts in the right place
    XMLElement* ptr = main_producer->NextSiblingElement();
    while (ptr != nullptr && ptr != main_bin && ptr != timeline_tractor) {
        last_added_root_element = ptr;
        ptr = ptr->NextSiblingElement();
    }
}

void KdenliveFile::ReconstructState() {
    chain_count = 0;
    for (XMLElement* el = root->FirstChildElement("chain"); el != nullptr;
         el = el->NextSiblingElement("chain")) {
        chain_count++;
    }

    producer_count = 0;
    for (XMLElement* el = root->FirstChildElement("producer"); el != nullptr;
         el = el->NextSiblingElement("producer")) {
        const char* id = el->Attribute("id");
        if (!id) continue;

        int n = -1;
        char extra = '\0';

        if (std::sscanf(id, "producer%d%c", &n, &extra) == 1 && n >= 0) {
            if (n + 1 > producer_count)
                producer_count = n + 1;
        }
    }

    clip_id_counter = 0;

    for (const char* tag : {"chain", "producer"}) {
        for (XMLElement* el = root->FirstChildElement(tag); el; el = el->NextSiblingElement(tag)) {
            for (XMLElement* p = el->FirstChildElement("property"); p; p = p->NextSiblingElement("property")) {
                if (p->Attribute("name", "kdenlive:id") && p->GetText()) {
                    int id = std::stoi(p->GetText());
                    if (id + 1 > clip_id_counter) clip_id_counter = id + 1;
                }
            }
        }
    }

    filter_count = 0;
    // filters live inside playlist entries — just count all filter elements
    XMLElement* ptr = root->FirstChildElement();
    while (ptr != nullptr) {
        if (strcmp(ptr->Name(), "filter") == 0) {
            filter_count++;
        }
        // also check children (filters inside entries)
        XMLElement* child = ptr->FirstChildElement();
        while (child != nullptr) {
            if (strcmp(child->Name(), "filter") == 0) {
                filter_count++;
            }
            XMLElement* grandchild = child->FirstChildElement();
            while (grandchild != nullptr) {
                if (strcmp(grandchild->Name(), "filter") == 0) {
                    filter_count++;
                }
                grandchild = grandchild->NextSiblingElement();
            }
            child = child->NextSiblingElement();
        }
        ptr = ptr->NextSiblingElement();
    }
    // Count tracks by looking at the timeline tractor's track elements
    // Skip the first one (black track / producer0)
    track_count = 0;
    track_lengths.clear();
    track_entries.clear();

    if (timeline_tractor == nullptr) return;

    for (XMLElement* track_el = timeline_tractor->FirstChildElement("track");
         track_el != nullptr;
         track_el = track_el->NextSiblingElement("track")) {

        const char* producer_id = track_el->Attribute("producer");
        if (producer_id == nullptr) continue;

        // Skip the black track producer (first track in timeline)
        string pid(producer_id);
        if (pid.find("producer") == 0) continue;  // skip producer0, etc.

        // This is an actual track tractor
        // Find its first playlist and measure length
        XMLElement* tractor = FindTractorElement(producer_id);
        if (tractor == nullptr) continue;

        float track_length = 0;
        vector<TrackEntry> entries;

        // Get the first playlist from this tractor
        XMLElement* first_track = tractor->FirstChildElement("track");
        if (first_track != nullptr) {
            const char* playlist_id = first_track->Attribute("producer");
            if (playlist_id != nullptr) {
                XMLElement* playlist = FindPlaylistElement(playlist_id);
                if (playlist != nullptr) {
                    // Walk playlist entries
                    for (XMLElement* entry = playlist->FirstChildElement();
                         entry != nullptr;
                         entry = entry->NextSiblingElement()) {

                        if (strcmp(entry->Name(), "blank") == 0) {
                            const char* len_str = entry->Attribute("length");
                            if (len_str != nullptr) {
                                // Parse timecode to seconds (rough)
                                float len = 0;
                                int h, m;
                                float s;
                                if (sscanf(len_str, "%d:%d:%f", &h, &m, &s) == 3) {
                                    len = h * 3600.0f + m * 60.0f + s;
                                }
                                track_length += len;
                                entries.push_back({EntryType::BLANK, len, 0});
                            }
                        }
                        else if (strcmp(entry->Name(), "entry") == 0) {
                            const char* in_str = entry->Attribute("in");
                            const char* out_str = entry->Attribute("out");
                            float in_time = 0, out_time = 0;
                            if (in_str) {
                                int h, m; float s;
                                if (sscanf(in_str, "%d:%d:%f", &h, &m, &s) == 3)
                                    in_time = h * 3600.0f + m * 60.0f + s;
                            }
                            if (out_str) {
                                int h, m; float s;
                                if (sscanf(out_str, "%d:%d:%f", &h, &m, &s) == 3)
                                    out_time = h * 3600.0f + m * 60.0f + s;
                            }
                            float clip_len = out_time - in_time;
                            track_length += clip_len;
                            entries.push_back({EntryType::CLIP, clip_len, in_time});
                        }
                    }
                }
            }
        }

        track_lengths.push_back(track_length);
        track_entries.push_back(entries);
        track_count++;
    }
}

string readEntireFile(ifstream &input_file){
	return string(istreambuf_iterator<char>(input_file), istreambuf_iterator<char>());
}

string convertToTimestamp(float seconds){
    // Calculate hours, minutes, seconds, and milliseconds
    int hours = static_cast<int>(seconds) / 3600;
    seconds = fmod(seconds, 3600);
    int minutes = static_cast<int>(seconds) / 60;
    seconds = fmod(seconds, 60);
    int secs = static_cast<int>(seconds);
    int milliseconds = static_cast<int>((seconds - secs) * 1000);
    
    // Use stringstream for formatting the string
    stringstream ss;
    ss << setw(2) << setfill('0') << hours << ":"
       << setw(2) << setfill('0') << minutes << ":"
       << setw(2) << setfill('0') << secs << "."
       << setw(3) << setfill('0') << milliseconds;
    
    return ss.str();
}


// CONSTRUCTORS
KdenliveFile::KdenliveFile(){
    // Initialize the counts of certain elements in the empty file
    chain_count = 0;
    clip_id_counter = 0;
    track_count = 0;
    filter_count = 0;
    producer_count = 0;
    track_lengths = vector<float>();
    track_entries = vector<vector<TrackEntry>>();
    
    // Get "empty" kdenlive file a string and parse it
    {
    ifstream input_file = openInputFile(KdenPaths::empty_project());
    static const string empty_project_string = readEntireFile(input_file);
    input_file.close(); 

    xml_doc.Parse( empty_project_string.c_str() );
    }

    // Set the root
    root = xml_doc.RootElement();

    // Set the profile and main producer
    profile = root->FirstChildElement();
    main_producer = profile->NextSiblingElement();

    // Find and set the main bin
    main_bin = FindPlaylistElement("main_bin");

    // Find the timeline tractor, which holds every track
    string timeline_tractor_id = FindDocUUID();
    timeline_tractor = FindTractorElement(timeline_tractor_id.c_str());

    // Set the final tractor
    final_tractor = main_bin->NextSiblingElement();
    final_tractor->SetAttribute("id", "final_tractor");

    // The element we want to start adding after is the producer
    last_added_root_element = main_producer;


    // Delete all the tracks the Kdenlive pre-generate in new files
    // We do this so we don't have to manually modify the generated empty file to get a file with 0 tracks
    DeletePreExistingTracks();
}


// SETTERS
void KdenliveFile::SetProfile(const int framerate, const int width, const int height){
    // Set profile values
    profile->SetAttribute("frame_rate_den", 1);         // We're just using a framerate density of 1
    profile->SetAttribute("frame_rate_num", framerate);
    profile->SetAttribute("width", width);
    profile->SetAttribute("height", height);

    // Set description of new profile
    string description =  to_string(width) + "x" + to_string(height) + ", " + to_string(framerate) + " fps";
    profile->SetAttribute("description", description.c_str());

    // Remove kdenlive:docproperties.profile property in main bin, so that these changes apply
    XMLElement* profile_property = main_bin->FirstChildElement();
    while(!profile_property->Attribute("name", "kdenlive:docproperties.profile")){
        profile_property = profile_property->NextSiblingElement();
        if(profile_property == nullptr)
            break;
    }
    main_bin->DeleteChild(profile_property);
}

TrackId KdenliveFile::AddTrack(const TrackType track_type){
    // Add two playlists
    int playlist_index_1 = (track_count) * 2;
    int playlist_index_2 = playlist_index_1 + 1;
    string playlist_str_1 = "playlist" + to_string(playlist_index_1);
    string playlist_str_2 = "playlist" + to_string(playlist_index_2);

    XMLElement* playlist_1 = CreatePlaylistElement(playlist_str_1.c_str());
    AddElementToRoot(playlist_1);
    XMLElement* playlist_2 = CreatePlaylistElement(playlist_str_2.c_str());
    AddElementToRoot(playlist_2);

    // Add a tractor
    string tractor_str = "tractor" + to_string(track_count);
    XMLElement* tractor = CreateTractorElement(tractor_str.c_str());
    AddElementToRoot(tractor);

    // Add playlists to tractor as tracks
    XMLElement* track_1 = AddTrackElement(tractor, playlist_str_1.c_str());
    XMLElement* track_2 = AddTrackElement(tractor, playlist_str_2.c_str());

    // Set track type specific propertes
    if(track_type == TrackType::VIDEO){ // VIDEO
        track_1->SetAttribute("hide", "audio");
        track_2->SetAttribute("hide", "audio");
    }

    else if(track_type == TrackType::AUDIO){ // AUDIO
        track_1->SetAttribute("hide", "video");
        track_2->SetAttribute("hide", "video");
        // Create the audio property to be added to the playlists and tractor
        XMLElement* audio_property = CreatePropertyElement("kdenlive:audio_track", "1");
        playlist_1->InsertFirstChild(audio_property);
        playlist_2->InsertFirstChild(audio_property);
        tractor->InsertFirstChild(audio_property);

    }

    // Add tractor to timeline as a track
    AddTrackElement(timeline_tractor, tractor_str.c_str());

    // Set internal data
    track_count ++;
    track_lengths.push_back(0);
    track_entries.push_back( vector<TrackEntry>() );

    return track_count - 1;
}

ClipId KdenliveFile::AddClipToBin(const std::string &clip_path){
    // Create chain
    ClipId clip_id = clip_id_counter++;
    std::string chain_str = "chain" + to_string(chain_count);
    XMLElement* chain = CreateChainElement(chain_str.c_str(), clip_path.c_str());
    AddPropertyElement(chain, "kdenlive:id", std::to_string(clip_id).c_str());    
    // Add chain above all playlists and tractors
    AddElementToTopOfRoot(chain);
    // Add entry to main bin
    AddEntryElement(main_bin, 0, 0, chain_str.c_str());

    // Set internal data
    chain_count++;
    return clip_id;
}

TrackEntryId KdenliveFile::AddBlankToTrack(const TrackId track_id, const float length){
    // Find the playlist to add to. Since there are two playlist "tracks" for every track, we will just set to the first even one
    int playlist_index = track_id * 2;
    const string playlist_id = "playlist" + to_string(playlist_index);
    XMLElement* track_playlist = FindPlaylistElement(playlist_id.c_str());

    // Add blank entry
    AddBlankElement(track_playlist, length);

    // Create TrackEntry
    TrackEntry entry;
    entry.entry_type = EntryType::BLANK;
    entry.length = length;
    entry.start_offset = 0;

    // Set internal data
    track_lengths[track_id] += length;
    track_entries[track_id].push_back(entry);

    return track_entries[track_id].size() - 1;
}

TrackEntryId KdenliveFile::AddClipToTrack(const TrackId track_id, const ClipId clip_id, const float clip_length, const float clip_start_offset){
    // Find the playlist to add to. Since there are two playlist "tracks" for every track, we will just set to the first even one
    int playlist_index = track_id * 2;
    const string playlist_id = "playlist" + to_string(playlist_index);
    XMLElement* track_playlist = FindPlaylistElement(playlist_id.c_str());

    // Add entry
    string chain_str = ProducerRef(clip_id);
    AddEntryElement(track_playlist, clip_start_offset, clip_length + clip_start_offset, chain_str.c_str());

    // Create TrackEntry
    TrackEntry entry;
    entry.entry_type = EntryType::CLIP;
    entry.length = clip_length;
    entry.start_offset = clip_start_offset;

    // Set internal data
    track_lengths[track_id] += clip_length;
    track_entries[track_id].push_back(entry);

    return  track_entries[track_id].size() - 1;
}

TrackEntryId KdenliveFile::InsertClipAtPosition(TrackId track_id, ClipId clip_id,
                                                 float timestamp, float length,
                                                 float clip_start_offset) {
    // Find the playlist for this track (always the even-indexed one)
    int playlist_index = track_id * 2;
    string playlist_id = "playlist" + to_string(playlist_index);
    XMLElement* playlist = FindPlaylistElement(playlist_id.c_str());
    string chain_str = ProducerRef(clip_id);

    // Walk the playlist, tracking position (in seconds !!!)
    float position = 0;
    int entry_index = 0;
    XMLElement* ptr = playlist->FirstChildElement();

    while (ptr != nullptr) {
        // Skip property elements
        if (strcmp(ptr->Name(), "property") == 0) {
            ptr = ptr->NextSiblingElement();
            continue;
        }
        
        // Check if this part is blank or an entry
        bool is_blank = (strcmp(ptr->Name(), "blank") == 0);
        bool is_entry = (strcmp(ptr->Name(), "entry") == 0);
        float entry_length = 0;

        if (is_blank) {
            entry_length = TimeUtil::parseTimecode(ptr->Attribute("length"));
        } else if (is_entry) {
            entry_length = TimeUtil::parseTimecode(ptr->Attribute("out"))
                         - TimeUtil::parseTimecode(ptr->Attribute("in"));
        }

        float entry_end = position + entry_length;

        // Case 1: The timestamp falls inside a blank so we need to split that
        if (is_blank && timestamp >= position && timestamp < entry_end) {
            float pre_blank = timestamp - position;
            float post_blank = entry_end - timestamp - length;

            if (post_blank < -0.001f)
                throw std::runtime_error("Clip extends past blank space");

            XMLElement* pre  = (pre_blank  > 0.001f) ? CreateBlankElement(pre_blank)  : nullptr;
            XMLElement* mid  = CreateEntryElement(clip_start_offset, clip_start_offset + length, chain_str.c_str());
            XMLElement* post_el = (post_blank > 0.001f) ? CreateBlankElement(post_blank) : nullptr;

            int idx = SplicePlaylistElement(playlist, ptr, pre, mid, post_el);
            ReconstructState();
            return idx;
        }

        // Case 2: timestamp falls inside an existing clip. Throws an error
        // We could consider using splitting the existing clip at the timestamp using SplicePlaylistElement
        // Basically doing Case 1 but treating the pre/post timestamp parts of the existing clip same way we treat blanks
        // In Case 1
        if (is_entry && timestamp > position && timestamp < entry_end) {
            throw std::runtime_error(
                "Cannot place clip: overlaps existing entry at " + to_string(timestamp) + "s");
        }

        position += entry_length;
        entry_index++;
        ptr = ptr->NextSiblingElement();
    }

    // Case 3: timestamp is at or past end of track -> append
    if (timestamp > position + 0.001f) {
        AddBlankToTrack(track_id, timestamp - position);
    }
    return AddClipToTrack(track_id, clip_id, length, clip_start_offset);
}

void KdenliveFile::FadeClip(const TrackId track_id, TrackEntryId entry_id, const float fade_in_time, const float fade_out_time){
    // Get this TrackEntry
    const TrackEntry this_entry = track_entries[track_id][entry_id];

    // Don't allow a filter to be applied to Blank entries
    if(this_entry.entry_type == EntryType::BLANK)
        return;

    // Get the entry in the doc
    const int playlist_index = track_id*2;
    const string playlist_id = "playlist" + to_string(playlist_index);
    XMLElement* entry = FindPlaylistEntry(playlist_id.c_str(), entry_id);
    

    // Fade in
    if(fade_in_time > 0){
        const string filter_id = "filter" + to_string(filter_count);
        XMLElement* filter = CreateFilterElement(filter_id.c_str(), this_entry.start_offset, this_entry.start_offset + fade_in_time);
        AddPropertyElement(filter, "start", "1");
        AddPropertyElement(filter, "level", "1");
        AddPropertyElement(filter, "mlt_service", "brightness");
        AddPropertyElement(filter, "kdenlive_id", "fade_from_black");
        AddPropertyElement(filter, "alpha", "0=0;-1=1");
        entry->InsertEndChild(filter);

        filter_count++;
    }
    // Fade out
    if(fade_out_time > 0){
        const string filter_id = "filter" + to_string(filter_count);
        XMLElement* filter = CreateFilterElement(filter_id.c_str(), this_entry.start_offset + this_entry.length - fade_out_time, this_entry.start_offset + this_entry.length);
        AddPropertyElement(filter, "start", "1");
        AddPropertyElement(filter, "level", "1");
        AddPropertyElement(filter, "mlt_service", "brightness");
        AddPropertyElement(filter, "kdenlive_id", "fade_to_black");
        AddPropertyElement(filter, "alpha", "0=1;-1=0");
        entry->InsertEndChild(filter);

        filter_count++;
    }
    

    
}


// GETTERS
float KdenliveFile::GetTrackLength(const TrackId track_id){
    return track_lengths[track_id];
}

std::vector<KdenliveFile::TrackInfo> KdenliveFile::GetTracks() {
    std::vector<TrackInfo> tracks;
    if (timeline_tractor == nullptr) return tracks;

    int index = 0;
    for (auto* track_el = timeline_tractor->FirstChildElement("track");
         track_el != nullptr;
         track_el = track_el->NextSiblingElement("track")) {

        const char* producer_id = track_el->Attribute("producer");
        if (producer_id == nullptr) continue;

        string pid(producer_id);
        if (pid.find("producer") == 0) continue;

        XMLElement* tractor = FindTractorElement(producer_id);
        if (tractor == nullptr) continue;

        TrackType type = TrackType::VIDEO;
        for (auto* prop = tractor->FirstChildElement("property");
             prop != nullptr;
             prop = prop->NextSiblingElement("property")) {
            if (prop->Attribute("name", "kdenlive:audio_track")) {
                type = TrackType::AUDIO;
                break;
            }
        }

        float length = (index < (int)track_lengths.size()) ? track_lengths[index] : 0;
        tracks.push_back({index, type, length});
        index++;
    }

    return tracks;
}

std::vector<KdenliveFile::ClipInfo> KdenliveFile::GetClips() {
    std::vector<ClipInfo> clips;

    auto propText = [](XMLElement* el, const char* name) -> std::string {
        for (auto* prop = el->FirstChildElement("property");
             prop != nullptr;
             prop = prop->NextSiblingElement("property")) {
            if (prop->Attribute("name", name))
                return prop->GetText() ? prop->GetText() : "";
        }
        return "";
    };

    auto propInt = [&](XMLElement* el, const char* name, int fallback = -1) -> int {
        std::string value = propText(el, name);
        if (value.empty()) return fallback;
        return std::stoi(value);
    };

    for (auto* chain = root->FirstChildElement("chain");
         chain != nullptr;
         chain = chain->NextSiblingElement("chain")) {

        ClipInfo info;
        info.id = propInt(chain, "kdenlive:id");
        info.resource = propText(chain, "resource");
        info.type = "media";
        info.producer = chain->Attribute("id") ? chain->Attribute("id") : "";
        info.name = fs::path(info.resource).filename().string();

        clips.push_back(info);
    }

    for (auto* producer = root->FirstChildElement("producer");
         producer != nullptr;
         producer = producer->NextSiblingElement("producer")) {

        if (propText(producer, "mlt_service") != "kdenlivetitle")
            continue;

        ClipInfo info;
        info.id = propInt(producer, "kdenlive:id");
        info.resource = propText(producer, "resource");
        info.type = "title";
        info.producer = producer->Attribute("id") ? producer->Attribute("id") : "";
        info.name = propText(producer, "kdenlive:clipname");

        clips.push_back(info);
    }

    return clips;
}

ClipId KdenliveFile::FindClipByResource(const std::string &filepath) {
    auto samePath = [](const std::string& a, const std::string& b) -> bool {
        if (a == b) return true;

        std::error_code ec_a, ec_b;
        fs::path ca = fs::weakly_canonical(a, ec_a);
        fs::path cb = fs::weakly_canonical(b, ec_b);
        return !ec_a && !ec_b && ca == cb;
    };

    for (auto* chain = root->FirstChildElement("chain");
         chain != nullptr;
         chain = chain->NextSiblingElement("chain")) {

        std::string resource;
        int clip_id = -1;

        for (auto* prop = chain->FirstChildElement("property");
             prop != nullptr;
             prop = prop->NextSiblingElement("property")) {
            if (prop->Attribute("name", "resource")) {
                resource = prop->GetText() ? prop->GetText() : "";
            } else if (prop->Attribute("name", "kdenlive:id") && prop->GetText()) {
                clip_id = std::stoi(prop->GetText());
            }
        }

        if (!resource.empty() && clip_id >= 0 && samePath(resource, filepath))
            return clip_id;
    }

    return -1;
}

tinyxml2::XMLElement* KdenliveFile::GetTimelineTractor() { 
    return timeline_tractor; 
}

tinyxml2::XMLElement* KdenliveFile::GetRoot() { 
    return root; 
}

tinyxml2::XMLElement* KdenliveFile::FindTractorById(const char* id) { 
    return FindTractorElement(id); 
}

KdenliveFile::ProfileInfo KdenliveFile::GetProfile() const {
    ProfileInfo info{30, 1920, 1080};   // defaults if attrs missing
    if (profile) {
        profile->QueryIntAttribute("frame_rate_num", &info.fps);
        profile->QueryIntAttribute("width", &info.width);
        profile->QueryIntAttribute("height", &info.height);
    }
    return info;
}


string KdenliveFile::ToString() const{
    XMLPrinter printer;
    xml_doc.Print(&printer);

    string xml_string = printer.CStr();
    return xml_string;
}

void KdenliveFile::SaveToFile(const string &file_name, const string &output_filepath) const{
    ofstream output;

    if(output_filepath != "")
        output = openOutputFile(output_filepath + "/" + file_name);
    else
        output = openOutputFile(file_name);
	
    output << ToString();

	output.close();
}


// HELPERS
XMLElement* KdenliveFile::CreatePropertyElement(const char* name, const char* value){
    XMLElement* property = xml_doc.NewElement("property");
    property->SetAttribute("name", name);
    property->SetText(value);

    return property;
}
XMLElement* KdenliveFile::AddPropertyElement(XMLElement* element_to_add_to, const char* name, const char* value){
    XMLElement* property = CreatePropertyElement(name, value);
    element_to_add_to->InsertEndChild(property);

    return property;
}

XMLElement* KdenliveFile::CreateEntryElement(const float in, const float out, const char* producer){
    XMLElement* entry = xml_doc.NewElement("entry");
    
    entry->SetAttribute("in", convertToTimestamp(in).c_str());
    entry->SetAttribute("out", convertToTimestamp(out).c_str());
    entry->SetAttribute("producer", producer);

    return entry;
}
XMLElement* KdenliveFile::AddEntryElement(XMLElement* element_to_add_to, const float in, const float out, const char* producer){
    XMLElement* entry = CreateEntryElement(in, out, producer);
    element_to_add_to->InsertEndChild(entry);

    return entry;
}

XMLElement* KdenliveFile::CreateBlankElement(const float length){
    XMLElement* blank = xml_doc.NewElement("blank");
    const string length_str = convertToTimestamp(length);

    blank->SetAttribute("length", length_str.c_str());

    return blank;
}
XMLElement* KdenliveFile::AddBlankElement(XMLElement* element_to_add_to, const float length){
    XMLElement* blank = CreateBlankElement(length);
    element_to_add_to->InsertEndChild(blank);

    return blank;
}

XMLElement* KdenliveFile::CreateTrackElement(const char* producer){
    XMLElement* track = xml_doc.NewElement("track");

    track->SetAttribute("producer", producer);

    return track;
}
XMLElement* KdenliveFile::AddTrackElement(XMLElement* element_to_add_to, const char* producer){
    XMLElement* track = CreateTrackElement(producer);
    element_to_add_to->InsertEndChild(track);

    return track;
}

XMLElement* KdenliveFile::CreateFilterElement(const char* id, const float in, const float out){
    XMLElement* filter = xml_doc.NewElement("filter");

    filter->SetAttribute("id", id);
    filter->SetAttribute("in", convertToTimestamp(in).c_str());
    filter->SetAttribute("out", convertToTimestamp(out).c_str());

    return filter;
}
XMLElement* KdenliveFile::AddFilterElement(XMLElement* element_to_add_to, const char* id, const float in, const float out){
    XMLElement* filter = CreateFilterElement(id, in, out);
    element_to_add_to->InsertEndChild(filter);

    return filter;
}

XMLElement* KdenliveFile::CreateChainElement(const char* id, const char* resource){
    XMLElement* chain = xml_doc.NewElement("chain");

    chain->SetAttribute("id", id);

    // Add resource as a property
    AddPropertyElement(chain, "resource", resource);

    return chain;
}
XMLElement* KdenliveFile::AddChainElement(XMLElement* element_to_add_to, const char* id, const char* resource, XMLElement* insert_after){
    XMLElement* chain = CreateChainElement(id, resource);
    
    if(insert_after == nullptr)
        element_to_add_to->InsertEndChild(chain);
    else
        element_to_add_to->InsertAfterChild(insert_after, chain);

    return chain;
}

XMLElement* KdenliveFile::CreatePlaylistElement(const char* id){
    XMLElement* playlist = xml_doc.NewElement("playlist");

    playlist->SetAttribute("id", id);

    return playlist;
}
XMLElement* KdenliveFile::AddPlaylistElement(XMLElement* element_to_add_to, const char* id, XMLElement* insert_after){
    XMLElement* playlist = CreatePlaylistElement(id);
    
    if(insert_after == nullptr)
        element_to_add_to->InsertEndChild(playlist);
    else
        element_to_add_to->InsertAfterChild(insert_after, playlist);

    return playlist;
}

XMLElement* KdenliveFile::CreateTractorElement(const char* id){
    XMLElement* tractor = xml_doc.NewElement("tractor");

    tractor->SetAttribute("id", id);

    return tractor;
}
XMLElement* KdenliveFile::AddTractorElement(XMLElement* element_to_add_to, const char* id, XMLElement* insert_after){
    XMLElement* tractor = CreateTractorElement(id);

    if(insert_after == nullptr)
        element_to_add_to->InsertEndChild(tractor);
    else
        element_to_add_to->InsertAfterChild(insert_after, tractor);

    return tractor;
}

// All elements added to the root (playlists and tractors) must be added after the main_producer, but before the main_bin.
void KdenliveFile::AddElementToTopOfRoot(XMLElement* element){
    // If nothing has been added to the root yet, then set this element as the last added element
    if(last_added_root_element == main_producer){
        last_added_root_element = element;
    }

    // Add after the main_producer
    root->InsertAfterChild(main_producer, element);
}
void KdenliveFile::AddElementToRoot(XMLElement* element){
    root->InsertAfterChild(last_added_root_element, element);
    
    // Set this as the last added root
    last_added_root_element = element;
}

XMLElement* KdenliveFile::FindPlaylistElement(const char* playlist_id) const{
    XMLElement* ptr = root->FirstChildElement();
    while(ptr != nullptr){
        if( strcmp(ptr->Name(), "playlist") == 0  &&  ptr->Attribute("id", playlist_id) ){
            break;
        }

        ptr = ptr->NextSiblingElement();
    }

    return ptr;
}

XMLElement* KdenliveFile::FindTractorElement(const char* tractor_id) const{
    XMLElement* ptr = root->FirstChildElement();
    while(ptr != nullptr){
        if( strcmp(ptr->Name(), "tractor") == 0  &&  ptr->Attribute("id", tractor_id) ){
            break;
        }

        ptr = ptr->NextSiblingElement();
    }

    return ptr;
}

XMLElement* KdenliveFile::FindPlaylistEntry(const char* playlist_id, const TrackEntryId entry_index){
    // Find playlist
    XMLElement* playlist = FindPlaylistElement(playlist_id);
    
    // Count through the entries of the playlist
    XMLElement* ptr = playlist->FirstChildElement();
    int c = 0;
    while(ptr != nullptr){
        if( strcmp(ptr->Name(), "entry") == 0  ||  strcmp(ptr->Name(), "blank") == 0){
            c++;

            if( c > entry_index)    break;
        }

        ptr = ptr->NextSiblingElement();
    }
    
    return ptr;
}

XMLElement* KdenliveFile::CreateTitleElement(const char* id,
                                             const char* title_xmldata,
                                             const char* clipname,
                                             int length_frames,
                                             const char* duration_tc) {
    XMLElement* producer = xml_doc.NewElement("producer");

    producer->SetAttribute("id", id);

    producer->SetAttribute("in", "0");
    producer->SetAttribute("out", length_frames - 1);

    AddPropertyElement(producer, "length", std::to_string(length_frames).c_str());
    AddPropertyElement(producer, "eof", "pause");
    AddPropertyElement(producer, "resource", "");
    AddPropertyElement(producer, "progressive", "1");
    AddPropertyElement(producer, "aspect_ratio", "1");
    AddPropertyElement(producer, "seekable", "1");
    AddPropertyElement(producer, "mlt_service", "kdenlivetitle");

    AddPropertyElement(producer, "kdenlive:duration", duration_tc);
    AddPropertyElement(producer, "kdenlive:clipname", clipname);

    AddPropertyElement(producer, "xmldata", title_xmldata);

    AddPropertyElement(producer, "kdenlive:clip_type", "2");
    AddPropertyElement(producer, "force_reload", "0");

    return producer;
}

XMLElement* KdenliveFile::AddTitleElement(XMLElement* element_to_add_to,
                                          const char* id,
                                          const char* title_xmldata,
                                          const char* clipname,
                                          int length_frames,
                                          const char* duration_tc,
                                          XMLElement* insert_after) {
    XMLElement* title = CreateTitleElement(id, title_xmldata, clipname,
                                           length_frames, duration_tc);
    if (insert_after == nullptr)
        element_to_add_to->InsertEndChild(title);
    else
        element_to_add_to->InsertAfterChild(insert_after, title);

    return title;
}

ClipId KdenliveFile::AddTitleToBin(const std::string &xmldata,
                                   const std::string &clipname,
                                   int length_frames,
                                   const std::string &duration_tc) {
    ClipId clip_id = clip_id_counter++;
    std::string prod_str = "producer" + to_string(producer_count);
    XMLElement* title = CreateTitleElement(prod_str.c_str(), xmldata.c_str(),
                                           clipname.c_str(), length_frames,
                                           duration_tc.c_str());
    AddPropertyElement(title, "kdenlive:id", std::to_string(clip_id).c_str());

    AddElementToTopOfRoot(title);
    AddEntryElement(main_bin, 0, 0, prod_str.c_str());

    producer_count++;
    return clip_id;
}

string KdenliveFile::FindDocUUID(){
    // Check main bin for kdenlive:docproperties.uuid property
    XMLElement* ptr = main_bin->FirstChildElement();

    while(ptr != nullptr){
        if( strcmp(ptr->Name(), "property") == 0  &&  ptr->Attribute("name", "kdenlive:docproperties.uuid") )
            break;
        
        ptr = ptr->NextSiblingElement();
    }

    if(ptr == nullptr)
        return "NULL_UUID";

    return ptr->GetText();
}

void KdenliveFile::DeletePreExistingTracks(){
    // Delete all playlists and tractors up to the timeline_tractor
    XMLElement* ptr = main_producer->NextSiblingElement();
    while(ptr != timeline_tractor){
        XMLElement* cur_ptr = ptr;
        ptr = ptr->NextSiblingElement();

        if(strcmp(cur_ptr->Name(), "playlist") == 0){
            root->DeleteChild(cur_ptr);
        }
        else if(strcmp(cur_ptr->Name(), "tractor") == 0){
            const char* tractor_id = cur_ptr->Attribute("id");
            root->DeleteChild(cur_ptr);

            // Delete tractor id from timeline tractor
            XMLElement* t_ptr = timeline_tractor->FirstChildElement();
            while(t_ptr != nullptr){
                if(t_ptr->Attribute("producer", tractor_id)){
                    timeline_tractor->DeleteChild(t_ptr);
                    break;
                }

                t_ptr = t_ptr->NextSiblingElement();
            }
        }
    }

    // Reset track count properties in the timeline tractor
    XMLElement* prop = timeline_tractor->FirstChildElement("property");
    while (prop != nullptr) {
        const char* name = prop->Attribute("name");
        if (name != nullptr) {
            string n(name);
            if (n == "kdenlive:sequenceproperties.activeTrack" ||
                n == "kdenlive:sequenceproperties.videoTarget" ||
                n == "kdenlive:sequenceproperties.audioTarget") {
                prop->SetText("0");
            }
            else if (n == "kdenlive:sequenceproperties.tracksCount") {
                prop->SetText("0");
            }
        }
        prop = prop->NextSiblingElement("property");
    }
}

int KdenliveFile::SplicePlaylistElement(XMLElement* playlist,
                                         XMLElement* target,
                                         XMLElement* pre,
                                         XMLElement* middle,
                                         XMLElement* post) {
    // Find the element before target
    XMLElement* prev = nullptr;
    for (XMLElement* scan = playlist->FirstChildElement();
         scan != target; scan = scan->NextSiblingElement())
        prev = scan;

    // Count entry/blank index up to target
    int index = 0;
    for (XMLElement* scan = playlist->FirstChildElement();
         scan != target; scan = scan->NextSiblingElement()) {
        if (strcmp(scan->Name(), "entry") == 0 ||
            strcmp(scan->Name(), "blank") == 0)
            index++;
    }

    // Remove the original element
    playlist->DeleteChild(target);

    // Insert new elements in order
    XMLElement* cursor = prev;
    auto insertNext = [&](XMLElement* el) {
        if (cursor)
            playlist->InsertAfterChild(cursor, el);
        else
            playlist->InsertFirstChild(el);
        cursor = el;
    };

    int middle_index = index;
    if (pre) {
        insertNext(pre);
        middle_index = index + 1;
    }
    insertNext(middle);
    if (post) {
        insertNext(post);
    }

    return middle_index;
}

// The producer element (chain OR producer) whose kdenlive:id == clip_id.
// Returns nullptr if none.
XMLElement* KdenliveFile::FindProducerByClipId(ClipId clip_id) {
    const std::string want = std::to_string(clip_id);
    for (const char* tag : {"chain", "producer"}) {
        for (XMLElement* el = root->FirstChildElement(tag); el;
             el = el->NextSiblingElement(tag)) {
            for (XMLElement* p = el->FirstChildElement("property"); p;
                 p = p->NextSiblingElement("property")) {
                if (p->Attribute("name", "kdenlive:id") && p->GetText()
                    && want == p->GetText())
                    return el;
            }
        }
    }
    return nullptr;
}

// The actual element-id string ("chain3", "producer1") to use in entries.
std::string KdenliveFile::ProducerRef(ClipId clip_id) {
    XMLElement* el = FindProducerByClipId(clip_id);
    if (!el || !el->Attribute("id"))
        throw std::runtime_error("No producer for clip id " + std::to_string(clip_id));
    return el->Attribute("id");
}


