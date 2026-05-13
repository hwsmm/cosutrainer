#include "mapeditor.h"
#include "tools.h"
#include "cosuplatform.h"
#include "actualzip.h"
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>

#define tkn(x) strtok(x, ",")
#define nexttkn() strtok(NULL, ",")

#define fail_nulltkn(x) \
if ((x = nexttkn()) == NULL) \
{ \
   printerr("Failed parsing!"); \
   return 1; \
}

static int _allocput(char **dest, const char *src)
{
    if (*dest != NULL) free(*dest);
    int namelen = strlen(src);
    *dest = (char*) malloc(namelen + 1);
    if (*dest == NULL) return -99;
    strcpy(*dest, src);
    return 0;
}

#define allocput(dest, src) \
if (_allocput(&dest, src) != 0) return -99;

static int _resize(struct editpass *ep, unsigned long size)
{
    char *resi = (char*) realloc(ep->editline, size);
    if (resi == NULL)
    {
        printerr("Failed reallocating!");
        return -1;
    }
    ep->editline = resi;
    ep->editsize = size;
    return 0;
}

#define resize(x) \
if (_resize(ep, x) != 0) return -1;

static int _putstr(struct editpass *ep, unsigned int *ecur, const char *src, unsigned int len)
{
    if (ep->editsize - *ecur - 1 < len)
    {
        if (_resize(ep, ep->editsize + len + 1024) != 0) return -1;
    }
    strcpy(ep->editline + *ecur, src);
    *ecur += len;
    return 0;
}

#define putstr(x, len) \
if (_putstr(ep, &ecur, x, len) != 0) return -1;
#define putsstr(x) putstr(x, sizeof(x) - 1);
#define putdstr(x) putstr(x, strlen(x));

static int _snpedit(struct editpass *ep, unsigned int *ecur, const char *format, ...)
{
    va_list args, cpy;
    va_start(args, format);
    va_copy(cpy, args);

    unsigned int written = 0;
    size_t writable = ep->editsize - *ecur - 1;
    while ((written = vsnprintf(ep->editline + *ecur, writable, format, cpy)) >= writable)
    {
        va_end(cpy);
        va_copy(cpy, args);
        if (_resize(ep, ep->editsize + 1024) != 0) return -1;
        writable = ep->editsize - *ecur - 1;
    }
    *ecur += written;

    va_end(cpy);
    va_end(args);
    return 0;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define snpedit(fmt, ...) \
if (_snpedit(ep, &ecur, fmt, ##__VA_ARGS__) != 0) return -1;
#pragma clang diagnostic pop

#define find_null(str) (str + strlen(str) + 1)

float scale_ar(float ar, double speed, int mode)
{
    if (mode == 1 || mode == 3 || speed == 1.0) return ar;

    double ms = ar <= 5.0 ? 1800.0 - 120.0 * ar : 1950.0 - 150.0 * ar;
    ms /= speed;
    ms = ms >= 1200.0 ? (1800.0 - ms) / 120.0 : (1950.0 - ms) / 150.0;
    return round(ms * 10.0) / 10.0;
}

float scale_od(float od, double speed, int mode)
{
    if (speed == 1.0) return od;

    double base;
    double multiplier;

    switch (mode)
    {
    case 0:
        base = 80;
        multiplier = 6;
        break;
    case 1:
        base = 50;
        multiplier = 3;
        break;
    case 3:
        base = 64;
        multiplier = 3;
        break;
    default:
        return od;
    }

    double ms = base - multiplier * od;
    ms /= speed;
    ms = (base - ms) / multiplier;
    return round(ms * 10.0) / 10.0;
}

static int loop_map(char *mapfile, int (*func)(char*, void*, enum SECTION), void *pass)
{
    FILE *source = fopen(mapfile, "rb");
    if (!source)
    {
        perror(mapfile);
        return 1;
    }

    unsigned int current_size = 1024;
    unsigned int realloc_step = 1024;
    char *line = (char*) malloc(current_size);
    char *tmpline = NULL;
    int ret = 0;

    if (line == NULL)
    {
        printerr("Failed allocating memory");
        return -99;
    }

    enum SECTION sect = root;
    enum SECTION newsect = root;
    bool file_end = false;
    while (ret == -20 /*loop again*/ || fgets(line, current_size, source))
    {
        sect = newsect;
        if (!file_end && strchr(line, '\n') == NULL)
        {
            tmpline = (char*) realloc(line, current_size + realloc_step);
            if (!tmpline)
            {
                printerr("Failed reallocating buffer");
                ret = -99;
                break;
            }
            line = tmpline;
            if (fgets(strchr(line, '\0'), realloc_step, source) == NULL) file_end = true;
            current_size += realloc_step;
            ret = -20;
            continue;
        }

        if (*line == '[')
        {
            if (CMPSTR(line, "[General]")) newsect = general;
            else if (CMPSTR(line, "[Editor]")) newsect = editor;
            else if (CMPSTR(line, "[Metadata]")) newsect = metadata;
            else if (CMPSTR(line, "[Difficulty]")) newsect = difficulty;
            else if (CMPSTR(line, "[Events]")) newsect = events;
            else if (CMPSTR(line, "[TimingPoints]")) newsect = timingpoints;
            else if (CMPSTR(line, "[Colours]")) newsect = colours;
            else if (CMPSTR(line, "[HitObjects]")) newsect = hitobjects;
            else newsect = unknown;
            sect = header;
        }
        else if (*line == '\r' || *line == '\n' || *line == '\0' || *line == '/')
        {
            newsect = sect;
            sect = empty;
        }
        else if (isspace(*line))
        {
            bool onlyspace = true;
            char *check = line;

            while (*(++check) != '\0')
            {
                if (!isspace(*check))
                {
                    onlyspace = false;
                    break;
                }
            }

            if (onlyspace)
            {
                newsect = sect;
                sect = empty;
            }
        }

        if (sect != unknown)
        {
            ret = (*func)(line, pass, sect);
            if (ret == -20)
            {
                // loop again
            }
            else if (ret == -12345)
            {
                // early end
                ret = 0;
                break;
            }
            else if (ret != 0)
            {
                break;
            }
        }
    }

    free(line);
    fclose(source);
    return ret;
}

struct readmap_pass
{
    struct mapinfo *info;
    bool calc_mainbpm;
    int read_sections;

    struct timingpoint *timingpoints;
    int timingpoints_num;
    int timingpoints_size;
    int timingpoints_idx;
    int uninherited_tp_idx;

    long last_end_time;
};

static int write_mapinfo(char *line, void *vpass, enum SECTION sect)
{
    struct readmap_pass *pass = (struct readmap_pass *) vpass;
    struct mapinfo *info = pass->info;

    if (sect != empty && (pass->read_sections & (1 << sect)) == 0)
    {
        if (!pass->calc_mainbpm && (pass->read_sections & 0b111101000) == 0b111101000)
        {
            return -12345;
        }
        pass->read_sections |= (1 << sect);
    }

    if (sect == root)
    {
        if (strstr(line, "osu file format") != NULL)
        {
            char *vstr = strrchr(line, 'v');
            if (vstr != NULL)
            {
                info->version = atoi(vstr + 1);
            }
        }
    }
    else if (sect == timingpoints)
    {
        long time = atol(tkn(line));
        char *beatlengthstr;
        fail_nulltkn(beatlengthstr);
        double beatlength = atof(beatlengthstr);

        if (*beatlengthstr != '-')
        {
            if (info->maxbpm == 0 || beatlength <= info->maxbpm) info->maxbpm = beatlength;
            if (info->minbpm == 0 || beatlength > info->minbpm) info->minbpm = beatlength;
        }

        if (pass->calc_mainbpm)
        {
            if (pass->timingpoints == NULL || pass->timingpoints_size < pass->timingpoints_num + 1)
            {
                struct timingpoint *newarr = (struct timingpoint*) realloc(pass->timingpoints,
                    (pass->timingpoints_size + 16) * sizeof(struct timingpoint));
                if (newarr == NULL)
                {
                    printerr("Failed timingpoint allocation");
                    return -99;
                }
                pass->timingpoints = newarr;
                pass->timingpoints_size += 16;
            }
            pass->timingpoints[pass->timingpoints_num].time = time;
            pass->timingpoints[pass->timingpoints_num].beatlength = beatlength;
            pass->timingpoints_num++;
        }
    }
    else if (sect == general)
    {
        if (CMPSTR(line, "AudioFilename:"))
        {
            char *filename = CUTFIRST(line, "AudioFilename:");
            allocput(info->audioname, filename);
        }
        else if (CMPSTR(line, "Mode:"))
        {
            info->mode = atoi(CUTFIRST(line, "Mode:"));
        }
    }
    else if (sect == difficulty)
    {
        if (CMPSTR(line, "HPDrainRate:"))            info->hp = atof(CUTFIRST(line, "HPDrainRate:"));
        else if (CMPSTR(line, "CircleSize:"))        info->cs = atof(CUTFIRST(line, "CircleSize:"));
        else if (CMPSTR(line, "OverallDifficulty:")) info->od = atof(CUTFIRST(line, "OverallDifficulty:"));
        else if (CMPSTR(line, "ApproachRate:"))      info->arexists = true, info->ar = atof(CUTFIRST(line, "ApproachRate:"));
        else if (CMPSTR(line, "SliderMultiplier:"))  info->slider_multiplier = atof(CUTFIRST(line, "SliderMultiplier:"));
        else if (CMPSTR(line, "SliderTickRate:"))    info->slider_tick_rate = atof(CUTFIRST(line, "SliderTickRate:"));
    }
    else if (sect == metadata)
    {
        if (CMPSTR(line, "Version:"))
        {
            info->diffexists = true;
            char *diffname = CUTFIRST(line, "Version:");
            allocput(info->diffname, diffname);
        }
        else if (CMPSTR(line, "Title:"))
        {
            char *songname = CUTFIRST(line, "Title:");
            allocput(info->songname, songname);
        }
        else if (CMPSTR(line, "Tags:"))
        {
            info->tagexists = true;
        }
    }
    else if (sect == events)
    {
        if (CMPSTR(line, "0,"))
        {
            char *bgname = tkn(line);
            fail_nulltkn(bgname);
            fail_nulltkn(bgname);

            if (*bgname == '\"')
            {
                bgname++;
                char *endhere = strchr(bgname, '\"');
                if (endhere == NULL)
                {
                    printerr("Failed parsing background line!");
                    return 1;
                }

                *endhere = '\0';
            }

            allocput(info->bgname, bgname);
        }
    }
    else if (sect == hitobjects && pass->calc_mainbpm)
    {
        char *xstr = tkn(line);
        char *ystr;
        fail_nulltkn(ystr);
        char *timestr;
        fail_nulltkn(timestr);
        long time = atol(timestr);

        char *typestr;
        fail_nulltkn(typestr);
        int type = atoi(typestr);

        while (pass->timingpoints_idx + 1 < pass->timingpoints_num &&
               time >= pass->timingpoints[pass->timingpoints_idx + 1].time)
        {
            pass->timingpoints_idx++;

            if (pass->timingpoints[pass->timingpoints_idx].beatlength > 0)
                pass->uninherited_tp_idx = pass->timingpoints_idx;
        }

        long end_time = time;

        if (type & (1 << 1))
        {
            char *hitsoundstr;
            fail_nulltkn(hitsoundstr);
            char *curvestr;
            fail_nulltkn(curvestr);
            char *slidestr;
            fail_nulltkn(slidestr);
            char *lengthstr;
            fail_nulltkn(lengthstr);
        }
        else if (type & (1 << 3 | 1 << 7))
        {
            char *hitsoundstr;
            fail_nulltkn(hitsoundstr);
            char *endstr = strtok(NULL, ":,");
            if (endstr != NULL)
                end_time = atol(endstr);
        }

        if (end_time > pass->last_end_time)
            pass->last_end_time = end_time;
    }

    return 0;
}

struct mainbpm_duration
{
    double beatlength;
    long duration;
    long first_time;
};

static void compute_mainbpm(struct readmap_pass *pass)
{
    struct mapinfo *info = pass->info;
    int durations_num = 0;
    int durations_size = 0;
    struct mainbpm_duration *durations = NULL;

    for (int i = 0; i < pass->timingpoints_num; i++)
    {
        double beatlength = pass->timingpoints[i].beatlength;
        if (beatlength <= 0)
            continue;

        long start_time = pass->timingpoints[i].time;
        long end_time = pass->last_end_time;

        for (int j = i + 1; j < pass->timingpoints_num; j++)
        {
            if (pass->timingpoints[j].beatlength > 0)
            {
                end_time = pass->timingpoints[j].time;
                break;
            }
        }

        if (end_time > pass->last_end_time)
            end_time = pass->last_end_time;

        long duration = end_time - start_time;
        if (duration <= 0)
            continue;

        int found = -1;
        for (int k = 0; k < durations_num; k++)
        {
            if (fabs(durations[k].beatlength - beatlength) < 1e-6)
            {
                durations[k].duration += duration;
                found = k;
                break;
            }
        }

        if (found < 0)
        {
            if (durations == NULL || durations_size < durations_num + 1)
            {
                struct mainbpm_duration *newarr =
                    (struct mainbpm_duration *) realloc(durations,
                        (durations_size + 8) * sizeof(struct mainbpm_duration));
                if (newarr == NULL)
                {
                    printerr("Failed main BPM allocation");
                    free(durations);
                    return;
                }
                durations = newarr;
                durations_size += 8;
            }
            durations[durations_num].beatlength = beatlength;
            durations[durations_num].duration = duration;
            durations[durations_num].first_time = start_time;
            durations_num++;
        }
    }

    int best_idx = -1;
    for (int i = 0; i < durations_num; i++)
    {
        if (best_idx < 0 ||
            durations[i].duration > durations[best_idx].duration ||
            (durations[i].duration == durations[best_idx].duration &&
             durations[i].first_time < durations[best_idx].first_time))
        {
            best_idx = i;
        }
    }

    if (best_idx >= 0)
        info->mainbpm = 60000.0 / durations[best_idx].beatlength;

    free(durations);
}

static void convert_vaildpath(struct mapinfo *mi)
{
    bool audok = false;
    bool bgok = false;

    int fdlen;
    char *fullpath = mi->fullpath;
    char *sep = strrchr(fullpath, PATHSEP);
    if (sep != NULL)
    {
        *sep = '\0';
        fdlen = strlen(mi->fullpath);
    }
    else
    {
        goto skip;
    }

    if (mi->audioname != NULL)
    {
#ifndef WIN32
        char *ill;
        while ((ill = strchr(mi->audioname, FOREIGN_PATHSEP)) != NULL)
            *ill = PATHSEP;
#endif

        int solen = strlen(mi->audioname);
        char *temp = (char*) malloc(fdlen + 1 + solen + 1);
        if (temp == NULL)
        {
            printerr("Failed allocation!");
            goto audskip;
        }
        strcpy(temp, fullpath);
        *(temp + fdlen) = PATHSEP;
        strcpy(temp + fdlen + 1, mi->audioname);

        int ares = try_convertwinpath(temp, fdlen + 1);
        if (ares >= 0)
        {
            free(mi->audioname);
            mi->audioname = temp;
            audok = true;
        }
        else if (ares < 0)
        {
            fprintf(stderr, "Failed checking path of audio file, make sure that it exists (not fatal): %s\n", temp);
            free(temp);
        }
    }

audskip:
    if (mi->bgname != NULL)
    {
#ifndef WIN32
        char *ill;
        while ((ill = strchr(mi->bgname, FOREIGN_PATHSEP)) != NULL)
            *ill = PATHSEP;
#endif

        int solen = strlen(mi->bgname);
        char *temp = (char*) malloc(fdlen + 1 + solen + 1);
        if (temp == NULL)
        {
            printerr("Failed allocation!");
            goto bgskip;
        }
        strcpy(temp, fullpath);
        *(temp + fdlen) = PATHSEP;
        strcpy(temp + fdlen + 1, mi->bgname);

        int bres = try_convertwinpath(temp, fdlen + 1);
        if (bres >= 0)
        {
            free(mi->bgname);
            mi->bgname = temp;
            bgok = true;
        }
        else if (bres < 0)
        {
            fprintf(stderr, "Failed checking path of BG file, make sure that it exists (not fatal): %s\n", temp);
            free(temp);
        }
    }
bgskip:
    *sep = PATHSEP;
skip:
    if (!audok)
    {
        free(mi->audioname);
        mi->audioname = NULL;
    }

    if (!bgok)
    {
        free(mi->bgname);
        mi->bgname = NULL;
    }
}

struct mapinfo *read_beatmap(char *mapfile)
{
    struct mapinfo *info = (struct mapinfo*) calloc(1, sizeof(struct mapinfo));
    if (info == NULL)
    {
        printerr("Failed allocation");
        return NULL;
    }

    struct readmap_pass pass = { 0, };
    pass.info = info;
    pass.calc_mainbpm = true;

    int ret = loop_map(mapfile, &write_mapinfo, &pass);
    if (ret == 0)
    {
        if (strchr(mapfile, PATHSEP) == NULL)
        {
            info->fullpath = (char*) malloc(2 + strlen(mapfile) + 1);
            if (info->fullpath != NULL)
            {
                strcpy(info->fullpath, "." STR_PATHSEP);
                strcat(info->fullpath, mapfile);
            }
        }
        else
        {
            info->fullpath = _strdup(mapfile);
        }

        if (info->fullpath == NULL)
        {
            printerr("Failed allocation");
            free_mapinfo(info);
            free(pass.timingpoints);
            return NULL;
        }

        compute_mainbpm(&pass);

        info->maxbpm = 1 / info->maxbpm * 1000 * 60;
        info->minbpm = 1 / info->minbpm * 1000 * 60;
        if (info->mainbpm <= 0.0)
            info->mainbpm = info->maxbpm;

        if (!info->arexists) info->ar = info->od;

        if (info->version <= 0)
        {
            printerr("Invalid osu! file without version");
            free_mapinfo(info);
            free(pass.timingpoints);
            return NULL;
        }

        convert_vaildpath(info);
    }
    else
    {
        printerr("Failed reading a map!");
        free_mapinfo(info);
        free(pass.timingpoints);
        return NULL;
    }

    free(pass.timingpoints);
    return info;
}

void free_mapinfo(struct mapinfo *info)
{
    if (info != NULL)
    {
        if (info->bgname != NULL) free(info->bgname);
        if (info->audioname != NULL) free(info->audioname);
        if (info->songname != NULL) free(info->songname);
        if (info->diffname != NULL) free(info->diffname);
        if (info->fullpath != NULL) free(info->fullpath);
        free(info);
    }
}

static int convert_map(char *line, void *vinfo, enum SECTION sect)
{
    int ret = 0;
    struct editpass *ep = (struct editpass*) vinfo;
    bool edited = false;
    unsigned int ecur = 0;
    double speed = ep->ed->speed;
    double basebpm = ep->ed->bpmrefmode == max_bpm_mode ? ep->ed->mi->maxbpm : ep->ed->mi->mainbpm;
    bool prior_read = ep->prior_read && !ep->done_saving;

    if (!prior_read && sect == root)
    {
        // https://osu.ppy.sh/beatmapsets/134151#osu/336571
        // some insane map files put weird characters at first
        if (strstr(line, "osu file format") != NULL)
        {
            edited = true;
            putsstr("osu file format v14\r\n");
        }
    }
    else if (!prior_read && sect == events)
    {
        if (*line == '2' || CMPSTR(line, "Break"))
        {
            edited = true;
            if (ep->ed->flip != invert)
            {
                tkn(line);
                char *start;
                fail_nulltkn(start);
                char *end;
                fail_nulltkn(end);
                snpedit("2,%ld,%ld\r\n", (long) (atol(start) / speed), (long) (atol(end) / speed));
            }
        }
        else if (*line == '0')
        {
        }
        else edited = true;
    }
    else if (sect == timingpoints)
    {
        edited = true;
        long time = atol(tkn(line)) / speed;
        char *btlenstr = nexttkn(); // most likely it won't fail if reading succeeded
        double btlen = atof(btlenstr);

        if (prior_read)
        {
            if (ep->prior_read == 1)
            {
                if (ep->timingpoints == NULL || ep->timingpoints_size < ep->timingpoints_num + 1)
                {
                    struct timingpoint *newarr = (struct timingpoint*)realloc(ep->timingpoints, (ep->timingpoints_size + 5) * sizeof(struct timingpoint));
                    if (newarr == NULL)
                    {
                        printerr("Failed timingpoint allocation");
                        return 3;
                    }

                    ep->timingpoints = newarr;
                    ep->timingpoints_size += 5;
                }

                ep->timingpoints[ep->timingpoints_num].time = time;
                ep->timingpoints[ep->timingpoints_num].beatlength = btlen >= 0 ? btlen / speed : btlen;
                ep->timingpoints_num++;
            }
        }
        else if (!ep->ed->remove_sv || !ep->first_tp_passed)
        {
            if (ep->ed->remove_sv)
                ep->first_tp_passed = true;

            if (*btlenstr != '-')
            {
                btlen /= speed;
                snpedit("%ld,%.12lf,%s", time, btlen, find_null(btlenstr));
            }
            else
            {
                snpedit("%ld,%s,%s", time, btlenstr, find_null(btlenstr));
            }
        }
    }
    else if (sect == hitobjects)
    {
        edited = true;
        char *xstr = tkn(line);
        char *ystr;
        fail_nulltkn(ystr);
        char *timestr;
        fail_nulltkn(timestr);

        int x = atoi(xstr);
        int y = atoi(ystr);
        long origtime = atol(timestr);
        long time = origtime / speed;

        char *typestr;
        fail_nulltkn(typestr);
        int type = atoi(typestr);

        if (ep->ed->flip == xflip || ep->ed->flip == transpose) x = 512 - x;
        if (ep->ed->flip == yflip || ep->ed->flip == transpose) y = 384 - y;

        if (type & (1<<3) && ep->ed->nospinner)
        {
            // do nothing to remove spinner lines
        }
        else if (!ep->ed->cut_combo && (origtime < ep->ed->cut_start || origtime > ep->ed->cut_end))
        {
            // do nothing to create a practice diff
        }
        else if (ep->done_saving && ep->ed->cut_combo &&
                 (ep->hitobjects[ep->hitobjects_idx].combo < ep->ed->cut_start || ep->hitobjects[ep->hitobjects_idx].combo > ep->ed->cut_end))
        {
            // do nothing to create a practice diff
        }
        else if (prior_read)
        {
            // this requires TimingPoints to be read first
            if ((ep->prior_read == 1 && ep->timingpoints_num > 0) || ep->prior_read == 2)
            {
                // this is run prior to main edit when hitobject data is needed for further processing (e.g. mania full LN)
                if (ep->hitobjects == NULL || ep->hitobjects_size < ep->hitobjects_num + 1)
                {
                    struct hitobject *newarr = (struct hitobject*)realloc(ep->hitobjects, (ep->hitobjects_size + 300) * sizeof(struct hitobject));
                    if (newarr == NULL)
                    {
                        printerr("Failed hitobject allocation");
                        return 3;
                    }

                    ep->hitobjects = newarr;
                    ep->hitobjects_size += 300;
                }

                ep->hitobjects[ep->hitobjects_num].x = x;
                ep->hitobjects[ep->hitobjects_num].y = y;
                ep->hitobjects[ep->hitobjects_num].time = time;
                ep->hitobjects[ep->hitobjects_num].type = type;

                while (ep->timingpoints_idx + 1 < ep->timingpoints_num && time >= ep->timingpoints[ep->timingpoints_idx + 1].time)
                {
                    ep->timingpoints_idx++;

                    if (ep->timingpoints[ep->timingpoints_idx].beatlength > 0)
                        ep->uninherited_tp_idx = ep->timingpoints_idx;
                }

                ep->hitobjects[ep->hitobjects_num].timing_real_idx = ep->timingpoints_idx;
                ep->hitobjects[ep->hitobjects_num].timing_idx = ep->uninherited_tp_idx;

                ep->max_combo += 1;
                ep->hitobjects[ep->hitobjects_num].combo = ep->max_combo;

                if (type & (1<<1))
                {
                    char *hitsoundstr;
                    fail_nulltkn(hitsoundstr);
                    char *curvestr;
                    fail_nulltkn(curvestr);
                    char *slidestr;
                    fail_nulltkn(slidestr);
                    char *lengthstr;
                    fail_nulltkn(lengthstr);

                    int repeat = atoi(slidestr);
                    int pixel_length = atoi(lengthstr);

                    // taken from https://github.com/JerryZhu99/osu-practice-tool/blob/master/src/osufile.js
                    double sv_multiplier = 1.0;
                    double ms_per_beat = ep->timingpoints[ep->timingpoints_idx].beatlength;
                    if (ms_per_beat < 0)
                        sv_multiplier = 100.0 / -ms_per_beat;

                    const double epsilon = 0.1;
                    double pixels_per_beat = ep->ed->mi->slider_multiplier * 100.0;
                    if (ep->ed->mi->version >= 8)
                    {
                        pixels_per_beat *= sv_multiplier;
                    }

                    double num_beats = pixel_length * repeat / pixels_per_beat;
                    int ticks = (int)ceil((num_beats - epsilon) / repeat * ep->ed->mi->slider_tick_rate) - 1;
                    if (ticks < 0)
                        ticks = 0;

                    ep->max_combo += ticks * repeat;
                    ep->max_combo += repeat;
                }
                else if (type & (1<<7))
                {
                    char *hitsoundstr;
                    fail_nulltkn(hitsoundstr);
                    char *endstr = strtok(NULL, ":,");
                    long time = atol(endstr) - origtime;
                    double length = time / speed;

                    ep->max_combo += (int)floor(length / 100.0);
                }

                ep->hitobjects_num++;
            }
        }
        else // does something
        {
            bool used = false;
            if (ep->ed->flip == invert)
            {
                if (type & (1<<7 | 1))
                {
                    for (int i = ep->hitobjects_idx + 1; i < ep->hitobjects_num; i++)
                    {
                        int c1 = (int)(x * ep->ed->mi->cs / 512.0);
                        int c2 = (int)(ep->hitobjects[i].x * ep->ed->mi->cs / 512.0);
                        if (c1 == c2 && (ep->hitobjects[i].type & (1<<7 | 1)) && ep->hitobjects[i].time > time)
                        {
                            long fourth = (long)(ep->timingpoints[ep->hitobjects[i].timing_idx].beatlength / 4.0);
                            long end_time = ep->hitobjects[i].time - fourth;
                            if (end_time - time >= fourth)
                            {
                                char *hitsoundstr = nexttkn();
                                char *afnul = find_null(hitsoundstr);
                                if (type & (1<<7))
                                {
                                    char *orig_len = strtok(NULL, ":,");
                                    afnul = find_null(orig_len);
                                }

                                snpedit("%d,%d,%ld,128,%s,%ld:%s", x, y, time, hitsoundstr, end_time, afnul);

                                used = true;
                            }
                            break;
                        }
                    }
                }
            }

            if (!used)
            {
                if (ep->ed->flip == fullrc && (type & (1<<7)))
                {
                    char *hitsoundstr;
                    fail_nulltkn(hitsoundstr);
                    char *endstr = strtok(NULL, ":,");
                    if (endstr == NULL)
                    {
                        printerr("Failed parsing long note!");
                        return 1;
                    }
                    char *afnul = find_null(endstr);

                    int rctype = (type & ~(1<<7)) | 1;
                    snpedit("%d,%d,%ld,%d,%s", x, y, time, rctype, hitsoundstr);

                    if (*(afnul - 2) == '\r' || *(afnul - 2) == '\n')
                    {
                        putsstr("\r\n");
                    }
                    else
                    {
                        putsstr(",");
                        putdstr(afnul);
                    }
                }
                else if (type & (1<<3 | 1<<7))
                {
                    char spinnertoken[] = { (type & (1<<7)) ? ':' : ',', '\0' };
                    char *hitsoundstr;
                    fail_nulltkn(hitsoundstr);
                    // This is different from spinnertoken, because some mania conversion tools produce
                    // weird long note objects that use , as delimiter like standard spinners, and osu! also parses them with no problem.
                    // So parse whatever if it is either spinner or long note, and produce a hitobject with 'correct' delimiter.
                    char *spinnerstr = strtok(NULL, ":,");
                    if (spinnerstr == NULL)
                    {
                        printerr("Failed parsing spinner/long note!");
                        return 1;
                    }
                    char *afnul = find_null(spinnerstr);

                    long spinnerlen = atol(spinnerstr) / speed;

                    snpedit("%d,%d,%ld,%s,%s,%ld", x, y, time, typestr, hitsoundstr, spinnerlen);

                    if (*(afnul - 2) == '\r' || *(afnul - 2) == '\n')
                    {
                        putsstr("\r\n");
                    }
                    else
                    {
                        putsstr(spinnertoken);
                        putdstr(afnul);
                    }
                }
                else if (type & (1<<1) && ep->ed->flip != none)
                {
                    char *hitsoundstr;
                    fail_nulltkn(hitsoundstr);
                    char *curvetype = strtok(NULL, "|");
                    if (curvetype == NULL)
                    {
                        printerr("Failed parsing slider!");
                        return 1;
                    }
                    char *afnul = find_null(curvetype);

                    snpedit("%d,%d,%ld,%s,%s,%s|", x, y, time, typestr, hitsoundstr, curvetype);

                    bool done = false;
                    while (!done)
                    {
                        char *slxc = afnul;
                        while (*(afnul++) != ':') {}
                        char *slyc = afnul;
                        while (*(afnul++))
                        {
                            if (*afnul == '|')
                            {
                                *afnul = '\0';
                                break;
                            }
                            else if (*afnul == ',')
                            {
                                done = true;
                                break;
                            }
                        }

                        int slx = atoi(slxc);
                        int sly = atoi(slyc);
                        if (ep->ed->flip == xflip || ep->ed->flip == transpose) slx = 512 - slx;
                        if (ep->ed->flip == yflip || ep->ed->flip == transpose) sly = 384 - sly;
                        snpedit("%d:%d", slx, sly);
                        if (!done)
                        {
                            putsstr("|");
                            afnul++;
                        }
                    }
                    putdstr(afnul);
                }
                else
                {
                    char *afnul = find_null(typestr);
                    snpedit("%d,%d,%ld,%s,%s", x, y, time, typestr, afnul);
                }
            }
        }

        if (!prior_read)
            ep->hitobjects_idx++;
    }
    else if (!prior_read && sect == editor)
    {
        if (CMPSTR(line, "Bookmarks:"))
        {
            edited = true;
            char *p = CUTFIRST(line, "Bookmarks:");
            char *token = tkn(p);
            putsstr("Bookmarks: ");
            while (1)
            {
                long time = atol(token) / speed;
                token = nexttkn();
                if (token == NULL)
                {
                    snpedit("%ld\r\n", time);
                    break;
                }
                else
                {
                    snpedit("%ld,", time);
                }
            }
        }
    }
    else if (!prior_read && sect == general)
    {
        if (CMPSTR(line, "AudioFilename:") && speed != 1.0)
        {
            edited = true;
            if (ep->bufs->audname == NULL)
            {
                printerr("Speed is not 1.0x, but audio file name is not set");
                return 5;
            }

            snpedit("AudioFilename: %s\r\n", ep->bufs->audname);
        }
        else if (CMPSTR(line, "AudioLeadIn:"))
        {
            edited = true;
            char *p = CUTFIRST(line, "AudioLeadIn:");
            long time = atol(p) / speed;
            snpedit("AudioLeadIn: %ld\r\n", time);
        }
        else if (CMPSTR(line, "PreviewTime:"))
        {
            edited = true;
            char *p = CUTFIRST(line, "PreviewTime:");
            long time = atol(p);
            if (time > 0)
                time /= speed;

            snpedit("PreviewTime: %ld\r\n", time);
        }
    }
    else if (!prior_read && sect == difficulty)
    {
        if (!(ep->arexists))
        {
            ret = -20;
            ep->arexists = true;
            edited = true;
            snpedit("ApproachRate:%.1f\r\n", ep->ed->ar);
        }
        else if (CMPSTR(line, "HPDrainRate:"))
        {
            edited = true;
            snpedit("HPDrainRate:%.1f\r\n", ep->ed->hp);
        }
        else if (CMPSTR(line, "CircleSize:"))
        {
            edited = true;
            snpedit("CircleSize:%.1f\r\n", ep->ed->cs);
        }
        else if (CMPSTR(line, "OverallDifficulty:"))
        {
            edited = true;
            snpedit("OverallDifficulty:%.1f\r\n", ep->ed->od);
        }
        else if (CMPSTR(line, "ApproachRate:"))
        {
            edited = true;
            snpedit("ApproachRate:%.1f\r\n", ep->ed->ar);
        }
    }
    else if (!prior_read && sect == metadata)
    {
        if (!ep->tagexists)
        {
            ep->tagexists = true;
            edited = true;
            ret = -20;
            putsstr("Tags:osutrainer\r\n");
        }
        else if (!ep->diffexists || CMPSTR(line, "Version:"))
        {
            edited = true;
            const char *customfmt = getenv(CUSTOM_DIFF_VAR);
            const char *usefmt = DEFAULT_FMT;
            if (customfmt != NULL)
                usefmt = customfmt;

            int fmtlen = strlen(usefmt);

            if (!ep->diffexists)
            {
                ret = -20;
                ep->diffexists = true;
            }

            putsstr("Version:");

#define _CMPSTR(a, b) (CMPSTR(a, b) && (i += sizeof(b) - 1))
            for (int i = 0; i < fmtlen;)
            {
                const char *cur = usefmt + i;

                if (_CMPSTR(cur, "@diffname@"))
                {
                    if (ep->ed->mi->diffname != NULL)
                        putdstr(ep->ed->mi->diffname);
                }
                else if (_CMPSTR(cur, "@rate@") || _CMPSTR(cur, "@RATE@"))
                {
                    double disprate = ep->emuldt == 0 ? speed : ep->emuldt;
                    if (fabs(disprate - 1.0) >= 1e-3 || CMPSTR(cur, "@RATE@"))
                        snpedit("%gx", disprate);
                }
                else if (_CMPSTR(cur, "@bpm@") || _CMPSTR(cur, "@BPM@"))
                {
                    double disprate = ep->emuldt == 0 ? speed : ep->emuldt;
                    double dispbpm = basebpm * disprate;
                    if (fabs(dispbpm - basebpm) >= 1e-3 || CMPSTR(cur, "@BPM@"))
                        snpedit("%.0lfbpm", dispbpm);
                }
                else if (_CMPSTR(cur, "@(bpm)@") || _CMPSTR(cur, "@(BPM)@"))
                {
                    double disprate = ep->emuldt == 0 ? speed : ep->emuldt;
                    double dispbpm = basebpm * disprate;
                    if (fabs(dispbpm - basebpm) >= 1e-3 || CMPSTR(cur, "@(BPM)@"))
                        snpedit("(%.0lfbpm)", dispbpm);
                }
                else if (_CMPSTR(cur, "@emuldt@"))
                {
                    if (ep->emuldt != 0)
                    {
                        if (ep->orig_ar > 10 || ep->orig_od > 10)
                        {
                            putsstr("(DT)");
                        }
                        else
                        {
                            putsstr("(HT)");
                        }
                    }
                }
                else if (_CMPSTR(cur, "@cs@") || _CMPSTR(cur, "@CS@"))
                {
                    if ((ep->ed->mi->mode != 1 && ep->ed->mi->mode != 3) && (ep->ed->mi->cs != ep->ed->cs || CMPSTR(cur, "@CS@")))
                        snpedit("CS%.1lf", ep->ed->cs);
                }
                else if (_CMPSTR(cur, "@ar@") || _CMPSTR(cur, "@AR@"))
                {
                    if (ep->ed->mi->mode != 1 && ep->ed->mi->mode != 3
                        && (ep->emuldt != 0 || ep->ed->mi->ar != ep->ed->ar || CMPSTR(cur, "@AR@")))
                    {
                        snpedit("AR%.1lf", ep->orig_ar);
                    }
                }
                else if (_CMPSTR(cur, "@od@") || _CMPSTR(cur, "@OD@"))
                {
                    if (ep->ed->mi->mode != 2 && (ep->emuldt != 0 || ep->ed->mi->od != ep->ed->od || CMPSTR(cur, "@OD@")))
                    {
                        snpedit("OD%.1lf", ep->orig_od);
                    }
                }
                else if (_CMPSTR(cur, "@hp@") || _CMPSTR(cur, "@HP@"))
                {
                    if (ep->ed->mi->hp != ep->ed->hp || CMPSTR(cur, "@HP@")) snpedit("HP%.1lf", ep->ed->hp);
                }
                else if (_CMPSTR(cur, "@cut@"))
                {
                    if (ep->ed->cut_start > 0 || ep->ed->cut_end < LONG_MAX)
                    {
                        putsstr("Cut:");
                        if (ep->ed->cut_start > 0)
                        {
                            if (ep->ed->cut_combo)
                            {
                                snpedit("%ld", ep->ed->cut_start);
                            }
                            else
                            {
                                long t = ep->ed->cut_start / 1000;
                                snpedit("%ld:%02ld", t / 60, t % 60);
                            }
                        }

                        putsstr("~");

                        if (ep->ed->cut_end < LONG_MAX)
                        {
                            if (ep->ed->cut_combo)
                            {
                                snpedit("%ld", ep->ed->cut_end);
                            }
                            else
                            {
                                long t = ep->ed->cut_end / 1000;
                                snpedit("%ld:%02ld", t / 60, t % 60);
                            }
                        }
                    }
                }
                else if (_CMPSTR(cur, "@flip@"))
                {
                    switch (ep->ed->flip)
                    {
                    case xflip:
                        putsstr("X(invert)");
                        break;
                    case yflip:
                        putsstr("Y(invert)");
                        break;
                    case transpose:
                        putsstr("TRANSPOSE");
                        break;
                    case invert:
                        putsstr("INVERT");
                        break;
                    case fullrc:
                        putsstr("FULL RC");
                        break;
                    default:
                        break;
                    }
                }
                else if (_CMPSTR(cur, "@nosv@"))
                {
                    if (ep->ed->remove_sv)
                    {
                        putsstr("NO SV");
                    }
                    else if (ep->ed->nospinner)
                    {
                        putsstr("NO SPIN");
                    }
                }
                else
                {
                    if (*cur != ' ' || ep->editline[ecur - 1] != ' ')
                    {
                        char a[] = { *cur, '\0' };
                        putsstr(a);
                    }

                    i++;
                }
            }
#undef _CMPSTR
            if (ep->editline[ecur - 1] == ' ')
                ep->editline[--ecur] = '\0';

            putsstr("\r\n");
        }
        else if (CMPSTR(line, "Tags:"))
        {
            edited = true;
            remove_newline(line);
            snpedit("%s osutrainer\r\n", line);
        }
        else if (CMPSTR(line, "BeatmapID:"))
        {
            edited = true;
        }
    }

    if (prior_read)
        return ret;

    if (edited && ecur == 0) return 0;
    if (ret == -20)
    {
        ret = buffers_map_put(ep->bufs, ep->editline, ecur);
        if (ret == 0) return -20;
    }
    if (ret != 0) return ret;
    ret = buffers_map_put(ep->bufs, edited ? ep->editline : line, edited ? ecur : strlen(line));
    return ret;
}

#define HASH_INIT(x) unsigned int hash = 0x811c9dc5; unsigned char *byte = (unsigned char*)x;
#define HASH_FUNC(type, field) for (size_t e = 0; e < sizeof(((type*)0)->field); e++) \
    hash = (hash ^ *(byte + offsetof(type, field) + e)) * 0x01000193;

static unsigned int hash_editdata(const struct editdata *edit)
{
    HASH_INIT(edit);

    HASH_FUNC(struct editdata, hp);
    HASH_FUNC(struct editdata, cs);
    HASH_FUNC(struct editdata, ar);
    HASH_FUNC(struct editdata, od);
    HASH_FUNC(struct editdata, scale_ar);
    HASH_FUNC(struct editdata, scale_od);
    HASH_FUNC(struct editdata, cap_ar);
    HASH_FUNC(struct editdata, cap_od);
    HASH_FUNC(struct editdata, makezip);
    HASH_FUNC(struct editdata, cv_mapset);
    HASH_FUNC(struct editdata, speed);
    HASH_FUNC(struct editdata, base_bpm);
    HASH_FUNC(struct editdata, bpmmode);
    HASH_FUNC(struct editdata, pitch);
    HASH_FUNC(struct editdata, nospinner);
    HASH_FUNC(struct editdata, remove_sv);
    HASH_FUNC(struct editdata, flip);
    HASH_FUNC(struct editdata, cut_combo);
    HASH_FUNC(struct editdata, cut_start);
    HASH_FUNC(struct editdata, cut_end);

    return hash;
}

static unsigned int hash_audiocfg(const struct audiospeed_config *cfg)
{
    HASH_INIT(cfg);

    HASH_FUNC(struct audiospeed_config, speed);
    HASH_FUNC(struct audiospeed_config, pitch);
    HASH_FUNC(struct audiospeed_config, emuldt);

    return hash;
}

int edit_beatmap(struct editdata *edit)
{
    if (edit->speed <= 0)
    {
        printerr("Use proper speed value instead!");
        return 1;
    }

    int ret = 0;
    struct buffers bufs;
    struct editpass ep;
    ep.editsize = 1024;
    ep.editline = (char*) malloc(ep.editsize);
    ep.ed = edit;
    ep.bufs = &bufs;

    ep.hitobjects = NULL;
    ep.hitobjects_num = ep.hitobjects_size = ep.hitobjects_idx = 0;
    ep.timingpoints = NULL;
    ep.timingpoints_num = ep.timingpoints_size = ep.timingpoints_idx = 0;
    ep.done_saving = false;
    ep.max_combo = 0;
    ep.uninherited_tp_idx = 0;
    ep.first_tp_passed = false;

    if (ep.editline == NULL)
    {
        printerr("Failed allocating memory");
        return -99;
    }

    ep.arexists   = edit->mi->arexists;
    ep.tagexists  = edit->mi->tagexists;
    ep.diffexists = edit->mi->diffexists;

    buffers_init(&bufs);

    if (edit->cv_mapset & 1)
    {
        if (!(edit->cv_mapset & 1<<1))
            edit->hp = edit->mi->hp;

        if (!(edit->cv_mapset & 1<<2))
            edit->cs = edit->mi->cs;

        if (!(edit->cv_mapset & 1<<3))
            edit->ar = edit->mi->ar;

        if (!(edit->cv_mapset & 1<<4))
            edit->od = edit->mi->od;
    }

    if (edit->base_bpm <= 0)
        edit->base_bpm = edit->mi->maxbpm;

    if (edit->bpmmode == guess)
    {
        double refbpm = edit->bpmrefmode == max_bpm_mode ? edit->mi->maxbpm : edit->mi->mainbpm;
        double esti = edit->speed * refbpm;
        if (esti > 10000)
        {
            puts("Using input value as BPM");
            edit->bpmmode = bpm;
        }
        else
        {
            puts("Using input value as RATE");
            edit->bpmmode = rate;
        }
    }

    if (edit->bpmmode == bpm)
    {
        double refbpm = edit->bpmrefmode == max_bpm_mode ? edit->mi->maxbpm : edit->mi->mainbpm;
        edit->speed /= refbpm;

        if (fabs(edit->speed - 1.0) < 1e-3)
            edit->speed = 1.0;
    }

    if (edit->scale_ar)
    {
        double origar = edit->ar;
        double tempar = scale_ar(origar, edit->speed, edit->mi->mode);
        if (edit->cap_ar)
        {
            edit->ar = tempar > origar ? origar : tempar;
        }
        else
        {
            edit->ar = tempar;
        }
    }

    if (edit->scale_od)
    {
        double origod = edit->od;
        double tempod = scale_od(origod, edit->speed, edit->mi->mode);
        if (edit->cap_od)
        {
            edit->od = tempod > origod ? origod : tempod;
        }
        else
        {
            edit->od = tempod;
        }
    }

    if (edit->mi->mode == 2) edit->od = 0;

    if (edit->mi->mode == 1 || edit->mi->mode == 3) edit->ar = 0;

    ep.orig_ar = edit->ar;
    ep.orig_od = edit->od;

    if (edit->ar > 10 || edit->od > 10)
    {
        ep.emuldt = edit->speed;
        edit->speed /= 1.5;
        edit->ar = scale_ar(edit->ar, 1.0/1.5, edit->mi->mode);
        edit->od = scale_od(edit->od, 1.0/1.5, edit->mi->mode);
        printf("AR/OD is higher than 10. Emulating DT...\n");

        double cap_od = scale_od(10.0, 1.5, edit->mi->mode);
        CLAMP(ep.orig_ar, 0, 11);
        CLAMP(ep.orig_od, 0, cap_od);

        if (fabs(edit->speed - 1.0) < 1e-3)
            edit->speed = 1.0;
    }
    else if (edit->ar < 0 || edit->od < 0)
    {
        ep.emuldt = edit->speed;
        edit->speed /= 0.75;
        edit->ar = scale_ar(edit->ar, 1.0/0.75, edit->mi->mode);
        edit->od = scale_od(edit->od, 1.0/0.75, edit->mi->mode);
        printf("AR/OD is lower than 0. Emulating HT...\n");

        double cap_od = scale_od(0.0, 0.75, edit->mi->mode);
        CLAMP(ep.orig_ar, -5, 0);
        CLAMP(ep.orig_od, cap_od, 0);

        if (fabs(edit->speed - 1.0) < 1e-3)
            edit->speed = 1.0;
    }
    else
    {
        ep.emuldt = 0;
    }

    CLAMP(edit->od, 0.0, 10.0);

    CLAMP(edit->ar, 0.0, 10.0);

    char *fname = strrchr(edit->mi->fullpath, PATHSEP) + 1;
    char *aname = edit->mi->audioname != NULL ? strrchr(edit->mi->audioname, PATHSEP) + 1 : NULL;

    long folderlen = (long) (fname - edit->mi->fullpath);
    char *folderpath = (char*) malloc(folderlen);
    if (folderpath)
    {
        memcpy(folderpath, edit->mi->fullpath, folderlen - 1);
        *(folderpath + folderlen - 1) = '\0';
    }
    else
    {
        printerr("Failed allocating!");
        buffers_free(&bufs);
        free(ep.editline);
        return -99;
    }

    long mapnlen = strlen(edit->mi->fullpath) + 1 + 8 + 1;
    long audnlen = 0;
    char *fcmappath = (char*) malloc(mapnlen);
    char *fcaudpath = NULL;
    char *zipf = NULL;
    if (aname)
    {
        if (edit->speed != 1.0)
        {
            audnlen = folderlen + 7 + 1 + strlen(aname) + 1;
            fcaudpath = (char*) malloc(audnlen);
        }
    }
    else
    {
        printerr("Audio file name is not available!");
    }

    if (fcmappath == NULL || (aname && edit->speed != 1.0 && fcaudpath == NULL))
    {
        printerr("Failed allocating memory");
        ret = -99;
        goto tryfree;
    }

    if (edit->makezip)
    {
        long zipflen = folderlen + sizeof(".osz") - 1;
        zipf = (char*) malloc(zipflen);

        if (zipf == NULL)
        {
            printerr("Failed allocating memory");
            ret = -99;
            goto tryfree;
        }

        snprintf(zipf, zipflen, "%s.osz", folderpath);
    }

    struct audiospeed_config cfg = { edit->mi->audioname, edit->speed, edit->pitch, ep.emuldt };

    unsigned int hashed = hash_editdata(edit);
    snprintf(fcmappath, mapnlen, "%s" STR_PATHSEP "%08x_%s", folderpath, hashed, fname);

    if (fcaudpath)
    {
        unsigned int audhashed = hash_audiocfg(&cfg);
        snprintf(fcaudpath, audnlen, "%s" STR_PATHSEP "%08x_%s", folderpath, audhashed, aname);
    }

    bufs.mapname = (char*) malloc(strlen(fcmappath + folderlen) + 1);
    if (bufs.mapname == NULL)
    {
        printerr("Failed allocation!");
        ret = -99;
        goto tryfree;
    }
    strcpy(bufs.mapname, fcmappath + folderlen);

    if (fcaudpath)
    {
        bufs.audname = (char*) malloc(strlen(fcaudpath + folderlen) + 1);
        if (bufs.audname == NULL)
        {
            printerr("Failed allocation!");
            ret = -99;
            goto tryfree;
        }
        strcpy(bufs.audname, fcaudpath + folderlen);

        if (access(fcaudpath, F_OK) == 0 || (edit->makezip && check_zip_file(zipf, bufs.audname) > 0))
        {
            printerr("A compatible audio file already exists! skipping audio conversion");
            free(fcaudpath);
            fcaudpath = NULL;
        }
        else
        {
            ret = change_audio_speed(cfg, &bufs, edit->data, edit->progress_callback);
        if (ret != 0)
        {
            printerr("Failed converting audio!");
            ret = -80;
            goto tryfree;
            }
        }
    }

    bool needs_preread = false;
    ep.prior_read = 0;

    if (edit->flip == invert)
    {
        if (edit->mi->mode == 3)
        {
            needs_preread = true;
        }
        else
        {
            printerr("Invert is only available on osu!mania!");
            edit->flip = none;
        }
    }
    else if (edit->flip == fullrc && edit->mi->mode != 3)
    {
        printerr("Full RC is only available on osu!mania!");
        edit->flip = none;
    }

    if (edit->cut_combo)
    {
        needs_preread = true;
    }

    if (needs_preread)
    {
        // fill timingpoints
        ep.prior_read = 1;
        ret = loop_map(edit->mi->fullpath, &convert_map, &ep);
        if (ret != 0)
        {
            printerr("Failed reading hitobjects!");
            ret = -70;
            goto tryfree;
        }

        if (ep.hitobjects_num <= 0)
        {
            // fill hitobjects in case hitobjects were written before timingpoints
            ep.prior_read = 2;
            ret = loop_map(edit->mi->fullpath, &convert_map, &ep);
            if (ret != 0)
            {
                printerr("Failed reading hitobjects!");
                ret = -70;
                goto tryfree;
            }
        }

        ep.done_saving = true;

        if (ep.hitobjects_num == 0 || ep.timingpoints_num == 0)
        {
            printerr("Map contains no objects to convert!");
            ret = -80;
            goto tryfree;
        }
    }

    ret = loop_map(edit->mi->fullpath, &convert_map, &ep);
    if (ret != 0)
    {
        printerr("Failed converting map!");
        ret = -70;
        goto tryfree;
    }

    if (edit->makezip)
    {
        ret = create_actual_zip(zipf, &bufs);

        if (ret == 0 && !(edit->cv_mapset & 1))
            ret = execute_file(zipf);
    }
    else
    {
        FILE *mapfd = fopen(fcmappath, "wb");
        if (mapfd == NULL || fwrite(bufs.mapbuf, 1, bufs.maplast, mapfd) < bufs.maplast)
        {
            printerr("Error writing a map");
            ret = -71;
        }
        else
        {
            if (fcaudpath && bufs.audlast > 0)
            {
                FILE *audfd = fopen(fcaudpath, "wb");
                if (audfd == NULL || fwrite(bufs.audbuf, 1, bufs.audlast, audfd) < bufs.audlast)
                {
                    printerr("Error writing an audio file");
                    ret = -71;
                }
                fclose(audfd);
            }
        }
        fclose(mapfd);
    }

tryfree:
    free(zipf);
    free(folderpath);
    free(fcmappath);
    free(fcaudpath);
    free(ep.editline);
    free(ep.hitobjects);
    free(ep.timingpoints);
    buffers_free(&bufs);
    return ret;
}
