#include "freader_win.h"
#include "tools.h"
#include "cosuplatform.h"
#include "cosumem.h"
#include <FL/Fl.H>

Freader::Freader() : thr(Freader::thread_func, this)
{
    info = NULL;
    oldinfo = NULL;
    songf = NULL;
    consumed = true;
    conti = true;
    pause = false;
    init_sigstatus(&st);
}

Freader::~Freader()
{
    this->conti = false;
    thr.join();
    if (songf) free(songf);
}

void Freader::thread_func(Freader *fr)
{
    ptr_type base = NULL;
    wchar_t *songpath = NULL;
    wchar_t *oldpath = NULL;
    unsigned int len = 0;
    while (fr->conti)
    {
        struct sigscan_status *sst = &(fr->st);
        if (fr->pause)
        {
            Sleep(1000);
            continue;
        }

        if (OSUMEM_NOT_FOUND(sst))
        {
            find_and_set_osu(sst);
            if (OSUMEM_NOT_FOUND(sst))
            {
                Sleep(1000);
                continue;
            }
        }
        else
        {
            find_and_set_osu(sst);
        }

        if (OSUMEM_OK(sst))
        {
            if (OSUMEM_NEW_OSU(sst))
            {
                if (init_memread(sst) == -1)
                {
                    printerr("Failed initializing memory reading!");
                    Sleep(1000);
                    continue;
                }
            }

            if (fr->songf == NULL)
            {
                fr->songf = get_songsfolder(sst);
                if (fr->songf == NULL)
                {
                    printerr("Song folder not found!");
                    Sleep(1000);
                    continue;
                }
            }

            if (base == NULL)
            {
                base = match_pattern(sst);
                if (base == NULL)
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

            snprintf(fullpath, fullsize, "%s/%ls", fr->songf, songpath);

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
            Sleep(1000);
        }
        else
        {
            // process lost!
            stop_memread(sst);
            base = NULL;
            if (fr->songf)
            {
                free(fr->songf);
                fr->songf = NULL;
            }
        }
    }
    free_mapinfo(fr->oldinfo);
    free_mapinfo(fr->info);
    free(songpath);
    free(oldpath);
}