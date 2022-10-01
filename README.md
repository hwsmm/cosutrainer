# cosu-trainer
cosu-trainer is a replacement for [FunOrange's osu-trainer](https://github.com/FunOrange/osu-trainer), but for Linux.
Unlike the original osu-trainer, cosu-trainer doesn't have GUI (No plan to write one).

You can use all of its features with commands, so you can even put the command in your DE/WM key shortcuts for easy conversion on the fly.

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
- Scales AR/OD
- Adds osutrainer tag like osu-trainer
- Can flip/transpose a map

### Requirements
- osu! must run on WINE with dotnet 4.5+ for proper memory scanning
- There is no build-time dependency for now, but FFmpeg is required to speed up/down audio at specific rate.

### How to build
1. Run `meson build && ninja -C build`
2. Binaries should be in 'build' folder.

### How to use
1. Grab `cosu-trainer` and `osumem`
2. Run osu! and get into song select
3. Run `./osumem` and set `OSU_SONG_FOLDER` variable
5. Now you can use `./cosu-trainer` (read usage below)
6. After converting a map with it, press f5 in the game to refresh

### `cosu-trainer` usage
```
./cosu-trainer <filename|auto> <rate|bpm> [a/o/h/c] [p] [x/y/t]
<> are requirements, [] are optional
<filename|auto> : specify file name like 'songfolder/diff.osu' or just put 'auto' if you are running `osumem`
<rate|bpm> : '1.2' if you want to use rate, '220bpm' if you want to use bpm (it uses max bpm of map to calculate, so be careful)
[a/o/h/c] : ar/od/hp/cs respectively. a9.9 to set ar as 9.9.
            cosu-trainer scales ar/od by default. use af/of to fix them
[p] : if you want daycore/nightcore (not available for now)
[x/y/t] : xflip, yflip and transpose respectively.

example: ./cosu-trainer auto 220bpm a7.2 c7.2 h7 p
```

## Thanks a lot to
- [josu-trainer](https://github.com/ngoduyanh/josu-trainer) for basic idea of speeding up the map
- [gosumemory](https://github.com/l3lackShark/gosumemory) and [ProcessMemoryDataFinder](https://github.com/Piotrekol/ProcessMemoryDataFinder) for memory reading
