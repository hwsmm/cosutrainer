project('cosu-trainer', 'c')

mainfiles=files('src/tools.c', src/audiospeed.c', 'src/emptyzip.c', 'src/mapspeed.c', 'src/main.c')
memfiles=files('src/tools.c', 'src/sigscan.c')
cleanfiles=files('src/tools.c', 'src/cleanup.c')

executable('cosu-trainer', mainfiles)
executable('osumem', memfiles)
executable('cleanup', cleanfiles)
