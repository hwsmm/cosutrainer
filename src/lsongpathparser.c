#include "lsongpathparser.h"
#include "tools.h"
#include "cosuplatform.h"
#include <stdlib.h>
#include <string.h>

void songpath_init(struct songpath_status *st)
{
    memset(st, 0, sizeof(struct songpath_status));
}

void songpath_free(struct songpath_status *st)
{
    if (st->songf) free(st->songf);
    if (st->save) free(st->save);
    if (!st->within && st->path) free(st->path);
}

bool songpath_get(struct songpath_status *st, char **path)
{
    int readbytes = 0;
    char *new_path = read_file("/tmp/osu_path", &readbytes);
    if (new_path == NULL)
    {
        return false;
    }

    char *sep = strchr(new_path, ' ');
    *sep = '\0';
    int songflen = atoi(new_path);
    *sep = ' ';

    if (st->save == NULL)
    {
    }
    else if (strcmp(st->save, new_path) == 0)
    {
        free(new_path);
        return false;
    }

    if (st->save != NULL)
    {
        free(st->save);
        if (!st->within)
        {
            free(st->path);
            st->path = NULL;
        }
    }

    st->save = new_path;

    char *new_osu = sep + 1;
    st->path = new_osu;
    st->within = true; // to prevent wrong free when song folder is not found

    if (*new_osu != '/')
    {
        if (st->songf == NULL)
        {
            st->songf = get_songspath();
            if (st->songf == NULL)
            {
                printerr("Song Folder not found!");
                return false;
            }
        }

        int songflen = strlen(st->songf);
        int fullsize = songflen + 1 + strlen(new_osu) + 1;

        st->path = (char*) malloc(fullsize);
        if (st->path == NULL)
        {
            printerr("Failed allocating!");
            return false;
        }

        st->within = false;

        strcpy(st->path, st->songf);
        *(st->path + songflen) = '/';
        strcpy(st->path + songflen + 1, new_osu);

        if (try_convertwinpath(st->path, songflen + 1) < 0)
        {
            printerr("Failed finding path!");
            free(st->path);
            st->path = NULL;
            return false;
        }
    }
    else
    {
        if (try_convertwinpath(st->path, songflen + 1) < 0)
        {
            printerr("Failed finding path!");
            return false;
        }
    }

    *path = st->path;
    return true;
}
