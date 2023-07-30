#pragma once
#include <windows.h>

LPWSTR getOsuPath(LPDWORD len);
LPWSTR getOsuSongsPath(LPWSTR osupath, DWORD pathsize);
