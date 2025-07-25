#define _GNU_SOURCE
#include "winregread.h"
#include <stdio.h>

#ifdef WIN32

#include <shlwapi.h>
#include <lmcons.h>
#include <strsafe.h>
#include <wchar.h>

static LPWSTR getRegistryValue(HKEY hkey, LPCWSTR subKey, LPDWORD len)
{
    DWORD size = 0;
    LSTATUS ret = RegGetValueW(hkey, subKey, L"", RRF_RT_REG_SZ, NULL, NULL, &size);
    if (ret != ERROR_SUCCESS)
    {
        fprintf(stderr, "Failed reading registry: %ld\n", ret);
        return NULL;
    }

    LPWSTR path = (LPWSTR) calloc(size, sizeof(WCHAR));
    if (path == NULL)
    {
        fprintf(stderr, "Failed allocation\n");
        return NULL;
    }

    ret = RegGetValueW(hkey, subKey, L"", RRF_RT_REG_SZ, NULL, path, &size);
    if (ret != ERROR_SUCCESS)
    {
        fprintf(stderr, "Failed reading registry: %ld\n", ret);
        free(path);
        return NULL;
    }

    *len = size;
    return path;
}

LPWSTR getOsuPath()
{
    DWORD size = 0;
    LPWSTR path = NULL;

    if (path == NULL) path = getRegistryValue(HKEY_CURRENT_USER, L"Software\\Classes\\osustable.File.osz\\Shell\\Open\\Command", &size);
    if (path == NULL) path = getRegistryValue(HKEY_CURRENT_USER, L"Software\\Classes\\osu\\Shell\\Open\\Command", &size);
    if (path == NULL) path = getRegistryValue(HKEY_CLASSES_ROOT, L"osustable.File.osz\\Shell\\Open\\Command", &size);
    if (path == NULL) path = getRegistryValue(HKEY_CLASSES_ROOT, L"osu\\Shell\\Open\\Command", &size);;

    if (path == NULL)
    {
        fprintf(stderr, "Failed getting path\n");
        return NULL;
    }

    // https://github.com/ppy/osu/blob/master/osu.Desktop/OsuGameDesktop.cs#L87
    PWSTR find = StrRChrW(path, NULL, L' ');
    if (find != NULL) *find = '\0';

    find = StrRChrW(path, NULL, L'"');
    if (find != NULL) *find = '\0';

    PWSTR start = StrChrW(path, L'"');
    if (start == NULL) start = path;
    else start++;

    find = StrStrIW(path, L"osu!.exe");
    if (find == NULL)
    {
        fprintf(stderr, "Registry value is not proper!");
        free(path);
        return NULL;
    }
    *find = '\0';

    DWORD i;
    for (i = 0; i < size; i++)
    {
        *(path + i) = *(start + i);
        if (*(start + i) == '\0')
        {
            break;
        }
    }

    LPWSTR tmp = (LPWSTR) realloc(path, ++i * sizeof(WCHAR));
    if (tmp == NULL)
    {
        fprintf(stderr, "Failed reallocation\n");
        free(path);
        return NULL;
    }

    return tmp;
}

LPWSTR getOsuSongsPath(LPWSTR osupath)
{
    DWORD pathsize = lstrlenW(osupath);
    WCHAR uname[UNLEN + 1];
    DWORD size = UNLEN + 1;
    if (GetUserNameW(uname, &size) == 0)
    {
        fprintf(stderr, "Failed getting user name: %ld", GetLastError());
        return NULL;
    }

    size = pathsize + 1 + (sizeof("osu!.") - 1) + (size - 1) + (sizeof(".cfg") - 1) + 1;
    wchar_t *cfgpath = (wchar_t*) calloc(size, sizeof(wchar_t));
    if (cfgpath == NULL)
    {
        fprintf(stderr, "Failed allocation!\n");
        return NULL;
    }

    StringCchPrintfW(cfgpath, size, L"%ls\\osu!.%ls.cfg", osupath, uname);

    FILE *fp = _wfopen(cfgpath, L"rt, ccs=UTF-8");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed opening %ls\n", cfgpath);
        free(cfgpath);
        return NULL;
    }
    free(cfgpath);

    wchar_t line[4096];
    wchar_t find[] = L"BeatmapDirectory = ";
    wchar_t *path = NULL;
    while (fgetws(line, 4096, fp) != NULL)
    {
        if (wcsncmp(line, find, sizeof(find)/sizeof(find[0]) - 1) == 0)
        {
            wchar_t *f = wcsrchr(line, L'\n');
            if (f != NULL) *f = '\0';

            path = line + sizeof(find)/sizeof(find[0]) - 1;
            break;
        }
    }

    fclose(fp);

    if (path == NULL)
    {
        fprintf(stderr, "BeatmapDirectory not found in config!\n");
        return NULL;
    }

    size_t psize = 0;
    if (StringCchLengthW(path, STRSAFE_MAX_CCH, &psize) != S_OK)
    {
        fprintf(stderr, "Error calculating string length\n");
        return NULL;
    }

    LPWSTR res = NULL;
    DWORD ressize = 0;
    if (PathIsRelativeW(path))
    {
        ressize = (pathsize - 1) + 1 + psize + 1;
        res = (LPWSTR) calloc(ressize, sizeof(WCHAR));
        if (res == NULL)
        {
            fprintf(stderr, "Failed allocation!\n");
            return NULL;
        }
        StringCchCopyW(res, pathsize, osupath);
        StringCchCatW(res, pathsize + 1, L"\\");
        StringCchCatW(res, pathsize + psize + 1, path);
    }
    else
    {
        ressize = psize + 1;
        res = (LPWSTR) calloc(ressize, sizeof(WCHAR));
        if (res == NULL)
        {
            fprintf(stderr, "Failed allocation!\n");
            return NULL;
        }
        StringCchCopyW(res, ressize, path);
    }
    return res;
}

#else

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include "cosuplatform.h"
#include "tools.h"

static char *try_get_osu_path(char *wineprefix, char *reg_file, const char **subkeys)
{
    int size = strlen(wineprefix) + 1 + strlen(reg_file) + 1;
    char *regpath = (char*) malloc(size);
    if (regpath == NULL)
    {
        printerr("Failed allocating memory!");
        return NULL;
    }

    snprintf(regpath, size, "%s/%s", wineprefix, reg_file);

    FILE *f = fopen(regpath, "r");
    if (!f)
    {
        perror(regpath);
        free(regpath);
        return NULL;
    }

    free(regpath);

    char line[4096];
    char *result = NULL;

    while (fgets(line, sizeof(line), f))
    {
        for (const char **temp = subkeys; *temp != NULL; temp++)
        {
            const char *subkey = *temp;
            if (strcasestr(line, subkey) != NULL)
            {
                while (fgets(line, sizeof(line), f))
                {
                    char *find = NULL;
                    if ((find = strstr(line, "osu!.exe")) != NULL)
                    {
                        *find = '\0';

                        char *first = strstr(line, ":\\\\");

                        if (first != NULL)
                            first--;
                        else
                            goto exit;

                        int len = strlen(first) + 1;
                        result = (char*) malloc(len);
                        if (result != NULL)
                            strcpy(result, first);

                        goto exit;
                    }
                }
            }
        }
    }

exit:
    fclose(f);
    return result;
}

char *get_osu_path(char *wineprefix)
{
    static const char *list[] =
    {
        "osu\\\\shell\\\\open\\\\command",
        "osustable.File.osz\\\\shell\\\\open\\\\command",
        NULL
    };

    char *res = try_get_osu_path(wineprefix, "system.reg", list);
    if (res == NULL)
        res = try_get_osu_path(wineprefix, "user.reg", list);

    if (res == NULL)
        printerr("Couldn't find song folder!");

    return res;
}

char *get_osu_songs_path(char *wineprefix, char *uid)
{
    FILE *pw = fopen("/etc/passwd", "r");
    if (!pw)
    {
        perror("/etc/passwd");
        return NULL;
    }

    char id[4096];
    bool uidfound = false;
    bool prefixnull = wineprefix == NULL;

    while (fgets(id, sizeof(id), pw))
    {
        /*char *lid =*/ strtok(id, ":");
        /*char *lpw =*/ strtok(NULL, ":");
        char *luid = strtok(NULL, ":");
        if (luid == NULL)
        {
            fclose(pw);
            printerr("Failed parsing /etc/passwd");
            return NULL;
        }

        if (strcmp(uid, luid) == 0)
        {
            uidfound = true;

            if (prefixnull)
            {
                /*char *lgid =*/ strtok(NULL, ":");
                /*char *lgecos =*/ strtok(NULL, ":");
                char *ldir = strtok(NULL, ":");
                if (ldir == NULL)
                {
                    fclose(pw);
                    printerr("Failed parsing /etc/passwd");
                    return NULL;
                }

                const char winedefp[] = ".wine";

                int defplen = strlen(ldir) + 1 + sizeof(winedefp);
                wineprefix = (char*) malloc(defplen);

                if (!wineprefix)
                {
                    fclose(pw);
                    printerr("Failed allocation");
                    return NULL;
                }
                else
                {
                    snprintf(wineprefix, defplen, "%s/%s", ldir, winedefp);
                }
            }

            break;
        }
    }

    fclose(pw);

    if (!uidfound)
    {
        printerr("Corresponding UID is not found");
        return NULL;
    }

    char *result = NULL;

    int prefixlen = strlen(wineprefix);

    char *osu = get_osu_path(wineprefix);
    if (osu == NULL)
        return NULL;

    int osulen = strlen(osu);

    for (int i = 0; i < osulen; i++)
    {
        if (*(osu + i) == '\\')
            *(osu + i) = '/';
    }

    *osu = tolower(*osu); // device letter

    const char dosd[] = "dosdevices";
    const char prefix[] = "osu!.";
    const char suffix[] = ".cfg";

    int idlen = strlen(id);
    int cfglen = prefixlen + 1 + (sizeof(dosd)-1) + 1 + osulen + 1 + (sizeof(prefix)-1) + idlen + (sizeof(suffix)-1) + 1;
    char *cfgpath = (char*) malloc(cfglen);
    if (cfgpath == NULL)
        return NULL;

    snprintf(cfgpath, cfglen, "%s/%s/%s/%s%s%s", wineprefix, dosd, osu, prefix, id, suffix);

    if (try_convertwinpath(cfgpath, prefixlen + 1 + (sizeof(dosd)-1) + 1) < 0)
    {
        fprintf(stderr, "Tried config path but failed: %s\n", cfgpath);
        goto freeq;
    }

    FILE* cfg = fopen(cfgpath, "r");
    if (!cfg)
    {
        perror(cfgpath);
        goto freeq;
    }

    char line[4096];
    char find[] = "BeatmapDirectory = ";
    char *found = NULL;
    while (fgets(line, sizeof(line), cfg))
    {
        if (strncmp(line, find, sizeof(find) - 1) == 0)
        {
            found = line + sizeof(find) - 1;
            found = trim(found, NULL);
            break;
        }
    }

    fclose(cfg);

    if (found == NULL)
    {
        printerr("BeatmapDirectory not found in osu config!");
        goto freeq;
    }

    int foundlen = strlen(found);
    char *sep = strchr(found, ':');
    bool abs = sep != NULL && sep - found == 1;

    for (int i = 0; i < foundlen; i++)
    {
        if (*(found + i) == '\\')
            *(found + i) = '/';
    }

    if (abs)
    {
        *found = tolower(*found);

        int reslen = prefixlen + 1 + (sizeof(dosd)-1) + 1 + foundlen + 1;
        result = (char*) malloc(reslen);
        if (result == NULL)
        {
            printerr("Failed allocation!");
            goto freeq;
        }

        snprintf(result, reslen, "%s/%s/%s", wineprefix, dosd, found);
    }
    else
    {
        int reslen = prefixlen + 1 + (sizeof(dosd)-1) + 1 + osulen + 1 + foundlen + 1;
        result = (char*) malloc(reslen);
        if (result == NULL)
        {
            printerr("Failed allocation!");
            goto freeq;
        }

        snprintf(result, reslen, "%s/%s/%s/%s", wineprefix, dosd, osu, found);
    }

    if (result != NULL && try_convertwinpath(result, prefixlen + 1 + (sizeof(dosd)-1) + 1) < 0)
    {
        fprintf(stderr, "Tried final path but failed: %s\n", result);
        free(result);
        result = NULL;
    }

    char *real = realpath(result, NULL);
    if (real != NULL)
    {
        free(result);
        result = real;
    }

freeq:
    if (prefixnull)
        free(wineprefix);

    free(osu);
    free(cfgpath);
    return result;
}

#endif
