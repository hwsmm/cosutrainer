#pragma once
#include "sigscan.h"

bool match_pattern(struct sigscan_status *st, ptr_type *baseaddr, ptr_type *statusaddr, ptr_type *modsaddr);
char *get_songsfolder(struct sigscan_status *st);
int get_mapid(struct sigscan_status *st, ptr_type base_address);
wchar_t *get_mappath(struct sigscan_status *st, ptr_type base_address, unsigned int *length);
int is_playing(struct sigscan_status *st, ptr_type statussigaddr);
unsigned int get_mods(struct sigscan_status *st, ptr_type modsaddr, int *err);
