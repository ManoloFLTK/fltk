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

#endif


/**
 Constructor.
 \param x,y,w,h,t used as with Fl_Group
 */
Fl_Dockable_Group::Fl_Dockable_Group(int x, int y, int w, int h, const char *t) : Fl_Group(x,y,w,h,t) {
  state_ = UNDOCK;
  driver_ = Fl_Dockable_Group_Driver::newDockableGroupDriver(this);
  color_for_states();
  label_for_states();
}


/** Creates the Fl_Dockable_Group's 'drag box'.
 The 'drag box' allows to drag away an Fl_Dockable_Group by clicking that box
 and dragging the mouse. The position and size of the 'drag box' inside the Fl_Dockable_Group
 can be freely set by the caller. Its changing colors and labels are best set via member functions color_for_states()
 and label_for_states(). Its boxtype is FL_DOWN_BOX by default but can be be freely changed.
 \param x,y,w,h Position and size of the 'drag box'.
 \return The created 'drag box'.
 */
Fl_Box *Fl_Dockable_Group::drag_box(int x, int y, int w, int h) {
  Fl_Dockable_Group_Driver::drag_box_class *drag =
    new Fl_Dockable_Group_Driver::drag_box_class(FL_DOWN_BOX, x, y, w, h, NULL);
  this->add(drag);
  drag->align(FL_ALIGN_CLIP);
  drag_box_ = drag;
  state(UNDOCK);
  return drag;
}


/**
 Sets the colors the object's 'drag box' takes in each of its possible states.
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
 Sets the text labels the object's 'drag box' uses in each of its possible states.
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


/** Label of Dockable_Box objects */
const char *Fl_Dockable_Group::Dockable_Box::dockable_label = "Dock here";


/** Constructor */
Fl_Dockable_Group::Dockable_Box::Dockable_Box(int x, int y, int w, int h) :
    Fl_Box(FL_DOWN_BOX, x, y, w, h, dockable_label) { }


/** Returns whether the mouse currently dragging an Fl_Dockable_Group is above  \p widget  */
bool Fl_Dockable_Group::is_dockable_inside(Fl_Widget *widget) {
  int X = Fl::event_x_root();
  int Y = Fl::event_y_root();
  int box_x, box_y, retval = 1;
  Fl_Window *top = widget->top_window_offset(box_x, box_y);
  box_x += top->x();
  box_y += top->y();
  return (X >= box_x && X < box_x + widget->w() && Y >= box_y && Y < box_y + widget->h());
}


void Fl_Dockable_Group::color_targets_following_dock_() {
  if (Fl::event_button() != FL_LEFT_MOUSE) return;
  int X = Fl::event_x_root(); // screen coordinates of the dockable-dragging mouse
  int Y = Fl::event_y_root();
  Fl_Window *dock = top_window(); // the dockable being dragged as a window
  Fl_Window *win = Fl::first_window();
  bool found = false;
  while (win) { // search other window on same screen below the mouse
    if (win != dock && !win->parent() && win->screen_num() == dock->screen_num()) {
      if (X >= win->x() && X < win->x() + win->w() && Y >= win->y() && Y < win->y() + win->h()) {
        int event = 0;
        if (this->state_ == DRAG) event = FL_DOCK_ENTER;
        else if (this->state_ == DOCK) event = FL_DOCK_DRAG;
        if (event) win->handle(event);
        found = true;
        break;
      }
    }
    win = Fl::next_window(win);
  }
  if (!found && active_dockable) {
    state(Fl_Dockable_Group::DRAG);
    if (Fl::belowmouse()) Fl::belowmouse()->handle(FL_DOCK_LEAVE);
  }
}


void Fl_Dockable_Group::after_release_() {
  driver_->after_release_();
}


/** Sets the state of the Fl_Dockable_Group */
void Fl_Dockable_Group::state(enum states s) { driver_->state(s); }


int Fl_Dockable_Group::handle_target(bool& processed, Fl_Widget *target, int event,
                                    inside_f_type inside_f, target_state_f_type target_state_f,
                                    dock_payload_f_type dock_payload_f, undock_f_type undock_f) {
  if (Fl_Dockable_Group::active_dockable) {
    Fl_Dockable_Group_Driver::driver(active_dockable)->check_event_(event);
  }
  /*if (event == FL_DOCK_ENTER)   puts("FL_DOCK_ENTER:");
   if (event == FL_DOCK_DRAG)   puts("FL_DOCK_DRAG:");
   if (event == FL_DOCK_RELEASE) puts("FL_DOCK_RELEASE:");
   if (event == FL_DOCK_LEAVE) puts("FL_DOCK_LEAVE:"); */
  processed = true;
  bool inside = inside_f(target); // true if mouse is inside the target's docking area
  if (event == FL_DOCK_ENTER || event == FL_DOCK_DRAG) {
    if (inside) {
      if (Fl::belowmouse() && Fl::belowmouse() != target && target_state_f) {
        target_state_f(Fl::belowmouse(), false);
      }
      Fl::belowmouse(target);
      if (Fl_Dockable_Group::active_dockable) {
        Fl_Dockable_Group::active_dockable->state(Fl_Dockable_Group::DOCK);
        if (target_state_f) target_state_f(target, true);
      }
    } else  {
      target->handle(FL_DOCK_LEAVE);
    }
    return 1;
  } else if (event == FL_DOCK_LEAVE) {
    if (Fl_Dockable_Group::active_dockable) {
      Fl_Dockable_Group::active_dockable->state(Fl_Dockable_Group::DRAG);
      if (target_state_f) target_state_f(target, false);
    }
    if (Fl::belowmouse() && target_state_f) {
      target_state_f(Fl::belowmouse(), false);
    }
    Fl::belowmouse(NULL);
    return 1;
  } else if (event == FL_DOCK_RELEASE && Fl_Dockable_Group::active_dockable) {
    Fl_Window *top = Fl_Dockable_Group::active_dockable->window();
    top->remove(Fl_Dockable_Group::active_dockable);
    delete top;
    dock_payload_f(target, Fl_Dockable_Group::active_dockable);
    Fl_Dockable_Group::active_dockable->after_release_();
    Fl_Dockable_Group::active_dockable->state(Fl_Dockable_Group::UNDOCK);
    Fl_Dockable_Group::active_dockable = NULL;
    return 1;
  } else if (event == FL_UNDOCK) {
    if (undock_f) undock_f(target);
    return 1;
  }
  processed = false;
  return 0;
}


static bool inside_f(Fl_Widget *target) {
    return Fl_Dockable_Group::is_dockable_inside(target); // true if mouse inside target
}

static void state_f(Fl_Widget *target, bool can_dock) {
  Fl_Box *box = (Fl_Box*)target;
  if (can_dock) {
    box->color(FL_SELECTION_COLOR);
    box->labelcolor(fl_contrast(FL_WHITE, FL_SELECTION_COLOR));
  } else {
    box->color(FL_BACKGROUND_COLOR);
    box->labelcolor(FL_BLACK);
  }
  box->redraw();
}
  
static void dock_payload_f(Fl_Widget *target, Fl_Dockable_Group *payload) {
    Fl_Group *group = target->parent();
    payload->resize(target->x(), target->y(), target->w(), target->h());
    group->add(payload);
}
    
int Fl_Dockable_Group::Dockable_Box::handle(int event) {
    bool processed;
    int retval = Fl_Dockable_Group::handle_target(processed, this, event,
                                                    inside_f, state_f, dock_payload_f, NULL);
    return (processed ? retval : Fl_Box::handle(event));
}


#ifndef FL_DOXYGEN

int Fl_Dockable_Group_Driver::drag_box_class::handle(int event) {
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)parent();
  return Fl_Dockable_Group_Driver::driver(dock)->handle(this, event);
}


static void do_nothing(Fl_Widget *win) {
}


int Fl_Dockable_Group_Driver::handle(Fl_Dockable_Group_Driver::drag_box_class *box, int event) {
  static int fromx, fromy, winx, winy, drag_count;
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)box->parent();
  if (event == FL_PUSH && (Fl::event_state() & FL_BUTTON1) &&
      (dock->state_ == Fl_Dockable_Group::UNDOCK || dock->state_ == Fl_Dockable_Group::DRAG)) {
    if (dock->state_ == Fl_Dockable_Group::UNDOCK) drag_count = 0;
    Fl_Group *top = dock->parent();
    Fl_Window *top_win = top->top_window();
    fromx = Fl::event_x_root();
    fromy = Fl::event_y_root();
    int offset_x = 0, offset_y = 0;
    if (top->window()) top->window()->top_window_offset(offset_x, offset_y);
    winx = top_win->x_root() + offset_x + dock->x();
    winy = top_win->y_root() + offset_y + dock->y();
    return 1;
  } else if (event == FL_DRAG && (Fl::event_state() & FL_BUTTON1) &&
             (dock->state_ == Fl_Dockable_Group::UNDOCK ||
              (dock->state_ == Fl_Dockable_Group::DRAG && Fl_Dockable_Group::active_dockable != dock))) {
    if (dock->state_ == Fl_Dockable_Group::DRAG) {
      Fl_Dockable_Group::active_dockable = dock; // re-drag previously abandoned dockable
      return handle(box, FL_DRAG);
    }
    if (++drag_count >= 3) {
      Fl_Dockable_Group::active_dockable = dock;
      Fl_Group *top = dock->parent();
      // transform the dockable group into a draggable, borderless toplevel window
      // and replace it by a "Dockable_Box"
      undock(winx, winy);
      top->handle(FL_UNDOCK);
    }
    return 1;
  } else if (event == FL_DRAG &&
             (dock->state_ == Fl_Dockable_Group::DRAG || dock->state_ == Fl_Dockable_Group::DOCK)) {
    int deltax = Fl::event_x_root() - fromx; // Drag dockable around
    int deltay = Fl::event_y_root() - fromy;
    dock->window()->position(winx + deltax, winy + deltay);
    dock->color_targets_following_dock_();
    return 1;
  } else if (event == FL_RELEASE && dock->state_ == Fl_Dockable_Group::DOCK) { // Dock dockable in place
    Fl_Widget *target = Fl::belowmouse();
    return target->handle(FL_DOCK_RELEASE);
  }
  return box->Fl_Box::handle(event);
}


Fl_Window *Fl_Dockable_Group_Driver::undock(int winx, int winy) {
  // transform the dockable group into a draggable, borderless toplevel window
  Fl_Group *top = dockable_->parent();
  top->remove(dockable_);
  Fl_Box *tmp = new Fl_Dockable_Group::Dockable_Box(
                              dockable_->x(), dockable_->y(), dockable_->w(), dockable_->h());
  tmp->align(FL_ALIGN_CLIP);
  top->add(tmp);
  top->redraw();
  Fl_Window *win = new Fl_Window(winx, winy, dockable_->w(), dockable_->h(), "Fl_Dockable_Group");
  dockable_->position(0,0);
  win->add(dockable_);
  dockable_->show(); // necessary for tabs
  win->end();
  win->callback((Fl_Callback0*)do_nothing);
  state(Fl_Dockable_Group::DRAG);
  win->border(0);
  Fl::pushed(dockable_->drag_box()); // necessary for tabs
  win->show();
  return win;
}


void Fl_Dockable_Group_Driver::state(enum Fl_Dockable_Group::states state) {
  dockable_->state_ = state;
  Fl_Color c;
  const char *t;
  if (state == Fl_Dockable_Group::UNDOCK) { c = undock_color_; t = undock_label_; }
  else if (state == Fl_Dockable_Group::DRAG) { c = drag_color_; t = drag_label_; }
  else if (state == Fl_Dockable_Group::DOCK) { c = dock_color_; t = dock_label_; }
  else if (state == Fl_Dockable_Group::DRAGGED) { c = dragged_color_; t = dragged_label_; }
  else { c = docked_color_; t = docked_label_; }
  dockable_->drag_box()->color(c);
  dockable_->drag_box()->labelcolor(fl_contrast(FL_FOREGROUND_COLOR, c));
  dockable_->drag_box()->label(t);
  dockable_->drag_box()->redraw();
}

#endif // FL_DOXYGEN
