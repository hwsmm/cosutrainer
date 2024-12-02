#include "cosuwindow.h"
#include "cuimain.h"
#include <locale.h>

#ifdef WIN32
#define UTF8LOC ".UTF-8"
#else
#define UTF8LOC "C.UTF-8"
#endif

int main(int argc, char *argv[])
{
    fprintf(stderr, "Current locale: %s\n", setlocale(LC_CTYPE, ""));
#ifdef WIN32
    fprintf(stderr, "New locale: %s\n", setlocale(LC_CTYPE, UTF8LOC));
#endif
    if (argc > 1)
    {
        return cuimain(argc, argv);
    }
    CosuWindow cosuw;
    cosuw.start();
    return 0;
}
