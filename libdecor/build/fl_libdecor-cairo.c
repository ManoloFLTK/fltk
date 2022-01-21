#if USE_SYSTEM_LIBDECOR

#include "../src/libdecor.h"
#include <pango/pangocairo.h>

// these definitions are copied from libdecor/src/plugins/cairo/libdecor-cairo.c
struct libdecor_frame_private;

struct libdecor_frame {
  struct libdecor_frame_private *priv;
  struct wl_list link;
};

struct libdecor_plugin_private;

struct libdecor_plugin {
  struct libdecor_plugin_private *priv;
};

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

#else // USE_SYSTEM_LIBDECOR

struct libdecor_frame;
extern void fl_libdecor_frame_clamp_min_content_size(struct libdecor_frame *frame,
                                                   int content_width, int content_height);
#define libdecor_frame_set_min_content_size fl_libdecor_frame_clamp_min_content_size
#ifdef HAVE_GTK

#  include <gtk/gtk.h>
static int gtk_widget_get_allocated_height_null(GtkWidget *wid) {
  return wid ? gtk_widget_get_allocated_height(wid) : 0;
}
#  define gtk_widget_get_allocated_height gtk_widget_get_allocated_height_null

static void gtk_widget_get_allocation_null(GtkWidget *wid, GtkAllocation *allocation) {
  if (wid) gtk_widget_get_allocation(wid, allocation);
}
#  define gtk_widget_get_allocation gtk_widget_get_allocation_null

static void gtk_widget_destroy_null(GtkWidget *wid) {
  if (wid) gtk_widget_destroy(wid);
}
#  define gtk_widget_destroy gtk_widget_destroy_null

#  include "../src/plugins/gtk/libdecor-gtk.c"
#else
#  include "../src/plugins/cairo/libdecor-cairo.c"
#endif
#undef libdecor_frame_set_min_content_size

#endif // USE_SYSTEM_LIBDECOR


#ifdef HAVE_GTK

static unsigned char *gtk_titlebar_buffer(struct libdecor_frame *frame,
                                                 int *width, int *height, int *stride)
{
  struct libdecor_frame_gtk *lfg = (struct libdecor_frame_gtk *)frame;
  struct border_component *bc = &lfg->headerbar;
  struct buffer *buffer = bc->buffer;
  *width = buffer->buffer_width;
  *height = buffer->buffer_height;
  *stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, buffer->buffer_width);
  return (unsigned char*)buffer->data;
}

#else

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

#endif // HAVE_GTK

/*
 FLTK-added utility function to give access to the pixel array representing
 the titlebar of a window decorated by the cairo plugin of libdecor.
   frame: a libdecor-defined pointer given by fl_xid(win)->frame (with Fl_Window *win);
   *width, *height: returned assigned to the width and height in pixels of the titlebar;
   *stride: returned assigned to the number of bytes per line of the pixel array;
   return value: start of the pixel array, which is in BGRA order.
 */
unsigned char *fl_libdecor_titlebar_buffer(struct libdecor_frame *frame,
                                                 int *width, int *height, int *stride)
{
#ifdef HAVE_GTK
  return gtk_titlebar_buffer(frame, width, height, stride);
#else
  return cairo_titlebar_buffer(frame, width, height, stride);
#endif
}
