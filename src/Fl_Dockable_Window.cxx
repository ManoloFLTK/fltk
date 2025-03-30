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
 Fl_Dockable_Group implementation.
 */

#include <FL/Fl_Dockable_Window.H>
#include "Fl_Dockable_Window_Driver.H"


Fl_Dockable_Group::Fl_Dockable_Group(int x, int y, int w, int h, const char *t) : Fl_Group(x,y,w,h,t) {
  target_box_ = NULL;
#ifdef FLTK_USE_WAYLAND
  if (fl_wl_display()) {
    driver_ = new Fl_Wayland_Dockable_Group_Driver();
    return;
  }
#endif
  driver_ = new Fl_Dockable_Group_Driver();
}


void Fl_Dockable_Group_Driver::delete_win_cb(Fl_Window *win) {
  Fl_Dockable_Group *w = (Fl_Dockable_Group*)win->child(0);
  w->driver_->delete_win(w);
}


void Fl_Dockable_Group::target_box(Fl_Boxtype bt, int x, int y, int w, int h, const char *t, Fl_Group *g) {
  if (w == 0 || h == 0) target_box_ = NULL;
  else {
    Fl_Group *save = Fl_Group::current();
    Fl_Group::current(NULL);
    target_box_ = driver_->new_target_box(bt, x, y, w, h, t, this);
    g->add(target_box_);
    Fl_Group::current(save);
  }
}


void Fl_Dockable_Group_Driver::delete_win(Fl_Dockable_Group *dock) {
  delete dock->window();
}


int Fl_Dockable_Group_Driver::drag_box_out::handle(int event) {
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)parent();
  return Fl_Dockable_Group_Driver::driver(dock)->handle(this, event);
}


int Fl_Dockable_Group_Driver::handle(Fl_Dockable_Group_Driver::drag_box_out *box, int event) {
  static int fromx, fromy, winx, winy;
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)box->parent();
  if (event == FL_PUSH && (Fl::event_state() & FL_BUTTON1) && strcmp(box->label(), "Undock")==0) {
    Fl_Group *top = dock->parent();
    Fl_Window *top_win = top->top_window();
    fromx = Fl::event_x_root();
    fromy = Fl::event_y_root();
    int offset_x = 0, offset_y = 0;
    if (top->window()) top->window()->top_window_offset(offset_x, offset_y);
    winx = top_win->x_root() + offset_x + dock->x();
    winy = top_win->y_root() + offset_y + dock->y();
    // transform the dockable group into a draggable, borderless toplevel window
    top->remove(dock);
    top->redraw();
    Fl_Window *win = new Fl_Window(winx, winy, dock->w(), dock->h(), "Drag");
    // TODO fix position of dock's widgets
    dock->position(0,0);
    win->add(dock);
    win->end();
    win->callback((Fl_Callback0*)Fl_Dockable_Group_Driver::delete_win_cb);
    box->label("Drag");
    win->border(0);
    box->can_dock_ = false;
    win->show();
    return 1;
  } else if (event == FL_PUSH && !dock->window()->parent()) {
    return 1;
  } else if (event == FL_DRAG && !dock->window()->parent()) { // Drag dockable around
    int deltax = Fl::event_x_root() - fromx;
    int deltay = Fl::event_y_root() - fromy;
    dock->window()->position(winx + deltax, winy + deltay);
    Fl_Box *target = dock->target_box();
    if (target) {
      int target_x, target_y;
      Fl_Window *top = target->top_window_offset(target_x, target_y);
      target_x += top->x();
      target_y += top->y();
      int dock_x = dock->window()->x() + dock->x();
      int dock_y = dock->window()->y() + dock->y();
      // does dockable partially cover target ?
      bool new_can_dock = (dock_x + dock->w() >= target_x &&
                           dock_x < target_x + target->w() &&
                           dock_y + dock->h() >= target_y &&
                           dock_y < target_y + target->h());
      if (box->can_dock_ != new_can_dock) {
        box->can_dock_ = new_can_dock;
        if (box->can_dock_){
          box->label("Dock");
          box->color(FL_RED);
          box->redraw();
          target->color(FL_RED);
          target->redraw();
        } else {
          box->label("Drag");
          box->color(FL_BACKGROUND_COLOR);
          box->redraw();
          target->color(FL_BACKGROUND_COLOR);
          target->redraw();
        }
      }
    }
    return 1;
  } else if (event == FL_RELEASE && !dock->window()->parent() && box->can_dock_) { // Dock dockable in place
    Fl_Box *target = dock->target_box();
    Fl_Window *parent = target->window();
    target->hide();
    Fl_Window *top = dock->window();
    top->hide();
    top->remove(dock);
    delete top;
    parent->add(dock);
    dock->position(target->x(), target->y());
    box->label("Undock");
    box->color(FL_BACKGROUND_COLOR);
    dock->target_box(FL_NO_BOX, 0, 0, 0, 0, NULL, NULL);
    box->can_dock_ = false;
    delete target;
    dock->show();
    return 1;
  }
  return box->Fl_Box::handle(event);
}


void Fl_Dockable_Group::command_box(int x, int y, int w, int h, const char *t) {
  Fl_Dockable_Group_Driver::drag_box_out *drag = new Fl_Dockable_Group_Driver::drag_box_out(FL_DOWN_BOX, x,y,w,h, t);
  this->add(drag);
  driver_->command_box(drag);
}
