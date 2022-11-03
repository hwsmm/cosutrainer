project('cosu-trainer', 'c', 'cpp')

mainfiles=['src/soundtouch-c.cpp', 'src/tools.c', 'src/audiospeed.c', 'src/mapspeed.c', 'src/actualzip.c', 'src/emptyzip.c', 'src/buffers.c', 'src/main.c']
memfiles=['src/tools.c', 'src/sigscan.c']
cleanfiles=['src/tools.c', 'src/cleanup.c']

inc_dir = include_directories('./src')

deps = [
  dependency('libmpg123'),
  dependency('minizip'),
  dependency('soundtouch'),
  dependency('sndfile'),
  meson.get_compiler('c').find_library('mp3lame')
]

executable('cosu-trainer', mainfiles, dependencies : deps, include_directories: inc_dir)
executable('osumem', memfiles, include_directories: inc_dir)
executable('cosu-cleanup', cleanfiles, include_directories: inc_dir)
