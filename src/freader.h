#pragma once
#include "mapeditor.h"
#include "tools.h"
#include <thread>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <FL/Fl.H>
#include <FL/Fl_RGB_Image.H>

#ifdef WIN32
#include "sigscan.h"
#else
#include "lsongpathparser.h"
#endif

#define READ_BG_W 370

class Freader
{
private:
    std::thread thr;
#ifdef WIN32
    struct sigscan_status st;
#else
    struct songpath_status st;
#endif

    static void thread_func(Freader *fr);
    std::atomic<bool> conti;
    std::atomic<struct mapinfo*> mi;

    void sleep_cnd()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void update_mapinfo(char *fullpath)
    {
        struct bgdata bgread = { NULL, 0, 0 };
        struct mapinfo *newinfo = read_beatmap(fullpath, &bgread);

        if (bgread.data != NULL && bgread.x > 0 && bgread.y > 0)
        {
            Fl_RGB_Image img(bgread.data, bgread.x, bgread.y);
            float newh = (READ_BG_W / (float)bgread.x) * (float)bgread.y;
            newinfo->extra = (void*)img.copy(READ_BG_W, (int)newh);
        }

        struct mapinfo *oldinfo = mi.exchange(newinfo);
        Fl::awake();

        free_bginfo(&bgread);
        if (oldinfo != NULL && oldinfo->extra != NULL)
            delete (Fl_Image*)oldinfo->extra;

        free_mapinfo(oldinfo);
    }

public:
    Freader();
    ~Freader();

    struct mapinfo *get_mapinfo()
    {
        return mi.exchange(nullptr);
    }
};
