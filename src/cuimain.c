#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <limits.h>
#include <math.h>
#include "mapeditor.h"
#include "tools.h"
#include "cuimain.h"
#include "cosuplatform.h"

#ifndef WIN32
#include "lsongpathparser.h"
#endif

static void showprog(void *data, float progress)
{
    float *progv = (float*)data;

    if (progress - *progv > 0.01)
    {
        fprintf(stderr, "\r%.0f%%", progress * 100.0f);
        *progv = progress;
    }
}

#ifdef DISABLE_GUI
int main(int argc, char *argv[])
#else
int cuimain(int argc, char *argv[])
#endif
{
    int ret = 0;
    if (argc < 2)
    {
        printerr("Not enough arguments.");
        return 1;
    }

    char *path = NULL;
    int argstart = 2;

#ifndef WIN32
    bool osumem = (strcmp(argv[1], "auto") == 0);

    if (!osumem && !endswith(argv[1], ".osu"))
    {
        printf("File name doesn't end with .osu, assuming that this is auto\n");
        osumem = true;
        argstart = 1;
    }

    struct songpath_status st;
    if (osumem)
    {
        songpath_init(&st);

        if (!songpath_get(&st, &path))
        {
            printerr("Failed to get song path, is osumem running?");
            songpath_free(&st);
            return 5;
        }
    }
    else
    {
        path = argv[1];
    }
#else
    bool osumem = false;
    path = argv[1];
#endif

    struct mapinfo mi;
    ret = read_beatmap(&mi, path);
#ifndef WIN32
    if (osumem) songpath_free(&st); // no longer needed since mi has fullpath
#endif
    if (ret != 0)
    {
        printerr("Failed reading a map!");
        return 1;
    }
    struct editdata edit;

    edit.mi = &mi;
    edit.hp = mi.hp;
    edit.cs = mi.cs;
    edit.od = mi.od;
    edit.ar = mi.ar;

    edit.scale_ar = true;
    edit.scale_od = true;
    edit.makezip = osumem;

    char *identifier = NULL;
    edit.speed = strtod(argv[argstart++], &identifier);
    edit.bpmmode = guess;
    if (identifier != NULL)
    {
        if (*identifier == 'x')
        {
            edit.bpmmode = rate;
        }
        else if (CMPSTR(identifier, "bpm"))
        {
            edit.bpmmode = bpm;
        }
    }

    edit.pitch = false;
    edit.flip = none;
    edit.nospinner = false;

    edit.cut_combo = false;
    edit.cut_start = 0;
    edit.cut_end = LONG_MAX;
    edit.remove_sv = false;

    if (argstart < argc)
    {
        int i;
        for (i = argstart; i < argc; i++)
        {
            double *diffv = NULL;
            char *curarg = argv[i];
            switch (*curarg)
            {
            case 'p':
                edit.pitch = true;
                continue;
            case 'x':
                edit.flip = xflip;
                continue;
            case 'y':
                edit.flip = yflip;
                continue;
            case 't':
                edit.flip = transpose;
                continue;
            case 'i':
                edit.flip = invert;
                continue;
            case 'h':
                diffv = &(edit.hp);
                break;
            case 'c':
                diffv = &(edit.cs);
                break;
            case 'o':
                diffv = &(edit.od);
                break;
            case 'a':
                diffv = &(edit.ar);
                break;
            case 's':
                edit.nospinner = true;
                continue;
            case 'v':
                edit.remove_sv = true;
                continue;
            default:
                break;
            }

            if (*curarg == 'e')
            {
                char *time = curarg + 1;
                char *temp;

                if (*time == '\0')
                    continue;

                double start = 0, end = INFINITY;

                if (*time != '-')
                {
                    start = strtod(time, &temp);
                    if (*temp == ':')
                    {
                        start *= 60.0;
                        start += strtod(++temp, &time);
                    }
                    else
                    {
                        time = temp;
                    }
                }

                if (*time == '-' && *(++time) != '\0')
                {
                    end = strtod(time, &temp);
                    if (*temp == ':')
                    {
                        end *= 60.0;
                        end += strtod(++temp, NULL);
                    }
                }

                if (start >= end)
                {
                    printerr("Cut time is invalid, ignoring this parameter");
                    continue;
                }

                start *= 1000.0;
                edit.cut_start = (long)start;

                if (!isinf(end))
                {
                    end *= 1000.0;
                    edit.cut_end = (long)end;
                }
            }
            else if (*curarg == 'w')
            {
                char *combo = curarg + 1;
                char *temp;

                if (*combo == '\0')
                    continue;

                long start = 0, end = LONG_MAX;

                if (*combo != '-')
                {
                    start = strtol(combo, &temp, 10);
                    combo = temp;
                }

                if (*combo == '-' && *(++combo) != '\0')
                {
                    end = strtol(combo, &temp, 10);
                }

                if (start >= end)
                {
                    printerr("Cut combo is invalid, ignoring this parameter");
                    continue;
                }

                edit.cut_start = start;
                edit.cut_end = end;
                edit.cut_combo = true;
            }

            if (diffv != NULL)
            {
                char *iden = NULL;
                if (*(curarg + 1) != 'f')
                {
                    *diffv = strtof(curarg + 1, &iden);
                    if (*diffv < 0) *diffv = 0;
                }

                if (*curarg == 'a')
                {
                    if (iden != NULL && *iden == 'c')
                        *diffv *= -1;
                    else
                        edit.scale_ar = false;
                }
                else if (*curarg == 'o')
                {
                    if (iden != NULL && *iden == 'c')
                        *diffv *= -1;
                    else
                        edit.scale_od = false;
                }

                diffv = NULL;
            }
        }
    }

    float progv = 0.0f;
    edit.data = &progv;
    edit.progress_callback = showprog;
    ret = edit_beatmap(&edit);

    fprintf(stderr, "\r100%%\n");

    free_mapinfo(&mi);

    return ret;
}
