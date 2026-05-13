#include "packeditor.h"
#include "tools.h"
#include "cosuplatform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

int edit_beatmap_pack(struct editdata *edit)
{
    if (edit == NULL || edit->mi == NULL || edit->mi->fullpath == NULL)
        return -1;

    char *folderpath = _strdup(edit->mi->fullpath);
    char *fsep = strrchr(folderpath, PATHSEP);
    if (fsep == NULL)
    {
        free(folderpath);
        printerr("Invalid beatmap path");
        return -1;
    }
    *fsep = '\0';

    DIR *d = opendir(folderpath);
    if (d == NULL)
    {
        perror(folderpath);
        free(folderpath);
        return -1;
    }

    int ret = 0;
    int edited_count = 0;
    struct dirent *ent;

    while ((ent = readdir(d)))
    {
        if (!endswith(ent->d_name, ".osu"))
            continue;

        struct editdata packed = *edit;

        if (strcmp(ent->d_name, fsep + 1) != 0)
        {
            int pathlen = strlen(folderpath) + 1 + strlen(ent->d_name) + 1;
            char *osupath = (char*) malloc(pathlen);
            if (osupath == NULL)
            {
                printerr("Failed allocation");
                continue;
            }
            snprintf(osupath, pathlen, "%s" STR_PATHSEP "%s", folderpath, ent->d_name);

            struct mapinfo *mi = read_beatmap(osupath);
            if (mi == NULL)
            {
                free(osupath);
                continue;
            }

            packed.mi = mi;
            free(osupath);
        }

        printf("Editing %s...\n", ent->d_name);

        ret = edit_beatmap(&packed);

        if (edit->mi != packed.mi)
            free_mapinfo((struct mapinfo*)packed.mi);

        if (ret == 0)
        {
            edited_count++;
        }
        else
        {
            fprintf(stderr, "Failed editing %s: %d\n", ent->d_name, ret);
            break;
        }
    }

    closedir(d);

    if (edited_count > 0 && edit->makezip)
    {
        long zipflen = strlen(folderpath) + sizeof(".osz");
        char *zipf = (char*) malloc(zipflen);
        snprintf(zipf, zipflen, "%s.osz", folderpath);

        ret = execute_file(zipf);
        free(zipf);
    }

    free(folderpath);
    printf("Edited %d beatmap(s) in the mapset.\n", edited_count);
    return ret;
}
