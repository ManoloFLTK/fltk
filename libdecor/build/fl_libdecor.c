#define libdecor_frame_set_minimized libdecor_frame_set_minimized_orig
#include "../src/libdecor.c"
#undef libdecor_frame_set_minimized

#include <dlfcn.h>

static bool using_weston() {
	typedef bool (*using_f)();
	using_f sym = (using_f)dlsym(NULL, "fl_libdecor_using_weston");
//fprintf(stderr, "dlsym(fl_libdecor_using_weston)=%p result=%d\n", sym, sym?sym():2);
	return sym ? sym() : false;
}

LIBDECOR_EXPORT void libdecor_frame_set_minimized(struct libdecor_frame *frame)
{
	if (using_weston()) libdecor_frame_set_visibility(frame, false);
	libdecor_frame_set_minimized_orig(frame);
}

