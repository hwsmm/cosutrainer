#include "sigscan.h"
#include <stdlib.h>
#include <string.h>

ptr_type find_pattern(struct sigscan_status *st, const uint8_t bytearray[], const unsigned int pattern_size)
{
    ptr_type result = NULL;
    uint8_t *buffer = NULL;
    unsigned long bufsize = 0;

    void *it = start_regionit(st);
    if (it == NULL)
    {
        return NULL;
    }

    struct vm_region rg;

    while (next_regionit(it, &rg) > 0)
    {
        if (result != NULL) break;

        if (rg.len < pattern_size)
        {
            continue;
        }

        if (rg.len > bufsize || buffer == NULL)
        {
            if (buffer != NULL) free(buffer);
            buffer = (uint8_t*) malloc(rg.len);
            if (buffer == NULL)
            {
                fputs("Failed allocating memory\n", stderr);
                break;
            }
            bufsize = rg.len;
        }

        if (!readmemory(st, rg.start, buffer, rg.len))
        {
            continue;
        }

        unsigned long i;
        for (i = 0; i < rg.len - pattern_size + 1L; i++)
        {
            if (memcmp(buffer + i, bytearray, pattern_size) == 0)
            {
                result = ptr_add(rg.start, i);
                break;
            }
        }
    }

    stop_regionit(it);
    free(buffer);
    return result;
}
