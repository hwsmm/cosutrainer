#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "mapspeed.h"
#include "audiospeed.h"
#include "tools.h"

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
   if (mode == 0) // standard
   {
      double od_ms = 79.5 - 6 * od;
      od_ms /= speed;
      return (79.5 - od_ms) / 6;
   }
   else if (mode == 1) // taiko
   {
      double od_ms = 49.5 - 3 * od;
      od_ms /= speed;
      return (49.5 - od_ms) / 3;
   }
   else if (mode == 3) // mania
   {
      double od_ms = 64.5 - 3 * od;
      od_ms /= speed;
      return (64.5 - od_ms) / 3;
   }
   else // catch doesn't use od
   {
      return od;
   }
}

// generate random string with alphabets and save it to 'string'
static void randomstr(char *string, int size)
{
   int i;
   for (i = 0; i < size; i++)
   {
      int randomnum = rand() % 26;
      randomnum += 97;
      *(string + i) = randomnum;
   }
}

int edit_beatmap(const char* beatmap, double speed, const bool bpm, struct difficulty diff, const bool pitch, const enum FLIP flip)
{
   FILE *source = fopen(beatmap, "r");
   if (!source)
   {
      perror(beatmap);
      return 1;
   }

   FILE *dest = NULL; // we didn't open this file yet since file name is not decided yet

   char result_file[512] = {0}; // result map file name
   char audio_file[512] = {0}; // original audio file name
   char new_audio_file[512] = {0}; // result audio file name

   int current_size = 1024; // it's initial size of line for now, may increase as we edit map
   int realloc_step = 1024; // program will increase size of line once it meets very long line
   char *line = (char*) malloc(current_size);

   bool read_mode = true; // program will read the entire file first (read_mode==true), and read again to write the new map file (read_mode==false)
   // it is used to read AR/OD and etc and preprocess before writing the actual result map file
   bool edited = false; // if it's set as true in write mode, program will not attempt to write the original line since an edited line is already written to a file
   // or you can just set it as true and do nothing, which will just remove the original line
   bool loop_again = false; // if it's set as true, the loop won't read a new line but just loop again, used when the program wrote an entirely new line, or line is realloc-ed
   bool failure = false; // set this to true and use 'goto cleanup' to clean memory properly

   bool arexists = false; // will be set as true once loop meets ApproachRate, some old maps don't have AR in the map (the client uses od as ar instead)
   bool tagexists = false; // will be set as true once loop meets Tag
   bool fixar = diff.ar == -2 || diff.ar >= 0; // ar == -2 means fixed, and bigger than 0 means the value is set
   bool fixod = diff.od == -2 || diff.od >= 0;

   bool emulate_dt = false; // when (scaled) od or ar is higher than 10. this variable is to indicate the program that it is emulating dt, since all the values are already adjusted
   double max_bpm = 0; // max bpm read in the map
   int mode = 0; // map mode

   enum SECTION sect = root;

   do
   {
      while (loop_again || fgets(line, current_size, source))
      {
         edited = false;
         loop_again = false;

         // dealing with very long lines, most of time, we don't need to edit rest of such lines, but we need it for inverting slider points
         // so dynamically allocate memory if the line doesn't have \n
         if (strchr(line, '\n') == NULL)
         {
            loop_again = true;
            char *tmpline = (char*) realloc(line, current_size + realloc_step);
            if (!tmpline)
            {
               failure = true;
               printf("Failed reallocating buffer. Conversion failed\n");
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

#ifdef DEBUG
         printf("(%ld) %s", strlen(line), line);
#endif
         enum SECTION oldsect = sect;
         if (line[0] == '[')
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
         if (oldsect == sect && !(line[0] == '\r' || line[0] == '\n' || line[0] == '\0' || line[0] == '/'))
         {
            if (sect == root && !read_mode)
            {
               if (CMPSTR(line, "osu file format"))
               {
                  edited = true;
                  fputs("osu file format v14\r\n", dest); // old maps don't allow decimal ar/od, and v14 is just more compatible with this program
               }
            }
            else if (sect == events && !read_mode)
            {
               if (line[0] == '2' || CMPSTR(line, "Break"))
               {
                  edited = true;
                  tkn(line);
                  long start = atol(nexttkn()) / speed;
                  long end = atol(nexttkn()) / speed;
                  fprintf(dest, "2,%ld,%ld\r\n", start, end);
               }
               else if (line[0] == '0')
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
               char* timestr = tkn(line);
               char* beatlengthstr = nexttkn();

               if (bpm && read_mode)
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
                     fprintf(dest, "%ld,%.12lf,", time, atof(beatlengthstr) / speed);
                  }
                  else // slider velocity
                  {
                     fprintf(dest, "%ld,%s,", time, beatlengthstr);
                  }

                  *(beatlengthstr + 1) = ',';
                  char* linestart = beatlengthstr + strlen(beatlengthstr) + 1;
                  fputs(linestart, dest);
               }
            }
            else if (sect == hitobjects && !read_mode)
            {
               edited = true;
               char* xstr = tkn(line);
               char* ystr = nexttkn();
               char* timestr = nexttkn();
               char* typestr = nexttkn();

               int x = atoi(xstr);
               int y = atoi(ystr);
               if (flip == xflip || flip == transpose) x = 512 - x;
               if (flip == yflip || flip == transpose) y = 384 - y;

               long time = atol(timestr) / speed;
               int type = atoi(typestr);
               char* linestart;
               if (type & (1<<3) || type & (1<<7)) // spinner or mania holds
               {
                  char* spinnertoken = type & (1<<7) ? ":" : ","; // mania holds use : as separator for its length
                  char* hitsoundstr = nexttkn();
                  char* spinnerstr = strtok(NULL, spinnertoken);

                  long spinnerlen = atol(spinnerstr) / speed;

                  fprintf(dest, "%d,%d,%ld,%s,%s,%ld", x, y, time, typestr, hitsoundstr, spinnerlen);

                  if (nexttkn() == NULL)
                  {
                     linestart = "\r\n";
                  }
                  else
                  {
                     fputs(spinnertoken, dest);
                     linestart = spinnerstr + strlen(spinnerstr) + 1;
                     *(spinnerstr + 1) = *spinnertoken;
                  }
               }
               else if (type & (1<<1) && flip != none) // need to edit slider points
               {
                  char *hitsoundstr = nexttkn();
                  char *curvestr = nexttkn();
                  char *slidestr = nexttkn();

                  *(slidestr + strlen(slidestr)) = ',';
                  char *curvetype = strtok(curvestr, "|");
                  fprintf(dest, "%d,%d,%ld,%s,%s,%s|", x, y, time, typestr, hitsoundstr, curvetype);

                  char *postok = strtok(NULL, "|");
                  while (1)
                  {
                     int xslider = atoi(postok);
                     int yslider = atoi(strchr(postok, ':') + 1);
                     postok = strtok(NULL, "|");
                     if (flip == xflip || flip == transpose) xslider = 512 - xslider;
                     if (flip == yflip || flip == transpose) yslider = 384 - yslider;

                     fprintf(dest, "%d:%d", xslider, yslider);

                     if (postok != NULL)
                     {
                        fputs("|", dest);
                     }
                     else
                     {
                        fputs(",", dest);
                        break;
                     }
                  }
                  linestart = slidestr;
               }
               else
               {
                  linestart = typestr + strlen(typestr) + 1;
                  fprintf(dest, "%d,%d,%ld,%s,", x, y, time, typestr);
                  *(typestr + 1) = ',';
               }
               fputs(linestart, dest);
            }
            else if (sect == editor && !read_mode)
            {
               if (CMPSTR(line, "Bookmarks"))
               {
                  edited = true;
                  fputs("Bookmarks: ", dest);
                  char *p = CUTFIRST(line, "Bookmarks: ");
                  char* token;
                  token = tkn(p);
                  while (1)
                  {
                     long time = atol(token) / speed;
                     token = nexttkn();
                     if (token == NULL)
                     {
                        fprintf(dest, "%ld\r\n", time);
                        break;
                     }
                     else
                     {
                        fprintf(dest, "%ld,", time);
                     }
                  }
               }
            }
            else if (sect == general)
            {
               if (speed != 1 && CMPSTR(line, "AudioFilename")) // don't edit mp3 when speed is 1.0
               {
                  if (read_mode)
                  {
                     char* filename = CUTFIRST(line, "AudioFilename: ");
                     remove_newline(filename);
                     strcpy(audio_file, filename);
                  }
                  else
                  {
                     edited = true;
                     if (change_audio_speed(audio_file, new_audio_file, speed, pitch) == 0)
                     {
                        fprintf(dest, "AudioFilename: %s\r\n", new_audio_file);
                     }
                     else
                     {
                        failure = true;
                        printf("Failed converting audio\n");
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
               if (!read_mode && diff.hp >= 0 && CMPSTR(line, "HPDrainRate"))
               {
                  edited = true;
                  fprintf(dest, "HPDrainRate:%.2f\r\n", diff.hp);
               }
               else if (!read_mode && diff.cs >= 0 && CMPSTR(line, "CircleSize"))
               {
                  edited = true;
                  fprintf(dest, "CircleSize:%.2f\r\n", diff.cs);
               }
               else if (CMPSTR(line, "OverallDifficulty"))
               {
                  if (read_mode)
                  {
                     if (!fixod) diff.od = atof(CUTFIRST(line, "OverallDifficulty:"));
                  }
                  else
                  {
                     if (diff.od != -2)
                     {
                        edited = true;
                        fprintf(dest, "OverallDifficulty:%.2f\r\n", diff.od);
                     }
                  }
               }
               else if (!(read_mode || arexists) || CMPSTR(line, "ApproachRate"))
               {
                  if (read_mode)
                  {
                     arexists = true;
                     if (!fixar)
                     {
                        diff.ar = atof(CUTFIRST(line, "ApproachRate:"));
                     }
                  }
                  else
                  {
                     if (diff.ar != -2)
                     {
                        edited = true;
                        if (!arexists)
                        {
                           arexists = true;
                           loop_again = true;
                        }
                        fprintf(dest, "ApproachRate:%.2f\r\n", diff.ar);
                     }
                  }
               }
            }
            else if (sect == metadata)
            {
               if (!read_mode && CMPSTR(line, "Version"))
               {
                  edited = true;
                  remove_newline(line);

                  if (!emulate_dt) fprintf(dest, "%s %.2fx", line, speed);
                  else             fprintf(dest, "%s %.2fx(DT)", line, speed * 1.5);

                  if (diff.hp != -2) fprintf(dest, " HP%.1f", diff.hp);

                  if (diff.cs != -2) fprintf(dest, " CS%.1f", diff.cs);

                  if (diff.od != -2) fprintf(dest, " OD%.1f", !emulate_dt ? diff.od : scale_od(diff.od, 1.5, mode));

                  if (diff.ar != -2) fprintf(dest, " AR%.1f", !emulate_dt ? diff.ar : scale_ar(diff.ar, 1.5, mode));

                  switch (flip)
                  {
                  case xflip:
                     fputs(" X(invert)", dest);
                     break;
                  case yflip:
                     fputs(" Y(invert)", dest);
                     break;
                  case transpose:
                     fputs(" TRANSPOSE", dest);
                     break;
                  default:
                     break;
                  }

                  fputs("\r\n", dest);
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
                     fprintf(dest, "%s osutrainer\r\n", line);
                  }
               }
               else if (!(tagexists || read_mode))
               {
                  edited = true;
                  tagexists = true;
                  loop_again = true;
                  fprintf(dest, "Tags:osutrainer\r\n");
               }
            }
         }

         if (!(read_mode || edited))
         {
            fputs(line, dest);
         }
#ifdef DEBUG
         else if (!read_mode)
         {
            printf("EDITED\n");
         }
#endif
      }

      if (read_mode)
      {
         read_mode = false;
         sect = root;
         rewind(source);

         if (bpm)
         {
            max_bpm = 1 / max_bpm * 1000 * 60;
            speed /= max_bpm; // speed=target bpm / max_bpm=map bpm
         }

         if (!arexists) diff.ar = diff.od; // old maps may not have ar, but we need to scale if needed, so set the value here.
         if (!fixod) diff.od = scale_od(diff.od, speed, mode);
         if (!fixar) diff.ar = scale_ar(diff.ar, speed, mode);

         if (diff.ar > 10 || diff.od > 10)
         {
            printf("AR/OD is higher than 10. Emulating DT...\n");
            emulate_dt = true;
            speed /= 1.5;

            diff.od = scale_od(diff.od, 1/1.5, mode);
            diff.ar = scale_ar(diff.ar, 1/1.5, mode);
         }

         // clamp ar and od here
         if (mode == 2) diff.od = -2;
         else diff.od = diff.od > 10 ? 10 : diff.od < 0 ? 0 : diff.od;

         if (mode == 1 || mode == 3) diff.ar = -2;
         else diff.ar = diff.ar > 10 ? 10 : diff.ar < 0 ? 0 : diff.ar;

         bool name_conflict = true;
         char prefix[8] = {0};

         snprintf(result_file + 7, 512 - 7, "_%s", beatmap);
         snprintf(new_audio_file + 7, 512 - 7, "_%s", audio_file);

         srand(time(NULL));

         while (name_conflict == true)
         {
            randomstr(prefix, 7);
            memcpy(result_file, prefix, 7);
            memcpy(new_audio_file, prefix, 7);
            if (access(result_file, F_OK) != 0 && access(new_audio_file, F_OK) != 0)
               name_conflict = false;
         }

         dest = fopen(result_file, "w");
         if (!dest)
         {
            perror(result_file);
            return 1;
         }
      }
      else
      {
         read_mode = true;
      }
   }
   while (read_mode == false);

cleanup:
   if (source && fclose(source) == EOF)
   {
      perror(beatmap);
   }

   if (dest && fclose(dest) == EOF)
   {
      perror(result_file);
      printf("Error while saving a file, result map file may be corrupted.\n");
   }

   if (failure)
   {
      if (unlink(result_file) != 0)
      {
         printf("Failed removing unfinished file\n");
      }
      unlink(new_audio_file);
   }

   free(line);

   return failure ? 1 : 0;
}
