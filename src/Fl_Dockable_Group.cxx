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

#include <FL/platform.H> // for FLTK_USE_WAYLAND
#include <FL/Fl_Dockable_Group.H>
#include <FL/Fl_Window.H>
#include "Fl_Dockable_Group_Driver.H"

Fl_Dockable_Group *Fl_Dockable_Group::active_dockable = NULL;


#if !defined(FLTK_USE_WAYLAND) && !defined(FL_DOXYGEN)

Fl_Dockable_Group_Driver *Fl_Dockable_Group_Driver::newDockableGroupDriver(Fl_Dockable_Group *dock) {
  return new Fl_Dockable_Group_Driver(dock);
}


Fl_Box *Fl_Dockable_Group_Driver::newTargetBoxClass(int x, int y, int w, int h) {
  return new target_box_class(x, y, w, h);
}

#endif


/**
 Constructor.
 \param x,y,w,h,t used as with Fl_Group
 */
Fl_Dockable_Group::Fl_Dockable_Group(int x, int y, int w, int h, const char *t) : Fl_Group(x,y,w,h,t) {
  state = UNDOCK;
  driver_ = Fl_Dockable_Group_Driver::newDockableGroupDriver(this);
  color_for_states();
  label_for_states();
  place_holder_while_dragged_ = NULL;
}


/**
 Create a 'target box' above which any Fl_Dockable_Group may be dragged and docked releasing the mouse.
 Its boxtype is FL_DOWN_BOX by default.
 \param x,y,w,h Position and size of the 'target box'.
 \param g Group (or derived object, e.g., Fl_Double_Window, Fl_Tabs) to which the 'target box' to be created will be added.
 \return The created 'target box'.
 */
Fl_Box *Fl_Dockable_Group::target_box(int x, int y, int w, int h, Fl_Group *g) {
  Fl_Group *save = Fl_Group::current();
  Fl_Group::current(NULL);
  Fl_Box *target_box = Fl_Dockable_Group_Driver::new_target_box(x, y, w, h);
  if (g) g->add(target_box);
  Fl_Group::current(save);
  return target_box;
}


/** Return the \p n 'th 'target box' present in the GUI */
Fl_Box *Fl_Dockable_Group::target_box(int n) { return Fl_Dockable_Group_Driver::target_[n]; }

/** Return the number of 'target boxes' present in the GUI */
int Fl_Dockable_Group::target_count() { return (int)Fl_Dockable_Group_Driver::target_.size(); }

/** Index of 'target box' where docking will occur if mouse is released.
 Value -1 is returned when mouse releasing would not dock the current Fl_Dockable_Group to a new GUI location. */
int Fl_Dockable_Group::target_index() { return Fl_Dockable_Group_Driver::target_index_; }


/** Create the Fl_Dockable_Group's 'command box'.
 The 'command box' allows to drag away an Fl_Dockable_Group by clicking that box
 and dragging the mouse. The position and size of the 'command box' inside the Fl_Dockable_Group
 can be freely set by the caller. Its changing colors and labels are best set via member functions color_for_states()
 and label_for_states(). Its boxtype is FL_DOWN_BOX by default but can be be freely changed.
 \param x,y,w,h Position and size of the 'command box'.
 \return The created 'command box'.
 */
Fl_Box *Fl_Dockable_Group::command_box(int x, int y, int w, int h) {
  Fl_Dockable_Group_Driver::cmd_box_class *drag =
    new Fl_Dockable_Group_Driver::cmd_box_class(FL_DOWN_BOX, x, y, w, h, NULL);
  this->add(drag);
  drag->align(FL_ALIGN_CLIP);
  command_box_ = drag;
  driver_->state(UNDOCK);
  return drag;
}


/**
 Set the colors the object's 'command box' takes in each of its possible states.
 \param undock, drag, dock, dragged Color used for states Fl_Dockable_Group::UNDOCK, Fl_Dockable_Group::DRAG,
 Fl_Dockable_Group::DOCK, Fl_Dockable_Group::DRAGGED, respectively.
 */
void Fl_Dockable_Group::color_for_states(Fl_Color undock, Fl_Color drag,
                                         Fl_Color dock, Fl_Color dragged) {
  driver_->undock_color_ = undock;
  driver_->drag_color_ = drag;
  driver_->dock_color_ = dock;
  driver_->dragged_color_ = dragged;
}


/**
 Set the text labels the object's 'command box' uses in each of its possible states.
 \param undock, drag, dock, dragged Text labels used for states Fl_Dockable_Group::UNDOCK, Fl_Dockable_Group::DRAG,
 Fl_Dockable_Group::DOCK, Fl_Dockable_Group::DRAGGED, respectively.
 */
void Fl_Dockable_Group::label_for_states(const char *undock, const char * drag,
                                         const char * dock, const char *dragged) {
  driver_->undock_label_ = undock;
  driver_->drag_label_ = drag;
  driver_->dock_label_ = dock;
  driver_->dragged_label_ = dragged;
}


/** Set the label of 'target boxes'.
 The default label is <tt>"Dock here"</tt>. */
void Fl_Dockable_Group::label_for_target_boxes(const char *l) {
  Fl_Dockable_Group_Driver::target_box_label_ = l;
}


void Fl_Dockable_Group::color_targets_following_dock_() {
  bool new_can_dock = false;
  int found = -1;
  for (int i = 0; i < this->target_count(); i++) {
    Fl_Box *target = Fl_Dockable_Group_Driver::target_[i];
    if (!target->visible_r()) continue;
    int target_x, target_y;
    Fl_Window *top = target->top_window_offset(target_x, target_y);
    target_x += top->x();
    target_y += top->y();
    int dock_x = this->window()->x() + this->x() + Fl::event_x();
    int dock_y = this->window()->y() + this->y() + Fl::event_y();
    // Is dragging cursor inside target ?
    new_can_dock = dock_x >= target_x && dock_x < target_x + target->w() &&
                   dock_y >= target_y && dock_y < target_y + target->h();
    if (new_can_dock) {
      found = i;
      break;
    }
  }
  bool can_dock = (this->state == Fl_Dockable_Group::DOCK);
  if (found >= 0 && can_dock && found != Fl_Dockable_Group_Driver::target_index_) { // dock crosses several target boxes
    found = -1;
    can_dock = false;
  }
  if (can_dock != new_can_dock) {
    if (found >= 0) {
      Fl_Dockable_Group_Driver::driver(this)->state(Fl_Dockable_Group::DOCK);
      Fl_Dockable_Group_Driver::target_index_ = found;
      Fl_Dockable_Group::active_dockable = this;
      Fl_Dockable_Group_Driver::target_box_class *target = (Fl_Dockable_Group_Driver::target_box_class*)Fl_Dockable_Group::target_box(found);
      target->state(Fl_Dockable_Group_Driver::DOCK_HERE);
    } else {
      Fl_Dockable_Group_Driver::driver(this)->state(Fl_Dockable_Group::DRAG);
      if (Fl_Dockable_Group_Driver::target_index_ >= 0) {
        Fl_Dockable_Group_Driver::target_box_class *target = (Fl_Dockable_Group_Driver::target_box_class*)Fl_Dockable_Group::target_box(Fl_Dockable_Group_Driver::target_index_);
        target->state(Fl_Dockable_Group_Driver::MAY_RECEIVE);
      }
      Fl_Dockable_Group_Driver::target_index_ = -1;
      Fl_Dockable_Group::active_dockable = NULL;
    }
  }
}


/** Assign a callback function that gets called when the object begins or ends being dragged.
 The callback function gets called with the Fl_Dockable_Group as 1st argument, the relevant dock_event
 as 2nd argument, and \p data as 3rd argument. */
void Fl_Dockable_Group::dock_cb(dock_cb_function f, void *data) {
  driver_->dock_cb_f = f;
  driver_->dock_cb_data = data;
}


#ifndef FL_DOXYGEN

int Fl_Dockable_Group_Driver::target_index_ = -1;

std::vector<Fl_Box *>Fl_Dockable_Group_Driver::target_; // empty vector of target boxes

const char *Fl_Dockable_Group_Driver::target_box_label_ = "Dock here";


Fl_Box *Fl_Dockable_Group_Driver::new_target_box(int x, int y, int w, int h) {
  Fl_Box *box = newTargetBoxClass(x, y, w, h);
  target_.insert(target_.begin(), box);
  return box;
}


Fl_Dockable_Group_Driver::target_box_class::~target_box_class() {
  for (int i = 0; i < target_.size(); i++) {
    if (target_[i] == this) {
      target_.erase(target_.begin()+i);
      if (target_index_ == i)
        target_index_ = -1;
      break;
    }
  }
}


void Fl_Dockable_Group_Driver::delete_win_cb(Fl_Window *win) {
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)win->child(0);
  if (Fl_Dockable_Group_Driver::target_index_ >= 0) {
    target_box_class *target = (target_box_class*)Fl_Dockable_Group::target_box(target_index_);
    target->state(MAY_RECEIVE);
  }
  Fl_Group *parent = dock->parent_when_docked_;
  if (!parent) {
    delete win;
    return;
  }
  win->hide();
  win->remove(dock);
  dock->hide();
  if (dock->place_holder_while_dragged_) {
    dock->resize(dock->place_holder_while_dragged_->x(), dock->place_holder_while_dragged_->y(),
               dock->place_holder_while_dragged_->w(), dock->place_holder_while_dragged_->h());
    int r = parent->find(dock->place_holder_while_dragged_);
    parent->insert(*dock, r);
    delete dock->place_holder_while_dragged_;
    dock->place_holder_while_dragged_ = NULL;
  }
  dock->show();
  if (parent) parent->redraw();
  Fl_Dockable_Group_Driver::driver(dock)->state(Fl_Dockable_Group::UNDOCK);
  delete win;
  if (driver(dock)->dock_cb_f)
    driver(dock)->dock_cb_f(dock, Fl_Dockable_Group::HOME, driver(dock)->dock_cb_data);
}


int Fl_Dockable_Group_Driver::cmd_box_class::handle(int event) {
  if (event == FL_FOCUS || event == FL_UNFOCUS) {
    redraw();
    return 1;
  }
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)parent();
  return Fl_Dockable_Group_Driver::driver(dock)->handle(this, event);
}


void Fl_Dockable_Group_Driver::cmd_box_class::draw() {
  Fl_Box::draw();
  if (Fl::focus() == this) draw_focus();
}


 void Fl_Dockable_Group_Driver::recursively_hide_subwindows(Fl_Group *g) {
  for (int i = 0; i < g->children(); i++) {
    Fl_Widget *child = g->child(i);
    if (child->as_group()) recursively_hide_subwindows(child->as_group());
    if (child->as_window() && child->as_window()->shown()) {
      child->hide();
      child->set_visible();
    }
  }
}


void Fl_Dockable_Group_Driver::insert_target_box_in_group(Fl_Widget *target,
                                        Fl_Group *parent_when_docked, int rd,
                                        int dock_x, int dock_y, int dock_w, int dock_h) {
  target->resize(dock_x, dock_y, dock_w, dock_h);
  target->set_visible(); // important if target was just removed from an Fl_Tabs
  parent_when_docked->insert(*target, rd);
  if (parent_when_docked->children() < 2) return;
  int count_visible_before = 0;
  for (int i = 0; i < parent_when_docked->children(); i++)
    if (parent_when_docked->child(i)->visible()) count_visible_before++;
  if (count_visible_before == 1) return;
  parent_when_docked->handle(FL_SHOW); // needed to get count_visible_after correct for Fl_Tabs
  int count_visible_after = 0;
  for (int i = 0; i < parent_when_docked->children(); i++)
    if (parent_when_docked->child(i)->visible()) count_visible_after++;
  if (count_visible_after == 1 && target->visible()) { // to detect Fl_Tabs
    if (++rd >= parent_when_docked->children()) rd = 0;
    parent_when_docked->child(rd)->set_visible();
    target->clear_visible();
  }
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
    if (++drag_count >= 3) {
      Fl_Group *top = dock->parent();
      // transform the dockable group into a draggable, borderless toplevel window
      if (driver(dock)->dock_cb_f)
        driver(dock)->dock_cb_f(dock, Fl_Dockable_Group::UNDOCKED, driver(dock)->dock_cb_data);
      store_docked_position(dock);
      int r = top->find(dock);
      dock->place_holder_while_dragged_ =
        new place_holder(dock->box(), dock->x(), dock->y(), dock->w(), dock->h(), dock);
      top->insert(*dock->place_holder_while_dragged_, r);
      top->remove(dock);
      if (!dock->place_holder_while_dragged_->visible()) { // useful with Fl_Tabs
        top->child(top->children()-1)->clear_visible();
        dock->place_holder_while_dragged_->set_visible();
      }
      if (top->parent()) top->parent()->damage(~FL_DAMAGE_CHILD);
      else top->redraw();
      Fl_Window *win = new Fl_Window(winx, winy, dock->w(), dock->h(), drag_label_);
      recursively_hide_subwindows(dock);// necessary if dock contains subwindows
      dock->clear_visible();
      dock->position(0,0);
      dock->set_visible();
      win->add(dock);
      win->end();
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
    dock->window()->show(); // necessary under Windows
    dock->color_targets_following_dock_();
    return 1;
  } else if (event == FL_RELEASE && dock->state == Fl_Dockable_Group::DOCK) { // Dock dockable in place
    Fl_Dockable_Group::active_dockable = NULL;
    target_box_class *target = (target_box_class*)Fl_Dockable_Group::target_box(target_index_);
    // move target-box target from its parent to dock's original parent = parent_when_docked_
    Fl_Group *parent = target->parent(); // can be an Fl_Tabs
    Fl_Window *top = dock->window(); // extract dock from its window and delete the containing window
    top->hide();
    top->remove(dock);
    delete top;
    Fl_Group *parent_when_docked = dock->parent_when_docked_;
    int r = parent->find(target), rd = 0, dock_x, dock_y, dock_w, dock_h;
    if (dock->place_holder_while_dragged_) {
      rd = parent_when_docked->find(dock->place_holder_while_dragged_);
      parent_when_docked->redraw();
      dock_x = dock->place_holder_while_dragged_->x();
      dock_y = dock->place_holder_while_dragged_->y();
      dock_w = dock->place_holder_while_dragged_->w();
      dock_h = dock->place_holder_while_dragged_->h();
      delete dock->place_holder_while_dragged_;
    }
    parent->insert(*dock, r);
    dock->resize(target->x(), target->y(), target->w(), target->h());
    if (parent_when_docked) {
      parent->remove(target);
      insert_target_box_in_group(target, parent_when_docked, rd, dock_x, dock_y, dock_w, dock_h);
      target->state(MAY_RECEIVE);
      target->parent()->redraw();
    } else delete target;
    Fl_Dockable_Group_Driver::driver(dock)->state(Fl_Dockable_Group::UNDOCK);
    dock->clear_visible();
    dock->show();
    if (driver(dock)->dock_cb_f)
      driver(dock)->dock_cb_f(dock, Fl_Dockable_Group::DOCKED, driver(dock)->dock_cb_data);
    return 1;
  }
  return box->Fl_Box::handle(event);
}


void Fl_Dockable_Group_Driver::store_docked_position(Fl_Dockable_Group *dock) {
  Fl_Group *gr = dock->parent();
  dock->parent_when_docked_ = gr;
}


void Fl_Dockable_Group_Driver::state(enum Fl_Dockable_Group::state state) {
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
  if (s == MAY_RECEIVE)  {
    c = (Fl_Dockable_Group::active_dockable ?
         driver(Fl_Dockable_Group::active_dockable)->drag_color_ : FL_BACKGROUND2_COLOR);
  } else if (s == DOCK_HERE) {
    c = driver(Fl_Dockable_Group::active_dockable)->dock_color_;
  }
  color(c);
  labelcolor(fl_contrast(FL_FOREGROUND_COLOR, c));
  label(target_box_label_);
  redraw();
}


Fl_Dockable_Group_Driver::place_holder::place_holder(
    Fl_Boxtype t, int x, int y, int w, int h, Fl_Dockable_Group *dock) : Fl_Box(t, x, y, w, h, "Dragged") {
  dock_ = dock;
  align(FL_ALIGN_CLIP);
}


Fl_Dockable_Group_Driver::place_holder::~place_holder() {
  dock_->place_holder_while_dragged_ = NULL;
  dock_->parent_when_docked_ = NULL;
}

#endif // FL_DOXYGEN
