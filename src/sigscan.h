#pragma once
#include <limits.h>

#define BEATMAP_OFFSET (-0xC)
#define MAPID_OFFSET 0xCC

#define FOLDER_OFFSET 0x78
#define PATH_OFFSET 0x94

pid_t find_and_set_osu();
void set_osu(pid_t osu);
int remote_process_read(pid_t remote_pid, void *address, char *buffer, size_t len);
void* match_pattern();
void* find_pattern(const char bytearray[], int pattern_size);
int get_mapid(void* base_address);
char *get_mappath(void* base_address);
