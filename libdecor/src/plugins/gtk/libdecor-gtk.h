/*
 * Copyright © 2018 Jonas Ådahl
 * Copyright © 2021 Christian Rauch
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBDECOR_GTK_H
#define LIBDECOR_GTK_H 1

#ifndef GTK_MAJOR_VERSION
/* types used both by GTK-aware libdecor-gtk-child.c and GTK-unaware libdecor-gtk.c */
typedef enum
{
  GTK_STATE_FLAG_ACTIVE   = 1 << 0,
  GTK_STATE_FLAG_PRELIGHT = 1 << 1,
} GtkStateFlags;
typedef struct opaque GtkWidget;
#endif /* ndef GTK_MAJOR_VERSION */

enum child_commands {
  CHILD_OP_COMPLETED = 1, /* sent by child to parent at end of operation */
  CHILD_INIT, /* other values sent by parent to ask child to perform given operation */
  CHILD_DRAW_HEADER,
  CHILD_DESTROY_HEADER,
  CHILD_DRAW_TITLEBAR,
  CHILD_ENSURE_SURFACES,
  CHILD_GET_FOCUS,
  CHILD_GET_ALLOCATED_WH,
  CHILD_CHECK_WIDGET,
};

enum header_element {
  HEADER_NONE,
  HEADER_FULL, /* entire header bar */
  HEADER_TITLE, /* label */
  HEADER_MIN,
  HEADER_MAX,
  HEADER_CLOSE,
};

struct header_element_data {
  const char *name;
  enum header_element type;
  /* pointer to button or NULL if not found*/
  GtkWidget *widget;
  GtkStateFlags state;
};

enum component {
  NONE = 0,
  SHADOW,
  HEADER,
};

enum decoration_type {
  DECORATION_TYPE_NONE,
  DECORATION_TYPE_ALL,
  DECORATION_TYPE_TITLE_ONLY
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

struct border_component {
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

enum titlebar_gesture_state {
  TITLEBAR_GESTURE_STATE_INIT,
  TITLEBAR_GESTURE_STATE_BUTTON_PRESSED,
  TITLEBAR_GESTURE_STATE_CONSUMED,
  TITLEBAR_GESTURE_STATE_DISCARDED,
};

struct libdecor_plugin_gtk {
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

  uint32_t color_scheme_setting;

  int double_click_time_ms;
  int drag_threshold;

  bool handle_cursor;

/* members regarding the memory zone shared between parent and child */
  char shared_name[32];
  off_t shm_size;
  void *shm_mmap;
  int shm_fd;
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

  struct border_component *active;
  struct border_component *touch_active;

  struct border_component *focus;
  struct border_component *grab;

  bool shadow_showing;
  struct border_component shadow;

  GtkWidget *window; /* offscreen window for rendering */
  GtkWidget *header; /* header bar with widgets */
  struct border_component headerbar;
  struct header_element_data hdr_focus;

  /* store pre-processed shadow tile */
  cairo_surface_t *shadow_blur;

  struct wl_list link;

  struct {
    enum titlebar_gesture_state state;
    int button_pressed_count;
    uint32_t first_pressed_button;
    uint32_t first_pressed_time;
    double pressed_x;
    double pressed_y;
    uint32_t pressed_serial;
  } titlebar_gesture;
};

#endif /* ndef LIBDECOR_GTK_H */

