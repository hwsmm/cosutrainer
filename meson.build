project('cosu-trainer', 'c', 'cpp',
        version : '0.13',
        license : 'GPL-3.0-only')

inc_dir = include_directories('./src')

mainfiles=['src/cosuui.cpp', 'src/cosuwindow.cpp', 'src/cuimain.c', 'src/main.cpp', 'src/tools.c', 'src/mapeditor.c',
           'src/actualzip.c', 'src/audiospeed.cpp', 'src/buffers.c']

deps = [
  dependency('libmpg123'),
  dependency('libzip'),
  dependency('soundtouch'),
  dependency('sndfile'),
  dependency('threads'),
  meson.get_compiler('c').find_library('mp3lame'),
  meson.get_compiler('cpp').find_library('fltk_images'),
  meson.get_compiler('cpp').find_library('fltk')
]

if host_machine.system() != 'windows'

deps += [
  dependency('x11')
]
mainfiles += ['src/freader.cpp', 'src/cosuplatform_linux.c', 'src/lsongpathparser.c']

else

add_project_arguments('-DWIN32', language : 'c')
add_project_arguments('-DWIN32', language : 'cpp')
deps += [
  meson.get_compiler('c').find_library('winpthread'),
  meson.get_compiler('c').find_library('shlwapi')
]
mainfiles += ['src/freader_win.cpp', 'src/sigscan.c', 'src/wsigscan.c', 'src/cosumem.c', 'src/cosuplatform_win.cpp', 'src/winregread.c']

endif

executable('cosu-trainer', mainfiles, dependencies : deps, include_directories : inc_dir, install : true, win_subsystem : 'windows')

if host_machine.system() != 'windows'

memfiles=['src/tools.c', 'src/sigscan.c', 'src/lsigscan.c', 'src/cosumem.c', 'src/cosuplatform_linux.c', 'src/winregread.c']
cleanfiles=['src/tools.c', 'src/cleanup.c', 'src/cosuplatform_linux.c']

executable('osumem', memfiles, include_directories : inc_dir, install : true)
executable('cosu-cleanup', cleanfiles, include_directories : inc_dir, install : true)
install_data('docs/cosu-trainer.desktop', install_dir : 'share/applications', install_mode : 'rwxr-xr-x')
install_data('docs/cosutrainer.png', install_dir : 'share/pixmaps')

endif
