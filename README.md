# KdenCLI
A small and simple CLI wrapper fork around [KdenCode](https://github.com/joshschnauber/KdenCode) (and, consequently, around KdenLive)

# Features
- Setting the profile of the project file.
- Adding clips to the project bin.
- Adding new video and audio tracks to the timeline.
- Adding clips from the project bin onto a video or audio track, with a given position, length, and starting offset.
- Adding a fade effect to clips added to a track.
- Ability (theorically, to test) to use any effect defined in XML by Kdenlive in its data folders (/usr/share/kdenlive/effects/ in most GNU/Linux distributions)

# Usage

## Build

```bash
g++ -std=c++17 kdencli.cpp lib/*.cpp -g -o kdencli
```

## Commands

### `create` — Create a new project

```bash
kdencli create <output.kdenlive> [--fps 30] [--width 1920] [--height 1080]
```

Creates a new `.kdenlive` project file with one video track and one audio track.

```bash
kdencli create my_project.kdenlive --fps 30 --width 1280 --height 720
```

---

### `import` — Import a media file into the project bin

```bash
kdencli import <project.kdenlive> <filepath>
```

Imports a media file into the project bin without placing it on the timeline. Returns a clip ID for use with `place --clipid`.

```bash
kdencli import my_project.kdenlive /home/user/Videos/clip.mp4
# Imported clip 0: /home/user/Videos/clip.mp4
```

---

### `add-track` — Add a track

```bash
kdencli add-track <project.kdenlive> --type video|audio
```

```bash
kdencli add-track my_project.kdenlive --type audio
```

---

### `place` — Place a clip on the timeline

```bash
kdencli place <project.kdenlive> --track <id> --file <filepath> [options]
kdencli place <project.kdenlive> --track <id> --clipid <id> [options]
```

Places a clip on a track at a given timestamp. If the clip is not in the project bin, it is imported automatically.

**Options:**

| Flag | Description |
|------|-------------|
| `--at`, `-a` | Timeline position in seconds (default: 0) |
| `--length`, `-l` | Clip length in seconds |
| `--offset`, `-o` | Start offset within the source clip |
| `--ss` | Cut start (seconds, MM:SS, or HH:MM:SS) |
| `--to` | Cut end (seconds, MM:SS, or HH:MM:SS) |
| `--video-only` | Place on specified track only, skip audio stream |
| `--audio-only` | Place on specified track only, skip video stream |

**A/V auto-split:** By default, if a file has both video and audio streams, `place` automatically places both streams on the nearest paired track. Use `--video-only` or `--audio-only` to override.

```bash
# Place full clip at 0s (auto-splits A/V)
kdencli place my_project.kdenlive --track 0 --file clip.mp4

# Place a specific cut at 10 seconds
kdencli place my_project.kdenlive --track 0 --file clip.mp4 --at 10 --ss 0:30 --to 0:45

# Place audio only
kdencli place my_project.kdenlive --track 1 --file clip.mp4 --audio-only

# Place by clip ID
kdencli place my_project.kdenlive --track 0 --clipid 0 --at 20
```

---

### `fade` — Apply a fade to a placed clip

```bash
kdencli fade <project.kdenlive> --track <id> --entry <id> [options]
```

Applies a fade-from-black and/or fade-to-black filter to a placed clip entry. Use `kdencli info` to find entry IDs and their timeline positions.

| Flag | Description |
|------|-------------|
| `--in-start` | Fade in start time (seconds) |
| `--in-end` | Fade in end time (seconds) |
| `--out-start` | Fade out start time (seconds) |
| `--out-end` | Fade out end time (seconds) |

```bash
# 1s fade in at start, 1s fade out at end of an 8s clip
kdencli fade my_project.kdenlive --track 0 --entry 0 \
    --in-start 0 --in-end 1 \
    --out-start 7 --out-end 8
```

---

### `effect` — Apply any effect from the Kdenlive catalog

```bash
kdencli effect <project.kdenlive> --track <id> --entry <id> --id <effect_id> [options]
```

Applies any effect defined in Kdenlive's effect catalog (`/usr/share/kdenlive/effects/` on most GNU/Linux distributions). Effect parameters can be overridden via `--param key=value`.

| Flag | Description |
|------|-------------|
| `--id` | Effect ID (see `--list`) |
| `--in-start` | Filter start time (seconds) |
| `--in-end` | Filter end time (seconds) |
| `--param`, `-p` | Parameter override: `key=value` (repeatable) |
| `--list`, `-l` | List all available effect IDs |
| `--describe`, `-d` | Show parameters for a specific effect |

```bash
# List all available effects
kdencli effect --list

# Describe an effect's parameters
kdencli effect --describe reverb

# Apply volume boost to audio track entry
kdencli effect my_project.kdenlive --track 1 --entry 0 \
    --id volume \
    --in-start 0 --in-end 8 \
    --param level=6dB

# Apply reverb
kdencli effect my_project.kdenlive --track 1 --entry 0 \
    --id reverb \
    --in-start 0 --in-end 8 \
    --param room=4.2 --param damp=0.25

# Apply fade from black (equivalent to the fade command)
kdencli effect my_project.kdenlive --track 0 --entry 0 \
    --id fade_from_black \
    --in-start 0 --in-end 2
```

---

### `info` — Print project info

```bash
kdencli info <project.kdenlive>
```

Prints tracks (with IDs, types, and lengths) and clips (with IDs and file paths).

```bash
kdencli info my_project.kdenlive
# Tracks: 2
#   [0] video - 8s
#   [1] audio - 8s
# Clips: 1
#   [0] /home/user/Videos/clip.mp4
```

---

## Environment

| Variable | Description |
|----------|-------------|
| `KDENLIVE_SHARE_PATH` | Override the Kdenlive data directory (default: `/usr/share/kdenlive/`) |

```bash
KDENLIVE_SHARE_PATH=/custom/kdenlive/ kdencli effect --list
```

---

## Example: full pipeline

```bash
# Create a 1280x720 project
kdencli create output.kdenlive --width 1280 --height 720

# Place a video clip with A/V auto-split at 0s
kdencli place output.kdenlive --track 0 --file intro.mp4

# Cut a section from another clip and place at 10s
kdencli place output.kdenlive --track 0 --file main.mp4 --at 10 --ss 0:30 --to 1:00

# Fade in the first clip
kdencli fade output.kdenlive --track 0 --entry 0 --in-start 0 --in-end 1

# Apply reverb to the audio track
kdencli effect output.kdenlive --track 1 --entry 0 \
    --id reverb --in-start 0 --in-end 40 \
    --param room=4.2

# Open in Kdenlive for final adjustments and export
kdenlive output.kdenlive
```

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
