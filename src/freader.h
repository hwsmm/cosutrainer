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
    std::atomic<bool> conti;
    CosuWindow *win;

    void init(CosuWindow *win)
    {
        info = NULL;
        oldinfo = NULL;
        consumed = true;
        conti = true;
        this->win = win;
    }

    void close()
    {
        conti = false;
        cnd.notify_one();
        thr.join();

        free_mapinfo(oldinfo);
        free_mapinfo(info);
    }

    std::condition_variable_any cnd;

    void sleep_cnd()
    {
        cnd.wait_for(mtx, std::chrono::milliseconds(500));
    }
public:
    Freader(CosuWindow *win);
    ~Freader();
    std::recursive_mutex mtx;
    struct mapinfo *info;
    struct mapinfo *oldinfo;
    volatile bool consumed;
};
