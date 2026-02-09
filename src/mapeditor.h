#pragma once
#include <stdbool.h>
#include "buffers.h"
#include "audiospeed.h"

enum SECTION
{
    //1   2       3      4        5       6         7           8       9             10       12          13
    root, header, empty, general, editor, metadata, difficulty, events, timingpoints, colours, hitobjects, unknown
};

enum FLIP
{
    none, xflip, yflip, transpose, invert
};

enum SPEED_MODE
{
    bpm, rate, guess
};

struct mapinfo
{
    char *fullpath; // should be absolute path if it's returned from read_beatmap
    char *audioname, *bgname;
    char *diffname, *songname;

    double maxbpm, minbpm;
    float hp, cs, ar, od;
    float slider_multiplier, slider_tick_rate;

    int mode;
    bool arexists;
    bool tagexists;
    bool diffexists;

    int read_sections;
    int version;

    void *extra;
};

struct bgdata
{
    unsigned char *data;
    int x, y;
};

struct editdata // data needed to edit a map
{
    const struct mapinfo *mi;
    double hp, cs, ar, od;
    bool scale_ar, scale_od;
    bool makezip;

    double speed;
    enum SPEED_MODE bpmmode;
    bool pitch;
    bool nospinner;
    bool remove_sv;
    enum FLIP flip;

    bool cut_combo;
    long cut_start;
    long cut_end;

    update_progress_cb progress_callback;
    void *data;
};

struct timingpoint
{
    long time;
    double beatlength;
};

struct hitobject
{
    int x, y, type;
    int combo;
    long time;
    int timing_real_idx;
    int timing_idx;
};

struct editpass // temporary data that's only needed in editing (only passed within edit_beatmap)
{
    unsigned int editsize;
    char *editline;
    bool emuldt;
    bool arexists;
    bool tagexists;
    bool diffexists;
    bool first_tp_passed;

    struct buffers *bufs;
    struct editdata *ed;

    int prior_read;
    bool done_saving;

    struct hitobject *hitobjects;
    int hitobjects_num;
    int hitobjects_size;
    int hitobjects_idx;

    struct timingpoint *timingpoints;
    int timingpoints_num;
    int timingpoints_size;
    int timingpoints_idx;
    int uninherited_tp_idx;

    int max_combo;
};

#ifdef __cplusplus
extern "C"
{
#endif

double scale_ar(double ar, double speed, int mode);
double scale_od(double od, double speed, int mode);
struct mapinfo *read_beatmap(char *mapfile, struct bgdata *bg);
void free_bginfo(struct bgdata *bg);
void free_mapinfo(struct mapinfo *info);
int edit_beatmap(struct editdata *edit);

#ifdef __cplusplus
}
#endif
