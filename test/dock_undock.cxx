#include "config.h"
#include <FL/Fl_Dockable_Group.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Round_Clock.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Flex.H>

#if HAVE_GL

#include <FL/gl.h>
#include <FL/Fl_Gl_Window.H>
#include <math.h>
/* Some <math.h> files do not define M_PI... */
#ifndef M_PI
#define M_PI 3.14159265
#endif

static bool highDPI = (Fl::use_high_res_GL(1), true);

class shape_window : public Fl_Gl_Window {
  void draw() FL_OVERRIDE;
  int handle(int) FL_OVERRIDE;
public:
  int sides;
  shape_window(int x,int y,int w,int h,const char *l=0);
};

shape_window::shape_window(int x, int y, int w, int h, const char *l) : Fl_Gl_Window(x,y,w,h,l) {
  sides = 3;
  begin();
  Fl_Box *text = new Fl_Box(FL_NO_BOX, 0, 0, 70, 110, "GL scene");
  text->labelcolor(FL_WHITE);
  end();
  resizable(text);
}

int shape_window::handle(int event) {
  if (event == FL_SHOW) gl_texture_reset();
  return Fl_Gl_Window::handle(event);
}

void shape_window::draw() {
  if (!valid()) {
    valid(1);
    glLoadIdentity();
    glViewport(0, 0, pixel_w(), pixel_h());
  }
  glClear(GL_COLOR_BUFFER_BIT);
  glColor3f(.5f, .6f, .7f);
  glBegin(GL_POLYGON);
  for (int j=0; j<sides; j++) {
    double ang = j*2*M_PI/sides;
    glVertex3f((GLfloat)cos(ang), (GLfloat)sin(ang), 0);
  }
  glEnd();
  Fl_Gl_Window::draw();
}

#else // ! HAVE_GL

class shape_window : public Fl_Box {
public:
  shape_window(int x,int y,int w,int h) : Fl_Box(FL_DOWN_BOX, x, y, w, h, "GL scene") {
    align(FL_ALIGN_INSIDE);
    color(FL_GRAY_RAMP + 10);
  }
};

#endif // HAVE_GL


void delete_win(Fl_Window *win) {
  delete win;
}


void dock_cb_f(Fl_Dockable_Group *dock, Fl_Dockable_Group::dock_event e, void*) {
  const char *kind = "UNDOCKED";
  if (e == Fl_Dockable_Group::DOCKED) kind = "DOCKED";
  else if (e == Fl_Dockable_Group::HOME) kind = "HOME";
  printf("dock_cb_f(%s in %s , %s)\n", dock->label(), dock->top_window()->label(), kind);
}

int main(int argc, char **argv) {
  Fl_Window *source = new Fl_Window(150, 150, 340, 180, "(Fl_Tabs 1)");
  source->callback((Fl_Callback0*)delete_win);
  Fl_Tabs *tabs = new Fl_Tabs(20, 40, 310, 115);
  Fl_Dockable_Group *dock = new Fl_Dockable_Group(20, 60, 310, 95, "Fl_Dockable_Group-1");
  dock->color(FL_YELLOW);
  dock->box(FL_THIN_UP_BOX);
  new Fl_Round_Clock(265, 90, 60, 60);
  Fl_Box *r = new Fl_Box(FL_NO_BOX, 0, 151, 155, 10, NULL);
  dock->end();
  dock->resizable(r);
  tabs->resizable(new Fl_Box(FL_DOWN_BOX, 20, 60, 310, 95, "Tab 2"));
  new Fl_Box(FL_DOWN_BOX, 20, 60, 310, 95, "Tab 3");
  tabs->end();
  source->end();
  source->resizable(dock);
  dock->command_box(27, 67, 300, 13);
  dock->command_box()->labelsize(10);
  source->show(argc, argv);
  
  source = new Fl_Window(source->x() + source->w() + 50, source->y(), 340, 180, "(Fl_Flex)");
  source->callback((Fl_Callback0*)delete_win);
  Fl_Flex *flex = new Fl_Flex(5, 60, 330, 95, Fl_Flex::HORIZONTAL);
  Fl_Box *c1 = new Fl_Box(FL_UP_BOX, 0, 0, 0, 0, "Left");
  flex->fixed(c1, 60);
  dock = new Fl_Dockable_Group(0,0,flex->w()-60,flex->h(), "Fl_Dockable_Group-2");
  dock->begin();
  dock->color(FL_GREEN);
  dock->box(FL_THIN_UP_BOX);
  new Fl_Clock(210, 5, 55, 55);
  r = new Fl_Box(FL_NO_BOX, 66, 60, 95, 10, NULL);
  dock->resizable(r);
  dock->command_box(5, 75, 60, 13);
  dock->command_box()->labelsize(10);
  dock->dock_cb(dock_cb_f);
  dock->end();
  Fl_Box *c2 = new Fl_Box(FL_UP_BOX, 0, 0, 0, 0, "Right");
  flex->fixed(c2, 60);
  flex->end();
  source->end();
  source->resizable(flex);
  source->show();

  Fl_Window *destination = new Fl_Window(150, source->y() + source->h() + 50, 340, 190, "(Fl_Tabs 2)");
  destination->callback((Fl_Callback0*)delete_win);
  new Fl_Input(4,5,100,30, NULL);
  tabs = new Fl_Tabs(4, 40, 310, 140);
  new Fl_Box(FL_DOWN_BOX, 4, 60, 310, 120, "Tab 1");
  Fl_Box *target = Fl_Dockable_Group::target_box(4, 60, 310, 120, tabs);
  new Fl_Box(FL_DOWN_BOX, 4, 60, 310, 120, "Tab 3");
  tabs->end();
  tabs->value(target);
  destination->end();
  destination->show();

  destination = new Fl_Window(destination->x() + destination->w() + 50, source->y() + source->h() + 50,
                              340, 190, "(Fl_Window)");
  destination->callback((Fl_Callback0*)delete_win);
  destination->end();
  Fl_Dockable_Group::target_box(10, 30, 320, 150, destination);
  destination->show();
  
  source = new Fl_Window(source->x()+source->w()+30, source->y(), 340, 180, "(Fl_Tile)");
  source->callback((Fl_Callback0*)delete_win);
  Fl_Tile *tile = new Fl_Tile(5, 20, 330, 145, "");
  Fl_Box *yellow = new Fl_Box(FL_DOWN_BOX, 5, 20, 250, 145, "");
  yellow->color(FL_YELLOW);
  dock = new Fl_Dockable_Group(255, 20, 80, 145, "");
  dock->box(FL_DOWN_BOX);
  dock->color(FL_BLUE);
  dock->command_box(260, 25, 70, 20);
  dock->begin();
  shape_window *gl = new shape_window(260, 50, 70, 110);
  dock->resizable(gl);
  dock->end();
  tile->end();
  source->end();
  source->resizable(tile);
  
  source->show();
  return Fl::run();
}
