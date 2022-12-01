#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef DISABLE_GUI
int main(int argc, char *argv[]);
#else
int cuimain(int argc, char *argv[]);
#endif

#ifdef __cplusplus
}
#endif
