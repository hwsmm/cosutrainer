#pragma once
#include "mapeditor.h"
#include "tools.h"
#include "stbi_config.h"
#include "cosuplatform.h"
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
        struct mapinfo *newinfo = read_beatmap(fullpath);

        unsigned char *bgdata = NULL;
        int x = 0, y = 0;

        if (newinfo != NULL && newinfo->bgname != NULL)
        {
            bgdata = stbi_load(newinfo->bgname, &x, &y, NULL, 3);
            if (bgdata == NULL)
            {
                printerr("Failed loading a BG");
            }
        }

        if (bgdata != NULL && x > 0 && y > 0)
        {
            Fl_RGB_Image img(bgdata, x, y);
            float newh = (READ_BG_W / (float)x) * (float)y;
            newinfo->extra = (void*)img.copy(READ_BG_W, (int)newh);
            stbi_image_free(bgdata);
        }

        struct mapinfo *oldinfo = mi.exchange(newinfo);
        Fl::awake();

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
