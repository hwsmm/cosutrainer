#pragma once
#include "sigscan.h"

bool match_pattern(struct sigscan_status *st, ptr_type *baseaddr);
char *get_songsfolder(struct sigscan_status *st);
int get_mapid(struct sigscan_status *st, ptr_type base_address);
wchar_t *get_mappath(struct sigscan_status *st, ptr_type base_address, unsigned int *length);
