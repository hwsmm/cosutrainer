#pragma once
#include "sigscan.h"

#define OSU_STATUS_SIG   { 0x48, 0x83, 0xF8, 0x04, 0x73, 0x1E }
#define OSU_STATUS_SIZE  6

#define OSU_RULESET_SIG  { 0xC7, 0x86, 0x48, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xA1 }
#define OSU_RULESET_SIZE 11

#define OSU_BASE_SIG     { 0xf8, 0x01, 0x74, 0x04, 0x83, 0x65 }
#define OSU_BASE_SIZE    6

#define OSU_MODS_SIG     { 0xC8, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00 }
#define OSU_MODS_MASK    { false, false, true, true, true, true, true, false, false, true, true, true, true, false, false, false, false }
#define OSU_MODS_SIZE    17

#define _osu_find_ptrn_mask(PTR_VAR, SIGSCAN, NAME) \
do \
{ \
    const uint8_t ptrn[] = OSU_##NAME##_SIG; \
    const bool mask[] = OSU_##NAME##_MASK; \
    PTR_VAR = find_pattern(SIGSCAN, ptrn, OSU_##NAME##_SIZE, mask); \
} \
while (0);

#define _osu_find_ptrn(PTR_VAR, SIGSCAN, NAME) \
do \
{ \
    const uint8_t ptrn[] = OSU_##NAME##_SIG; \
    PTR_VAR = find_pattern(SIGSCAN, ptrn, OSU_##NAME##_SIZE, NULL); \
} \
while (0);
