#!/bin/bash
ROOT_DIR=$(realpath $(dirname ${BASH_SOURCE[0]}))

if ! test -d "$DESTDIR"; then
mkdir -p $PWD/bin
DESTDIR=$PWD/bin
fi

ORIGDIR=$PWD
DESTDIR=$(realpath $DESTDIR)

cd $ROOT_DIR/src
gcc $CFLAGS soundtouch-c.cpp tools.c audiospeed.c mapspeed.c emptyzip.c main.c -o $DESTDIR/cosu-trainer -I/usr/include/soundtouch -lmpg123 -lmp3lame -lstdc++ -lSoundTouch -lsndfile -g
gcc $CFLAGS tools.c sigscan.c -o $DESTDIR/osumem
gcc $CFLAGS tools.c cleanup.c -o $DESTDIR/cosu-cleanup

cd $DESTDIR
strip -s cosu-trainer osumem cosu-cleanup
cd $ORIGDIR
