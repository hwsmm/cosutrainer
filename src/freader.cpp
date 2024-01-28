#include "freader.h"
#include "tools.h"
#include "cosuplatform.h"
#include "cosuwindow.h"
#include <unistd.h>
#include <FL/Fl.H>

Freader::Freader(CosuWindow *win) : thr(Freader::thread_func, this)
{
    init(win);
}

Freader::~Freader()
{
    close();
    if (path != NULL) free(path);
}

void Freader::thread_func(Freader *fr)
{
    fr->conti = true;
    fr->pause = false;
    
    while (fr->conti)
    {
        if (fr->pause)
        {
            edit_beatmap(&(fr->win->edit), &(fr->win->progress));
            fr->pause = false;
            fr->win->done = true;
        }

        int readbytes = 0;
        char *new_path = read_file("/tmp/osu_path", &readbytes);
        if (new_path == NULL)
        {
            sleep(1);
            continue;
        }

        char *sep = strchr(new_path, ' ');
        *sep = '\0';
        int songflen = atoi(new_path);
        *sep = ' ';

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

        char *new_osu = sep + 1;
        char *fullpath = new_osu;

        if (*new_osu != '/')
        {
            if (fr->songf == NULL)
            {
                fr->songf = get_songspath();
                if (fr->songf == NULL)
                {
                    printerr("Song Folder not found!");
                    sleep(1);
                    continue;
                }
            }

            int songflen = strlen(fr->songf);
            int fullsize = songflen + 1 + strlen(new_osu) + 1;
            fullpath = (char*) malloc(fullsize);
            if (fullpath == NULL)
            {
                printerr("Failed allocating!");
                continue;
            }

            strcpy(fullpath, fr->songf);
            *(fullpath + songflen) = '/';
            strcpy(fullpath + songflen + 1, new_osu);

            if (try_convertwinpath(fullpath, songflen + 1) < 0)
            {
                printerr("Failed finding path!");
                free(fullpath);
                continue;
            }
        }
        else
        {
            if (try_convertwinpath(fullpath, songflen + 1) < 0)
            {
                printerr("Failed finding path!");
                sleep(1);
                continue;
            }
        }

        // free_mapinfo(fr->info);
        free_mapinfo(fr->oldinfo);
        fr->oldinfo = fr->info;
        fr->info = read_beatmap(fullpath);
        if (fr->info == NULL)
        {
            printerr("Failed reading!");
            if (fullpath != new_osu) free(fullpath);
            continue;
        }

        if (fullpath != new_osu)
            free(fullpath);

        fr->consumed = false;
        Fl::awake();
        sleep(1);
    }
}
