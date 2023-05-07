#pragma once
#include "mapeditor.h"
#include <thread>

// wondering if i should use wchar_t even in here. things barely matter here
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
