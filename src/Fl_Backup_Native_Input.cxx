//
// Fl_Native_Input_Driver::newNativeInputDriver for the Fast Light Tool Kit (FLTK).
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

#include "Fl_Native_Input_Driver.H"


Fl_Native_Input_Driver *Fl_Native_Input_Driver::newNativeInputDriver(Fl_Native_Input *n) {
  Fl_Native_Input_Driver *retval = new Fl_Backup_Native_Input_Driver();
  retval->widget = n;
  return retval;
}

#endif // ndef FL_DOXYGEN
