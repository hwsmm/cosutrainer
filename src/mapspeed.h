#pragma once
enum SECTION
{
   root, general, editor, metadata, difficulty, events, timingpoints, colours, hitobjects
};

enum FLIP
{
   none, xflip, yflip, transpose
};

struct difficulty
{
   float hp;
   float cs;
   float od;
   float ar;
};

double scale_ar(double ar, double speed, int mode);
double scale_od(double od, double speed, int mode);
int edit_beatmap(const char* beatmap, double speed, const bool bpm, struct difficulty diff, const bool pitch, const enum FLIP flip);

