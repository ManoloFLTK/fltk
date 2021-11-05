#define libdecor_frame_set_minimized libdecor_frame_set_minimized_orig
#define libdecor_new libdecor_new_orig
#include "../src/libdecor.c"
#undef libdecor_frame_set_minimized
#undef libdecor_new

extern bool fl_libdecor_using_weston(void);
//#include <stdio.h>

LIBDECOR_EXPORT void libdecor_frame_set_minimized(struct libdecor_frame *frame)
{
  static bool done = false;
  static bool using_weston = false;
  if (!done) {
    typedef bool (*ext_f)(void);
    volatile ext_f ext = fl_libdecor_using_weston;
    done = true;
    if (ext) using_weston = fl_libdecor_using_weston();
//fprintf(stderr, "fl_libdecor_using_weston=%p using_weston=%d\n", fl_libdecor_using_weston, using_weston);
  }
  if (using_weston) libdecor_frame_set_visibility(frame, false);
  libdecor_frame_set_minimized_orig(frame);
}

// defined in libdecor-cairo.c
extern const struct libdecor_plugin_description libdecor_plugin_description;

/*
 FLTK modifies libdecor's libdecor_new() function so it uses the plugin implemented in libdecor-cairo.c
 to decorate windows. No shared library is searched. No shared library is necessary.
 */
LIBDECOR_EXPORT struct libdecor *libdecor_new(struct wl_display *wl_display, struct libdecor_interface *iface)
{
  struct libdecor *context;
  context = zalloc(sizeof *context);
  context->ref_count = 1;
  context->iface = iface;
  context->wl_display = wl_display;
  context->wl_registry = wl_display_get_registry(wl_display);
  wl_registry_add_listener(context->wl_registry, &registry_listener, context);
  context->init_callback = wl_display_sync(context->wl_display);
  wl_callback_add_listener(context->init_callback, &init_wl_display_callback_listener, context);
  wl_list_init(&context->frames);

  context->plugin = libdecor_plugin_description.constructor(context);

  wl_display_flush(wl_display);
  return context;
}

/* Avoid undoing a previously set min-content-size */
void fl_libdecor_frame_clamp_min_content_size(struct libdecor_frame *frame,
                                            int content_width, int content_height) {
  struct libdecor_frame_private *frame_priv = frame->priv;
  frame_priv->state.content_limits.min_width = MAX(frame_priv->state.content_limits.min_width, content_width);
  frame_priv->state.content_limits.min_height = MAX(frame_priv->state.content_limits.min_height, content_height);
}
