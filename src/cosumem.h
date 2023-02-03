#pragma once
#include "sigscan.h"

ptr_type match_pattern();
int get_mapid(ptr_type base_address);
char *get_mappath(ptr_type base_address, unsigned int *length);
