#pragma once
#include "sigscan.h"
#include "osusig_patterns.h"

#ifdef __cplusplus
extern "C"
{
#endif

bool match_pattern(struct sigscan_status *st, ptr_type *baseaddr);
int get_mapid(struct sigscan_status *st, ptr_type base_address);
wchar_t *get_mappath(struct sigscan_status *st, ptr_type base_address, unsigned int *length);

#ifdef __cplusplus
}
#endif