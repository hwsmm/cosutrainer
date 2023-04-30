#pragma once
#include "sigscan.h"

ptr_type match_pattern(struct sigscan_status *st);
char *get_songsfolder(struct sigscan_status *st);
int get_mapid(struct sigscan_status *st, ptr_type base_address);
char *get_mappath(struct sigscan_status *st, ptr_type base_address, unsigned int *length);
