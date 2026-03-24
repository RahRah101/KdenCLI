# KdenCLI
A small and simple CLI wrapper fork around [KdenCode](https://github.com/joshschnauber/KdenCode) (and, consequently, around KdenLive)

# Features
- Setting the profile of the project file.
- Adding clips to the project bin.
- Adding new video and audio tracks to the timeline.
- Adding clips from the project bin onto a video or audio track, with a given position, length, and starting offset.
- Adding a fade effect to clips added to a track.

# Usage
TODO

# KdenliveFile.h vs. KdenCLIProject.h
To create a .kdenlive file, you can either use the KdenliveFile class or KdenCLIProject class.

### KdenliveFile: 
KdenliveFile is basically a wrapper for the XMLDocument class of the tinyxml library, so it offers the lowest level of control of the file.
However, this means that you can only add clips to a track sequentially, and must keep track of adding blanks to the track to ensure your clip starts at the correct time.
If you simply have a list of clips you want to play one after the other, and are sure you won't need to change their position after adding them, KdenliveFile is probably the best choice.

### KdenCLIProject: 
KdenliveProject offers the functionality you would actually want when adding clips to the timeline. You can simply set the position of a clip, and don't need to worry about the order of entering them or which track they need to be on. When the file is generated, the clips will automatically be assigned tracks that will allow them to be placed at a specific position, and blanks are automatically inserted to ensure the clips starts at the correct position.

Note that KdenCLIProject uses KdenliveFile in it's implementation, so you would need to include KdenliveFile if you are to use KdenCLIProject.

# Dependencies
The tinyxml2 library (https://github.com/leethomason/tinyxml2) is used to parse and edit the .kdenlive file, as the .kdenlive files use the XML format.
The tinyxml2 library has been included in the lib folder.

# Future features
Other effects will be implemented later. Most likely by defining yaml files for the effects and using the Cpp code as a general applicator. 
So we would have something like :
```yaml
# effects/fade_from_black.yaml
name: fade_from_black
mlt_service: brightness
kdenlive_id: fade_from_black
properties:
  start: "1"
  level: "1"
  alpha: "0=0;-1=1"
```
Or perhaps using embedded Lua helpers or something for effects that might need more conditional logic/tweaks. All-in-all we don't want to hardcode the effects in the Cpp. We want something modular.
