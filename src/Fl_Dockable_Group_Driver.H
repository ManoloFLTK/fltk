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

#ifndef FL_DOXYGEN

#include <FL/Fl_Dockable_Group.H>
#include <vector>

class Fl_Dockable_Group_Driver {
  friend class Fl_Dockable_Group;
private:
  Fl_Color undock_color_, drag_color_, dock_color_, docked_color_, dragged_color_;
  const char *undock_label_, *drag_label_, *dock_label_, *docked_label_, *dragged_label_;
  virtual void after_release_() {}
  virtual void check_event_(int& event) {}
protected:
  Fl_Dockable_Group *dockable_;
  Fl_Window *undock(int x, int y);
public:
  class drag_box_class : public Fl_Box {
  public:
    drag_box_class(Fl_Boxtype b, int x, int y, int w, int h, const char *l) : Fl_Box(b, x, y, w, h, l) {
      labelsize(12);
    }
    int handle(int event) override;
  };
  static Fl_Dockable_Group_Driver *newDockableGroupDriver(Fl_Dockable_Group*);
  Fl_Dockable_Group_Driver(Fl_Dockable_Group *from) { dockable_ = from; }
  virtual int handle(drag_box_class *, int event);
  static Fl_Dockable_Group_Driver *driver(Fl_Dockable_Group *dock) { return dock->driver_; }
  void state(enum Fl_Dockable_Group::states);
};

#endif // ! FL_DOXYGEN
