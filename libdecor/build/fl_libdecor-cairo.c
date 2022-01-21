//
// Interface with the libdecor library for the Fast Light Tool Kit (FLTK).
//
// Copyright 2022 by Bill Spitzak and others.
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

#include <dlfcn.h>
#include <string.h>

#if USE_SYSTEM_LIBDECOR

#include "../src/libdecor-plugin.h"
#include <pango/pangocairo.h>

/* these definitions are copied from libdecor/src/plugins/cairo/libdecor-cairo.c */

struct libdecor_plugin_cairo {
  struct libdecor_plugin plugin;

  struct wl_callback *globals_callback;
  struct wl_callback *globals_callback_shm;

  struct libdecor *context;

  struct wl_registry *wl_registry;
  struct wl_subcompositor *wl_subcompositor;
  struct wl_compositor *wl_compositor;

  struct wl_shm *wl_shm;
  struct wl_callback *shm_callback;
  bool has_argb;

  struct wl_list visible_frame_list;
  struct wl_list seat_list;
  struct wl_list output_list;

  char *cursor_theme_name;
  int cursor_size;

  PangoFontDescription *font;
};

struct buffer {
  struct wl_buffer *wl_buffer;
  bool in_use;
  bool is_detached;

  void *data;
  size_t data_size;
  int width;
  int height;
  int scale;
  int buffer_width;
  int buffer_height;
};

enum component {Component};
enum composite_mode {Composite_mode};
enum decoration_type {Decoration_type};

struct border_component {
  enum component type;

  bool is_hidden;
  bool opaque;

  enum composite_mode composite_mode;
  struct {
    struct wl_surface *wl_surface;
    struct wl_subsurface *wl_subsurface;
    struct buffer *buffer;
    struct wl_list output_list;
    int scale;
  } server;
  struct {
    cairo_surface_t *image;
    struct border_component *parent_component;
  } client;

  struct wl_list child_components; /* border_component::link */
  struct wl_list link; /* border_component::child_components */
};

struct libdecor_frame_cairo {
  struct libdecor_frame frame;

  struct libdecor_plugin_cairo *plugin_cairo;

  int content_width;
  int content_height;

  enum decoration_type decoration_type;

  enum libdecor_window_state window_state;

  char *title;

  enum libdecor_capabilities capabilities;

  struct border_component *focus;
  struct border_component *active;
  struct border_component *grab;

  bool shadow_showing;
  struct border_component shadow;

  struct {
    bool is_showing;
    struct border_component title;
    struct border_component min;
    struct border_component max;
    struct border_component close;
  } title_bar;

  /* store pre-processed shadow tile */
  cairo_surface_t *shadow_blur;

  struct wl_list link;
};

/* Definitions inspired from libdecor-gtk.c */

typedef struct _GtkWidget GtkWidget;
enum header_element { HDR_NONE };
typedef enum { GTK_STATE_FLAG_NORMAL = 0 } GtkStateFlags;

struct border_component_gtk {
  enum component type;
  struct wl_surface *wl_surface;
  struct wl_subsurface *wl_subsurface;
  struct buffer *buffer;
  bool opaque;
  struct wl_list output_list;
  int scale;
  struct wl_list child_components; /* border_component::link */
  struct wl_list link; /* border_component::child_components */
};

struct header_element_data {
  const char* name;
  enum header_element type;
  GtkWidget *widget;
  GtkStateFlags state;
};

struct libdecor_frame_gtk {
  struct libdecor_frame frame;
  struct libdecor_plugin_gtk *plugin_gtk;
  int content_width;
  int content_height;
  enum libdecor_window_state window_state;
  enum decoration_type decoration_type;
  char *title;
  enum libdecor_capabilities capabilities;
  struct border_component_gtk *active;
  struct border_component_gtk *focus;
  struct border_component_gtk *grab;
  bool shadow_showing;
  struct border_component_gtk shadow;
  GtkWidget *window; /* offscreen window for rendering */
  GtkWidget *header; /* header bar with widgets */
  struct border_component_gtk headerbar;
  struct header_element_data hdr_focus;
  cairo_surface_t *shadow_blur;
  struct wl_list link;
};

#else // USE_SYSTEM_LIBDECOR

struct libdecor_frame;
extern void fl_libdecor_frame_clamp_min_content_size(struct libdecor_frame *frame,
                                                   int content_width, int content_height);
#define libdecor_frame_set_min_content_size fl_libdecor_frame_clamp_min_content_size
#ifdef HAVE_GTK

#  include <gtk/gtk.h>
static int gtk_widget_get_allocated_height_null(GtkWidget *wid) {
  return GTK_IS_WIDGET(wid) ? gtk_widget_get_allocated_height(wid) : 0;
}
#  define gtk_widget_get_allocated_height gtk_widget_get_allocated_height_null

static void gtk_widget_get_allocation_null(GtkWidget *wid, GtkAllocation *allocation) {
  if (wid) gtk_widget_get_allocation(wid, allocation);
}
#  define gtk_widget_get_allocation gtk_widget_get_allocation_null

static void gtk_widget_destroy_null(GtkWidget *wid) {
  if (GTK_IS_WIDGET(wid)) gtk_widget_destroy(wid);
}
#  define gtk_widget_destroy gtk_widget_destroy_null

#  define border_component border_component_gtk
#  include "../src/plugins/gtk/libdecor-gtk.c"

#else // HAVE_GTK
#  include "../src/plugins/cairo/libdecor-cairo.c"
#endif // HAVE_GTK

#undef libdecor_frame_set_min_content_size

#endif // USE_SYSTEM_LIBDECOR


#if USE_SYSTEM_LIBDECOR || defined(HAVE_GTK)
static unsigned char *gtk_titlebar_buffer(struct libdecor_frame *frame,
                                                 int *width, int *height, int *stride)
{
  struct libdecor_frame_gtk *lfg = (struct libdecor_frame_gtk *)frame;
  struct border_component_gtk *bc = &lfg->headerbar;
  struct buffer *buffer = bc->buffer;
  *width = buffer->buffer_width;
  *height = buffer->buffer_height;
  *stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, buffer->buffer_width);
  return (unsigned char*)buffer->data;
}
#endif // USE_SYSTEM_LIBDECOR || defined(HAVE_GTK)


#if USE_SYSTEM_LIBDECOR || !defined(HAVE_GTK)
static unsigned char *cairo_titlebar_buffer(struct libdecor_frame *frame,
                                                 int *width, int *height, int *stride)
{
  struct libdecor_frame_cairo *lfc = (struct libdecor_frame_cairo *)frame;
  struct border_component *bc = &lfc->title_bar.title;
  struct buffer *buffer = bc->server.buffer;
  *width = buffer->buffer_width;
  *height = buffer->buffer_height;
  *stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, buffer->buffer_width);
  return (unsigned char*)buffer->data;
}
#endif // USE_SYSTEM_LIBDECOR || !defined(HAVE_GTK)


 char *fl_get_libdecor_plugin_description() {
  const struct libdecor_plugin_description *plugin_description = NULL;
  plugin_description = (const struct libdecor_plugin_description*)dlsym(RTLD_DEFAULT, "libdecor_plugin_description");
  if (!plugin_description) {
    char fname[PATH_MAX];
    const char *dir = getenv("LIBDECOR_PLUGIN_DIR");
    if (!dir) dir = LIBDECOR_PLUGIN_DIR;
    if (dir) {
      sprintf(fname, "%s/libdecor-gtk.so", dir);
      void *dl = dlopen(fname, RTLD_LAZY | RTLD_GLOBAL);
      if (!dl) {
        sprintf(fname, "%s/libdecor-cairo.so", dir);
        dl = dlopen(fname, RTLD_LAZY | RTLD_GLOBAL);
      }
      if (dl) plugin_description = (const struct libdecor_plugin_description*)dlsym(dl, "libdecor_plugin_description");
    }
  }
  return plugin_description ? plugin_description->description : NULL;
}


/*
 FLTK-added utility function to give access to the pixel array representing
 the titlebar of a window decorated by the cairo plugin of libdecor.
   frame: a libdecor-defined pointer given by fl_xid(win)->frame (with Fl_Window *win);
   *width, *height: returned assigned to the width and height in pixels of the titlebar;
   *stride: returned assigned to the number of bytes per line of the pixel array;
   return value: start of the pixel array, which is in BGRA order, or NULL.
 */
unsigned char *fl_libdecor_titlebar_buffer(struct libdecor_frame *frame,
                                                 int *width, int *height, int *stride)
{
  static char *my_plugin = NULL;
  if (!my_plugin) my_plugin = fl_get_libdecor_plugin_description();
  //puts(my_plugin?my_plugin:"");
#if USE_SYSTEM_LIBDECOR || defined(HAVE_GTK)
  if (my_plugin && !strcmp(my_plugin, "GTK plugin")) {
    return gtk_titlebar_buffer(frame, width, height, stride);
  }
#endif
#if USE_SYSTEM_LIBDECOR || !defined(HAVE_GTK)
  if (my_plugin && !strcmp(my_plugin, "libdecor plugin using Cairo")) {
    return cairo_titlebar_buffer(frame, width, height, stride);
  }
#endif
  return NULL;
}
