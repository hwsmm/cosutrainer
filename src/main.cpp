#include "cosuwindow.h"
#include "cuimain.h"

int main(int argc, char *argv[])
{
   if (argc > 1)
   {
      return cuimain(argc, argv);
   }
   CosuWindow cosuw;
   cosuw.start();
   return 0;
}
