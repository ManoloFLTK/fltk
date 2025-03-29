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
   Fl_Dockable_Window widget . */

#ifndef Fl_Dockable_Window_H
#define Fl_Dockable_Window_H

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>

class Fl_Dockable_Window_Driver;

/** A window that can be detached from its parent, dragged with the mouse to some destination, and docked there. */
class Fl_Dockable_Window : public Fl_Double_Window {
  friend class Fl_Dockable_Window_Driver;
private:
  Fl_Box *target_box_;
  Fl_Dockable_Window_Driver *driver_;
public:
  Fl_Dockable_Window(int x, int y, int w, int h, const char *t=0);
  void command_box(int x, int y, int w, int h, const char *t);
  void target_box(Fl_Boxtype bt, int x, int y, int w, int h, const char *t, Fl_Group *g);
  Fl_Box *target_box() { return target_box_; }
};

#endif // Fl_Dockable_Window_H
