#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Radio_Round_Button.H>
#include <FL/fl_ask.H>
#include <FL/platform.H>

void doit(Fl_Widget *wid, void *) {
  //fl_message_position(wid);
  //fl_message_position(50,150);
  fl_message_title("Message title");
  fl_message("message");
}

#if FLTK_USE_WAYLAND
  extern bool new_behavior;
#else
  bool new_behavior = true;
#endif

void cb(Fl_Widget *, void *d) {
  new_behavior = ! new_behavior;
  ((Fl_Widget*)d)->take_focus();
}

int main(int argc, char **argv) {
  Fl_Window *window = new Fl_Window(340, 180, "top-level");
  Fl_Box behave(FL_NO_BOX, 55, 5, 80, 25,"Behavior:");
  Fl_Radio_Round_Button *old_b = new Fl_Radio_Round_Button(85+50,5, 45, 25, "old");
  Fl_Radio_Round_Button *new_b = new Fl_Radio_Round_Button(130+50, 5, 50, 25, "new");
  new_b->setonly();
  Fl_Button *box = new Fl_Return_Button(20, 40, 300, 100, "Show message");
  box->box(FL_UP_BOX);
  box->labelsize(25);
  box->callback(doit);
  old_b->callback(cb, box);
  new_b->callback(cb, box);
  window->end();
  window->position(300,350);
  window->show(argc, argv);
  box->take_focus();
  return Fl::run();
}
