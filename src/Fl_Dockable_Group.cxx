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

#include <FL/Fl_Dockable_Group.H>
#include "Fl_Dockable_Group_Driver.H"


Fl_Dockable_Group::Fl_Dockable_Group(int x, int y, int w, int h, const char *t) : Fl_Group(x,y,w,h,t) {
  state = UNDOCK;
#ifdef FLTK_USE_WAYLAND
  if (fl_wl_display()) {
    driver_ = new Fl_Wayland_Dockable_Group_Driver();
    return;
  }
#endif
  driver_ = new Fl_Dockable_Group_Driver();
  target_index_ = -1;
}


Fl_Dockable_Group::~Fl_Dockable_Group() {
  while (target_.size()) {
    target_.erase(target_.begin());
  }
}

void Fl_Dockable_Group_Driver::delete_win_cb(Fl_Window *win) {
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)win->child(0);
  win->hide();
  win->remove(dock);
  Fl_Group *parent = dock->parent_when_docked_;
  dock->position(dock->x_when_docked_, dock->y_when_docked_);
  parent->add(dock);
  dock->hide(); // to re-activate widgets in dock (e.g. make clocks tick again)
  dock->show();
  parent->redraw();
  dock->state = (dock->target_count() > 0 ? Fl_Dockable_Group::UNDOCK : Fl_Dockable_Group::DOCKED);
  dock->command_box()->label( (dock->target_count() > 0 ? "Undock" : "Docked") );
  delete win;
}


Fl_Box *Fl_Dockable_Group::target_box(Fl_Boxtype bt, int x, int y, int w, int h, const char *t, Fl_Group *g) {
  if (w == 0 || h == 0) {
    target_.erase(target_.begin() + target_index_);
    return NULL;
  } else {
    Fl_Group *save = Fl_Group::current();
    Fl_Group::current(NULL);
    Fl_Box *target_box = driver_->new_target_box(bt, x, y, w, h, t, this);
    target_box->labelfont(FL_ITALIC);
    g->add(target_box);
    target_.insert(target_.begin(), target_box);
    Fl_Group::current(save);
    return target_box;
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
  if (event == FL_PUSH && (Fl::event_state() & FL_BUTTON1) &&
      (dock->state == Fl_Dockable_Group::UNDOCK || dock->state == Fl_Dockable_Group::DRAG)) {
    Fl_Group *top = dock->parent();
    Fl_Window *top_win = top->top_window();
    fromx = Fl::event_x_root();
    fromy = Fl::event_y_root();
    int offset_x = 0, offset_y = 0;
    if (top->window()) top->window()->top_window_offset(offset_x, offset_y);
    winx = top_win->x_root() + offset_x + dock->x();
    winy = top_win->y_root() + offset_y + dock->y();
    if (dock->state == Fl_Dockable_Group::UNDOCK) {
      // transform the dockable group into a draggable, borderless toplevel window
      store_docked_position(dock);
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
      win->show();
      dock->state = Fl_Dockable_Group::DRAG;
    }
    return 1;
  } else if (event == FL_DRAG &&
             (dock->state == Fl_Dockable_Group::DRAG || dock->state == Fl_Dockable_Group::DOCK)) { // Drag dockable around
    int deltax = Fl::event_x_root() - fromx;
    int deltay = Fl::event_y_root() - fromy;
    dock->window()->position(winx + deltax, winy + deltay);
    for (int i = 0; i < dock->target_count(); i++) {
      Fl_Box *target = dock->target_[i];
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
      bool can_dock = (dock->state == Fl_Dockable_Group::DOCK);
      if (can_dock != new_can_dock) {
        if (new_can_dock){
          box->label("Dock");
          dock->state = Fl_Dockable_Group::DOCK;
          box->color(FL_RED);
          box->redraw();
          target->color(FL_RED);
          target->redraw();
          dock->target_index_ = i;
          break;
        } else {
          dock->state = Fl_Dockable_Group::DRAG;
          box->label("Drag");
          box->color(FL_BACKGROUND_COLOR);
          box->redraw();
          target->color(FL_BACKGROUND_COLOR);
          target->redraw();
          dock->target_index_ = -1;
        }
      }
    }
    return 1;
  } else if (event == FL_RELEASE && dock->state == Fl_Dockable_Group::DOCK) { // Dock dockable in place
    Fl_Box *target = dock->target_box(dock->target_index_);
    Fl_Window *parent = target->window();
    target->hide();
    Fl_Window *top = dock->window();
    top->hide();
    top->remove(dock);
    delete top;
    parent->add(dock);
    dock->position(target->x(), target->y());
    box->label("Docked");
    dock->state = Fl_Dockable_Group::DOCKED;
    box->color(FL_BACKGROUND_COLOR);
    dock->target_box(FL_NO_BOX, 0, 0, 0, 0, NULL, NULL);
    dock->clear_visible();
    dock->show();
    return 1;
  }
  return box->Fl_Box::handle(event);
}


void Fl_Dockable_Group::command_box(int x, int y, int w, int h, const char *t) {
  Fl_Dockable_Group_Driver::drag_box_out *drag = new Fl_Dockable_Group_Driver::drag_box_out(FL_DOWN_BOX, x,y,w,h, t);
  this->add(drag);
  command_box_ = drag;
}


void Fl_Dockable_Group_Driver::store_docked_position(Fl_Dockable_Group *dock) {
  dock->parent_when_docked_ = dock->parent();
  dock->x_when_docked_ = dock->x();
  dock->y_when_docked_ = dock->y();
}
