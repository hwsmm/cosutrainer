#pragma once

#ifdef WIN32

#include <windows.h>
LPWSTR getOsuPath(LPDWORD len);
LPWSTR getOsuSongsPath(LPWSTR osupath, DWORD pathsize);

#else

char *get_osu_path(char *wineprefix);
char *get_osu_songs_path(char *wineprefix, char *uid);

#endif
