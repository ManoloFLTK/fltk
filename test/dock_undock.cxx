#include <FL/Fl_Dockable_Group.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Round_Clock.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Tile.H>

void delete_win(Fl_Window *win) {
  delete win;
}


class dockable_tabs : public Fl_Tabs {
public:
  dockable_tabs(int x, int y, int w, int h, const char *l=NULL) : Fl_Tabs(x,y,w,h,l) {}
  
  bool in_tabs_area() {
    bool inside = Fl_Dockable_Group::is_dockable_inside(this); // true if mouse inside Fl_Tabs
    if (inside) { // is mouse inside the Fl_Tabs' tab area?
      int X, Y, W, H; // rectangle of tab area relatively to Fl_Tabs top-left
      if (children() > 0) {
        X = child(0)->x(); Y= 0; W = child(0)->w(); H = h() - child(0)->h();
      } else {
        this->client_area(X, Y, W, H);
        Y = 0; H = h() - H;
      }
      int pos_x, pos_y; // screen coordinates of Fl_Tabs top-left
      Fl_Window *top = this->top_window_offset(pos_x, pos_y);
      pos_x += top->x();
      pos_y += top->y();
      // mouse position relatively to Fl_Tabs top-left
      int e_x = Fl::event_x_root() - pos_x;
      int e_y = Fl::event_y_root() - pos_y;
      inside = (e_x >= X && e_x < X + W && e_y >= Y && e_y < Y + H);
    }
    return inside;
  }
  
  int handle(int event) {
    if (event == FL_DND_ENTER) event = FL_DOCK_ENTER;
    else if (event == FL_DND_DRAG) event = FL_DOCK_DRAG;
    else if (event == FL_DND_LEAVE) event = FL_DOCK_LEAVE;
    else if (event == FL_DND_RELEASE) event = FL_DOCK_RELEASE;
    
  /*if (event == FL_DOCK_ENTER)   puts("FL_DOCK_ENTER:");
    if (event == FL_DOCK_DRAG)   puts("FL_DOCK_DRAG:");
    if (event == FL_DOCK_RELEASE) puts("FL_DOCK_RELEASE:");
    if (event == FL_DOCK_LEAVE) puts("FL_DOCK_LEAVE:"); */

    bool inside = in_tabs_area(); // true if mouse is inside the Fl_Tabs' tabs area
    if (event == FL_DOCK_ENTER || event == FL_DOCK_DRAG) {
      int retval = 1;
      if (inside) {
        Fl::belowmouse(this);
        if (Fl_Dockable_Group::active_dockable) {
          Fl_Dockable_Group::active_dockable->state(Fl_Dockable_Group::DOCK);
        }
      } else  {
        handle(FL_DOCK_LEAVE);
        retval = 0; // important
      }
      return retval;
    } else if (event == FL_DOCK_LEAVE) {
      if (Fl_Dockable_Group::active_dockable) {
        Fl_Dockable_Group::active_dockable->state(Fl_Dockable_Group::DRAG);
      }
      Fl::belowmouse(NULL);
      return 1;
    } else if (event == FL_DOCK_RELEASE && Fl_Dockable_Group::active_dockable) {
      int X, Y, W, H;
      this->client_area(X, Y, W, H);
      Fl_Dockable_Group::active_dockable->resize(X, Y, W, H);
      Fl_Window *top = Fl_Dockable_Group::active_dockable->window();
      top->remove(Fl_Dockable_Group::active_dockable);
      delete top;
      add(Fl_Dockable_Group::active_dockable);
      value(Fl_Dockable_Group::active_dockable);
      Fl_Dockable_Group::active_dockable->after_release();
      Fl_Dockable_Group::active_dockable->state(Fl_Dockable_Group::UNDOCK);
      Fl_Dockable_Group::active_dockable = NULL;
      return 1;
    } else if (event == FL_UNDOCK) {
      Fl_Widget *c = child(children() - 1);
      remove(c);
      delete c;
      return 1;
    }
    return Fl_Tabs::handle(event);
  }
};


int main(int argc, char **argv) {
  Fl_Window *source = new Fl_Window(150, 150, 340, 180, "source (dockable_tabs)");
  source->callback((Fl_Callback0*)delete_win);
  Fl_Tabs *tabs = new dockable_tabs(20, 40, 310, 115);
  Fl_Dockable_Group *dock = new Fl_Dockable_Group(25, 60, 305, 95, "Fl_Dockable_Group-1");
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
  new Fl_Box(FL_FLAT_BOX, 10, 60, 300, 120, "Tab 1");
  tabs->end();
  tabs->value(dock);
  destination->end();
  destination->resizable(destination);
  destination->show();

  destination = new Fl_Window(destination->x() + destination->w() + 50, source->y() + source->h() + 50,
                              340, 190, "destination (Dockable_Box)");
  destination->callback((Fl_Callback0*)delete_win);
  Fl_Dockable_Group::newDockableBox(10, 30, 320, 150);
  
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
