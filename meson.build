project('cosu-trainer', 'c', 'cpp',
        version : '0.10',
        license : 'GPL-3.0-only')

mainfiles=['src/cosuui.cpp', 'src/cosuwindow.cpp', 'src/freader.cpp', 'src/cuimain.c', 'src/main.cpp', 'src/cosuplatform_linux.c',
           'src/tools.c', 'src/mapeditor.c', 'src/actualzip.c', 'src/audiospeed.cpp', 'src/buffers.c']
memfiles=['src/tools.c', 'src/lsigscan.c', 'src/cosumem.c', 'src/cosuplatform_linux.c', 'src/winregread.c']
cleanfiles=['src/tools.c', 'src/cleanup.c', 'src/cosuplatform_linux.c']

inc_dir = include_directories('./src')

deps = [
  dependency('libmpg123'),
  dependency('libzip'),
  dependency('soundtouch'),
  dependency('sndfile'),
  dependency('threads'),
  dependency('x11'),
  meson.get_compiler('c').find_library('mp3lame'),
  meson.get_compiler('cpp').find_library('fltk_images'),
  meson.get_compiler('cpp').find_library('fltk')
]

executable('cosu-trainer', mainfiles, dependencies : deps, include_directories : inc_dir, install : true)
executable('osumem', memfiles, include_directories : inc_dir, install : true)
executable('cosu-cleanup', cleanfiles, include_directories : inc_dir, install : true)
install_data('docs/cosu-trainer.desktop', install_dir : 'share/applications', install_mode : 'rwxr-xr-x')
install_data('docs/cosutrainer.png', install_dir : 'share/pixmaps')
