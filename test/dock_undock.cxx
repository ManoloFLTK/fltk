#include <FL/Fl_Dockable_Group.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Round_Clock.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Box.H>


int main(int argc, char **argv) {
  Fl_Window *source = new Fl_Window(150, 150, 340, 180, "source");
  Fl_Dockable_Group *dock = new Fl_Dockable_Group(20, 40, 310, 115, "Fl_Dockable_Group");
  dock->color(FL_YELLOW);
  dock->box(FL_THIN_UP_BOX);
  new Fl_Round_Clock(160, 65, 80, 80);
  new Fl_Clock(245, 65, 80, 80);
  Fl_Box *r = new Fl_Box(FL_NO_BOX, 0, 145, 155, 10, NULL);
  dock->end();
  dock->resizable(r);
  source->end();
  source->resizable(dock);
  dock->command_box(22, 42, 306, 20);
  
  Fl_Window *destination = new Fl_Window(source->x(), source->y() + source->h() + 50, 340, 180, "destination");
  new Fl_Input(5,5,100,30, NULL);
  destination->end();
  dock->target_box(FL_DOWN_BOX, 20, 40, 310, 115, destination);
  
  destination->show(argc, argv);
  source->show();
  return Fl::run();
}
