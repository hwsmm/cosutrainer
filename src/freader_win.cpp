#include "freader.h"
#include "cosuplatform.h"
#include "cosumem.h"
#include <FL/Fl.H>

Freader::Freader() : thr(Freader::thread_func, this)
{
    init();
    init_sigstatus(&st);
}

Freader::~Freader()
{
    close();
    free(path);
}

void Freader::thread_func(Freader *fr)
{
    ptr_type base = NULL;
    wchar_t *songpath = NULL;
    wchar_t *oldpath = NULL;
    char *songf = NULL;
    unsigned int len = 0;
    while (fr->conti)
    {
        std::lock_guard<std::recursive_mutex> lck(fr->mtx);

        struct sigscan_status *sst = &(fr->st);

        DEFAULT_LOGIC(sst,
        {
            if (songf != NULL)
            {
                free(songf);
                songf = NULL;
            }
        },
        {
            if (songf == NULL)
            {
                songf = get_songspath(get_rootpath(sst));
                if (songf == NULL)
                {
                    printerr("Song folder not found!");
                    fr->sleep_cnd();
                    continue;
                }
            }

            if (base == PTR_NULL)
            {
                if (!match_pattern(sst, &base) || base == PTR_NULL)
                {
                    printerr("Failed scanning memory!");
                    fr->sleep_cnd();
                    continue;
                }
            }

            songpath = get_mappath(sst, base, &len);
            if (songpath != NULL)
            {
                if (oldpath != NULL && wcscmp(songpath, oldpath) == 0)
                {
                    free(songpath);
                    songpath = NULL;
                    fr->sleep_cnd();
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

            int fullsize = strlen(songf) + 1 + len * MB_CUR_MAX;
            char *fullpath = (char*) malloc(fullsize);
            if (fullpath == NULL)
            {
                printerr("Failed allocating!");
                continue;
            }

            snprintf(fullpath, fullsize, "%s\\%ls", songf, songpath);

            char *curpath = fr->path;
            fr->path = fullpath;
            fr->consumed = false;
            Fl::awake();
            free(curpath);
        },
        {
            base = NULL;
            if (songf)
            {
                free(songf);
                songf = NULL;
            }
        })
        fr->sleep_cnd();
    }
    free(songpath);
    free(oldpath);
    free(songf);
}
