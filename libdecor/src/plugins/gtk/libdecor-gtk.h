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
/* residual GTK types used by GTK-unaware libdecor-gtk.c */
typedef enum
{ /* only enum values used by libdecor-gtk.c are defined here */
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

/*     How parent and child exchange information and synchronize
One pipe sends data from child to parent.
Another pipe sends data from parent to child.

Parent writes to the to-child pipe the value of the desired child operation
and the values of that operation's parameters.
Parent next reads from the from-child pipe and therefore blocks until
something appears there that will be the result of the requested operation.
Child reads from the to-child pipe the operation value and its parameters. It performs that
operation and writes to the from-child pipe operation results and CHILD_OP_COMPLETED.
Child next reads from the to-child pipe and therefore blocks until something appears
there that will be the next operation request sent by parent.
Child ends with exit(0) when that read operation returns -1.
Parent's ongoing read from the from-child pipe unblocks and delivers the operation results
and an operation value that is necessarily CHILD_OP_COMPLETED. Parent continues its work.

    Details of operation input parameters and output results
OPERATION-NAME
parameters                type        results        type
CHILD_INIT
plugin_gtk->color_scheme_setting    uint_32t      did_error_occur?  bool

CHILD_DRAW_HEADER
buffer->buffer_width          int
buffer->buffer_height          int
buffer->buffer_scale          int
buffer->fd            int
*frame_gtk            struct libdecor_frame_gtk

CHILD_DESTROY_HEADER
frame_gtk->header            void*
frame_gtk->window            void*

CHILD_DRAW_TITLEBAR
*frame_gtk            struct libdecor_frame_gtk  need_commit    int
libdecor_frame_get_window_state()    int          current_min_w  int
libdecor_frame_is_floating()      int          current_min_h  int
W-libdecor_frame_get_min_content_size()  int          current_max_w  int
H-libdecor_frame_get_min_content_size()  int          current_max_h  int
W-libdecor_frame_get_max_content_size()  int          W        int
H-libdecor_frame_get_max_content_size()  int          H        int
libdecor_frame_get_content_width()    int
libdecor_frame_get_content_height()    int
libdecor_frame_get_title()        strlen()+1 bytes

CHILD_ENSURE_SURFACES
*frame_gtk             struct libdecor_frame_gtk  *frame_gtk  struct libdecor_frame_gtk
libdecor_frame_has_capability()      int          frame_gtk->plugin_gtk->double_click_time_ms  int
libdecor_frame_get_title()        strlen()+1 bytes  frame_gtk->plugin_gtk->drag_threshold  int

CHILD_GET_FOCUS
frame_gtk->header            void*        new_focus     struct header_element_data
seat->pointer_x              int
seat->pointer_y              int

CHILD_GET_ALLOCATED_WH
frame_gtk->header            void*        skip      int
                              title_height  int
CHILD_CHECK_WIDGET
frame_gtk->header            void*        is_gtk_widget  int

*/

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

