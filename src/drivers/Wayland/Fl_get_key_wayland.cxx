//
// Keyboard state routines for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2021 by Bill Spitzak and others.
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

#include <config.h>
#if !defined(FL_DOXYGEN)

// Return the current state of a key.  This is the Wayland version.  I identify
// keys (mostly) by the keysym.

#include <FL/Fl.H>
#include "Fl_Wayland_System_Driver.H"

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

#endif // FL_DOXYGEN
