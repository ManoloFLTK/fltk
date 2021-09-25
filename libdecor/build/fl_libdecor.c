#define libdecor_frame_set_minimized libdecor_frame_set_minimized_orig
#include "../src/libdecor.c"
#undef libdecor_frame_set_minimized

extern bool fl_libdecor_using_weston(void);

LIBDECOR_EXPORT void libdecor_frame_set_minimized(struct libdecor_frame *frame)
{
  static bool done = false;
  static bool using_weston = false;
  if (!done) {
    done = true;
    using_weston = fl_libdecor_using_weston();
//fprintf(stderr, "fl_libdecor_using_weston=%p using_weston=%d\n",fl_libdecor_using_weston,using_weston);
  }
  if (using_weston) libdecor_frame_set_visibility(frame, false);
  libdecor_frame_set_minimized_orig(frame);
}
