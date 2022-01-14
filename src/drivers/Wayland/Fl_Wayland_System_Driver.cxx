//
// Definition of Wayland system driver
// for the Fast Light Tool Kit (FLTK).
//
// Copyright 2010-2021 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//

#include "Fl_Wayland_System_Driver.H"
#include <FL/Fl_File_Browser.H>
#include <FL/fl_string.h>  // fl_strdup
#include <FL/platform.H>
#include "../../flstring.h"
#include "Fl_Wayland_Screen_Driver.H"
#include "Fl_Wayland_Window_Driver.H"
#include "xdg-shell-client-protocol.h"

#include <locale.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>     // strerror(errno)
#include <errno.h>      // errno
#include <dlfcn.h>   // for dlsym


/**
 Creates a driver that manages all system related calls.

 This function must be implemented once for every platform.
 */
Fl_System_Driver *Fl_System_Driver::newSystemDriver()
{
  return new Fl_Wayland_System_Driver();
}


void Fl_Wayland_System_Driver::make_transient(void *ptr_gtk, void *gtk_window, Fl_Window *win) {
  //TODO
  typedef struct _GdkDrawable GdkWindow;
  typedef struct _GtkWidget GtkWidget;

  typedef GdkWindow* (*XX_gtk_widget_get_window_type)(GtkWidget *);
  static XX_gtk_widget_get_window_type fl_gtk_widget_get_window = NULL;

  typedef struct wl_surface *(*XX_gdk_wayland_window_get_wl_surface_type)(GdkWindow *);
  static XX_gdk_wayland_window_get_wl_surface_type fl_gdk_wayland_window_get_wl_surface = NULL;

  if (!fl_gtk_widget_get_window) {
    fl_gtk_widget_get_window = (XX_gtk_widget_get_window_type)dlsym(ptr_gtk, "gtk_widget_get_window");
    if (!fl_gtk_widget_get_window) return;
  }

  GdkWindow* gdkw = fl_gtk_widget_get_window((GtkWidget*)gtk_window);

  if (!fl_gdk_wayland_window_get_wl_surface) {
    fl_gdk_wayland_window_get_wl_surface = (XX_gdk_wayland_window_get_wl_surface_type)dlsym(ptr_gtk, "gdk_wayland_window_get_wl_surface");
  }
  struct wl_surface *wld_surf = fl_gdk_wayland_window_get_wl_surface(gdkw);
  // but what next?
/* not good:
Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
struct xdg_surface *xdgs = xdg_wm_base_get_xdg_surface(scr_driver->xdg_wm_base, wld_surf);
struct xdg_toplevel *top = xdg_surface_get_toplevel(xdgs);
xdg_toplevel_set_parent(top, fl_xid(win)->xdg_toplevel);
*/
}


int Fl_Wayland_System_Driver::event_key(int k) {
  if (k > FL_Button && k <= FL_Button+8)
    return Fl::event_state(8<<(k-FL_Button));
  int sym = Fl::event_key();
  if (sym >= 'a' && sym <= 'z' ) sym -= 32;
  return (Fl::event() == FL_KEYDOWN || Fl::event() == FL_SHORTCUT) && sym == k;
}

int Fl_Wayland_System_Driver::get_key(int k) {
  return event_key(k);
}
