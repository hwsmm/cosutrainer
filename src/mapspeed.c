#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "mapspeed.h"
#include "audiospeed.h"
#include "tools.h"
#include "buffers.h"

#define tkn(x) strtok(x, ",")
#define nexttkn() strtok(NULL, ",")

// #define DEBUG

double scale_ar(double ar, double speed, int mode)
{
   if (mode == 1 || mode == 3) return ar;

   double ar_ms = ar <= 5 ? 1200 + 600 * (5 - ar) / 5 : 1200 - 750 * (ar - 5) / 5;
   ar_ms /= speed;
   return ar_ms >= 1200 ? 15 - ar_ms / 120 : (1200 / 150) - (ar_ms / 150) + 5;
}

double scale_od(double od, double speed, int mode)
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

static int countchunks(char *line, char delim)
{
   int count = 0;
   char *p;
   for (p = line; *p != '\0'; p++)
   {
      if (*p == delim) count++;
   }
   return count + 1;
}

#define chktkn(l, num) \
if (countchunks(l, ',') < num) \
{ \
   goto parsefail; \
}

#define resize(x) \
if (x > editsize) \
{ \
   char *resi = (char*) realloc(editline, x); \
   if (resi == NULL) \
   { \
      printerr("Failed reallocating"); \
      failure = true; \
      goto cleanup; \
   } \
   editline = resi; \
   editsize = x; \
}

#define putstr(x, len) \
do \
{ \
   if (editsize - ecur - 1 < len) \
   { \
      resize(editsize + len + 1024); \
   } \
   strcpy(editline + ecur, x); \
   ecur += len; \
} \
while (0);

#define putsstr(x) putstr(x, sizeof x - 1);
#define putdstr(x) putstr(x, strlen(x));
#define snpedit(...) \
do \
{ \
   unsigned int written = 0; \
   size_t writable = editsize - ecur - 1; \
   while ((written = snprintf(editline + ecur, writable, ##__VA_ARGS__)) >= writable) \
   { \
      resize(editsize + 1024); \
      writable = editsize - ecur - 1; \
   } \
   ecur += written; \
} \
while (0);

#define APPLYDIFF(x, y) \
if (read_mode) \
{ \
   x.val = atof(CUTFIRST(line, y ":")); \
   x.orig_value = x.val; \
} \
else \
{ \
   if (x.mode != fix) \
   { \
      edited = true; \
      snpedit(y ":%.1f\r\n", x.val); \
   } \
}

int edit_beatmap(const char* beatmap, double speed, enum SPEED_MODE rate_mode, struct difficulty diff, const bool pitch, enum FLIP flip, struct buffers *bufs)
{
   FILE *source = fopen(beatmap, "r");
   if (!source)
   {
      perror(beatmap);
      return 1;
   }

   char *audiofile = NULL;

   unsigned int current_size = 1024; // it's initial size of line for now, may increase as we edit map
   unsigned int realloc_step = 1024; // program will increase size of line once it meets very long line
   char *line = (char*) malloc(current_size);
   char *tmpline = NULL;

   unsigned int editsize = current_size;
   char *editline = (char*) malloc(editsize);
   unsigned int ecur = 0;

   bool read_mode = true; // program will read the entire file first (read_mode==true), and read again to write the new map file (read_mode==false)
   // it is used to read AR/OD and etc and preprocess before writing the actual result map file
   bool edited = false; // if it's set as true in write mode, program will not attempt to write the original line since an edited line is already written to a file
   // or you can just set it as true and do nothing, which will just remove the original line
   bool loop_again = false; // if it's set as true, the loop won't read a new line but just loop again, used when the program wrote an entirely new line, or line is realloc-ed
   bool failure = false; // set this to true and use 'goto cleanup' to clean memory properly

   bool arexists = false; // will be set as true once loop meets ApproachRate, some old maps don't have AR in the map (the client uses od as ar instead)
   bool tagexists = false; // will be set as true once loop meets Tag
   bool versionexists = false;

   bool emulate_dt = false; // when (scaled) od or ar is higher than 10. this variable is to indicate the program that it is emulating dt, since all the values are already adjusted
   double max_bpm = 0; // max bpm read in the map
   int mode = 0; // map mode
   int linenum = 0;

   enum SECTION sect = root;

   if (line == NULL)
   {
      printerr("Failed allocating memory");
      failure = true;
      goto cleanup;
   }

   do
   {
      while (loop_again || fgets(line, current_size, source))
      {
         edited = false;
         ecur = 0;
         loop_again = false;

         // dealing with very long lines, most of time, we don't need to edit rest of such lines, but we need it for inverting slider points
         // so dynamically allocate memory if the line doesn't have \n
         // we don't decrease buffer size after making it bigger, so there's no point in doing this in write mode since buffer size is already big enough
         if (read_mode && strchr(line, '\n') == NULL)
         {
            loop_again = true;
            tmpline = (char*) realloc(line, current_size + realloc_step);
            if (!tmpline)
            {
               failure = true;
               printerr("Failed reallocating buffer. Conversion failed");
               goto cleanup;
            }
            line = tmpline;
            if (fgets(strchr(line, '\0'), realloc_step, source) == NULL)
            {
               // just placeholder
            }
            current_size += realloc_step;
            continue;
         }

         linenum++;
#ifdef DEBUG
         printf("(%ld) %s", strlen(line), line);
#endif
         enum SECTION oldsect = sect;
         if (*line == '[')
         {
            if (CMPSTR(line, "[General]")) sect = general;
            else if (CMPSTR(line, "[Editor]")) sect = editor;
            else if (CMPSTR(line, "[Metadata]")) sect = metadata;
            else if (CMPSTR(line, "[Difficulty]")) sect = difficulty;
            else if (CMPSTR(line, "[Events]")) sect = events;
            else if (CMPSTR(line, "[TimingPoints]")) sect = timingpoints;
            else if (CMPSTR(line, "[Colours]")) sect = colours;
            else if (CMPSTR(line, "[HitObjects]")) sect = hitobjects;
         }

         // oldsect being different to sect means the section has just changed. which means line is [section] right now.
         // but we need to write [section] to modified map, so we don't use continue here.
         if (oldsect == sect && !(*line == '\r' || *line == '\n' || *line == '\0' || *line == '/'))
         {
            if (sect == root && !read_mode)
            {
               if (CMPSTR(line, "osu file format"))
               {
                  edited = true;
                  putsstr("osu file format v14\r\n");
                  // old maps don't allow decimal ar/od, and v14 is just more compatible with this program
               }
            }
            else if (sect == events && !read_mode)
            {
               if (*line == '2' || CMPSTR(line, "Break"))
               {
                  chktkn(line, 3);

                  edited = true;
                  tkn(line);
                  char *startstr = nexttkn();
                  char *endstr = nexttkn();

                  long start = atol(startstr) / speed;
                  long end = atol(endstr) / speed;
                  snpedit("2,%ld,%ld\r\n", start, end);
               }
               else if (*line == '0')
               {
                  // do nothing, it's background
               }
               else // remove everything related to video and storyboard
               {
                  edited = true;
               }
            }
            else if (sect == timingpoints)
            {
               if (read_mode) chktkn(line, 8);

               char *timestr = tkn(line);
               char *beatlengthstr = nexttkn();

               if (read_mode && (rate_mode == bpm || rate_mode == guess))
               {
                  double curbpm = atof(beatlengthstr);
                  if (*beatlengthstr != '-')
                  {
                     if (max_bpm == 0) max_bpm = curbpm;
                     else if (curbpm < max_bpm) max_bpm = curbpm;
                  }
               }
               else if (!read_mode)
               {
                  edited = true;
                  long time = atol(timestr) / speed;
                  if (*beatlengthstr != '-') // bpm
                  {
                     snpedit("%ld,%.12lf,", time, atof(beatlengthstr) / speed);
                  }
                  else // slider velocity
                  {
                     snpedit("%ld,%s,", time, beatlengthstr);
                  }

                  *(beatlengthstr + 1) = ',';
                  char* linestart = beatlengthstr + strlen(beatlengthstr) + 1;
                  putdstr(linestart);
               }
            }
            else if (sect == hitobjects && !read_mode)
            {
               chktkn(line, 6);

               edited = true;
               char *xstr = tkn(line);
               char *ystr = nexttkn();
               char *timestr = nexttkn();
               char *typestr = nexttkn();

               int x = atoi(xstr);
               int y = atoi(ystr);
               if (flip == xflip || flip == transpose) x = 512 - x;
               if (flip == yflip || flip == transpose) y = 384 - y;

               long time = atol(timestr) / speed;
               int type = atoi(typestr);
               char* linestart;
               if (type & (1<<3) || type & (1<<7)) // spinner or mania holds
               {
                  char *spinnertoken = type & (1<<7) ? ":" : ","; // mania holds use : as separator for its length
                  char *hitsoundstr = nexttkn();
                  char *spinnerstr = strtok(NULL, spinnertoken);
                  if (!(hitsoundstr && spinnerstr)) goto parsefail;

                  long spinnerlen = atol(spinnerstr) / speed;

                  snpedit("%d,%d,%ld,%s,%s,%ld", x, y, time, typestr, hitsoundstr, spinnerlen);

                  if (nexttkn() == NULL)
                  {
                     linestart = "\r\n";
                  }
                  else
                  {
                     putdstr(spinnertoken);
                     linestart = spinnerstr + strlen(spinnerstr) + 1;
                     *(spinnerstr + 1) = *spinnertoken;
                  }
               }
               else if (type & (1<<1) && flip != none) // need to edit slider points
               {
                  char *hitsoundstr = nexttkn();
                  char *curvestr = nexttkn();
                  char *slidestr = nexttkn();
                  if (!(hitsoundstr && curvestr && slidestr)) goto parsefail;

                  *(slidestr + strlen(slidestr)) = ',';
                  char *curvetype = strtok(curvestr, "|");
                  if (!curvestr) goto parsefail;

                  snpedit("%d,%d,%ld,%s,%s,%s|", x, y, time, typestr, hitsoundstr, curvetype);

                  char *postok = strtok(NULL, "|");
                  while (1)
                  {
                     char *ysliderstr = strchr(postok, ':');
                     if (!(ysliderstr && postok)) goto parsefail;

                     int xslider = atoi(postok);
                     int yslider = atoi(ysliderstr + 1);
                     postok = strtok(NULL, "|");
                     if (flip == xflip || flip == transpose) xslider = 512 - xslider;
                     if (flip == yflip || flip == transpose) yslider = 384 - yslider;

                     snpedit("%d:%d", xslider, yslider);

                     if (postok != NULL)
                     {
                        putsstr("|");
                     }
                     else
                     {
                        putsstr(",");
                        break;
                     }
                  }
                  linestart = slidestr;
               }
               else
               {
                  linestart = typestr + strlen(typestr) + 1;
                  snpedit("%d,%d,%ld,%s,", x, y, time, typestr);
                  *(typestr + 1) = ',';
               }
               putdstr(linestart);
            }
            else if (sect == editor && !read_mode)
            {
               if (CMPSTR(line, "Bookmarks"))
               {
                  char *p = CUTFIRST(line, "Bookmarks: ");
                  char *token;
                  token = tkn(p);
                  if (token == NULL)
                  {
                     edited = false;
                  }
                  else
                  {
                     edited = true;
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
            }
            else if (sect == general)
            {
               if (CMPSTR(line, "AudioFilename"))
               {
                  if (read_mode)
                  {
                     char *filename = CUTFIRST(line, "AudioFilename: ");
                     remove_newline(filename);

                     int namelen = strlen(filename);
                     if (namelen <= 0) goto parsefail;

                     audiofile = (char*) malloc(namelen + 1);
                     if (!audiofile)
                     {
                        printerr("Failed allocating memory");
                        failure = true;
                        goto cleanup;
                     }
                     strcpy(audiofile, filename);
                  }
                  else if (!read_mode && speed != 1) // we don't actually convert audio if speed is 1.0
                  {
                     edited = true;
                     if (change_audio_speed(audiofile, bufs, speed, pitch) == 0)
                     {
                        snpedit("AudioFilename: %s\r\n", bufs->audname);
                     }
                     else
                     {
                        printerr("Failed converting audio");
                        failure = true;
                        goto cleanup;
                     }
                  }
               }
               else if (read_mode && CMPSTR(line, "Mode"))
               {
                  mode = atoi(CUTFIRST(line, "Mode: "));
               }
            }
            else if (sect == difficulty)
            {
               if (CMPSTR(line, "HPDrainRate"))
               {
                  APPLYDIFF(diff.hp, "HPDrainRate");
               }
               else if (CMPSTR(line, "CircleSize"))
               {
                  APPLYDIFF(diff.cs, "CircleSize");
               }
               else if (CMPSTR(line, "OverallDifficulty"))
               {
                  APPLYDIFF(diff.od, "OverallDifficulty");
               }
               else if (!(read_mode || arexists) || CMPSTR(line, "ApproachRate"))
               {
                  if (read_mode) arexists = true;
                  else if (!(read_mode || arexists))
                  {
                     arexists = true;
                     loop_again = true;
                  }
                  APPLYDIFF(diff.ar, "ApproachRate");
               }
            }
            else if (sect == metadata)
            {
               if (!(read_mode || versionexists) || CMPSTR(line, "Version"))
               {
                  if (read_mode)
                  {
                     versionexists = true;
                  }
                  else
                  {
                     char *vline = line;
                     if (!versionexists)
                     {
                        versionexists = true;
                        loop_again = true;
                        vline = "Version:";
                     }
                     else
                     {
                        remove_newline(vline);
                     }
                     edited = true;

                     if (!emulate_dt)
                     {
                        snpedit("%s %.2fx", vline, speed);
                     }
                     else
                     {
                        snpedit("%s %.2fx(DT)", vline, speed * 1.5);
                     }

                     if (diff.hp.mode != fix && diff.hp.orig_value != diff.hp.val) snpedit(" HP%.1f", diff.hp.val);

                     if (diff.cs.mode != fix && diff.cs.orig_value != diff.cs.val) snpedit(" CS%.1f", diff.cs.val);

                     if (diff.od.mode != fix &&
                           (emulate_dt || diff.od.orig_value != diff.od.val)) snpedit(" OD%.1f", !emulate_dt ? diff.od.val : scale_od(diff.od.val, 1.5, mode));

                     if (diff.ar.mode != fix &&
                           (emulate_dt || diff.ar.orig_value != diff.ar.val)) snpedit(" AR%.1f", !emulate_dt ? diff.ar.val : scale_ar(diff.ar.val, 1.5, mode));

                     switch (flip)
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
               }
               else if (CMPSTR(line, "Tags"))
               {
                  if (read_mode)
                  {
                     tagexists = true;
                  }
                  else
                  {
                     edited = true;
                     remove_newline(line);
                     snpedit("%s osutrainer\r\n", line);
                  }
               }
               else if (!(tagexists || read_mode))
               {
                  edited = true;
                  tagexists = true;
                  loop_again = true;
                  putsstr("Tags:osutrainer\r\n");
               }
            }
         }

         if (!read_mode)
         {
            if (edited && ecur == 0)
            {
               continue;
            }
            if (buffers_map_put(bufs, edited ? editline : line, edited ? ecur : strlen(line)) != 0)
            {
               failure = true;
               goto cleanup;
            }
         }
      }

      if (read_mode)
      {
         linenum = 0;
         read_mode = false;
         sect = root;
         rewind(source);

         if (rate_mode == bpm)
         {
            max_bpm = 1 / max_bpm * 1000 * 60;
            speed /= max_bpm; // speed=target bpm / max_bpm=map bpm
         }
         else if (rate_mode == guess)
         {
            max_bpm = 1 / max_bpm * 1000 * 60;
            double estimate_bpm = max_bpm * speed;
            if (estimate_bpm > 10000)
            {
               puts("Using input value as BPM...");
               rate_mode = bpm;
               speed /= max_bpm;
            }
            else
            {
               puts("Using input value as RATE...");
               rate_mode = rate;
            }
         }

         if (speed <= 0)
         {
            printerr("Speed value is wrong!");
            failure = true;
            goto cleanup;
         }

         if (!arexists) diff.ar.val = diff.od.val; // old maps may not have ar, but we need to scale if needed, so set the value here.

         if (diff.hp.mode == specify) diff.hp.val = diff.hp.user_value;
         if (diff.cs.mode == specify) diff.cs.val = diff.cs.user_value;

         if (diff.od.mode == specify) diff.od.val = diff.od.user_value;
         else if (diff.od.mode != fix) diff.od.val = scale_od(diff.od.val, speed, mode);

         if (diff.ar.mode == specify) diff.ar.val = diff.ar.user_value;
         else if (diff.ar.mode != fix) diff.ar.val = scale_ar(diff.ar.val, speed, mode);

         if (diff.hp.mode == cap) CLAMP(diff.hp.val, 0, diff.hp.user_value);
         if (diff.cs.mode == cap) CLAMP(diff.cs.val, 0, diff.cs.user_value);
         if (diff.od.mode == cap) CLAMP(diff.od.val, 0, diff.od.user_value);
         if (diff.ar.mode == cap) CLAMP(diff.ar.val, 0, diff.ar.user_value);

         if (diff.ar.val > 10 || diff.od.val > 10)
         {
            puts("AR/OD is higher than 10. Emulating DT...");
            emulate_dt = true;
            speed /= 1.5;

            diff.od.val = scale_od(diff.od.val, 1/1.5, mode);
            diff.ar.val = scale_ar(diff.ar.val, 1/1.5, mode);
         }

         // clamp ar and od here
         if (mode == 2) diff.od.mode = fix;
         else CLAMP(diff.od.val, 0, 10);

         if (mode == 1 || mode == 3) diff.ar.mode = fix;
         else CLAMP(diff.ar.val, 0, 10);

         bool name_conflict = true;
         char prefix[8] = {0};

         bufs->mapname = (char*) malloc(7 + 1 + strlen(beatmap) + 1);
         if (audiofile && speed != 1) bufs->audname = (char*) malloc(7 + 1 + strlen(audiofile) + 1);

         if (bufs->mapname == NULL || (audiofile && speed != 1 && bufs->audname == NULL))
         {
            printerr("Failed allocating memory");
            failure = true;
            goto cleanup;
         }

         snprintf(bufs->mapname + 7, 1 + strlen(beatmap) + 1, "_%s", beatmap);
         if (bufs->audname) snprintf(bufs->audname + 7, 1 + strlen(audiofile) + 1, "_%s", audiofile);

         randominit();

         while (name_conflict == true)
         {
            randomstr(prefix, 7);
            memcpy(bufs->mapname, prefix, 7);
            if (bufs->audname) memcpy(bufs->audname, prefix, 7);
            if (access(bufs->mapname, F_OK) != 0 && (bufs->audname == NULL || access(bufs->audname, F_OK) != 0))
               name_conflict = false;
         }
      }
      else
      {
         read_mode = true;
      }
   }
   while (read_mode == false);


   if (false)
   {
parsefail:
      failure = true;
      fprintf(stderr, "Failed parsing a line at %d: %s\n", linenum, line);
   }
cleanup:
   if (source && fclose(source) == EOF)
   {
      perror(beatmap);
   }

   free(line);

   return failure ? 1 : 0;
}
