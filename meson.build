project('cosu-trainer', 'c')

mainfiles=files('src/audiospeed.c', 'src/emptyzip.c', 'src/mapspeed.c', 'src/main.c')
memfiles=files('src/sigscan.c')
cleanfiles=files('src/cleanup.c')

executable('cosu-trainer', mainfiles)
executable('osumem', memfiles)
executable('cleanup', cleanfiles)
