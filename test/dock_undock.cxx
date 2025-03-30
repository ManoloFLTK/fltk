#include <FL/Fl_Dockable_Window.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Round_Clock.H>


int main(int argc, char **argv) {
  Fl_Window *source = new Fl_Window(150, 150, 340, 180, "source");
  Fl_Dockable_Group *dock = new Fl_Dockable_Group(20, 40, 310, 110, "Fl_Dockable_Group");
  dock->color(FL_YELLOW);
  dock->box(FL_THIN_UP_BOX);
  new Fl_Round_Clock(105, 45, 100, 100);
  new Fl_Clock(205, 45, 100, 100);
  dock->end();
  source->end();
  dock->command_box(25, 125, 50, 20, "Undock");
  
  Fl_Window *destination = new Fl_Window(source->x(), source->y() + source->h() + 50, 340, 180, "destination");
  destination->end();
  dock->target_box(FL_DOWN_BOX, 20, 40, 60, 30, "Target", destination);
  dock->target_box()->labelfont(FL_ITALIC);
  
  destination->show(argc, argv);
  source->show();
  return Fl::run();
}
