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

#include "Fl_Wayland_System_Driver.H"
#include <FL/Fl.H>

/**
 Creates a driver that manages all system related calls.

 This function must be implemented once for every platform.
 */
Fl_System_Driver *Fl_System_Driver::newSystemDriver()
{
  return new Fl_Wayland_System_Driver();
}


int Fl_Wayland_System_Driver::event_key(int k) {
  if (k > FL_Button && k <= FL_Button+8)
    return Fl::event_state(8<<(k-FL_Button));
  int sym = Fl::event_key();
  if (sym >= 'a' && sym <= 'z' ) sym -= 32;
  return (Fl::event() == FL_KEYDOWN || Fl::event() == FL_SHORTCUT) && sym == k;
}

int Fl_Wayland_System_Driver::get_key(int k) {
  return event_key(k);
}
