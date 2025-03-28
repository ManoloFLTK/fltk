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

#include <FL/platform.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>


/** A window that can be detached from its parent, dragged with the mouse to some destination, and docked there. */
class Fl_Dockable_Window : public Fl_Double_Window {
  
private:
  Fl_Box *target_box_;
#ifdef FLTK_USE_WAYLAND
  Fl_Dockable_Window *old_dock_;
  struct xdg_toplevel_drag_v1 *drag_;
  Fl_Box *command_box_;
  Fl_Dockable_Window *copy(const char *t);

  class target_box_class : public Fl_Box {
  public:
    target_box_class(Fl_Boxtype bt, int x, int y, int w, int h, const char *t) : Fl_Box(bt,x,y,w,h,t) {}
    int handle(int event) FL_OVERRIDE;
  };
#endif
  
  static void delete_win(Fl_Dockable_Window *w, void *);
  class drag_box_out : public Fl_Box {
  private:
    bool can_dock_;
  public:
    drag_box_out(Fl_Boxtype b, int x, int y, int w, int h, const char *l) : Fl_Box(b, x, y, w, h, l) {
      can_dock_ = false;
    }
    int handle(int event) FL_OVERRIDE;
  };
  
public:
  Fl_Dockable_Window(int x, int y, int w, int h, const char *t=0) : Fl_Double_Window(x,y,w,h,t) {
    target_box_ = NULL;
#ifdef FLTK_USE_WAYLAND
    old_dock_ = NULL;
    drag_ = NULL;
    command_box_ = NULL;
#endif
  }
  void command_box(int x, int y, int w, int h, const char *t);
  void target_box(Fl_Boxtype bt, int x, int y, int w, int h, const char *t, Fl_Group *g);
  Fl_Box *target_box() { return target_box_; }
};

#endif // Fl_Dockable_Window_H
