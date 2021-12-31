//
// Definition of Wayland system driver
// for the Fast Light Tool Kit (FLTK).
//
// Copyright 2010-2021 by Bill Spitzak and others.
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

#ifndef FL_WAYLAND_SYSTEM_DRIVER_H
#define FL_WAYLAND_SYSTEM_DRIVER_H

#include "../Nix/Fl_Nix_System_Driver.H"

class FL_EXPORT Fl_Wayland_System_Driver : public Fl_Nix_System_Driver {
public:
  Fl_Wayland_System_Driver() : Fl_Nix_System_Driver() {
    // Wayland system driver uses the default key table
  }
  // this one is in Fl_wayland.cxx
  virtual void copy(const char *stuff, int len, int clipboard, const char *type);
  // this one is in Fl_wayland.cxx
  virtual void paste(Fl_Widget &receiver, int clipboard, const char *type);
  // this one is in Fl_wayland.cxx
  virtual int clipboard_contains(const char *type);
  virtual void make_transient(void *ptr_gtk, void*, Fl_Window*);
  virtual int need_menu_handle_part2() {return 0;}
  char *get_prog_name();
  int event_key(int k);
  int get_key(int k);
};

#endif /* FL_WAYLAND_SYSTEM_DRIVER_H */
