#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include "mapeditor.h"
#include "tools.h"
#include "cuimain.h"
#include "cosuplatform.h"

static void *showprog(void *arg)
{
    float *progress = (float*) arg;
    do
    {
        printf("\r%d%%", (int) (*progress * 100));
        usleep(1000);
    }
    while (*progress < 1);
    puts("\r100%");
    return 0;
}

#ifdef DISABLE_GUI
int main(int argc, char *argv[])
#else
int cuimain(int argc, char *argv[])
#endif
{
    int ret = 0;
    if (argc < 3)
    {
        printerr("Not enough arguments.");
        return 1;
    }

    char *path = NULL;
    bool osumem = (strcmp(argv[1], "auto") == 0);
    if (osumem)
    {
        char *song_folder = getenv("OSU_SONG_FOLDER");
        if (song_folder == NULL)
        {
            printerr("Set OSU_SONG_FOLDER variable to use 'auto' option.");
            return 2;
        }

        int readbytes = 0;
        char *song_path = read_file("/tmp/osu_path", &readbytes);
        if (song_path == NULL)
        {
            return 2;
        }

        int pathsize = strlen(song_folder) + 1 + readbytes + 1;
        path = (char*) malloc(pathsize);
        if (path == NULL)
        {
            printerr("Failed allocating memory");
            free(song_path);
            return 3;
        }
        snprintf(path, pathsize, "%s/%s", song_folder, song_path);
        remove_newline(path);
        free(song_path);
    }
    else
    {
        path = argv[1];
    }

    struct mapinfo *mi = read_beatmap(path);
    if (osumem) free(path); // no longer needed since mi has fullpath
    if (mi == NULL)
    {
        printerr("Failed reading a map!");
        return 1;
    }
    struct editdata edit;

    edit.mi = mi;
    edit.hp = mi->hp;
    edit.cs = mi->cs;
    edit.od = mi->od;
    edit.ar = mi->ar;

    edit.scale_ar = true;
    edit.scale_od = true;
    edit.makezip = osumem;

    char *identifier = NULL;
    edit.speed = strtof(argv[2], &identifier);
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

    if (argc >= 4)
    {
        int i;
        for (i = 3; i < argc; i++)
        {
            float *diffv = NULL;
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
            default:
                continue;
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
            }
        }
    }

    float progress = 0;

    pthread_t thr;
    int threrr = pthread_create(&thr, NULL, showprog, &progress);
    ret = edit_beatmap(&edit, &progress);
    progress = 1;
    if (!threrr)
    {
        pthread_join(thr, NULL);
    }

    free_mapinfo(mi);

    return ret;
}
