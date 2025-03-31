//
// Dockable window header file for the Fast Light Tool Kit (FLTK).
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

/* \file
   Fl_Dockable_Group widget . */

#ifndef Fl_Dockable_Group_H
#define Fl_Dockable_Group_H

#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>

class Fl_Dockable_Group_Driver;

/** A group that can be detached from its parent, dragged with the mouse to some destination, and docked there. */
class Fl_Dockable_Group : public Fl_Group {
  friend class Fl_Dockable_Group_Driver;
private:
  Fl_Dockable_Group_Driver *driver_;
  Fl_Box *command_box_;
  std::vector<Fl_Box *>target_; // vector of target boxes
  int target_index_; // index in vector to target where docking will occur
  Fl_Group *parent_when_docked_;
  int x_when_docked_, y_when_docked_;
public:
  Fl_Dockable_Group(int x, int y, int w, int h, const char *t=0);
  virtual ~Fl_Dockable_Group();
  void command_box(int x, int y, int w, int h, const char *t);
  Fl_Box *command_box() { return command_box_; }
  Fl_Box *target_box(Fl_Boxtype bt, int x, int y, int w, int h, const char *t, Fl_Group *g);
  Fl_Box *target_box(int n) { return target_[n]; }
  int target_count() { return (int)target_.size(); }
  int target_index() { return target_index_; }
  enum {UNDOCK, DRAG, DOCK, DOCKED} state;
};

#endif // Fl_Dockable_Group_H
