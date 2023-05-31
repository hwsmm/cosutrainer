#include "freader.h"
#include "tools.h"
#include "cosuplatform.h"
#include <unistd.h>
#include <FL/Fl.H>

Freader::Freader() : thr(Freader::thread_func, this)
{
    path = NULL;
    info = NULL;
    oldinfo = NULL;
    songf = NULL;
    consumed = true;
    conti = true;
    pause = false;
}

Freader::~Freader()
{
    this->conti = false;
    thr.join();
    free_mapinfo(oldinfo);
    free_mapinfo(info);
    free(path);
    free(songf);
}

void Freader::thread_func(Freader *fr)
{
    while (fr->conti)
    {
        if (fr->pause)
        {
            sleep(1);
            continue;
        }

        if (fr->songf == NULL)
        {
            fr->songf = get_songspath();
            if (fr->songf == NULL)
            {
                printerr("Song Folder not found!");
                continue;
            }
        }

        int readbytes = 0;
        char *new_path = read_file("/tmp/osu_path", &readbytes);
        if (new_path == NULL)
        {
            sleep(1);
            continue;
        }

        if (fr->path == NULL)
        {
        }
        else if (strcmp(fr->path, new_path) == 0)
        {
            free(new_path);
            sleep(1);
            continue;
        }

        if (fr->path != NULL) free(fr->path);
        fr->path = new_path;
        int fullsize = strlen(fr->songf) + 1 + readbytes + 1;
        char *fullpath = (char*) malloc(fullsize);
        if (fullpath == NULL)
        {
            printerr("Failed allocating!");
            continue;
        }

        snprintf(fullpath, fullsize, "%s/%s", fr->songf, trim(fr->path, &readbytes));

        // free_mapinfo(fr->info);
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
        sleep(1);
    }
}
