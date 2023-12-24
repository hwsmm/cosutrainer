#pragma once
#include "mapeditor.h"
#include <thread>

class Freader
{
private:
    std::thread thr;
    static void thread_func(Freader *fr);
    char *songf;
    bool conti;
public:
    Freader();
    ~Freader();
    char *path;
    struct mapinfo *info;
    struct mapinfo *oldinfo;
    bool consumed;
    bool pause;
};
