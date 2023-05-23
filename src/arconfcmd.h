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

/*
~/.cosu_arcmd
0,5,xgamma -gamma 2.4
5,8,xgamma -gamma 2.2
8,10,xgamma -gamma 2.0
10,11,xgamma -gamma 1.8
*/

int init_arconfcmd(struct ar_conf *conf);
int run_ardefcmd(struct ar_conf *conf);
int run_arconfcmd(struct ar_conf *conf, unsigned int mods, struct mapinfo *info, bool new_playing);
void free_arconfcmd(struct ar_conf *conf);
