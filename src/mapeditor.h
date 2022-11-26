#pragma once
#include <stdbool.h>
#include "buffers.h"

enum SECTION
{
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
   char *audioname, *songname, *diffname, *bgname;

   double maxbpm, minbpm;
   float hp, cs, ar, od;

   int mode;
   bool arexists;
   bool tagexists;
   bool diffexists;
};

struct editdata // data needed to edit a map
{
   const struct mapinfo *mi;
   float hp, cs, ar, od;
   bool scale_ar, scale_od;
   bool makezip;

   double speed;
   enum SPEED_MODE bpmmode;
   bool pitch;
   enum FLIP flip;
};

struct editpass // temporary data that's only needed in editing
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

struct mapinfo *read_beatmap(char *mapfile);
void free_mapinfo(struct mapinfo *info);
int edit_beatmap(struct editdata *edit, float *progress);

#ifdef __cplusplus
}
#endif
