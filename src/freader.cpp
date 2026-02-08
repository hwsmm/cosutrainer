extern "C"
{
#include <unistd.h>
}

#include "freader.h"
#include "cosuplatform.h"
#include <FL/Fl.H>
#include <csignal>

Freader::Freader() : thr(Freader::thread_func, this)
{
    conti = true;
    mi = nullptr;
    songpath_init(&st);
}

Freader::~Freader()
{
    conti = false;

    if (pthread_kill(thr.native_handle(), SIGUSR1) == 0)
    {
        thr.join();
    }
    else
    {
        printerr("Failed to signal a reader thread");
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
                fr->sleep_cnd();
            else
                break;

            continue;
        }

        free_mapinfo(fr->mi.exchange(read_beatmap(fullpath)));
        Fl::awake();
        fr->sleep_cnd();
    }
}
