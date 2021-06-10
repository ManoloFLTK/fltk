CC = gcc
BUILD = `readlink -f ../../lib`

CFLAGS = -I. -I../src -fPIC -D_GNU_SOURCE

all : demo 

depend:
	: echo "libdecor/build: make depend..."


config.h :
	touch config.h

libdecor.o : ../src/libdecor.c xdg-shell-protocol.c xdg-decoration-protocol.c config.h
	$(CC) $(CFLAGS) -c  ../src/libdecor.c -DLIBDECOR_PLUGIN_API_VERSION=1 -DLIBDECOR_PLUGIN_DIR=\"$(BUILD)\" 

libdecor-fallback.o : ../src/libdecor-fallback.c
	$(CC) $(CFLAGS) -c  ../src/libdecor-fallback.c

libdecor-cairo.o : ../src/plugins/cairo/libdecor-cairo.c
	$(CC) $(CFLAGS) -c  ../src/plugins/cairo/libdecor-cairo.c -D_GNU_SOURCE -DLIBDECOR_PLUGIN_API_VERSION=1 `pkg-config --cflags pangocairo`


libdecor-cairo-blur.o : ../src/plugins/cairo/libdecor-cairo-blur.c
	$(CC) $(CFLAGS) -c  ../src/plugins/cairo/libdecor-cairo-blur.c -D_GNU_SOURCE

cursor-settings.o : ../src/cursor-settings.c
	$(CC) $(CFLAGS)  -c ../src/cursor-settings.c -DHAS_DBUS -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include 

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
	
#this shared lib is used by Wayland client programs and also as a plugin (for the Wayland composer ?)
../../lib/libdecor-0.1.so : libdecor.o libdecor-cairo-blur.o libdecor-cairo.o libdecor-fallback.o xdg-decoration-protocol.o \
		xdg-shell-protocol.o cursor-settings.o
	$(CC) -shared  libdecor.o libdecor-cairo-blur.o libdecor-cairo.o libdecor-fallback.o xdg-decoration-protocol.o \
		xdg-shell-protocol.o cursor-settings.o -o ../../lib/libdecor-0.1.so -lcairo

demo : ../../lib/libdecor-0.1.so ../demo/demo.c  
	$(CC)  -o demo ../demo/demo.c  xdg-decoration-protocol.o xdg-shell-protocol.o -D_GNU_SOURCE -I../src -I. -L../../lib -ldecor-0.1  -lwayland-cursor -lwayland-client -lxkbcommon -lpangocairo-1.0 -ldl -ldbus-1 -Wl,-rpath,$(BUILD) -no-pie

clean:
	rm *.o ../../lib/libdecor-0.1.so
