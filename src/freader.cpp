extern "C"
{
#include <unistd.h>
}

#include "freader.h"
#include "tools.h"
#include "cosuplatform.h"
#include "cosuwindow.h"
#include <FL/Fl.H>
#include <signal.h>

Freader::Freader(CosuWindow *win) : thr(Freader::thread_func, this)
{
    init(win);
    songpath_init(&st);
}

Freader::~Freader()
{
    conti = false;

    if (pthread_kill(thr.native_handle(), SIGUSR1) == 0)
    {
        close();
    }
    else
    {
        printerr("Failed to signal a reader thread");
        free_mapinfo(oldinfo);
        free_mapinfo(info);
    }

    songpath_free(&st);
}

static void signal_handle(int sig)
{
    (void)sig;
}

void Freader::thread_func(Freader *fr)
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handle;
    if (sigaction(SIGUSR1, &act, NULL) != 0)
    {
        perror("sigaction");
        return;
    }

    while (fr->conti)
    {
        char *fullpath;
        if (!songpath_get(&(fr->st), &fullpath))
        {
            if (fr->conti)
                usleep(500000);
            else
                break;

            continue;
        }

        std::lock_guard<std::recursive_mutex> lck(fr->mtx);

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
    }
}
