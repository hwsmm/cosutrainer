#include "freader.h"
#include "tools.h"
#include "cosuplatform.h"
#include "cosuwindow.h"
#include "cosumem.h"
#include <FL/Fl.H>

Freader::Freader(CosuWindow *win) : thr(Freader::thread_func, this)
{
    init(win);
    init_sigstatus(&st);
    songf = NULL;
}

Freader::~Freader()
{
    close();
    free(songf);
}

void Freader::thread_func(Freader *fr)
{
    fr->conti = true;

    ptr_type base = NULL;
    wchar_t *songpath = NULL;
    wchar_t *oldpath = NULL;
    unsigned int len = 0;
    while (fr->conti)
    {
        struct sigscan_status *sst = &(fr->st);

        DEFAULT_LOGIC(sst,
        {
            if (fr->songf != NULL)
            {
                free(fr->songf);
                fr->songf = NULL;
            }
        },
        {
            if (fr->songf == NULL)
            {
                fr->songf = get_songspath();
                if (fr->songf == NULL)
                {
                    printerr("Song folder not found!");
                    fr->sleep();
                    continue;
                }
            }

            if (base == PTR_NULL)
            {
                if (!match_pattern(sst, &base) || base == PTR_NULL)
                {
                    printerr("Failed scanning memory!");
                    fr->sleep();
                    continue;
                }
            }

            songpath = get_mappath(sst, base, &len);
            if (songpath != NULL)
            {
                if (oldpath != NULL && wcscmp(songpath, oldpath) == 0)
                {
                    free(songpath);
                    fr->sleep();
                    continue;
                }
                else
                {
                    free(oldpath);
                    oldpath = songpath;
                }
            }
            else
            {
                printerr("Failed reading memory!");
                base = NULL;
                continue;
            }

            int fullsize = strlen(fr->songf) + 1 + len * MB_CUR_MAX;
            char *fullpath = (char*) malloc(fullsize);
            if (fullpath == NULL)
            {
                printerr("Failed allocating!");
                continue;
            }

            snprintf(fullpath, fullsize, "%s\\%ls", fr->songf, songpath);

            {
                std::lock_guard<std::recursive_mutex> lck(fr->mtx);
                free_mapinfo(fr->oldinfo);
                fr->oldinfo = fr->info;
                fr->info = read_beatmap(fullpath);
                if (fr->info == NULL)
                {
                    printerr("Failed reading!");
                    free(fullpath);
                    continue;
                }
                free(fullpath);
    
                fr->consumed = false;
                Fl::awake();
            }
        },
        {
            base = NULL;
            if (fr->songf)
            {
                free(fr->songf);
                fr->songf = NULL;
            }
        })
        fr->sleep();
    }
    free_mapinfo(fr->oldinfo);
    free_mapinfo(fr->info);
    free(songpath);
    free(oldpath);
}
