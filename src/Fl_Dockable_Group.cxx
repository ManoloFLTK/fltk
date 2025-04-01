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


Fl_Dockable_Group *Fl_Dockable_Group::active_dockable = NULL;


Fl_Dockable_Group::Fl_Dockable_Group(int x, int y, int w, int h, const char *t) : Fl_Group(x,y,w,h,t) {
  state = UNDOCK;
  target_index_ = -1;
#ifdef FLTK_USE_WAYLAND
  if (fl_wl_display()) {
    driver_ = new Fl_Wayland_Dockable_Group_Driver(this);
  } else
#endif
  {
    driver_ = new Fl_Dockable_Group_Driver(this);
  }
  color_for_states();
  label_for_states();
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
  Fl_Dockable_Group_Driver::driver(dock)->state( (dock->target_count() > 0 ? Fl_Dockable_Group::UNDOCK : Fl_Dockable_Group::DOCKED) );
  Fl_Dockable_Group::active_dockable = NULL;
  delete win;
}


Fl_Box *Fl_Dockable_Group::target_box(Fl_Boxtype bt, int x, int y, int w, int h, Fl_Group *g) {
  if (w == 0 || h == 0) {
    if (target_index_ >= 0) target_.erase(target_.begin() + target_index_);
    return NULL;
  } else {
    Fl_Group *save = Fl_Group::current();
    Fl_Group::current(NULL);
    Fl_Box *target_box = driver_->new_target_box(bt, x, y, w, h, this);
    g->add(target_box);
    target_.insert(target_.begin(), target_box);
    Fl_Group::current(save);
    return target_box;
  }
}


int Fl_Dockable_Group_Driver::drag_box_out::handle(int event) {
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)parent();
  return Fl_Dockable_Group_Driver::driver(dock)->handle(this, event);
}


int Fl_Dockable_Group_Driver::handle(Fl_Dockable_Group_Driver::drag_box_out *box, int event) {
  static int fromx, fromy, winx, winy, drag_count;
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)box->parent();
  if (event == FL_PUSH && (Fl::event_state() & FL_BUTTON1) &&
      (dock->state == Fl_Dockable_Group::UNDOCK || dock->state == Fl_Dockable_Group::DRAG)) {
    if (dock->state == Fl_Dockable_Group::UNDOCK) drag_count = 0;
    Fl_Group *top = dock->parent();
    Fl_Window *top_win = top->top_window();
    fromx = Fl::event_x_root();
    fromy = Fl::event_y_root();
    int offset_x = 0, offset_y = 0;
    if (top->window()) top->window()->top_window_offset(offset_x, offset_y);
    winx = top_win->x_root() + offset_x + dock->x();
    winy = top_win->y_root() + offset_y + dock->y();
    return 1;
  } else if (event == FL_DRAG && dock->state == Fl_Dockable_Group::UNDOCK && (Fl::event_state() & FL_BUTTON1)) {
    if (++drag_count > 5) {
      Fl_Group *top = dock->parent();
      // transform the dockable group into a draggable, borderless toplevel window
      store_docked_position(dock);
      top->remove(dock);
      top->redraw();
      Fl_Window *win = new Fl_Window(winx, winy, dock->w(), dock->h(), drag_label_);
      dock->position(0,0);
      win->add(dock);
      win->end();
      win->callback((Fl_Callback0*)Fl_Dockable_Group_Driver::delete_win_cb);
      Fl_Dockable_Group_Driver::driver(dock)->state(Fl_Dockable_Group::DRAG);
      win->border(0);
      win->show();
    }
    return 1;
  } else if (event == FL_DRAG &&
             (dock->state == Fl_Dockable_Group::DRAG || dock->state == Fl_Dockable_Group::DOCK)) { // Drag dockable around
    int deltax = Fl::event_x_root() - fromx;
    int deltay = Fl::event_y_root() - fromy;
    dock->window()->position(winx + deltax, winy + deltay);
    for (int i = 0; i < dock->target_count(); i++) {
      Fl_Box *target = dock->target_[i];
      if (!target->visible_r()) continue;
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
      Fl_Dockable_Group::active_dockable = dock;
      if (can_dock != new_can_dock) {
        if (new_can_dock){
          Fl_Dockable_Group_Driver::driver(dock)->state(Fl_Dockable_Group::DOCK);
          dock->target_index_ = i;
          target_box_class *target = (target_box_class*)dock->target_box(i);
          target->state(DOCK_HERE);
          break;
        } else {
          Fl_Dockable_Group_Driver::driver(dock)->state(Fl_Dockable_Group::DRAG);
          dock->target_index_ = -1;
          target_box_class *target = (target_box_class*)dock->target_box(i);
          target->state(MAY_RECEIVE);
        }
      }
    }
    return 1;
  } else if (event == FL_RELEASE && dock->state == Fl_Dockable_Group::DOCK) { // Dock dockable in place
    Fl_Dockable_Group::active_dockable = NULL;
    target_box_class *target = (target_box_class*)dock->target_box(dock->target_index_);
    target->state(INACTIVE);
    Fl_Window *parent = target->window();
    target->hide();
    Fl_Window *top = dock->window();
    top->hide();
    top->remove(dock);
    delete top;
    parent->add(dock);
    dock->resize(target->x(), target->y(), target->w(), target->h());
    Fl_Dockable_Group_Driver::driver(dock)->state(Fl_Dockable_Group::DOCKED);
    dock->target_box(FL_NO_BOX, 0, 0, 0, 0, NULL);
    target->state(INACTIVE);
    dock->clear_visible();
    dock->show();
    return 1;
  }
  return box->Fl_Box::handle(event);
}


void Fl_Dockable_Group::command_box(int x, int y, int w, int h) {
  Fl_Dockable_Group_Driver::drag_box_out *drag = new Fl_Dockable_Group_Driver::drag_box_out(FL_DOWN_BOX, x,y,w,h, NULL);
  this->add(drag);
  command_box_ = drag;
  driver_->state(UNDOCK);
}


void Fl_Dockable_Group_Driver::store_docked_position(Fl_Dockable_Group *dock) {
  dock->parent_when_docked_ = dock->parent();
  dock->x_when_docked_ = dock->x();
  dock->y_when_docked_ = dock->y();
}


void Fl_Dockable_Group::color_for_states(Fl_Color undock, Fl_Color drag,
                                         Fl_Color dock, Fl_Color docked) {
  driver_->undock_color_ = undock;
  driver_->drag_color_ = drag;
  driver_->dock_color_ = dock;
  driver_->docked_color_ = docked;
}


void Fl_Dockable_Group::label_for_states(const char *undock, const char * drag,
                                         const char * dock, const char * docked) {
  driver_->undock_label_ = undock;
  driver_->drag_label_ = drag;
  driver_->dock_label_ = dock;
  driver_->docked_label_ = docked;
}


void Fl_Dockable_Group_Driver::state(enum Fl_Dockable_Group::states state) {
  dockable_->state = state;
  Fl_Color c;
  const char *t;
  if (state == Fl_Dockable_Group::UNDOCK) { c = undock_color_; t = undock_label_; }
  else if (state == Fl_Dockable_Group::DRAG) { c = drag_color_; t = drag_label_; }
  else if (state == Fl_Dockable_Group::DOCK) { c = dock_color_; t = dock_label_; }
  else { c = docked_color_; t = docked_label_; }
  dockable_->command_box()->color(c);
  dockable_->command_box()->labelcolor(fl_contrast(FL_FOREGROUND_COLOR, c));
  dockable_->command_box()->label(t);
  dockable_->command_box()->redraw();
}


void Fl_Dockable_Group_Driver::target_box_class::state(enum Fl_Dockable_Group_Driver::target_states s) {
  state_ = s;
  Fl_Color c;
  const char *t = NULL;
  if (s == Fl_Dockable_Group_Driver::MAY_RECEIVE)  {
    c = (Fl_Dockable_Group::active_dockable ?
         driver(Fl_Dockable_Group::active_dockable)->drag_color_ : FL_BACKGROUND2_COLOR);
    t = "Dock target";
  } else if (s == Fl_Dockable_Group_Driver::DOCK_HERE) {
    c = driver(Fl_Dockable_Group::active_dockable)->dock_color_;
    t = "Accept";
  }
  else hide();
  color(c);
  labelcolor(fl_contrast(FL_FOREGROUND_COLOR, c));
  label(t);
  redraw();
}
