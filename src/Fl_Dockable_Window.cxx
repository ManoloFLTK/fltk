//
// Dockable window code for the Fast Light Tool Kit (FLTK).
//
// Copyright 2025 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//
/** \file
 Fl_Dockable_Window implementation.
 */

#include <FL/Fl_Dockable_Window.H>
#include "Fl_Dockable_Window_Driver.H"


Fl_Dockable_Window::Fl_Dockable_Window(int x, int y, int w, int h, const char *t) : Fl_Double_Window(x,y,w,h,t) {
  target_box_ = NULL;
#ifdef FLTK_USE_WAYLAND
  if (fl_wl_display()) {
    driver_ = new Fl_Wayland_Dockable_Window_Driver();
    return;
  }
#endif
  driver_ = new Fl_Dockable_Window_Driver();
}


void Fl_Dockable_Window_Driver::delete_win_cb(Fl_Dockable_Window *w) {
  w->driver_->delete_win(w);
}


void Fl_Dockable_Window::target_box(Fl_Boxtype bt, int x, int y, int w, int h, const char *t, Fl_Group *g) {
  if (w == 0 || h == 0) target_box_ = NULL;
  else {
    Fl_Group *save = Fl_Group::current();
    Fl_Group::current(NULL);
    target_box_ = driver_->new_target_box(bt, x, y, w, h, t);
    g->add(target_box_);
    Fl_Group::current(save);
  }
}


void Fl_Dockable_Window_Driver::delete_win(Fl_Dockable_Window *dock) {
  delete dock;
}


int Fl_Dockable_Window_Driver::drag_box_out::handle(int event) {
  Fl_Dockable_Window *dock = (Fl_Dockable_Window*)window();
  return Fl_Dockable_Window_Driver::driver(dock)->handle(this, event);
}


int Fl_Dockable_Window_Driver::handle(Fl_Dockable_Window_Driver::drag_box_out *box, int event) {
  static int fromx, fromy, winx, winy;
  Fl_Dockable_Window *dock = (Fl_Dockable_Window*)box->window();
  if (event == FL_PUSH && (Fl::event_state() & FL_BUTTON1) && dock->parent()) {
    Fl_Window *top = dock->top_window();
    
      dock->hide();
      top->remove(dock);
      dock->position(top->x() + dock->x() , top->y() + dock->y() );
    
    dock->callback((Fl_Callback0*)Fl_Dockable_Window_Driver::delete_win_cb);
    box->label("Drag");
    dock->border(0);
    box->can_dock_ = false;
    dock->show();
    return 1;
  } else if (event == FL_PUSH && (Fl::event_state() & FL_BUTTON1)) {
    fromx = Fl::event_x_root();
    fromy = Fl::event_y_root();
    winx = dock->x_root();
    winy = dock->y_root();
    return 1;
  } else if (event == FL_DRAG && !dock->parent()) {
    int deltax = Fl::event_x_root() - fromx;
    int deltay = Fl::event_y_root() - fromy;
    dock->position(winx + deltax, winy + deltay);
    Fl_Box *target = dock->target_box();
    if (target) {
      int target_x = target->window()->x() + target->x();
      int target_y = target->window()->y() + target->y();
      box->can_dock_ = (dock->x() + dock->w() >= target_x &&
                   dock->x() < target_x + target->w() &&
                   dock->y() >= target_y && dock->y() < target_y + target->h());
      if (box->can_dock_){
        box->label("Dock");
        target->color(FL_RED);
        target->redraw();
      } else {
        box->label("Drag");
        target->color(FL_BACKGROUND_COLOR);
        target->redraw();
      }
    }
    return 1;
  } else if (event == FL_RELEASE && !dock->parent() && box->can_dock_) {
    Fl_Box *target = dock->target_box();
    Fl_Window *parent = target->window();
    target->hide();
    dock->hide();
    parent->add(dock);
    dock->position(target->x(), target->y());
    box->label("Undock");
    dock->target_box(FL_NO_BOX, 0, 0, 0, 0, NULL, NULL);
    delete target;
    dock->show();
    return 1;
  }
  return box->Fl_Box::handle(event);
}


void Fl_Dockable_Window::command_box(int x, int y, int w, int h, const char *t) {
  Fl_Dockable_Window_Driver::drag_box_out *drag = new Fl_Dockable_Window_Driver::drag_box_out(FL_DOWN_BOX, x,y,w,h, t);
  this->add(drag);
  driver_->command_box(drag);
}
