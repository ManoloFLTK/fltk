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
 By default, FLTK modifies libdecor's libdecor_new() function to determine the plugin as follows :
 1) the directory pointed by environment variable LIBDECOR_PLUGIN_DIR or, in absence of this variable,
    by -DLIBDECOR_PLUGIN_DIR=xxx at build time is searched for a libdecor plugin;
 2) if this directory does not exist or contains no plugin, the built-in plugin is used.
    * if FLTK was built with package libgtk-3-dev, the GTK plugin is used
    * if FLTK was built without package libgtk-3-dev, the Cairo plugin is used
 
 If FLTK was built with OPTION_USE_SYSTEM_LIBDECOR turned ON, the present modification
 isn't compiled, so the plugin-searching algorithm of libdecor_new() in libdecor-0.so is used.
 This corresponds to step 1) above and to use no titlebar is no plugin is found.
 
 N.B.: only the system package is built with a meaningful value of -DLIBDECOR_PLUGIN_DIR=
 so a plugin may be loaded that way only if FLTK was built with OPTION_USE_SYSTEM_LIBDECOR turned ON.
 
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
  // attempt to dynamically load a libdecor plugin with dlopen()
  FILE *old_stderr = stderr;
  stderr = fopen("/dev/null", "w+"); // avoid "Couldn't open plugin directory" messages
  if (init_plugins(context) != 0) { // no plugin loaded yet
    // use built-in plugin
    context->plugin = libdecor_plugin_description.constructor(context);
  }
  fclose(stderr); // restore stderr as it was before
  stderr = old_stderr;

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
