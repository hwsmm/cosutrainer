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

    char *foldername = strrchr(folderpath, PATHSEP);
    foldername = foldername != NULL ? foldername + 1 : folderpath;

    DIR *d = opendir(folderpath);
    if (d == NULL)
    {
        perror(folderpath);
        free(folderpath);
        return -1;
    }

    struct editdata orig = *edit;
    double user_speed = edit->speed;

    int edited_count = 0;
    struct dirent *ent;

    while ((ent = readdir(d)))
    {
        if (!endswith(ent->d_name, ".osu"))
            continue;

        int pathlen = strlen(folderpath) + 1 + strlen(ent->d_name) + 1;
        char *osupath = (char*) malloc(pathlen);
        if (osupath == NULL)
        {
            printerr("Failed allocation");
            continue;
        }
        snprintf(osupath, pathlen, "%s%c%s", folderpath, PATHSEP, ent->d_name);

        struct mapinfo *mi = read_beatmap(osupath);
        if (mi == NULL)
        {
            free(osupath);
            continue;
        }

        free(mi->diffname);
        mi->diffname = _strdup(foldername);

        struct editdata packed = orig;
        packed.mi = mi;
        packed.speed = user_speed;
        packed.makezip = false;

        printf("Editing %s...\n", ent->d_name);

        int ret = edit_beatmap(&packed);
        if (ret == 0)
            edited_count++;

        free_mapinfo(mi);
        free(osupath);
    }

    closedir(d);
    free(folderpath);

    printf("Edited %d beatmap(s) in the pack.\n", edited_count);
    return edited_count;
}
