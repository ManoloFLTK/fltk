#include "../../src/drivers/Wayland/Fl_Wayland_Screen_Driver.H"

extern "C" {
  bool fl_libdecor_using_weston(void) {
    return Fl_Wayland_Screen_Driver::compositor == Fl_Wayland_Screen_Driver::WESTON;
  };
}
