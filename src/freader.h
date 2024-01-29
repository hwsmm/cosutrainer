#pragma once
#include "mapeditor.h"
#include <thread>
#include <cstdlib>

#ifdef WIN32
#include "sigscan.h"
#endif

class CosuWindow;

class Freader
{
private:
    std::thread thr;
#ifdef WIN32
    struct sigscan_status st;
#endif
    static void thread_func(Freader *fr);
    char *songf;
    bool conti;
    CosuWindow *win;

    void init(CosuWindow *win)
    {
        path = NULL;
        info = NULL;
        oldinfo = NULL;
        songf = NULL;
        consumed = true;
        this->win = win;
    }

    void close()
    {
        conti = false;
        thr.join();

        free_mapinfo(oldinfo);
        free_mapinfo(info);
        if (songf) free(songf);
    }
public:
    Freader(CosuWindow *win);
    ~Freader();
    char *path;
    struct mapinfo *info;
    struct mapinfo *oldinfo;
    bool consumed;
    bool pause;
};
