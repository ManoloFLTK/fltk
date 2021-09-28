#define libdecor_frame_set_minimized libdecor_frame_set_minimized_orig
#define libdecor_frame_unref libdecor_frame_unref_orig
#include "../src/libdecor.c"
#undef libdecor_frame_set_minimized
#undef libdecor_frame_unref

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


LIBDECOR_EXPORT void libdecor_frame_unref(struct libdecor_frame *frame)
{
  struct libdecor_frame_private *frame_priv = frame->priv;
  if (frame_priv->ref_count == 1) {
    wl_list_remove(&frame->link);
  }
  libdecor_frame_unref_orig(frame);
}
