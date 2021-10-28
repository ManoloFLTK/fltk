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

CFLAGS = -I. -I../src -fPIC -D_GNU_SOURCE
OBJECTS =  fl_libdecor.o libdecor-cairo-blur.o fl_libdecor-cairo.o libdecor-fallback.o \
  xdg-decoration-protocol.o xdg-shell-protocol.o cursor-settings.o
PROTOCOLS = /usr/share/wayland-protocols

all : demo egl $(DSONAME_DECOR)

depend:
	: echo "libdecor/build: make depend..."

config.h :
	touch config.h

fl_libdecor.o : fl_libdecor.c ../src/libdecor.c xdg-shell-protocol.c xdg-decoration-protocol.c config.h
	$(CC) $(CFLAGS) -c  fl_libdecor.c -DLIBDECOR_PLUGIN_API_VERSION=1 -DLIBDECOR_PLUGIN_DIR=\"Unused\"

libdecor-fallback.o : ../src/libdecor-fallback.c
	$(CC) $(CFLAGS) -c  ../src/libdecor-fallback.c

fl_libdecor-cairo.o : fl_libdecor-cairo.c ../src/plugins/cairo/libdecor-cairo.c
	$(CC) $(CFLAGS) -c  fl_libdecor-cairo.c  -DLIBDECOR_PLUGIN_API_VERSION=1 `pkg-config --cflags pangocairo`

libdecor-cairo-blur.o : ../src/plugins/cairo/libdecor-cairo-blur.c
	$(CC) $(CFLAGS) -c  ../src/plugins/cairo/libdecor-cairo-blur.c

cursor-settings.o : ../src/cursor-settings.c
	$(CC) $(CFLAGS)  -c ../src/cursor-settings.c -DHAS_DBUS `pkg-config --cflags dbus-1`

xdg-shell-protocol.c :
	wayland-scanner private-code < $(PROTOCOLS)/stable/xdg-shell/xdg-shell.xml > \
	    xdg-shell-protocol.c
	wayland-scanner client-header < $(PROTOCOLS)/stable/xdg-shell/xdg-shell.xml > \
	    xdg-shell-client-protocol.h

xdg-decoration-protocol.c :
	wayland-scanner private-code < \
	    $(PROTOCOLS)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml > \
	    xdg-decoration-protocol.c
	wayland-scanner client-header < \
	    $(PROTOCOLS)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml > \
	    xdg-decoration-client-protocol.h

xdg-decoration-protocol.o : xdg-decoration-protocol.c
	$(CC) $(CFLAGS) -c xdg-decoration-protocol.c

xdg-shell-protocol.o : xdg-shell-protocol.c
	$(CC) $(CFLAGS) -c xdg-shell-protocol.c


../../lib/libfltk_decor.a : $(OBJECTS)
	echo Creating libfltk_decor.a
	$(RM) $@
	$(LIBCOMMAND) $@ $(OBJECTS)
	$(RANLIB) $@

libfltk_decor.so.$(FL_DSO_VERSION) : $(OBJECTS)
	$(CC) -shared -o $@ -Wl,-soname,$@ $(OBJECTS)
	$(RM) libfltk_decor.so
	$(LN) -s  $@ libfltk_decor.so

	
demo : ../../lib/libfltk_decor.a ../demo/demo.c
	$(CC)  -o demo ../demo/demo.c -D_GNU_SOURCE -I../src -I. -L../../lib -lfltk_decor  -lwayland-cursor -lwayland-client -lxkbcommon `pkg-config --libs pangocairo` -ldl -ldbus-1 -lm -no-pie -Wl,--defsym=fl_libdecor_using_weston=0

egl : ../../lib/libfltk_decor.a ../demo/egl.c
	$(CC)  -o egl ../demo/egl.c -D_GNU_SOURCE -I../src -I. -L../../lib -lfltk_decor -lEGL -lGLU -lOpenGL -lwayland-egl  -lwayland-cursor -lwayland-client `pkg-config --libs pangocairo` -ldbus-1 -ldl -lm -no-pie -Wl,--defsym=fl_libdecor_using_weston=0


install : libfltk_decor.so.$(FL_DSO_VERSION)
	echo "Installing libfltk_decor in $(DESTDIR)$(libdir)..."
	-$(INSTALL_DIR) $(DESTDIR)$(libdir)
	$(RM) $(DESTDIR)$(libdir)/libfltk_decor.so
	$(INSTALL_LIB) libfltk_decor.so.$(FL_DSO_VERSION) $(DESTDIR)$(libdir)
	$(LN) -s $(DESTDIR)$(libdir)/libfltk_decor.so.$(FL_DSO_VERSION) $(DESTDIR)$(libdir)/libfltk_decor.so

uninstall:
	echo "Uninstalling libraries..."
	$(RM) $(DESTDIR)$(libdir)/libfltk_decor.so*


clean:
	$(RM) *.o xdg-*.c xdg-*.h config.h demo egl ../../lib/libfltk_decor.a libfltk_decor.so*
