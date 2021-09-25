#define libdecor_frame_set_minimized libdecor_frame_set_minimized_orig
#include "../src/libdecor.c"
#undef libdecor_frame_set_minimized

#include <dlfcn.h>

LIBDECOR_EXPORT void libdecor_frame_set_minimized(struct libdecor_frame *frame)
{
  typedef bool (*using_f)();
  static using_f sym = NULL;
  static bool using_weston = false;
  if (!sym) {
    sym = (using_f)dlsym(NULL, "fl_libdecor_using_weston");
    if (sym) using_weston = sym();
  }
  if (using_weston) libdecor_frame_set_visibility(frame, false);
  libdecor_frame_set_minimized_orig(frame);
}
