#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef WIN32

#include <windows.h>
LPWSTR getOsuPath();
LPWSTR getOsuSongsPath(LPWSTR osupath);

#else

char *get_osu_path(char *wineprefix);
char *get_osu_songs_path(char *wineprefix, char *uid);

#endif

#ifdef __cplusplus
}
#endif
