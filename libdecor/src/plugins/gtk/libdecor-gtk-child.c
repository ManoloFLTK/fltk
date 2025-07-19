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

#include "libdecor-plugin.h"
#include "utils.h"
#include "desktop-settings.h"

#include <gtk/gtk.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#if GTK_MAJOR_VERSION != 3 && GTK_MAJOR_VERSION != 4
# error GTK3 or GTK4 is required
#endif

#if GTK_MAJOR_VERSION == 4
# include <stdlib.h> /* setenv() */
#endif

#include "libdecor-gtk.h"

static int pipe_to_child;
static int pipe_from_child;
static int shm_fd;
static void *mmap_data;
static off_t shm_size;

static void
child_gtk_init()
{
  uint32_t color_scheme;
  bool error_found = false;
  read(pipe_to_child, &color_scheme, sizeof(uint32_t));
  gdk_set_allowed_backends("wayland");
  gtk_disable_setlocale();

  if (!gtk_init_check(
#if GTK_MAJOR_VERSION == 3
    NULL, NULL
#endif
  )) {
    fprintf(stderr, "libdecor-gtk-WARNING: Failed to initialize GTK\n");
    error_found = true;
  } else {
#if GTK_MAJOR_VERSION == 4
    setenv("GSK_RENDERER", "cairo", 1);
#endif
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
      color_scheme == LIBDECOR_COLOR_SCHEME_PREFER_DARK, NULL);
  }
  write(pipe_from_child, &error_found, sizeof(bool));
  mmap_data = NULL;
  shm_size = 0;
}

static void
child_get_allocated_WH()
{
  int W = 0, H = 0;
  GtkWidget *header;
  read(pipe_to_child, &header, sizeof(GtkWidget*));
  if (GTK_IS_WIDGET(header)) {
#if GTK_MAJOR_VERSION == 3
    W = gtk_widget_get_allocated_width(header);
    H = gtk_widget_get_allocated_height(header);
#else
    graphene_rect_t out_bounds;
    if (gtk_widget_compute_bounds(header, gtk_widget_get_parent(header), &out_bounds)) {
      W = out_bounds.size.width;
      H = out_bounds.size.height;
    }
#endif
  }
  write(pipe_from_child, &W, sizeof(int));
  write(pipe_from_child, &H, sizeof(int));
}


#if GTK_MAJOR_VERSION == 4
static GtkWindowControls *
find_window_controls(GtkWidget *header)
{
  /* The found GtkWindowControls has 1 to 3 GtkButton children
     depending on "Tweaks" settings and win resizability. */
  GtkWidget *w = gtk_widget_get_first_child(header);
  while (w) {
    GtkWindowControls *r = (GTK_IS_WINDOW_CONTROLS(w) ? GTK_WINDOW_CONTROLS(w) : NULL);
    if (!r) r = find_window_controls(w);
    else if (gtk_window_controls_get_empty(r)) r = NULL;
    if (r) return r;
    w = gtk_widget_get_next_sibling(w);
  }
  return NULL;
}

static GtkWidget *
find_child_with_css(GtkWindowControls *ctrls, const char *css)
{
  GtkWidget *w = gtk_widget_get_first_child(GTK_WIDGET(ctrls));
  while (w) {
    if (gtk_widget_has_css_class(w, css)) return w;
    w = gtk_widget_get_next_sibling(w);
  }
  return NULL;
}

#else

static void
find_widget_by_name(GtkWidget *widget, void *data)
{
  if (GTK_IS_WIDGET(widget)) {
    char *style_ctx = gtk_style_context_to_string(
      gtk_widget_get_style_context(widget),
      GTK_STYLE_CONTEXT_PRINT_SHOW_STYLE);
    if (strstr(style_ctx, ((struct header_element_data *)data)->name)) {
      ((struct header_element_data *)data)->widget = widget;
      free(style_ctx);
      return;
    }
    free(style_ctx);
  }

  if (GTK_IS_CONTAINER(widget)) {
    /* recursively traverse container */
    gtk_container_forall(GTK_CONTAINER(widget), &find_widget_by_name, data);
  }
}

#endif


static struct header_element_data
find_widget_by_type(GtkWidget *widget, enum header_element type)
{
  char* name = NULL;
#if GTK_MAJOR_VERSION == 4
  GtkWindowControls *ctrls = NULL;
  if (type >= HEADER_MIN) {
    ctrls = find_window_controls(widget);
    switch (type) {
    case HEADER_MIN:
      name = ".minimize";
      break;
    case HEADER_MAX:
      name = ".maximize";
      break;
    case HEADER_CLOSE:
      name = ".close";
      break;
    default:
      break;
    }
  struct header_element_data data = {
    .name = name,
    .type = type,
    .widget = (name ? find_child_with_css(ctrls, name + 1) : NULL)
  };
  return data;
  }
#endif
  switch (type) {
  case HEADER_FULL:
    name = "headerbar.titlebar:";
    break;
  case HEADER_TITLE:
    name = "label.title:";
    break;
#if GTK_MAJOR_VERSION == 3
  case HEADER_MIN:
    name = ".minimize";
    break;
  case HEADER_MAX:
    name = ".maximize";
    break;
  case HEADER_CLOSE:
    name = ".close";
    break;
#endif
  default:
    break;
  }

  struct header_element_data data = {.name = name, .type = type, .widget = NULL};
#if GTK_MAJOR_VERSION == 3
  find_widget_by_name(widget, &data);
#endif
  return data;
}

static bool
in_region(const cairo_rectangle_int_t *rect, const int *x, const int *y)
{
  return (*x>=rect->x) & (*y>=rect->y) &
    (*x<(rect->x+rect->width)) & (*y<(rect->y+rect->height));
}

static void
get_header_focus()
{
  GtkHeaderBar *header_bar;
  int x, y;
  /* we have to check child widgets (buttons, title) before the 'HDR_HDR' root widget */
  static const enum header_element elems[] =
    {HEADER_TITLE, HEADER_MIN, HEADER_MAX, HEADER_CLOSE};

  read(pipe_to_child, &header_bar, sizeof(GtkHeaderBar*));
  read(pipe_to_child, &x, sizeof(int));
  read(pipe_to_child, &y, sizeof(int));
  if (GTK_HEADER_BAR(header_bar)) for (size_t i = 0; i < ARRAY_SIZE(elems); i++) {
    struct header_element_data elem =
      find_widget_by_type(GTK_WIDGET(header_bar), elems[i]);
    if (elem.widget) {
      GtkAllocation allocation;
#if GTK_MAJOR_VERSION == 3
      gtk_widget_get_allocation(GTK_WIDGET(elem.widget), &allocation);
#else
      graphene_rect_t out_bounds;
      if (gtk_widget_compute_bounds(GTK_WIDGET(elem.widget),
          GTK_WIDGET(header_bar), &out_bounds)) {
        allocation.x = (int)out_bounds.origin.x;
        allocation.y = (int)out_bounds.origin.y;
        allocation.width = (int)out_bounds.size.width;
        allocation.height = (int)out_bounds.size.height;
      }
#endif
      if (in_region(&allocation, &x, &y)) {
        /* for security because that's a pointer to child's address space */
        elem.name = NULL;
        write(pipe_from_child, &elem, sizeof(struct header_element_data));
        return;
      }
    }
  }

  struct header_element_data elem_none = { .widget=NULL};
  write(pipe_from_child, &elem_none, sizeof(struct header_element_data));
}


static void
destroy_window_header()
{
  void *header, *window;
  read(pipe_to_child, &header, sizeof(void*));
  read(pipe_to_child, &window, sizeof(void*));
#if GTK_MAJOR_VERSION == 3
  if (header) gtk_widget_destroy(header);
  if (window) gtk_widget_destroy(window);
#else
  if (window) gtk_window_destroy((GtkWindow*)window);
#endif
}

static void
ensure_title_bar_surfaces()
{
  struct libdecor_frame_gtk frame_gtk;
  char *p, title[1000];
  int double_click_time_ms, drag_threshold, resizable;
#if GTK_MAJOR_VERSION == 3
  GtkStyleContext *context_hdr;
#endif

  read(pipe_to_child, &frame_gtk, sizeof(struct libdecor_frame_gtk));
  read(pipe_to_child, &resizable, sizeof(int));
  p = title;
  do read(pipe_to_child, p, sizeof(char));
  while (*p++);
  /* create an offscreen window with a header bar */
  /* TODO: This should only be done once at frame construction, but then
   *       the window and headerbar would not change style (e.g. backdrop)
   *       after construction. So we just destroy and re-create them.
   */
  /* avoid warning when restoring previously turned off decoration */
  if (GTK_IS_WIDGET(frame_gtk.header)) {
#if GTK_MAJOR_VERSION == 3
    gtk_widget_destroy(frame_gtk.header);
#else
    gtk_widget_set_visible(frame_gtk.header, false);
#endif
    frame_gtk.header = NULL;
  }
  /* avoid warning when restoring previously turned off decoration */
  if (GTK_IS_WIDGET(frame_gtk.window)) {
#if GTK_MAJOR_VERSION == 3
    gtk_widget_destroy(frame_gtk.window);
#else
    gtk_window_destroy((GtkWindow*)frame_gtk.window);
#endif
    frame_gtk.window = NULL;
  }
#if GTK_MAJOR_VERSION == 3
  frame_gtk.window = gtk_offscreen_window_new();
#else
  frame_gtk.window = gtk_window_new();
  gtk_widget_set_visible(frame_gtk.window, false);
#endif
  frame_gtk.header = gtk_header_bar_new();

  g_object_get(gtk_widget_get_settings(frame_gtk.window),
    "gtk-double-click-time",
    &double_click_time_ms,
    "gtk-dnd-drag-threshold",
    &drag_threshold,
    NULL);
#if GTK_MAJOR_VERSION == 3
  /* set as "default" decoration */
  g_object_set(frame_gtk.header,
    "title", title,
    "has-subtitle", FALSE,
    "show-close-button", TRUE,
    NULL);

  context_hdr = gtk_widget_get_style_context(frame_gtk.header);
  gtk_style_context_add_class(context_hdr, GTK_STYLE_CLASS_TITLEBAR);
  gtk_style_context_add_class(context_hdr, "default-decoration");

  gtk_window_set_titlebar(GTK_WINDOW(frame_gtk.window), frame_gtk.header);
  gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(frame_gtk.header), TRUE);
#else
  gtk_window_set_child(GTK_WINDOW(frame_gtk.window), frame_gtk.header);
#endif

  gtk_window_set_resizable(GTK_WINDOW(frame_gtk.window), resizable);
  write(pipe_from_child, &frame_gtk, sizeof(struct libdecor_frame_gtk));
  write(pipe_from_child, &double_click_time_ms, sizeof(int));
  write(pipe_from_child, &drag_threshold, sizeof(int));
}

static void
draw_title_bar_child()
{
  char *p, title[1000];
  int state, floating, need_commit = false;
  GtkAllocation allocation = {0, 0, 0, 0};
#if GTK_MAJOR_VERSION == 3
  GtkStyleContext *style;
#endif
  int pref_width;
  int current_min_w, current_min_h, current_max_w, current_max_h, W, H;
  struct libdecor_frame_gtk frame_gtk;
  read(pipe_to_child, &frame_gtk, sizeof(struct libdecor_frame_gtk));
  read(pipe_to_child, &state, sizeof(int));
  read(pipe_to_child, &floating, sizeof(int));
  read(pipe_to_child, &current_min_w, sizeof(int));
  read(pipe_to_child, &current_min_h, sizeof(int));
  read(pipe_to_child, &current_max_w, sizeof(int));
  read(pipe_to_child, &current_max_h, sizeof(int));
  read(pipe_to_child, &W, sizeof(int));
  read(pipe_to_child, &H, sizeof(int));
  p = title;
  do read(pipe_to_child, p, sizeof(char));
  while (*p++);
  allocation.width = frame_gtk.content_width;

#if GTK_MAJOR_VERSION == 4
  if (GTK_IS_WIDGET(frame_gtk.hdr_focus.widget)) {
    gtk_widget_set_state_flags(frame_gtk.hdr_focus.widget,
      GTK_STATE_FLAG_PRELIGHT|GTK_STATE_FLAG_ACTIVE, 1);
  }
#endif

#if GTK_MAJOR_VERSION == 3
  style = gtk_widget_get_style_context(frame_gtk.window);
#endif

  if (frame_gtk.window) {
    if (!(state & LIBDECOR_WINDOW_STATE_ACTIVE)) {
      gtk_widget_set_state_flags(frame_gtk.window, GTK_STATE_FLAG_BACKDROP, true);
    } else {
      gtk_widget_unset_state_flags(frame_gtk.window, GTK_STATE_FLAG_BACKDROP);
    }
  }

#if GTK_MAJOR_VERSION == 3
  if (floating) {
    gtk_style_context_remove_class(style, "maximized");
  } else {
    gtk_style_context_add_class(style, "maximized");
  }

  gtk_widget_show_all(frame_gtk.window);

  /* set default width, using an empty title to estimate its smallest admissible value */
  gtk_header_bar_set_title(GTK_HEADER_BAR(frame_gtk.header), "");
  gtk_widget_get_preferred_width(frame_gtk.header, NULL, &pref_width);
  gtk_header_bar_set_title(GTK_HEADER_BAR(frame_gtk.header), title);
#else
  if (floating) {
    gtk_widget_remove_css_class(frame_gtk.window, "maximized");
  } else {
    gtk_widget_add_css_class(frame_gtk.window, "maximized");
  }
  pref_width = 12; /* smallest value that doesn't trigger GTK errors */
  gtk_window_set_title(GTK_WINDOW(frame_gtk.window), title/*libdecor_frame_get_title(&frame_gtk.frame)*/);
#endif
  if (current_min_w < pref_width) {
    current_min_w = pref_width;
  }
  if (current_max_w && current_max_w < current_min_w) {
    current_max_w = current_min_w;
  }
  if (W < current_min_w) {
    need_commit = true;
    W = current_min_w;
  } else {
    /* set default height */
#if GTK_MAJOR_VERSION == 3
    gtk_widget_get_preferred_height(frame_gtk.header, NULL, &allocation.height);
#else
    {
      GtkRequisition *rq = gtk_requisition_new();
      gtk_widget_get_preferred_size(frame_gtk.header, NULL, rq);
      allocation.height = rq->height;
      gtk_requisition_free(rq);
    }
#endif

    gtk_widget_size_allocate(frame_gtk.header, &allocation
#if GTK_MAJOR_VERSION == 4
          , -1
#endif
          );
  }
  write(pipe_from_child, &need_commit, sizeof(int));
  write(pipe_from_child, &current_min_w, sizeof(int));
  write(pipe_from_child, &current_min_h, sizeof(int));
  write(pipe_from_child, &current_max_w, sizeof(int));
  write(pipe_from_child, &current_max_h, sizeof(int));
  write(pipe_from_child, &W, sizeof(int));
  write(pipe_from_child, &H, sizeof(int));
}

#if GTK_MAJOR_VERSION == 3
static void
array_append(enum header_element **array, size_t *n, enum header_element item)
{
  (*n)++;
  *array = realloc(*array, (*n) * sizeof (enum header_element));
  (*array)[(*n)-1] = item;
}

static void
draw_header_background(struct libdecor_frame_gtk *frame_gtk,
        cairo_t *cr)
{
  /* background */
  GtkAllocation allocation;
  GtkStyleContext* style;

  gtk_widget_get_allocation(GTK_WIDGET(frame_gtk->header), &allocation);
  style = gtk_widget_get_style_context(frame_gtk->header);
  gtk_render_background(style, cr, allocation.x, allocation.y, allocation.width, allocation.height);
}

static void
draw_header_title(struct libdecor_frame_gtk *frame_gtk,
        cairo_surface_t *surface)
{
  /* title */
  GtkWidget *label;
  GtkAllocation allocation;
  cairo_surface_t *label_surface = NULL;
  cairo_t *cr;

  label = find_widget_by_type(frame_gtk->header, HEADER_TITLE).widget;
  gtk_widget_get_allocation(label, &allocation);

  /* create subsection in which to draw label */
  label_surface = cairo_surface_create_for_rectangle(
    surface,
    allocation.x, allocation.y,
    allocation.width, allocation.height);
  cr = cairo_create(label_surface);
  gtk_widget_size_allocate(label, &allocation);
  gtk_widget_draw(label, cr);
  cairo_destroy(cr);
  cairo_surface_destroy(label_surface);
}

static void
draw_header_button(struct libdecor_frame_gtk *frame_gtk,
         cairo_t *cr,
        cairo_surface_t *surface,
        enum header_element button_type,
        enum libdecor_window_state window_state)
{
  struct header_element_data elem;
  GtkWidget *button;
  GtkStyleContext* button_style;
  GtkStateFlags style_state;

  GtkAllocation allocation;

  gchar *icon_name;
  int scale;
  GtkWidget *icon_widget;
  GtkAllocation allocation_icon;
  GtkIconInfo* icon_info;

  double sx, sy;

  gint icon_width, icon_height;

  GdkPixbuf* icon_pixbuf;
  cairo_surface_t* icon_surface;

  gint width = 0, height = 0;

  gint left = 0, top = 0, right = 0, bottom = 0;
  GtkBorder border;

  GtkBorder padding;

  elem = find_widget_by_type(frame_gtk->header, button_type);
  button = elem.widget;
  if (!button)
    return;
  button_style = gtk_widget_get_style_context(button);
  style_state = elem.state;

  /* change style based on window state and focus */
  if (!(window_state & LIBDECOR_WINDOW_STATE_ACTIVE)) {
    style_state |= GTK_STATE_FLAG_BACKDROP;
  }
  if (frame_gtk->hdr_focus.widget == button) {
    style_state |= GTK_STATE_FLAG_PRELIGHT;
    if (frame_gtk->hdr_focus.state & GTK_STATE_FLAG_ACTIVE) {
      style_state |= GTK_STATE_FLAG_ACTIVE;
    }
  }

  /* background */
  gtk_widget_get_clip(button, &allocation);

  gtk_style_context_save(button_style);
  gtk_style_context_set_state(button_style, style_state);
  gtk_render_background(button_style, cr,
    allocation.x, allocation.y,
    allocation.width, allocation.height);
  gtk_render_frame(button_style, cr,
    allocation.x, allocation.y,
    allocation.width, allocation.height);
  gtk_style_context_restore(button_style);

  /* symbol */
  switch (button_type) {
  case HEADER_MIN:
    icon_name = "window-minimize-symbolic";
    break;
  case HEADER_MAX:
    icon_name = (window_state & LIBDECOR_WINDOW_STATE_MAXIMIZED) ?
      "window-restore-symbolic" :
      "window-maximize-symbolic";
    break;
  case HEADER_CLOSE:
    icon_name = "window-close-symbolic";
    break;
  default:
    icon_name = NULL;
  break;
  }

  /* get scale */
  cairo_surface_get_device_scale(surface, &sx, &sy);
  scale = (sx+sy) / 2.0;

  /* get original icon dimensions */
  icon_widget = gtk_bin_get_child(GTK_BIN(button));
  gtk_widget_get_allocation(icon_widget, &allocation_icon);

  /* icon info */
  if (!gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &icon_width, &icon_height)) {
    icon_width = 16;
    icon_height = 16;
  }
  icon_info = gtk_icon_theme_lookup_icon_for_scale(
    gtk_icon_theme_get_default(), icon_name,
    icon_width, scale, (GtkIconLookupFlags)0);

  /* icon pixel buffer*/
  gtk_style_context_save(button_style);
  gtk_style_context_set_state(button_style, style_state);
  icon_pixbuf = gtk_icon_info_load_symbolic_for_context(
    icon_info, button_style, NULL, NULL);
  icon_surface = gdk_cairo_surface_create_from_pixbuf(icon_pixbuf, scale, NULL);
  gtk_style_context_restore(button_style);

  /* dimensions and position */
  gtk_style_context_get(button_style, gtk_style_context_get_state(button_style),
      "min-width", &width, "min-height", &height, NULL);

  if (width < icon_width)
    width = icon_width;
  if (height < icon_height)
    height = icon_height;

  gtk_style_context_get_border(button_style, gtk_style_context_get_state(button_style), &border);
  left += border.left;
  right += border.right;
  top += border.top;
  bottom += border.bottom;

  gtk_style_context_get_padding(button_style, gtk_style_context_get_state(button_style), &padding);
  left += padding.left;
  right += padding.right;
  top += padding.top;
  bottom += padding.bottom;

  width += left + right;
  height += top + bottom;

  gtk_render_icon_surface(gtk_widget_get_style_context(icon_widget),
    cr, icon_surface,
    allocation.x + ((width - icon_width) / 2),
    allocation.y + ((height - icon_height) / 2));
  cairo_paint(cr);
  cairo_surface_destroy(icon_surface);
  g_object_unref(icon_pixbuf);
}

static void
draw_header_buttons(struct libdecor_frame_gtk *frame_gtk, cairo_t *cr, cairo_surface_t *surface)
{
  /* buttons */
  enum header_element *buttons = NULL;
  size_t nbuttons = 0;

  /* set buttons by capability */
  if (frame_gtk->capabilities & LIBDECOR_ACTION_MINIMIZE)
    array_append(&buttons, &nbuttons, HEADER_MIN);
  if (frame_gtk->capabilities & LIBDECOR_ACTION_RESIZE)
    array_append(&buttons, &nbuttons, HEADER_MAX);
  if (frame_gtk->capabilities & LIBDECOR_ACTION_CLOSE)
    array_append(&buttons, &nbuttons, HEADER_CLOSE);

  for (size_t i = 0; i < nbuttons; i++) {
    draw_header_button(frame_gtk, cr, surface, buttons[i], frame_gtk->window_state);
  } /* loop buttons */
  free(buttons);
}
#endif /* GTK_MAJOR_VERSION == 3 */

static void draw_header()
{
  int W, H, scale;
  off_t size;
  struct libdecor_frame_gtk frame_gtk;
  read(pipe_to_child, &W, sizeof(int));
  read(pipe_to_child, &H, sizeof(int));
  read(pipe_to_child, &scale, sizeof(int));
  read(pipe_to_child, &frame_gtk, sizeof(struct libdecor_frame_gtk));
  size = H * cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, W);
  if (size > shm_size || !mmap_data) {
    if (mmap_data)
      munmap(mmap_data, shm_size);
    if (size > shm_size)
      shm_size = size;
    mmap_data = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  }

  cairo_surface_t *surface = cairo_image_surface_create_for_data(mmap_data,
    CAIRO_FORMAT_ARGB32,
    W, H, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, W));
  cairo_surface_set_device_scale(surface, scale, scale);
  cairo_t *cr = cairo_create(surface);

#if GTK_MAJOR_VERSION == 3
  draw_header_background(&frame_gtk, cr);
  draw_header_title(&frame_gtk, surface);
  draw_header_buttons(&frame_gtk, cr, surface);
#else
  GtkAllocation allocation = {0, 0, 0, 0};
  graphene_rect_t out_bounds;
  GtkSnapshot *snapshot;
  GskRenderNode *rendernode;
  snapshot = gtk_snapshot_new();
  gtk_widget_set_visible(frame_gtk.window, true);
  if (gtk_widget_compute_bounds(frame_gtk.header, frame_gtk.header, &out_bounds)) {
    allocation.width = (int)out_bounds.size.width;
    allocation.height = (int)out_bounds.size.height;
  }
  gtk_widget_size_allocate(frame_gtk.header, &allocation, -1);
  gtk_widget_snapshot_child(frame_gtk.window, frame_gtk.header, snapshot);
  gtk_widget_set_visible(frame_gtk.window, false);
  rendernode = gtk_snapshot_free_to_node(snapshot);
  gsk_render_node_draw(rendernode, cr);
  gsk_render_node_unref(rendernode);
#endif
  cairo_surface_destroy(surface);
  cairo_destroy(cr);
}

static void
child_check_widget()
{
  GtkWidget *widget;
  bool result;
  read(pipe_to_child, &widget, sizeof(GtkWidget*));
  result = GTK_IS_WIDGET(widget);
  write(pipe_from_child, &result, sizeof(bool));
}

int
main(int argc, char **argv)
{
/* Expectation: argc == 2, argv[1] contains 3 file descriptors separated by ',' for
   pipe from parent to child, pipe from child to parent, memory object created by shm_open
   and shared by parent and child. This shared memory object is destined to contain
   the graphical content of the window titlebar in CAIRO_FORMAT_ARGB32. The child process
   writes, using GTK calls, the content of this memory object. Next, the parent process
   copies this content to the libdecor buffer corresponding to the titlebar wl_surface
   and commits that surface to Wayland. The two pipes are used to exchange small-sized
   information between parent and child and to synchronize child vs parent.
 */
  enum child_commands cmd;
  int n = 0;

  if (argc == 2)
    n = sscanf(argv[1], "%d,%d,%d", &pipe_to_child, &pipe_from_child, &shm_fd);
  if (argc != 2 || n != 3) {
    fprintf(stderr, "This program is only meant to run as a child of the libdecor-gtk plugin.\n");
    exit(0);
  }
  while (true) {
    /* receive from parent sign of what operation is the child asked to perform */
    if (read(pipe_to_child, &cmd, sizeof(enum child_commands)) <= 0) {
      exit(0); /* successful end of the child process */
    }
    switch (cmd) { /* across all possible child operations */
    /* Each child operation involves GTK. It begins by receiving parameter values
     from the parent. Then, GTK operations are performed. It optionally ends
     sending operation results to the parent.
     */
    case CHILD_INIT:
      child_gtk_init();
      break;

    case CHILD_DRAW_HEADER:
      draw_header();
      break;

    case CHILD_DESTROY_HEADER:
      destroy_window_header();
      break;

    case CHILD_DRAW_TITLEBAR:
      draw_title_bar_child();
      break;

    case CHILD_ENSURE_SURFACES:
      ensure_title_bar_surfaces();
      break;

    case CHILD_GET_FOCUS:
      get_header_focus();
      break;

    case CHILD_GET_ALLOCATED_WH:
      child_get_allocated_WH();
      break;

    case CHILD_CHECK_WIDGET:
      child_check_widget();
      break;

    default:
      fprintf(stderr,
        "libdecor-gtk: error in communication between parent and child.\n");
      exit(0);
    } /* switch */
    /* send parent sign of completion of the requested operation by child */
    cmd = CHILD_OP_COMPLETED;
    write(pipe_from_child, &cmd, sizeof(enum child_commands));
  } /* while(true) */
  return 0;
}
