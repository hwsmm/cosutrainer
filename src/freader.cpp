#include "freader.h"
#include "tools.h"
#include "cosuplatform.h"
#include "cosuwindow.h"
#include <unistd.h>
#include <FL/Fl.H>

Freader::Freader(CosuWindow *win) : thr(Freader::thread_func, this)
{
    init(win);
    songpath_init(&st);
}

Freader::~Freader()
{
    close();
    songpath_free(&st);
}

void Freader::thread_func(Freader *fr)
{
    fr->conti = true;
    fr->pause = false;
    
    while (fr->conti)
    {
        std::lock_guard<std::mutex> lck(fr->mtx);
        
        if (fr->pause)
        {
            edit_beatmap(&(fr->edit), &(fr->win->progress));
            fr->pause = false;
            fr->win->done = true;
        }

        char *fullpath;
        if (!songpath_get(&(fr->st), &fullpath))
        {
            fr->sleep();
            continue;
        }

        // free_mapinfo(fr->info);
        free_mapinfo(fr->oldinfo);
        fr->oldinfo = fr->info;
        fr->info = read_beatmap(fullpath);
        if (fr->info == NULL)
        {
            printerr("Failed reading!");
        }
        else
        {
            fr->consumed = false;
            Fl::awake();
        }

        fr->sleep();
    }
}
