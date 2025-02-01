#include "tools.h"
#include "cosuplatform.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>

int fork_launch(char* cmd)
{
    int ret = fork();
    if (ret == 0)
    {
        exit(system(cmd));
    }
    return ret;
}

static char *read_file_fallback(int fd, int *size)
{
    int cursize = 0;
    char *buf = NULL;
    ssize_t rb = 0;
    ssize_t curpoint = 0;
    do
    {
        char *rebuf = (char*) realloc(buf, (cursize += 1024));
        if (rebuf == NULL)
        {
            printerr("Failed allocating memory while reading a file");
            free(buf);
            close(fd);
            return NULL;
        }

        buf = rebuf;
        rb = read(fd, buf + curpoint, 1024 - 1);
        if (rb == -1)
        {
            printerr("Failed reading a file");
            free(buf);
            close(fd);
            return NULL;
        }

        curpoint += rb;
    }
    while (rb >= 1024 - 1);

    *(buf + curpoint) = '\0';
    if (size != NULL) *size = curpoint;
    close(fd);
    return buf;
}

char *read_file(const char *file, int *size)
{
    int fd = open(file, O_RDONLY);
    if (fd == -1)
    {
        perror(file);
        return NULL;
    }

    struct stat stat;
    if (fstat(fd, &stat) == -1 || stat.st_size <= 0)
    {
        return read_file_fallback(fd, size);
    }

    char *buf = (char*) malloc(stat.st_size + 1);
    if (buf == NULL)
    {
        printerr("Out of memory.");
        close(fd);
        return NULL;
    }

    ssize_t r = read(fd, buf, stat.st_size);
    if (r == -1)
    {
        perror(file);
        close(fd);
        free(buf);
        return NULL;
    }

    *(buf + r) = '\0';
    if (size != NULL) *size = r;
    close(fd);
    return buf;
}

char *get_realpath(char *path)
{
    struct stat buf;
    if (lstat(path, &buf) == 0)
    {
        if (!S_ISLNK(buf.st_mode)) return realpath(path, NULL);
    }
    else
    {
        return NULL;
    }

    // workaround for path that may be a symlink
    // get a real path for a parent directory, but don't follow file link
    char *lastspr = strrchr(path, '/');
    if (lastspr != NULL) *lastspr = '\0';

    char *tmp = (char*) malloc(PATH_MAX);
    if (tmp != NULL)
    {
        char *tmpr = realpath(lastspr == NULL ? "." : path, tmp);
        if (tmpr == NULL)
        {
            free(tmp);
            return NULL;
        }
        if (lastspr != NULL) *lastspr = '/';
        else strcat(tmpr, "/");
        strncat(tmpr, lastspr == NULL ? path : lastspr, PATH_MAX - strlen(tmpr));
        char *real = (char*) realloc(tmpr, strlen(tmpr) + 1);
        return real == NULL ? tmpr : real;
    }
    return NULL;
}

char *get_songspath()
{
    char *tmpenv = getenv("OSU_SONG_FOLDER");
    if (tmpenv != NULL)
    {
        char *songse = (char*) malloc(strlen(tmpenv) + 1);
        if (songse != NULL)
        {
            strcpy(songse, tmpenv);
            return songse;
        }
    }

    char *homef = getenv("HOME");
    if (homef != NULL)
    {
        // /home/yes/.cosu_songsfd
        size_t fpathsize = strlen(homef) + sizeof("/.cosu_songsfd");
        char *fpath = (char*) malloc(fpathsize);
        if (fpath != NULL)
        {
            snprintf(fpath, fpathsize, "%s/.cosu_songsfd", homef);
            char *songsf = read_file(fpath, NULL);
            free(fpath);
            if (songsf != NULL)
            {
                remove_newline(songsf);
                return songsf;
            }
        }
    }

    return NULL;
}

int try_convertwinpath(char *path, int pos)
{
    if (access(path, F_OK) == 0)
    {
        return 1;
    }
    bool last = false;
    char *start = path + pos;
    do
    {
        char *end = strchr(start, '/');
        while (end != NULL && *(end + 1) != '\0' && *(end + 1) == '/')
        {
            end++;
        }
        last = end == NULL;
        char *tempend = last ? path + strlen(path) - 1 : end - 1;

        if (!last)
        {
            if (path + strlen(path) - 1 == end) // if / is at the end
            {
                last = true;
                *end = '\0';
                end--;
            }
        }

        bool trim = false;
        while (tempend > start)
        {
            // Windows path shouldn't end with period or space
            if (isspace(*tempend) || *tempend == '.' || *tempend == '/')
            {
                if (!trim) trim = true;
                tempend--;
            }
            else
            {
                break;
            }
        }

        if (trim)
        {
            if (!last)
            {
                for (unsigned int i = 0; i < strlen(end) + 1; i++)
                {
                    char newchr = *(end + i);
                    *(tempend + 1 + i) = newchr;
                }
            }
            else
            {
                *(tempend + 1) = '\0';
            }
        }

        bool match = false;
        if (access(path, F_OK) == 0)
        {
            return 0;
        }
        else // try case insensitive searching through directory
        {
            char backup;
            if (start - 1 > path)
            {
                backup = *(start - 1);
                *(start - 1) = '\0';
            }
            else
            {
                backup = *start;
                *start = '\0';
            }

            DIR *d = opendir(path);
            struct dirent *ent = NULL;
            if (d == NULL)
            {
                perror(path);
                (start - 1 > path) ? (*(start - 1) = backup) : (*start = backup);
                return -1;
            }
            (start - 1 > path) ? (*(start - 1) = backup) : (*start = backup);

            char *deli = trim ? strchr(start, '/') : end;
            if (deli != NULL) *deli = '\0';

            while ((ent = readdir(d)))
            {
                if (strcasecmp(ent->d_name, start) == 0) // may cause a problem with unicode string
                {
                    memcpy(start, ent->d_name, strlen(start));
                    match = true;
                    start = deli + 1;
                    break;
                }
            }

            if (deli != NULL) *deli = '/';
            closedir(d);

            if (!match)
                return -1;
            else if (last)
                return 0;
        }

        if (last)
            return -1;
    }
    while (1);
    return 0;
}

static const char iconpaths[2][35] =
{
    "/usr/share/pixmaps/cosutrainer.png",
    "./cosutrainer.png"
};

char *get_iconpath()
{
    for (int i = 0; i < 2; i++)
    {
        if (access(iconpaths[i], F_OK) == 0)
        {
            char *cpy = (char*) malloc(sizeof(iconpaths[i])); // may allocate larger memory than needed if it's short but whatever
            if (cpy == NULL)
            {
                printerr("Failed allocating!");
                return NULL;
            }
            strcpy(cpy, iconpaths[i]);
            return cpy;
        }
    }

    char *appi = getenv("APPDIR");
    if (appi != NULL)
    {
        const char suffix[] = "/usr/share/pixmaps/cosutrainer.png";
        char *path = (char*) malloc(strlen(appi) + sizeof(suffix));
        if (path == NULL)
        {
            printerr("Failed allocating!");
            return NULL;
        }
        strcpy(path, appi);
        strcat(path, suffix);
        return path;
    }

    return NULL;
}


char *alloc_wcstombs(wchar_t *wide)
{
    char tmp[MB_CUR_MAX];
    int msize = 0;
    int wsize = wcslen(wide) + 1;

    for (int i = 0; i < wsize; i++)
    {
        int s = wctomb(tmp, *(wide + i));
        if (s == -1)
        {
            perror("wctomb");
            return NULL;
        }

        msize += s;
    }

    char *mb = (char*) malloc(msize);
    if (wcstombs(mb, wide, msize) == (size_t) -1)
    {
        free(mb);
        perror("wcstombs");
        return NULL;
    }
    return mb;
}

wchar_t *alloc_mbstowcs(char *multi)
{
    mbstate_t ms;
    memset(&ms, 0, sizeof(ms));

    char *e = multi;
    size_t ret = 0, wlen = 0;
    while ((ret = mbrlen(e++, 1, &ms)) != 0)
    {
        if (ret == (size_t) -1)
        {
            perror("mbrlen");
            return NULL;
        }
        else if (ret != (size_t) -2)
        {
            wlen++;
        }
    }
    wlen++; // null character

    wchar_t *wc = (wchar_t*) malloc(wlen * sizeof(wchar_t));
    if (wc == NULL)
    {
        fprintf(stderr, "Failed allocating!\n");
        return NULL;
    }

    if (mbstowcs(wc, multi, wlen) == (size_t) -1)
    {
        free(wc);
        perror("mbstowcs");
        return NULL;
    }
    return wc;
}
