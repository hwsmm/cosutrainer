#pragma once
#include "mapeditor.h"

struct ar_entry
{
    double minar;
    double maxar;
    char *command;
    bool maxoverlap;
    struct ar_entry *next;
};

struct ar_conf
{
    struct ar_entry *ar_entry_list;
    char *default_cmd;
    bool playing;
};

#ifdef __cplusplus
extern "C"
{
#endif

int init_arconfcmd(struct ar_conf *conf);
int run_ardefcmd(struct ar_conf *conf);
int run_arconfcmd(struct ar_conf *conf, unsigned int mods, struct mapinfo *info, bool new_playing);
void free_arconfcmd(struct ar_conf *conf);

#ifdef __cplusplus
}
#endif
