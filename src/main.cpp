#include "cosuwindow.h"
#include "cuimain.h"
#include <locale.h>

#ifdef WIN32
#define UTF8LOC ".UTF-8"
#else
#define UTF8LOC "C.UTF-8"
#endif

double pitch_override = 0.0;

int actual_main(int argc, char *argv[])
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "--pitch-override") == 0)
        {
            printerr("Pitch is overridden to 0.67x");
            pitch_override = 1.0 / 1.5;
            return actual_main(argc - 1, argv + 1);
        }
        return cuimain(argc, argv);
    }
    CosuWindow cosuw;
    cosuw.start();
    return 0;
}

int main(int argc, char *argv[])
{
    fprintf(stderr, "Current locale: %s\n", setlocale(LC_CTYPE, ""));
#ifdef WIN32
    fprintf(stderr, "New locale: %s\n", setlocale(LC_CTYPE, UTF8LOC));
#endif
    return actual_main(argc, argv);
}
