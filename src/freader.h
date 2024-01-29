#pragma once
#include "mapeditor.h"
#include <thread>
#include <cstdlib>

#ifdef WIN32
#include "sigscan.h"
#else
#include "lsongpathparser.h"
#endif

class CosuWindow;

class Freader
{
private:
    std::thread thr;
#ifdef WIN32
    struct sigscan_status st;
    char *songf;
#else
    struct songpath_status st;
#endif
    static void thread_func(Freader *fr);
    bool conti;
    CosuWindow *win;

    void init(CosuWindow *win)
    {
        info = NULL;
        oldinfo = NULL;
        consumed = true;
        this->win = win;
    }

    void close()
    {
        conti = false;
        thr.join();

        free_mapinfo(oldinfo);
        free_mapinfo(info);
    }
public:
    Freader(CosuWindow *win);
    ~Freader();
    struct mapinfo *info;
    struct mapinfo *oldinfo;
    bool consumed;
    bool pause;
};
