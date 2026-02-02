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
    volatile bool consumed;
    std::recursive_mutex mtx;
    char *path;

    void init()
    {
        consumed = true;
        conti = true;
        path = NULL;
    }

    void close()
    {
        conti = false;
        cnd.notify_one();
        thr.join();
    }

    std::condition_variable_any cnd;

    void sleep_cnd()
    {
        cnd.wait_for(mtx, std::chrono::milliseconds(500));
    }

public:
    Freader();
    ~Freader();

    int get_mapinfo(struct mapinfo *info)
    {
        std::lock_guard<std::recursive_mutex> lck(mtx);

        if (!consumed)
        {
            free_mapinfo(info);
            consumed = true;

            if (path != NULL)
                return read_beatmap(info, path);
        }

        return -10;
    }
};
