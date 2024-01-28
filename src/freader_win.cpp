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
}

Freader::~Freader()
{
    close();
}

void Freader::thread_func(Freader *fr)
{
    fr->conti = true;
    fr->pause = false;
    
    ptr_type base = NULL;
    wchar_t *songpath = NULL;
    wchar_t *oldpath = NULL;
    unsigned int len = 0;
    while (fr->conti)
    {
        struct sigscan_status *sst = &(fr->st);
        if (fr->pause)
        {
            edit_beatmap(&(fr->win->edit), &(fr->win->progress));
            fr->pause = false;
            fr->win->done = true;
        }

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
                    Sleep(1000);
                    continue;
                }
            }

            if (base == PTR_NULL)
            {
                if (!match_pattern(sst, &base) || base == PTR_NULL)
                {
                    printerr("Failed scanning memory!");
                    Sleep(1000);
                    continue;
                }
            }

            songpath = get_mappath(sst, base, &len);
            if (songpath != NULL)
            {
                if (oldpath != NULL && wcscmp(songpath, oldpath) == 0)
                {
                    free(songpath);
                    Sleep(1000);
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
        },
        {
            base = NULL;
            if (fr->songf)
            {
                free(fr->songf);
                fr->songf = NULL;
            }
        })
        Sleep(1000);
    }
    free_mapinfo(fr->oldinfo);
    free_mapinfo(fr->info);
    free(songpath);
    free(oldpath);
}
