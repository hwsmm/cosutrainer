#pragma once
#include "mapeditor.h"
#include "sigscan.h"
#include <thread>

class Freader
{
private:
    std::thread thr;
    struct sigscan_status st;
    static void thread_func(Freader *fr);
    char *songf;
    bool conti;
    bool songf_env;
public:
    Freader();
    ~Freader();
    struct mapinfo *info;
    struct mapinfo *oldinfo;
    bool consumed;
    bool pause;
};
