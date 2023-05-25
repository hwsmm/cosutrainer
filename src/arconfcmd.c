#include "arconfcmd.h"
#include "tools.h"
#include "cosuplatform.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

int init_arconfcmd(struct ar_conf *conf)
{
    conf->ar_entry_list = NULL;
    conf->default_cmd = NULL;
    conf->playing = false;

    char *homed = (char*) getenv("HOME");
    if (homed == NULL)
    {
        printerr("HOME environment variable is not available!");
        return -1;
    }

    int arconfnsize = strlen(homed) + sizeof("/.cosu_arconf");
    char *arconfn = (char*) malloc(arconfnsize);
    if (arconfn == NULL)
    {
        printerr("Failed allocation");
        return -99;
    }
    snprintf(arconfn, arconfnsize, "%s/.cosu_arconf", homed);

    FILE *cf = fopen(arconfn, "r");
    if (cf == NULL)
    {
        perror(arconfn);
        return -2;
    }

    char line[1024]; // should be plenty enough... i guess
    struct ar_entry *index = conf->ar_entry_list;
    while (fgets(line, 1024, cf))
    {
        char *startars = strtok(line, " ");
        if (startars == NULL) continue;
        char *cmd;
        if (CMPSTR(startars, "default") && conf->default_cmd == NULL)
        {
            cmd = trim(startars + strlen(startars) + 1, NULL);
            conf->default_cmd = (char*) malloc(strlen(cmd) + 1);
            if (conf->default_cmd == NULL)
            {
                printerr("Failed allocation!");
                free_arconfcmd(conf);
                fclose(cf);
                return -3;
            }
            strcpy(conf->default_cmd, cmd);
            continue;
        }

        char *endars = strtok(NULL, " ");
        if (endars == NULL) continue;
        cmd = trim(endars + strlen(endars) + 1, NULL);

        float startar = atof(startars);
        float endar = atof(endars);

        if (startar < 0.0f || endar < 0.0f || endar < startar) continue;

        if (index == NULL)
        {
            conf->ar_entry_list = (struct ar_entry*) malloc(sizeof(struct ar_entry));
            index = conf->ar_entry_list;
        }
        else
        {
            index->next = (struct ar_entry*) malloc(sizeof(struct ar_entry));
            index = index->next;
        }

        if (index == NULL)
        {
            printerr("Failed allocation!");
            free_arconfcmd(conf);
            fclose(cf);
            return -3;
        }

        index->next = NULL;
        index->minar = startar;
        index->maxar = endar;
        index->maxoverlap = false;

        index->command = (char*) malloc(strlen(cmd) + 1);
        if (index->command == NULL)
        {
            printerr("Failed allocation!");
            free_arconfcmd(conf);
            fclose(cf);
            return -3;
        }
        strcpy(index->command, cmd);
    }
    fclose(cf);

    index = conf->ar_entry_list;
    while (index != NULL)
    {
        struct ar_entry *index2 = conf->ar_entry_list;
        while (index2 != NULL)
        {
            if (index != index2 && index->maxar == index2->minar)
            {
                index->maxoverlap = true;
                break;
            }
            index2 = index2->next;
        }
        index = index->next;
    }
    return 0;
}

int run_ardefcmd(struct ar_conf *conf)
{
    return fork_launch(conf->default_cmd);
}

int run_arconfcmd(struct ar_conf *conf, unsigned int mods, struct mapinfo *info, bool new_playing)
{
    if (conf->playing && !new_playing)
    {
        conf->playing = false;
        run_ardefcmd(conf);
        return 0;
    }
    else if (!conf->playing && new_playing)
    {
        conf->playing = true;
    }
    else if (conf->playing == new_playing)
    {
        return 0;
    }

    if (info->mode == 1 || info->mode == 3)
    {
        run_ardefcmd(conf);
        return 0;
    }

    bool run = false;
    double ar = info->ar;

    if (mods & (1 << 4)) // HR
    {
        ar *= 1.4;
    }
    else if (mods & (1 << 1)) // EZ
    {
        ar *= 0.5;
    }

    CLAMP(ar, 0.0f, 10.0f);

    if (mods & (1 << 6)) // DT or NC
    {
        ar = scale_ar(ar, 1.5, info->mode);
    }
    else if (mods & (1 << 8)) // HT
    {
        ar = scale_ar(ar, 0.75, info->mode);
    }

    struct ar_entry *index = conf->ar_entry_list;
    while (index != NULL)
    {
        if (ar >= index->minar && (index->maxoverlap ? ar < index->maxar : ar <= index->maxar))
        {
            run = true;
            fork_launch(index->command);
            break;
        }
        index = index->next;
    }

    if (!run) run_ardefcmd(conf);
    return 0;
}

void free_arconfcmd(struct ar_conf *conf)
{
    struct ar_entry *index = conf->ar_entry_list;
    while (index != NULL)
    {
        free(index->command);
        free(index);
        index = index->next;
    }
    free(conf->default_cmd);
}
