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
    none, xflip, yflip, transpose, invert, fullrc
};

enum SPEED_MODE
{
    bpm, rate, guess
};

enum BPM_MODE
{
    main_bpm_mode, max_bpm_mode
};

struct mapinfo
{
    char *fullpath; // should be absolute path if it's returned from read_beatmap
    char *audioname, *bgname;
    char *diffname, *songname;

    double maxbpm, minbpm;
    int tp_count;
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

struct editdata // data needed to edit a map
{
    const struct mapinfo *mi;
    float hp, cs, ar, od;
    bool scale_ar, scale_od;
    bool cap_ar, cap_od;
    bool makezip;

    unsigned char cv_mapset;

    double speed;
    double base_bpm;
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

    int inv_value;
    int inv_divisor;
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
    double emuldt;
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

    float orig_ar, orig_od;
};

#define DEFAULT_FMT "@diffname@ @RATE@ @BPM@ @emuldt@ @cs@ @ar@ @od@ @hp@ @cut@ @flip@ @nosv@"

#define CUSTOM_DIFF_VAR "COSU_CUSTOM_DIFF_FORMAT"

#define BPM_MODE_VAR "COSU_BPM_MODE"

#define GET_DEFAULT_BPM_MODE() ((getenv(BPM_MODE_VAR) && strcmp(getenv(BPM_MODE_VAR), "main") == 0) ? main_bpm_mode : max_bpm_mode)

#define BTLEN_BPM(x) (1.0 / (x) * 1000.0 * 60.0)

#ifdef __cplusplus
extern "C"
{
#endif

float scale_ar(float ar, double speed, int mode);
float scale_od(float od, double speed, int mode);
struct mapinfo *read_beatmap(char *mapfile);
double read_main_bpm(struct mapinfo *info);
void free_mapinfo(struct mapinfo *info);
int edit_beatmap(struct editdata *edit);

#ifdef __cplusplus
}
#endif
