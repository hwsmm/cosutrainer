#include "mapeditor.h"
#include "tools.h"
#include "cosuplatform.h"
#include "audiospeed.h"
#include "actualzip.h"
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <inttypes.h>

#define tkn(x) strtok(x, ",")
#define nexttkn() strtok(NULL, ",")

#define fail_nulltkn(x) \
if ((x = nexttkn()) == NULL) \
{ \
   printerr("Failed parsing!"); \
   return 1; \
}

#define allocput(dest, src) \
do { \
   if (dest != NULL) free(dest); \
   int namelen = strlen(src); \
   dest = (char*) malloc(namelen + 1); \
   if (!dest) return -99; \
   strcpy(dest, src); \
} while (0);

#define resize(x) \
if (x > ep->editsize) \
{ \
   char *resi = (char*) realloc(ep->editline, x); \
   if (resi == NULL) \
   { \
      printerr("Failed reallocating!"); \
      return -1; \
   } \
   ep->editline = resi; \
   ep->editsize = x; \
}

#define putstr(x, len) \
do \
{ \
   if (ep->editsize - ecur - 1 < len) \
   { \
      resize(ep->editsize + len + 1024); \
   } \
   strcpy(ep->editline + ecur, x); \
   ecur += len; \
} \
while (0);

#define putsstr(x) putstr(x, sizeof x - 1);
#define putdstr(x) putstr(x, strlen(x));
#define snpedit(...) \
do \
{ \
   unsigned int written = 0; \
   size_t writable = ep->editsize - ecur - 1; \
   while ((written = snprintf(ep->editline + ecur, writable, ##__VA_ARGS__)) >= writable) \
   { \
      resize(ep->editsize + 1024); \
      writable = ep->editsize - ecur - 1; \
   } \
   ecur += written; \
} \
while (0);

static char *find_null(char *str)
{
    while (*(str++)) {}
    return str;
}

static double scale_ar(double ar, double speed, int mode)
{
    if (mode == 1 || mode == 3) return ar;

    double ar_ms = ar <= 5 ? 1200 + 600 * (5 - ar) / 5 : 1200 - 750 * (ar - 5) / 5;
    ar_ms /= speed;
    return ar_ms >= 1200 ? 15 - ar_ms / 120 : (1200 / 150) - (ar_ms / 150) + 5;
}

// fix mania od
static double scale_od(double od, double speed, int mode)
{
    switch (mode)
    {
    case 0:
        return (80 - (80 - 6 * od) / speed) / 6;
    case 1:
        return (50 - (50 - 3 * od) / speed) / 3;
    case 3:
        return (64 - (64 - 3 * od) / speed) / 3;
    default:
        return od;
    }
}

static int loop_map(char *mapfile, int (*func)(char*, void*, enum SECTION), void *pass)
{
    FILE *source = fopen(mapfile, "r");
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

        if (sect != unknown)
        {
            ret = (*func)(line, pass, sect);
            if (ret == -20)
            {
                // loop again
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
    if (sect == timingpoints)
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
    else if (sect == general)
    {
        if (CMPSTR(line, "AudioFilename: "))
        {
            char *filename = CUTFIRST(line, "AudioFilename: ");
            remove_newline(filename);
            allocput(info->audioname, filename);
        }
        else if (CMPSTR(line, "Mode: "))
        {
            info->mode = atoi(CUTFIRST(line, "Mode: "));
        }
    }
    else if (sect == difficulty)
    {
        if (CMPSTR(line, "HPDrainRate:"))            info->hp = atof(CUTFIRST(line, "HPDrainRate:"));
        else if (CMPSTR(line, "CircleSize:"))        info->cs = atof(CUTFIRST(line, "CircleSize:"));
        else if (CMPSTR(line, "OverallDifficulty:")) info->od = atof(CUTFIRST(line, "OverallDifficulty:"));
        else if (CMPSTR(line, "ApproachRate:"))      info->arexists = true, info->ar = atof(CUTFIRST(line, "ApproachRate:"));
    }
    else if (sect == metadata)
    {
        if (CMPSTR(line, "Version:"))
        {
            info->diffexists = true;
            char *diffname = CUTFIRST(line, "Version:");
            remove_newline(diffname);
            allocput(info->diffname, diffname);
        }
        else if (CMPSTR(line, "Title:"))
        {
            char *songname = CUTFIRST(line, "Title:");
            remove_newline(songname);
            allocput(info->songname, songname);
        }
        else if (CMPSTR(line, "Tags:"))
        {
            info->tagexists = true;
        }
    }
    else if (sect == events)
    {
        if (CMPSTR(line, "0,0,"))
        {
            char *bgname = CUTFIRST(line, "0,0,");
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

struct mapinfo *read_beatmap(char *mapfile)
{
    struct mapinfo *info = (struct mapinfo*) calloc(sizeof(struct mapinfo), 1);
    int ret = loop_map(mapfile, &write_mapinfo, info);
    if (ret == 0)
    {
        info->fullpath = get_realpath(mapfile);
        if (info->fullpath == NULL)
        {
            perror(mapfile);
            free_mapinfo(info);
            return NULL;
        }

        info->maxbpm = 1 / info->maxbpm * 1000 * 60;
        info->minbpm = 1 / info->minbpm * 1000 * 60;

        if (!info->arexists) info->ar = info->od;
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

    if (sect == root)
    {
        if (CMPSTR(line, "osu file format"))
        {
            edited = true;
            putsstr("osu file format v14\r\n");
        }
    }
    else if (sect == events)
    {
        if (*line == '2' || CMPSTR(line, "Break"))
        {
            edited = true;
            tkn(line);
            char *start;
            fail_nulltkn(start);
            char *end;
            fail_nulltkn(end);
            snpedit("2,%ld,%ld\r\n", (long) (atol(start) / speed), (long) (atol(end) / speed));
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
        if (*btlenstr != '-')
        {
            snpedit("%ld,%.12lf,", time, atof(btlenstr) / speed);
        }
        else
        {
            snpedit("%ld,%s,", time, btlenstr);
        }
        putdstr(find_null(btlenstr));
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
        long time = atol(timestr) / speed;

        char *typestr;
        fail_nulltkn(typestr);
        int type = atoi(typestr);

        if (ep->ed->flip == xflip || ep->ed->flip == transpose) x = 512 - x;
        if (ep->ed->flip == yflip || ep->ed->flip == transpose) y = 384 - y;

        if (type & (1<<3) && ep->ed->nospinner)
        {
            // do nothing to remove spinner lines
        }
        else if (type & (1<<3) || type & (1<<7))
        {
            const char spinnertoken[] = { (type & (1<<7)) ? ':' : ',', '\0' };
            char *hitsoundstr;
            fail_nulltkn(hitsoundstr);
            char *spinnerstr = strtok(NULL, spinnertoken);
            if (spinnerstr == NULL)
            {
                printerr("Failed parsing spinner!");
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
    else if (sect == editor)
    {
        if (CMPSTR(line, "Bookmarks: "))
        {
            edited = true;
            char *p = CUTFIRST(line, "Bookmarks: ");
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
    else if (sect == general)
    {
        if (CMPSTR(line, "AudioFilename: ") && speed != 1)
        {
            edited = true;
            snpedit("AudioFilename: %s\r\n", ep->bufs->audname);
        }
        else if (CMPSTR(line, "AudioLeadIn: "))
        {
            edited = true;
            char *p = CUTFIRST(line, "AudioLeadIn: ");
            long time = atol(p) / speed;
            snpedit("AudioLeadIn: %ld\r\n", time);
        }
        else if (CMPSTR(line, "PreviewTime: "))
        {
            edited = true;
            char *p = CUTFIRST(line, "PreviewTime: ");
            long time = atol(p) / speed;
            snpedit("PreviewTime: %ld\r\n", time);
        }
    }
    else if (sect == difficulty)
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
    else if (sect == metadata)
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
                snpedit("%.2fx", speed);
            }
            else
            {
                snpedit("%.2fx(DT)", speed * 1.5);
            }

            if (ep->ed->mi->hp != ep->ed->hp) snpedit(" HP%.1f", ep->ed->hp);
            if ((ep->ed->mi->mode != 1 && ep->ed->mi->mode != 3) && ep->ed->mi->cs != ep->ed->cs) snpedit(" CS%.1f", ep->ed->cs);

            if (ep->emuldt)
            {
                if (ep->ed->mi->mode != 2) snpedit(" OD%.1f", scale_od(ep->ed->od, 1.5, ep->ed->mi->mode));
                if (ep->ed->mi->mode != 1 && ep->ed->mi->mode != 3) snpedit(" AR%.1f", scale_ar(ep->ed->ar, 1.5, ep->ed->mi->mode));
            }
            else
            {
                if (ep->ed->mi->mode != 2 && ep->ed->mi->od != ep->ed->od) snpedit(" OD%.1f", ep->ed->od);
                if ((ep->ed->mi->mode != 1 && ep->ed->mi->mode != 3) && ep->ed->mi->ar != ep->ed->ar) snpedit(" AR%.1f", ep->ed->ar);
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
            default:
                break;
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

int edit_beatmap(struct editdata *edit, float *progress)
{
    int ret = 0;
    struct buffers bufs;
    struct editpass ep;
    ep.editsize = 1024;
    ep.editline = (char*) malloc(ep.editsize);
    ep.ed = edit;
    ep.bufs = &bufs;

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
        float origar = fabs(edit->ar);
        float tempar = scale_ar(origar, edit->speed, edit->mi->mode);
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
        float origod = fabs(edit->od);
        float tempod = scale_ar(origod, edit->speed, edit->mi->mode);
        if (edit->od < 0) // capping
        {
            edit->od = tempod > origod ? origod : tempod;
        }
        else
        {
            edit->od = tempod;
        }
    }

    if (edit->ar > 10 || edit->od > 10)
    {
        puts("AR/OD is higher than 10. Emulating DT...");
        ep.emuldt = true;
        edit->speed /= 1.5;
        edit->ar = scale_ar(edit->ar, 1/1.5, edit->mi->mode);
        edit->od = scale_od(edit->od, 1/1.5, edit->mi->mode);
    }
    else
    {
        ep.emuldt = false;
    }

    if (edit->mi->mode == 2) edit->od = 0;
    else CLAMP(edit->od, 0, 10);

    if (edit->mi->mode == 1 || edit->mi->mode == 3) edit->ar = 0;
    else CLAMP(edit->ar, 0, 10);

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
    if (edit->mi->audioname && edit->speed != 1)
    {
        audnlen = folderlen + 7 + 1 + strlen(edit->mi->audioname) + 1;
        fcaudpath = (char*) malloc(audnlen);
    }

    if (fcmappath == NULL || (edit->mi->audioname && edit->speed != 1 && fcaudpath == NULL))
    {
        printerr("Failed allocating memory");
        buffers_free(&bufs);
        return -99;
    }

    snprintf(fcmappath, mapnlen, "%s" STR_PATHSEP "xxxxxxx_%s", folderpath, fname);
    if (fcaudpath) snprintf(fcaudpath, audnlen, "%s" STR_PATHSEP "xxxxxxx_%s", folderpath, edit->mi->audioname);

    randominit();

    while (name_conflict == true)
    {
        randomstr(prefix, 7);
        memcpy(fcmappath + folderlen, prefix, 7);
        if (fcaudpath) memcpy(fcaudpath + folderlen, prefix, 7);
        if (access(fcmappath, F_OK) != 0 && (fcaudpath == NULL || access(fcaudpath, F_OK) != 0))
            name_conflict = false;
    }

    bufs.mapname = (char*) malloc(strlen(fcmappath + folderlen) + 1);
    if (bufs.mapname == NULL)
    {
        printerr("Failed allocation!");
        free(folderpath);
        free(fcmappath);
        if (fcaudpath) free(fcaudpath);
        buffers_free(&bufs);
        return -99;
    }
    strcpy(bufs.mapname, fcmappath + folderlen);

    if (fcaudpath)
    {
        bufs.audname = (char*) malloc(strlen(fcaudpath + folderlen) + 1);
        if (bufs.audname == NULL)
        {
            printerr("Failed allocation!");
            free(folderpath);
            free(fcmappath);
            if (fcaudpath) free(fcaudpath);
            buffers_free(&bufs);
            return -99;
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
            free(folderpath);
            free(fcmappath);
            if (fcaudpath) free(fcaudpath);
            buffers_free(&bufs);
            return -99;
        }
        ret = change_audio_speed(audp, &bufs, edit->speed, edit->pitch, progress);
        if (ret != 0)
        {
            printerr("Failed converting audio!");
            free(folderpath);
            free(audp);
            buffers_free(&bufs);
            return -80;
        }
        free(audp);
    }

    ret = loop_map(edit->mi->fullpath, &convert_map, &ep);
    free(ep.editline);
    if (ret != 0)
    {
        printerr("Failed converting map!");
        free(folderpath);
        free(fcmappath);
        if (fcaudpath) free(fcaudpath);
        buffers_free(&bufs);
        return -70;
    }

    if (edit->makezip)
    {
        long zipflen = folderlen + sizeof(".osz") - 1;
        char *zipf = (char*) malloc(zipflen);
        snprintf(zipf, zipflen, "%s.osz", folderpath);
        ret = create_actual_zip(zipf, &bufs);

        char *open_cmd = getenv("OSZ_HANDLER");
        if (ret == 0 && open_cmd)
        {
            char *real_cmd = replace_string(open_cmd, "{osz}", zipf);
            ret = fork_launch(real_cmd);
            free(real_cmd);
        }
        free(zipf);
    }
    else
    {
        FILE *mapfd = fopen(fcmappath, "w");
        if (mapfd == NULL || fwrite(bufs.mapbuf, 1, bufs.maplast, mapfd) < bufs.maplast)
        {
            printerr("Error writing a map");
        }
        else
        {
            if (fcaudpath)
            {
                FILE *audfd = fopen(fcaudpath, "w");
                if (audfd == NULL || fwrite(bufs.audbuf, 1, bufs.audlast, audfd) < bufs.audlast)
                {
                    printerr("Error writing an audio file");
                }
                fclose(audfd);
            }
        }
        fclose(mapfd);
    }
    free(folderpath);
    free(fcmappath);
    if (fcaudpath) free(fcaudpath);
    buffers_free(&bufs);
    return ret;
}
