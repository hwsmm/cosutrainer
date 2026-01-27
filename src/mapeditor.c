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

#define snpedit(fmt, ...) \
if (_snpedit(ep, &ecur, fmt, ##__VA_ARGS__) != 0) return -1;

static char *find_null(char *str)
{
    while (*(str++)) {}
    return str;
}

double scale_ar(double ar, double speed, int mode)
{
    if (mode == 1 || mode == 3) return ar;

    double ms = ar <= 5.0 ? 1800.0 - 120.0 * ar : 1950.0 - 150.0 * ar;
    ms /= speed;
    return ms >= 1200.0 ? (1800.0 - ms) / 120.0 : (1950.0 - ms) / 150.0;
}

// fix mania od
double scale_od(double od, double speed, int mode)
{
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
    return (base - ms) / multiplier;
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

static int write_mapinfo(char *line, void *vinfo, enum SECTION sect)
{
    struct mapinfo *info = (struct mapinfo*) vinfo;
    if (sect != empty && (info->read_sections & (1 << sect)) == 0)
    {
        if ((info->read_sections & 0b111101000) == 0b111101000)
        {
            return -12345;
        }
        info->read_sections |= (1 << sect);
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
    else if (sect == timingpoints) // 9
    {
        // char *timestr =
        tkn(line);
        char *beatlengthstr;
        fail_nulltkn(beatlengthstr);
        if (*beatlengthstr != '-')
        {
            double bpm = atof(beatlengthstr);
            if (info->maxbpm == 0 || bpm <= info->maxbpm) info->maxbpm = bpm;
            if (info->minbpm == 0 || bpm > info->minbpm) info->minbpm = bpm;
        }
    }
    else if (sect == general) // 4
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
    else if (sect == difficulty) // 7
    {
        if (CMPSTR(line, "HPDrainRate:"))            info->hp = atof(CUTFIRST(line, "HPDrainRate:"));
        else if (CMPSTR(line, "CircleSize:"))        info->cs = atof(CUTFIRST(line, "CircleSize:"));
        else if (CMPSTR(line, "OverallDifficulty:")) info->od = atof(CUTFIRST(line, "OverallDifficulty:"));
        else if (CMPSTR(line, "ApproachRate:"))      info->arexists = true, info->ar = atof(CUTFIRST(line, "ApproachRate:"));
        else if (CMPSTR(line, "SliderMultiplier:"))  info->slider_multiplier = atof(CUTFIRST(line, "SliderMultiplier:"));
        else if (CMPSTR(line, "SliderTickRate:"))    info->slider_tick_rate = atof(CUTFIRST(line, "SliderTickRate:"));
    }
    else if (sect == metadata) // 6
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
    else if (sect == events) // 8
    {
        if (CMPSTR(line, "0,0,"))
        {
            char *bgname = CUTFIRST_NOTRIM(line, "0,0,");
            char *endhere = NULL;
            if (*bgname == '\"')
            {
                bgname++;
                endhere = strchr(bgname, '\"'); // using " in filename is illegal in NTFS so
            }
            else
            {
                endhere = strchr(bgname, ','); // what if , is escaped? how does osu handle it?
            }
            if (endhere != NULL)
            {
                *endhere = '\0';
            }
            else
            {
                printerr("Failed parsing background line!");
                return 1;
            }
            allocput(info->bgname, bgname);
        }
    }
    return 0;
}

#ifndef WIN32
static int convert_vaildpath(struct mapinfo *mi)
{
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
        return -2;
    }

    int solen = mi->audioname != NULL ? strlen(mi->audioname) : 0;
    int bglen = mi->bgname != NULL ? strlen(mi->bgname) : 0;
    int res = 0;
    char *temp = (char*) malloc(fdlen + 1 + (solen > bglen ? solen : bglen) + 1);
    if (temp == NULL)
    {
        printerr("Failed allocation!");
        return -1;
    }
    strcpy(temp, fullpath);
    *sep = PATHSEP;
    *(temp + fdlen) = PATHSEP;

    if (mi->audioname != NULL)
    {
        strcpy(temp + fdlen + 1, mi->audioname);
        int ares = try_convertwinpath(temp, fdlen + 1);
        if (ares == 0)
        {
            strcpy(mi->audioname, temp + fdlen + 1);
        }
        res = ares;
    }

    if (mi->bgname != NULL)
    {
        strcpy(temp + fdlen + 1, mi->bgname);
        int bres = try_convertwinpath(temp, fdlen + 1);
        if (bres == 0)
        {
            strcpy(mi->bgname, temp + fdlen + 1);
        }
        if (res >= 0) res = bres;
    }

    free(temp);
    return res;
}
#endif

struct mapinfo *read_beatmap(char *mapfile)
{
    struct mapinfo *info = (struct mapinfo*) calloc(1, sizeof(struct mapinfo));
    if (info == NULL)
    {
        printerr("Failed allocation");
        return NULL;
    }

    int ret = loop_map(mapfile, &write_mapinfo, info);
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
            return NULL;
        }

        info->maxbpm = 1 / info->maxbpm * 1000 * 60;
        info->minbpm = 1 / info->minbpm * 1000 * 60;

        if (!info->arexists) info->ar = info->od;

        if (info->version <= 0)
            info->version = 14;

#ifndef WIN32
        if (convert_vaildpath(info) < 0)
        {
            printerr("Failed converting path of audio and bg");
            free_mapinfo(info);
            return NULL;
        }
#endif
    }
    else
    {
        free_mapinfo(info);
        return NULL;
    }
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
                if (type & (1<<3 | 1<<7))
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
            snpedit("ApproachRate:%.1lf\r\n", ep->ed->ar);
        }
        else if (CMPSTR(line, "HPDrainRate:"))
        {
            edited = true;
            snpedit("HPDrainRate:%.1lf\r\n", ep->ed->hp);
        }
        else if (CMPSTR(line, "CircleSize:"))
        {
            edited = true;
            snpedit("CircleSize:%.1lf\r\n", ep->ed->cs);
        }
        else if (CMPSTR(line, "OverallDifficulty:"))
        {
            edited = true;
            snpedit("OverallDifficulty:%.1lf\r\n", ep->ed->od);
        }
        else if (CMPSTR(line, "ApproachRate:"))
        {
            edited = true;
            snpedit("ApproachRate:%.1lf\r\n", ep->ed->ar);
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
            if (ep->diffexists)
            {
                remove_newline(line);
                putdstr(line);
                putsstr(" ");
            }
            else
            {
                ep->diffexists = true;
                ret = -20;
                putsstr("Version:");
            }

            if (!ep->emuldt)
            {
                snpedit("%.2lfx %.0lfbpm", speed, ep->ed->mi->maxbpm * speed);
            }
            else
            {
                double dtspeed = speed * 1.5;
                snpedit("%.2lfx %.0lfbpm (DT)", dtspeed, ep->ed->mi->maxbpm * dtspeed);
            }

            if ((ep->ed->mi->mode != 1 && ep->ed->mi->mode != 3) && ep->ed->mi->cs != ep->ed->cs) snpedit(" CS%.1lf", ep->ed->cs);

            if (ep->emuldt)
            {
                if (ep->ed->mi->mode != 1 && ep->ed->mi->mode != 3) snpedit(" AR%.1lf", scale_ar(ep->ed->ar, 1.5, ep->ed->mi->mode));
                if (ep->ed->mi->mode != 2) snpedit(" OD%.1lf", scale_od(ep->ed->od, 1.5, ep->ed->mi->mode));
            }
            else
            {
                if ((ep->ed->mi->mode != 1 && ep->ed->mi->mode != 3) && ep->ed->mi->ar != ep->ed->ar) snpedit(" AR%.1lf", ep->ed->ar);
                if (ep->ed->mi->mode != 2 && ep->ed->mi->od != ep->ed->od) snpedit(" OD%.1lf", ep->ed->od);
            }

            if (ep->ed->mi->hp != ep->ed->hp) snpedit(" HP%.1lf", ep->ed->hp);

            if (ep->ed->cut_start > 0 || ep->ed->cut_end < LONG_MAX)
            {
                putsstr(" Cut:");
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

            switch (ep->ed->flip)
            {
            case xflip:
                putsstr(" X(invert)");
                break;
            case yflip:
                putsstr(" Y(invert)");
                break;
            case transpose:
                putsstr(" TRANSPOSE");
                break;
            case invert:
                putsstr(" INVERT");
                break;
            default:
                break;
            }

            if (ep->ed->remove_sv)
            {
                putsstr(" NO SV");
            }

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

int edit_beatmap(struct editdata *edit)
{
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

    if (edit->bpmmode == guess)
    {
        double esti = edit->speed * edit->mi->maxbpm;
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
        edit->speed /= edit->mi->maxbpm;
    }

    if (edit->scale_ar)
    {
        double origar = fabs(edit->ar);
        double tempar = scale_ar(origar, edit->speed, edit->mi->mode);
        if (edit->ar < 0) // capping
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
        double origod = fabs(edit->od);
        double tempod = scale_od(origod, edit->speed, edit->mi->mode);
        if (edit->od < 0) // capping
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

    if (edit->ar > 10 || edit->od > 10)
    {
        puts("AR/OD is higher than 10. Emulating DT...");
        ep.emuldt = true;
        edit->speed /= 1.5;
        edit->ar = scale_ar(edit->ar, 1.0/1.5, edit->mi->mode);
        edit->od = scale_od(edit->od, 1.0/1.5, edit->mi->mode);
    }
    else
    {
        ep.emuldt = false;
    }

    CLAMP(edit->od, 0.0, 10.0);

    CLAMP(edit->ar, 0.0, 10.0);

    char *fname = strrchr(edit->mi->fullpath, PATHSEP) + 1;

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
        return -99;
    }

    bool name_conflict = true;
    char prefix[8] = {0};

    long mapnlen = strlen(edit->mi->fullpath) + 1 + 7 + 1;
    long audnlen = 0;
    char *fcmappath = (char*) malloc(mapnlen);
    char *fcaudpath = NULL;
    if (edit->mi->audioname)
    {
        if (edit->speed != 1.0)
        {
            audnlen = folderlen + 7 + 1 + strlen(edit->mi->audioname) + 1;
            fcaudpath = (char*) malloc(audnlen);
        }
    }
    else
    {
        printerr("Audio file name is not available!");
    }

    if (fcmappath == NULL || (edit->mi->audioname && edit->speed != 1.0 && fcaudpath == NULL))
    {
        printerr("Failed allocating memory");
        ret = -99;
        goto tryfree;
    }

    snprintf(fcmappath, mapnlen, "%s" STR_PATHSEP "xxxxxxx_%s", folderpath, fname);
    if (fcaudpath) snprintf(fcaudpath, audnlen, "%s" STR_PATHSEP "xxxxxxx_%s", folderpath, edit->mi->audioname);

    randominit();

    while (name_conflict == true)
    {
        randomstr(prefix, 7);
        memcpy(fcmappath + folderlen, prefix, 7);
        if (fcaudpath) memcpy(fcaudpath + folderlen, prefix, 7);
        if (_access(fcmappath, F_OK) != 0 && (fcaudpath == NULL || _access(fcaudpath, F_OK) != 0))
            name_conflict = false;
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

        char *audp = NULL;
        long audplen = folderlen - 1 + 1 + strlen(edit->mi->audioname) + 1;
        audp = (char*) malloc(audplen);
        if (audp)
        {
            snprintf(audp, audplen, "%s" STR_PATHSEP "%s", folderpath, edit->mi->audioname);
        }
        else
        {
            printerr("Failed allocating!");
            ret = -99;
            goto tryfree;
        }
        ret = change_audio_speed(audp, &bufs, edit->speed, edit->pitch, edit->data, edit->progress_callback);
        if (ret != 0)
        {
            printerr("Failed converting audio!");
            free(audp);
            ret = -80;
            goto tryfree;
        }
        free(audp);
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
    else if (edit->cut_combo)
    {
        // may add proper mania support in future
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
        long zipflen = folderlen + sizeof(".osz") - 1;
        char *zipf = (char*) malloc(zipflen);
        snprintf(zipf, zipflen, "%s.osz", folderpath);
        ret = create_actual_zip(zipf, &bufs);

        if (ret == 0)
            ret = execute_file(zipf);

        free(zipf);
    }
    else
    {
        FILE *mapfd = fopen(fcmappath, "w");
        if (mapfd == NULL || fwrite(bufs.mapbuf, 1, bufs.maplast, mapfd) < bufs.maplast)
        {
            printerr("Error writing a map");
            ret = -71;
        }
        else
        {
            if (fcaudpath)
            {
                FILE *audfd = fopen(fcaudpath, "w");
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
    free(folderpath);
    free(fcmappath);
    if (fcaudpath) free(fcaudpath);
    free(ep.editline);
    free(ep.hitobjects);
    free(ep.timingpoints);
    buffers_free(&bufs);
    return ret;
}
