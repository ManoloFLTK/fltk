#include <FL/Fl_Dockable_Group.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Round_Clock.H>


int main(int argc, char **argv) {
  Fl_Window *source = new Fl_Window(150, 150, 340, 180, "source");
  Fl_Dockable_Group *dock = new Fl_Dockable_Group(20, 40, 310, 115, "Fl_Dockable_Group");
  dock->color(FL_YELLOW);
  dock->box(FL_THIN_UP_BOX);
  new Fl_Round_Clock(105, 70, 80, 80);
  new Fl_Clock(205, 70, 80, 80);
  dock->end();
  source->end();
  dock->command_box(22, 42, 306, 20, "Undock");
  
  Fl_Window *destination = new Fl_Window(source->x(), source->y() + source->h() + 50, 340, 180, "destination");
  destination->end();
  dock->target_box(FL_DOWN_BOX, 20, 40, 60, 30, "Target", destination);
  
  destination->show(argc, argv);
  source->show();
  return Fl::run();
}
