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
    std::atomic<struct mapinfo*> mi;

    void sleep_cnd()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

public:
    Freader();
    ~Freader();

    struct mapinfo *get_mapinfo()
    {
        return mi.exchange(nullptr);
    }
};
