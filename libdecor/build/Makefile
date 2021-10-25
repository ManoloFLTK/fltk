CC = gcc

CFLAGS = -I. -I../src -fPIC -D_GNU_SOURCE

all : demo egl

depend:
	: echo "libdecor/build: make depend..."

config.h :
	touch config.h

fl_libdecor.o : fl_libdecor.c ../src/libdecor.c xdg-shell-protocol.c xdg-decoration-protocol.c config.h
	$(CC) $(CFLAGS) -c  fl_libdecor.c -DLIBDECOR_PLUGIN_API_VERSION=1 -DLIBDECOR_PLUGIN_DIR=\"Unused\"

libdecor-fallback.o : ../src/libdecor-fallback.c
	$(CC) $(CFLAGS) -c  ../src/libdecor-fallback.c

fl_libdecor-cairo.o : fl_libdecor-cairo.c ../src/plugins/cairo/libdecor-cairo.c
	$(CC) $(CFLAGS) -c  fl_libdecor-cairo.c -D_GNU_SOURCE -DLIBDECOR_PLUGIN_API_VERSION=1 `pkg-config --cflags pangocairo`

libdecor-cairo-blur.o : ../src/plugins/cairo/libdecor-cairo-blur.c
	$(CC) $(CFLAGS) -c  ../src/plugins/cairo/libdecor-cairo-blur.c -D_GNU_SOURCE

cursor-settings.o : ../src/cursor-settings.c
	$(CC) $(CFLAGS)  -c ../src/cursor-settings.c -DHAS_DBUS `pkg-config --cflags dbus-1`

xdg-shell-protocol.c :
	wayland-scanner private-code < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml  > xdg-shell-protocol.c
	wayland-scanner client-header < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml > \
	    xdg-shell-client-protocol.h

xdg-decoration-protocol.c :
	wayland-scanner private-code < /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
	    > xdg-decoration-protocol.c
	wayland-scanner client-header < /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
	    > xdg-decoration-client-protocol.h

xdg-decoration-protocol.o : xdg-decoration-protocol.c
	$(CC) $(CFLAGS) -c xdg-decoration-protocol.c

xdg-shell-protocol.o : xdg-shell-protocol.c
	$(CC) $(CFLAGS) -c xdg-shell-protocol.c


../../lib/libfltk_decor.a : fl_libdecor.o libdecor-cairo-blur.o fl_libdecor-cairo.o libdecor-fallback.o xdg-decoration-protocol.o \
		xdg-shell-protocol.o cursor-settings.o
	ar rv ../../lib/libfltk_decor.a  fl_libdecor.o  libdecor-cairo-blur.o fl_libdecor-cairo.o libdecor-fallback.o xdg-decoration-protocol.o \
		xdg-shell-protocol.o cursor-settings.o
	
demo : ../../lib/libfltk_decor.a ../demo/demo.c
	$(CC)  -o demo ../demo/demo.c -D_GNU_SOURCE -I../src -I. -L../../lib -lfltk_decor  -lwayland-cursor -lwayland-client -lxkbcommon `pkg-config --libs pangocairo` -ldl -ldbus-1 -lm -no-pie -Wl,--defsym=fl_libdecor_using_weston=0

egl : ../../lib/libfltk_decor.a ../demo/egl.c
	$(CC)  -o egl ../demo/egl.c -D_GNU_SOURCE -I../src -I. -L../../lib -lfltk_decor -lEGL -lGLU -lOpenGL -lwayland-egl  -lwayland-cursor -lwayland-client `pkg-config --libs pangocairo` -ldbus-1 -ldl -lm -no-pie -Wl,--defsym=fl_libdecor_using_weston=0

clean:
	rm *.o xdg-*.c xdg-*.h config.h demo egl ../../lib/libfltk_decor.a
