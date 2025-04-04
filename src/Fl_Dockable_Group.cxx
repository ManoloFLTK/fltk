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
#ifdef FLTK_USE_WAYLAND
#  include "drivers/Wayland/Fl_Wayland_Screen_Driver.H"
#endif

Fl_Dockable_Group *Fl_Dockable_Group::active_dockable = NULL;


Fl_Dockable_Group::Fl_Dockable_Group(int x, int y, int w, int h, const char *t) : Fl_Group(x,y,w,h,t) {
  state = UNDOCK;
  target_index_ = -1;
#ifdef FLTK_USE_WAYLAND
  fl_open_display();
  if (fl_wl_display()) {
    Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
    driver_ = ( scr_driver->xdg_toplevel_drag ? new Fl_Wayland_Dockable_Group_Driver(this) :
                new Fl_oldWayland_Dockable_Group_Driver(this) );
printf("Using %s\n",( scr_driver->xdg_toplevel_drag ?  "Fl_Wayland_Dockable_Group_Driver" :
                         "Fl_oldWayland_Dockable_Group_Driver" ));
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
  if (dock->target_index_ >= 0) {
    target_box_class *target = (target_box_class*)dock->target_box(dock->target_index_);
    target->state(MAY_RECEIVE);
  }
  win->hide();
  win->remove(dock);
  Fl_Group *parent = dock->parent_when_docked_;
  dock->position(dock->x_when_docked_, dock->y_when_docked_);
  if (dock->next_in_group_when_docked_) parent->insert(*dock, dock->next_in_group_when_docked_);
  else parent->add(dock);
  dock->hide(); // to re-activate widgets in dock (e.g. make clocks tick again)
  dock->show();
  parent->redraw();
  Fl_Dockable_Group_Driver::driver(dock)->state( (dock->target_count() > 0 ? Fl_Dockable_Group::UNDOCK : Fl_Dockable_Group::DOCKED) );
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


void Fl_Dockable_Group::target_box_add(Fl_Box *target_box) {
  target_.insert(target_.begin(), target_box);
}


int Fl_Dockable_Group_Driver::cmd_box_class::handle(int event) {
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)parent();
  return Fl_Dockable_Group_Driver::driver(dock)->handle(this, event);
}


int Fl_Dockable_Group_Driver::handle(Fl_Dockable_Group_Driver::cmd_box_class *box, int event) {
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
      dock->show(); // necessary for tabs
      win->callback((Fl_Callback0*)Fl_Dockable_Group_Driver::delete_win_cb);
      Fl_Dockable_Group_Driver::driver(dock)->state(Fl_Dockable_Group::DRAG);
      win->border(0);
      win->show();
      Fl::pushed(dock->command_box()); // necessary for tabs
    }
    return 1;
  } else if (event == FL_DRAG &&
             (dock->state == Fl_Dockable_Group::DRAG || dock->state == Fl_Dockable_Group::DOCK)) {
    int deltax = Fl::event_x_root() - fromx; // Drag dockable around
    int deltay = Fl::event_y_root() - fromy;
    dock->window()->position(winx + deltax, winy + deltay);
    dock->color_targets_following_dock_();
    return 1;
  } else if (event == FL_RELEASE && dock->state == Fl_Dockable_Group::DOCK) { // Dock dockable in place
    Fl_Dockable_Group::active_dockable = NULL;
    target_box_class *target = (target_box_class*)dock->target_box(dock->target_index_);
    target->state(INACTIVE);
    Fl_Group *parent = target->parent();
    parent->remove(target);
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


void Fl_Dockable_Group::color_targets_following_dock_() {
  bool new_can_dock = false;
  int found = -1;
  for (int i = 0; i < this->target_count(); i++) {
    Fl_Box *target = this->target_[i];
    if (!target->visible_r()) continue;
    int target_x, target_y;
    Fl_Window *top = target->top_window_offset(target_x, target_y);
    target_x += top->x();
    target_y += top->y();
    int dock_x = this->window()->x() + this->x();
    int dock_y = this->window()->y() + this->y();
    // does dockable partially cover target ?
    new_can_dock = (dock_x + this->w() >= target_x &&
                         dock_x < target_x + target->w() &&
                         dock_y + this->h() >= target_y &&
                         dock_y < target_y + target->h());
    if (new_can_dock) {
      found = i;
      break;
    }
  }
  bool can_dock = (this->state == Fl_Dockable_Group::DOCK);
  if (found >= 0 && can_dock && found != this->target_index_) { // dock crosses several target boxes
    found = -1;
    can_dock = false;
  }
  if (can_dock != new_can_dock) {
    if (found >= 0) {
      Fl_Dockable_Group_Driver::driver(this)->state(Fl_Dockable_Group::DOCK);
      this->target_index_ = found;
      Fl_Dockable_Group::active_dockable = this;
      Fl_Dockable_Group_Driver::target_box_class *target = (Fl_Dockable_Group_Driver::target_box_class*)this->target_box(found);
      target->state(Fl_Dockable_Group_Driver::DOCK_HERE);
    } else {
      Fl_Dockable_Group_Driver::driver(this)->state(Fl_Dockable_Group::DRAG);
      if (this->target_index_ >= 0) {
        Fl_Dockable_Group_Driver::target_box_class *target = (Fl_Dockable_Group_Driver::target_box_class*)this->target_box(this->target_index_);
        target->state(Fl_Dockable_Group_Driver::MAY_RECEIVE);
      }
      this->target_index_ = -1;
      Fl_Dockable_Group::active_dockable = NULL;
    }
  }
}


void Fl_Dockable_Group::command_box(int x, int y, int w, int h) {
  Fl_Dockable_Group_Driver::cmd_box_class *drag = new Fl_Dockable_Group_Driver::cmd_box_class(FL_DOWN_BOX, x,y,w,h, NULL);
  this->add(drag);
  command_box_ = drag;
  driver_->state(UNDOCK);
}


void Fl_Dockable_Group_Driver::store_docked_position(Fl_Dockable_Group *dock) {
  Fl_Group *gr = dock->parent();
  dock->parent_when_docked_ = gr;
  dock->x_when_docked_ = dock->x();
  dock->y_when_docked_ = dock->y();
  const int i = gr->find(dock);
  dock->next_in_group_when_docked_ = (i < gr->children() - 1 ? gr->child(i + 1) : NULL);
}


void Fl_Dockable_Group::color_for_states(Fl_Color undock, Fl_Color drag,
                                         Fl_Color dock, Fl_Color docked, Fl_Color dragged) {
  driver_->undock_color_ = undock;
  driver_->drag_color_ = drag;
  driver_->dock_color_ = dock;
  driver_->docked_color_ = docked;
  driver_->dragged_color_ = dragged;
}


void Fl_Dockable_Group::label_for_states(const char *undock, const char * drag,
                                         const char * dock, const char * docked, const char *dragged) {
  driver_->undock_label_ = undock;
  driver_->drag_label_ = drag;
  driver_->dock_label_ = dock;
  driver_->docked_label_ = docked;
  driver_->dragged_label_ = dragged;
}


void Fl_Dockable_Group_Driver::state(enum Fl_Dockable_Group::states state) {
  dockable_->state = state;
  Fl_Color c;
  const char *t;
  if (state == Fl_Dockable_Group::UNDOCK) { c = undock_color_; t = undock_label_; }
  else if (state == Fl_Dockable_Group::DRAG) { c = drag_color_; t = drag_label_; }
  else if (state == Fl_Dockable_Group::DOCK) { c = dock_color_; t = dock_label_; }
  else if (state == Fl_Dockable_Group::DRAGGED) { c = dragged_color_; t = dragged_label_; }
  else { c = docked_color_; t = docked_label_; }
  dockable_->command_box()->color(c);
  dockable_->command_box()->labelcolor(fl_contrast(FL_FOREGROUND_COLOR, c));
  dockable_->command_box()->label(t);
  dockable_->command_box()->redraw();
}


void Fl_Dockable_Group_Driver::target_box_class::state(enum Fl_Dockable_Group_Driver::target_states s) {
  state_ = s;
  Fl_Color c = 0;
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
