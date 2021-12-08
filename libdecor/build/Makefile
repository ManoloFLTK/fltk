#
# Library Makefile for the Fast Light Tool Kit (FLTK).
#
# Copyright 2021 by Bill Spitzak and others.
#
# This library is free software. Distribution and use rights are outlined in
# the file "COPYING" which should have been included with this file.  If this
# file is missing or damaged, see the license at:
#
#      https://www.fltk.org/COPYING.php
#
# Please see the following page on how to report bugs and issues:
#
#      https://www.fltk.org/bugs.php
#

include ../../makeinclude

CFLAGS = -I. -I../.. -I../../src -I../src -fPIC -D_GNU_SOURCE -DUSE_SYSTEM_LIBDECOR=0
OBJECTS =  fl_libdecor.o libdecor-cairo-blur.o fl_libdecor-cairo.o libdecor-fallback.o \
  ../../src/xdg-decoration-protocol.o ../../src/xdg-shell-protocol.o cursor-settings.o
PROTOCOLS = /usr/share/wayland-protocols

all : demo egl

depend:
	: echo "libdecor/build: make depend..."

fl_libdecor.o : fl_libdecor.c ../src/libdecor.c ../../src/xdg-shell-protocol.c ../../src/xdg-decoration-protocol.c
	$(CC) $(CFLAGS) -c  fl_libdecor.c -DLIBDECOR_PLUGIN_API_VERSION=1 -DLIBDECOR_PLUGIN_DIR=\"Unused\"

libdecor-fallback.o : ../src/libdecor-fallback.c
	$(CC) $(CFLAGS) -c  ../src/libdecor-fallback.c

fl_libdecor-cairo.o : fl_libdecor-cairo.c ../src/plugins/cairo/libdecor-cairo.c
	$(CC) $(CFLAGS) -c  fl_libdecor-cairo.c  -DLIBDECOR_PLUGIN_API_VERSION=1 `pkg-config --cflags pangocairo`

libdecor-cairo-blur.o : ../src/plugins/cairo/libdecor-cairo-blur.c
	$(CC) $(CFLAGS) -c  ../src/plugins/cairo/libdecor-cairo-blur.c

cursor-settings.o : ../src/cursor-settings.c
	$(CC) $(CFLAGS)  -c ../src/cursor-settings.c -DHAS_DBUS `pkg-config --cflags dbus-1`

../../src/xdg-shell-protocol.c :
	wayland-scanner private-code $(PROTOCOLS)/stable/xdg-shell/xdg-shell.xml \
	    ../../src/xdg-shell-protocol.c
	wayland-scanner client-header $(PROTOCOLS)/stable/xdg-shell/xdg-shell.xml \
	    ../../src/xdg-shell-client-protocol.h

../../src/xdg-decoration-protocol.c :
	wayland-scanner private-code \
	    $(PROTOCOLS)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
	    ../../src/xdg-decoration-protocol.c
	wayland-scanner client-header \
	    $(PROTOCOLS)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
	    ../../src/xdg-decoration-client-protocol.h

	
demo : ../demo/demo.c $(OBJECTS)
	$(CC)  -o demo ../demo/demo.c -D_GNU_SOURCE -I../.. -I../src -I. -I../../src $(OBJECTS)  -lwayland-cursor -lwayland-client -lxkbcommon `pkg-config --libs pangocairo` -ldl -ldbus-1 -lm -no-pie -Wl,--defsym=fl_libdecor_using_weston=0

egl : ../demo/egl.c $(OBJECTS)
	$(CC)  -o egl ../demo/egl.c -D_GNU_SOURCE -I../.. -I../src -I. -I../../src $(OBJECTS) -lEGL -lGLU -lOpenGL -lwayland-egl  -lwayland-cursor -lwayland-client `pkg-config --libs pangocairo` -ldbus-1 -ldl -lm -no-pie -Wl,--defsym=fl_libdecor_using_weston=0


install:
	echo "Nothing to install"

uninstall:

clean:
	$(RM) *.o ../../src/xdg-*.c ../../src/xdg-*.h ../../src/xdg-*.o demo egl
