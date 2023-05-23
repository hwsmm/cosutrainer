#pragma once
#include "mapeditor.h"
#include "arconfcmd.h"
#include <thread>

class Freader
{
private:
    std::thread thr;
    static void thread_func(Freader *fr);
    char *songf;
    bool conti;
    bool songf_env;
    bool gui;
    struct ar_conf arc;
public:
    Freader(bool gui);
    Freader();
    ~Freader();
    char *path;
    struct mapinfo *info;
    struct mapinfo *oldinfo;
    bool consumed;
    bool pause;
};
