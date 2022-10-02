#pragma once
enum SECTION
{
   root, general, editor, metadata, difficulty, events, timingpoints, colours, hitobjects
};

enum FLIP
{
   none, xflip, yflip, transpose
};

enum SPEED_MODE
{
   bpm, rate, guess
};

enum DIFF_MODE
{
   fix, scale, cap, specify
};

struct diff
{
   enum DIFF_MODE mode;
   float user_value;
   float val;
};

struct difficulty
{
   struct diff hp;
   struct diff cs;
   struct diff od;
   struct diff ar;
};

double scale_ar(double ar, double speed, int mode);
double scale_od(double od, double speed, int mode);
int edit_beatmap(const char* beatmap, double speed, enum SPEED_MODE rate_mode, struct difficulty diff, const bool pitch, enum FLIP flip);

