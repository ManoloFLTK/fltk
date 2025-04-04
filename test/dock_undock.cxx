#include <FL/Fl_Dockable_Group.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Round_Clock.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tabs.H>


int main(int argc, char **argv) {
  Fl_Window *source = new Fl_Window(150, 150, 340, 180, "source (tab)");
  Fl_Tabs *tabs = new Fl_Tabs(20, 40, 310, 115);
  Fl_Dockable_Group *dock = new Fl_Dockable_Group(25, 60, 305, 95, "Fl_Dockable_Group");
  dock->color(FL_YELLOW);
  dock->box(FL_THIN_UP_BOX);
  new Fl_Round_Clock(160, 90, 60, 60);
  new Fl_Clock(245, 90, 60, 60);
  Fl_Box *r = new Fl_Box(FL_NO_BOX, 0, 151, 155, 10, NULL);
  dock->end();
  dock->resizable(r);
  new Fl_Box(FL_FLAT_BOX, 25, 60, 305, 95, "Tab 2");
  tabs->end();
  source->end();
  source->resizable(dock);
  dock->command_box(27, 67, 300, 13);
  dock->command_box()->labelsize(10);
  
  Fl_Window *destination = new Fl_Window(source->x(), source->y() + source->h() + 50,
                                         340, 190, "destination (tab)");
  new Fl_Input(5,5,100,30, NULL);
  tabs = new Fl_Tabs(5, 40, 310, 140);
  new Fl_Box(FL_FLAT_BOX, 10, 60, 300, 120, "Tab 1");
  dock->target_box(FL_DOWN_BOX, 10, 60, 300, 120, tabs);
  tabs->end();
  destination->end();
  destination->show(argc, argv);

  destination = new Fl_Window(source->x() + destination->w() + 50, source->y() + source->h() + 50,
                              340, 190, "destination (group)");
  destination->end();
  dock->target_box(FL_DOWN_BOX, 10, 30, 320, 150, destination);
  destination->show();
  
  source->show();
  return Fl::run();
}
