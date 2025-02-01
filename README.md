# cosu-trainer
cosu-trainer is a replacement for [FunOrange's osu-trainer](https://github.com/FunOrange/osu-trainer), but for Linux (and experimental support for Windows)

You can use all of its features with commands, so you can even put the command in your DE/WM key shortcuts for easy conversion on the fly. GUI is also provided within a single executable.

## Description
`cosu-trainer` is an actual program, which can speed up/down audio/map, and edit difficulty.

`osumem` is a memory reader for osu!, it can read what .osu file is being read and save it to a temp file so that `cosu-trainer` can read.

`cleanup` is a MP3 cleaner that can run in osu! songs folder (`cleanup <osu song folder>`).
Basically removes every MP3 that is not used as a main audio file in any .osu file in the same folder.
It's very destructive and experimental, so don't use unless you desperately need it.

### Features
- Can speed up/down audio/map
- Supports all modes, but not tested enough tho
- Emulates DT if either (scaled) AR or OD is over 10
- Scales AR/OD, but you can cap so that it won't scale over the value you put
- Adds osutrainer tag like osu-trainer
- Can flip/transpose a map
- Can generate a full LN difficulty for noodle enjoyers
- Can generate a practice difficulty by cutting
- Generates an osz file for easier import
- An user-defined osz handler can be set for seamless integration with osu!
- Supports MP3 and [all formats that libsndfile supports](https://libsndfile.github.io/libsndfile/formats.html). (though osu! only accepts ogg and mp3 as rankable formats)

### Linux requirements
- osu! must run on WINE with dotnet 4.5+ for proper memory scanning

### `cosu-trainer` GUI screenshot
![Screenshot](docs/cosu.png)

### How to use
1. Download `cosu-trainer-bin.tar.zst` (Linux) or `cosu-trainer-win-bin.zip` (Windows) from [Releases](https://github.com/hwsmm/cosutrainer/releases) and extract
2. Run osu! and get into song select
3. Linux: Run `./osumem` (Most systems require root permission to run this)
4. (Not necessary) Linux: Set `OSU_SONG_FOLDER` variable (`export OSU_SONG_FOLDER=/put/osu/Songs/`, I recommend putting it into your .profile), `echo "<your Songs path>" > ~/.cosu_songsfd` if `osumem` doesn't detect your song folder. Please report if this is the case.
5. Now you can use `./cosu-trainer` or `cosu-trainer.exe` (read usage below)
6. After converting a map with it, press f5 in the game to refresh

### DEB/RPM/Arch repository
My [wine-osu repository](https://build.opensuse.org/project/show/home:hwsnemo:packaged-wine-osu) also provides cosu-trainer.
You can get it [here](https://software.opensuse.org//download.html?project=home%3Ahwsnemo%3Apackaged-wine-osu&package=cosu-trainer)!

### Build dependencies
- libmpg123, libmp3lame, libsndfile, libsoundtouch, libzip, libfltk1.3

### How to build
1. Install header packages of dependencies
2. Run `meson build && ninja -C build`
3. Binaries should be in 'build' folder.

Run `cosu-trainer` with no arguments to launch GUI. You can use below arguments if you don't want GUI.

### `cosu-trainer` command usage
```
./cosu-trainer <filename|auto> <rate|bpm> [a/o/h/c] [p] [x/y/t] [eMM:SS-MM:SS]
<> are requirements, [] are optional
<filename|auto> : specify file name like 'songfolder/diff.osu' or just put 'auto' if you are running `osumem`
<rate|bpm> : '1.2x' if you want to use rate, '220bpm' if you want to use bpm (it uses max bpm of map to calculate, so be careful)
             the program will try to guess if x or bpm is not specified
[a/o/h/c] : ar/od/hp/cs respectively. a9.9 to set ar as 9.9.
            cosu-trainer scales ar/od by default. use af/of to fix them
            you can add 'c' at the end to cap the value, it still scales, but won't scale over the value you put
[p] : if you want daycore/nightcore
[x/y/t/i] : xflip, yflip, transpose and invert(mania-only full LN) respectively.
[eMM:SS-MM:SS] : extracts specified section from the map, [e-MM:SS] removes all hitobjects after the specified time,
                 either [eMM:SS] or [eMM:SS-] removes all hitobjects before the time. you can omit MM, so you can do eSS
                 note that time you give should be in pre-rate-adjusted time.

example: ./cosu-trainer auto 220bpm a7.2c c7.2 h7 p e27-
```

### How to open a converted map automatically
You can set `OSZ_HANDLER` variable (`export OSZ_HANDLER="xdg-open \"{osz}\""`) to make cosu-trainer open the map automatically just after the conversion. Unsetting the variable disables this feature.

I recommend getting [osu-handler](https://aur.archlinux.org/packages/osu-handler) by openglfreak, and let it handle .osz files by setting file association.
`xdg-open` will open osu-handler if a given file is identified as an osu! beatmap archive.
You can also get DEB/RPM of osu-handler [here](https://software.opensuse.org//download.html?project=home%3Ahwsnemo%3Apackaged-wine-osu&package=osu-handler-wine).
I package this one, so if there's any problem with it, let me know!

### Use Registry to get Songs folder
This works out of box on Windows, and is pretty reliable, but it's a bit flaky on Linux because osu! runs on WINE.
`osumem` tries to get the song directory once osu! is found, and it will show you the found directory. `cosu-trainer` will use the found one even if you have set `OSU_SONG_FOLDER` or `~/.cosu_songsfd`.

Please let me know if it works well or not!

### Experimental Windows support
Things mostly work but it's kinda flaky. You can use [MSYS2](https://msys2.org) (Note that cosu-trainer only supports UCRT).
Install MinGW GCC and all dependencies through their package manager and use a following command in `src` to compile cosu-trainer.
```
x86_64-w64-mingw32-g++ -DWIN32 `fltk-config --use-images --cxxflags` \
cosuui.cpp cosuwindow.cpp cuimain.c main.cpp tools.c mapeditor.c actualzip.c audiospeed.cpp buffers.c cosumem.c freader_win.cpp sigscan.c wsigscan.c cosuplatform_win.c winregread.c \
-o ../cosu-trainer `fltk-config --use-images --ldflags` -lwinpthread -lmpg123 -lmp3lame -lzip -lSoundTouch -lsndfile -lshlwapi
```

**Some limitations**
- CLI `auto` doesn't work
- `OSZ_HANDLER` doesn't work

## Thanks a lot to
- Thanks a lot to developers of libraries I used in this program!!!
- [josu-trainer](https://github.com/ngoduyanh/josu-trainer) for basic idea of speeding up the map
- [gosumemory](https://github.com/l3lackShark/gosumemory) and [ProcessMemoryDataFinder](https://github.com/Piotrekol/ProcessMemoryDataFinder) for memory reading
