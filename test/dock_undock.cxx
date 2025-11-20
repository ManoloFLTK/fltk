#include <FL/Fl_Dockable_Group.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Round_Clock.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Tile.H>
#include <FL/platform.H>


void delete_win(Fl_Window *win) {
  delete win;
}


class dockable_tabs : public Fl_Tabs {
private:
  static bool inside_f(Fl_Widget *target) {
    Fl_Tabs *tabs = (Fl_Tabs*)target;
    bool inside = Fl_Dockable_Group::is_dockable_inside(tabs); // true if mouse inside Fl_Tabs
    if (inside) { // is mouse inside the Fl_Tabs' tab area?
      int X, Y, W, H; // rectangle of tab area relatively to Fl_Tabs top-left
      if (tabs->children() > 0) {
        X = tabs->child(0)->x(); Y= 0; W = tabs->child(0)->w(); H = tabs->h() - tabs->child(0)->h();
      } else {
        tabs->client_area(X, Y, W, H);
        Y = 0; H = tabs->h() - H;
      }
      int pos_x, pos_y; // screen coordinates of Fl_Tabs top-left
      Fl_Window *top = tabs->top_window_offset(pos_x, pos_y);
      pos_x += top->x();
      pos_y += top->y();
      // mouse position relatively to Fl_Tabs top-left
      int e_x = Fl::event_x_root() - pos_x;
      int e_y = Fl::event_y_root() - pos_y;
      inside = (e_x >= X && e_x < X + W && e_y >= Y && e_y < Y + H);
    }
    return inside;
  }
  
  static void dock_payload_f(Fl_Widget *target, Fl_Dockable_Group *payload) {
    Fl_Tabs *tabs = (Fl_Tabs*)target;
    int x, y, w, h;
    tabs->client_area(x, y, w, h);
    payload->resize(x, y, w, h);
    tabs->add(payload);
    tabs->value(payload);
  }
  
  static void undock_f(Fl_Widget *target) {
    Fl_Tabs *tabs = (Fl_Tabs*)target;
    Fl_Widget *c = tabs->child(tabs->children() - 1);
    tabs->remove(c);
    delete c;
  }
  
public:
  dockable_tabs(int x, int y, int w, int h, const char *l=NULL) : Fl_Tabs(x,y,w,h,l) {}

  int handle(int event) override {
    bool processed;
    int retval =  Fl_Dockable_Group::handle_target(processed, this, event,
                                                    inside_f, NULL, dock_payload_f, undock_f);
    return (processed ? retval : Fl_Tabs::handle(event));
  }
};


int main(int argc, char **argv) {
  fl_open_display();
  Fl_Window *source = new Fl_Window(150, 150, 340, 180, "source (dockable_tabs)");
  source->callback((Fl_Callback0*)delete_win);
  Fl_Tabs *tabs = new dockable_tabs(20, 40, 310, 115);
  int X, Y, W, H;
  tabs->client_area(X, Y, W, H);
  Fl_Dockable_Group *dock = new Fl_Dockable_Group(X, Y, W, H, "Fl_Dockable_Group-1");
  dock->color(FL_YELLOW);
  dock->box(FL_THIN_UP_BOX);
  new Fl_Round_Clock(160, 90, 60, 60);
  new Fl_Clock(245, 90, 60, 60);
  Fl_Box *r = new Fl_Box(FL_NO_BOX, 0, 151, 155, 10, NULL);
  dock->end();
  dock->resizable(r);
  new Fl_Box(FL_FLAT_BOX, X, Y, W, H, "Tab 2");
  tabs->end();
  source->end();
  source->resizable(dock);
  dock->drag_box(27, 67, 300, 13);
  dock->drag_box()->labelsize(10);
  source->show(argc, argv);
  
  source = new Fl_Window(source->x() + source->w() + 50, source->y(), 340, 180, "source (group)");
  source->callback((Fl_Callback0*)delete_win);
  dock = new Fl_Dockable_Group(25, 60, 305, 95, "Fl_Dockable_Group-2");
  dock->color(FL_GREEN);
  dock->box(FL_THIN_UP_BOX);
  new Fl_Clock(160, 65, 60, 60);
  new Fl_Round_Clock(245, 65, 60, 60);
  new Fl_Input(30, 110, 60, 20);
  r = new Fl_Box(FL_NO_BOX, 0, 126, 155, 10, NULL);
  dock->end();
  dock->resizable(r);
  source->end();
  source->resizable(dock);
  dock->drag_box(27, 140, 60, 13);
  dock->drag_box()->labelsize(10);
  source->show();

  Fl_Window *destination = new Fl_Window(150, source->y() + source->h() + 50,
                                         340, 190, "destination (dockable_tabs)");
  destination->callback((Fl_Callback0*)delete_win);
  new Fl_Input(5,5,100,30, NULL);
  tabs = new dockable_tabs(5, 40, 310, 140);
  tabs->client_area(X, Y, W, H);
  new Fl_Box(FL_FLAT_BOX, X, Y, W, H, "Tab 1");
  tabs->end();
  tabs->value(dock);
  destination->end();
  destination->resizable(destination);
  destination->show();

  destination = new Fl_Window(destination->x() + destination->w() + 50, source->y() + source->h() + 50,
                              340, 190, "destination (Dockable_Box)");
  destination->callback((Fl_Callback0*)delete_win);
  new Fl_Dockable_Group::Dockable_Box(10, 30, 320, 150);
  
  destination->end();
  destination->show();
  
  source = new Fl_Window(source->x()+source->w()+30, source->y(), 340, 180, "source (Fl_Tile)");
  Fl_Tile *tile = new Fl_Tile(5, 5, 330, 160, "");
  Fl_Box *yellow = new Fl_Box(FL_DOWN_BOX, 5, 5, 250, 160, "");
  yellow->color(FL_YELLOW);
  dock = new Fl_Dockable_Group(255, 5, 80, 160, "Dockable");
  dock->box(FL_DOWN_BOX);
  dock->align(FL_ALIGN_INSIDE);
  dock->color(FL_BLUE);
  dock->drag_box(260, 10, 70, 20);
  dock->begin();
  new Fl_Round_Clock(300, 130, 30, 30);
  dock->resizable(new Fl_Box(260, 40, 30, 50, ""));
  dock->end();
  tile->end();
  source->end();
  source->resizable(tile);
  
  source->show();
  return Fl::run();
}
