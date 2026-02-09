project('cosu-trainer', 'c', 'cpp',
        version : '0.16',
        license : 'GPL-3.0-only')

inc_dir = include_directories('./src', './docs')

mainfiles = ['src/cosuui.cpp', 'src/cosuwindow.cpp', 'src/cuimain.c', 'src/main.cpp', 'src/mapeditor.c',
             'src/actualzip.c', 'src/audiospeed.cpp', 'src/buffers.c', 'src/tools.c']

deps = [
    dependency('libmpg123'),
    dependency('minizip'),
    dependency('soundtouch'),
    dependency('sndfile'),
    dependency('threads'),
    meson.get_compiler('cpp').find_library('fltk'),
    meson.get_compiler('c').find_library('mp3lame')
]

if host_machine.system() != 'windows'
    deps += dependency('x11')
    mainfiles += ['src/freader.cpp', 'src/lsongpathparser.c', 'src/cosuplatform_linux.c']
else
    add_project_arguments('-DWIN32', language : ['c', 'cpp'])
    deps += [
        meson.get_compiler('c').find_library('winpthread'),
        meson.get_compiler('c').find_library('shlwapi')
    ]

    windows = import('windows')
    res = windows.compile_resources('docs/app.rc', depend_files : ['docs/icon.ico'], include_directories : ['docs/'])

    mainfiles += ['src/freader_win.cpp', 'src/sigscan.c', 'src/wsigscan.c', 'src/cosumem.c', 'src/cosuplatform_win.cpp', 'src/winregread.c', 'src/cosuplatform_win.cpp', res]
endif

executable('cosu-trainer', mainfiles, dependencies : deps, include_directories : inc_dir, install : true, win_subsystem : 'windows')

if host_machine.system() != 'windows'
    memfiles = ['src/sigscan.c', 'src/lsigscan.c', 'src/cosumem.c', 'src/winregread.c', 'src/cosuplatform_linux.c', 'src/tools.c']
    cleanfiles = ['src/cleanup.c', 'src/cosuplatform_linux.c', 'src/tools.c']

    executable('osumem', memfiles, include_directories : inc_dir, install : true)
    executable('cosu-cleanup', cleanfiles, include_directories : inc_dir, install : true)
    install_data('docs/cosu-trainer.desktop', install_dir : 'share/applications', install_mode : 'rwxr-xr-x')
    install_data('docs/cosutrainer.png', install_dir : 'share/pixmaps')
endif
