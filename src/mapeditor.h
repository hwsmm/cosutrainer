#pragma once
#include <stdbool.h>
#include "buffers.h"

enum SECTION
{
    //1   2       3      4        5       6         7           8       9             10       12          13
    root, header, empty, general, editor, metadata, difficulty, events, timingpoints, colours, hitobjects, unknown
};

enum FLIP
{
    none, xflip, yflip, transpose
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

    int mode;
    bool arexists;
    bool tagexists;
    bool diffexists;

    int read_sections;
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
    enum FLIP flip;
    
    long cut_start;
    long cut_end;
};

struct editpass // temporary data that's only needed in editing (only passed within edit_beatmap)
{
    unsigned int editsize;
    char *editline;
    bool emuldt;
    bool arexists;
    bool tagexists;
    bool diffexists;

    struct buffers *bufs;
    struct editdata *ed;
};

#ifdef __cplusplus
extern "C"
{
#endif

double scale_ar(double ar, double speed, int mode);
double scale_od(double od, double speed, int mode);
struct mapinfo *read_beatmap(char *mapfile);
void free_mapinfo(struct mapinfo *info);
int edit_beatmap(struct editdata *edit, volatile float *progress);

#ifdef __cplusplus
}
#endif
