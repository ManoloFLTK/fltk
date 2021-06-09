//
// Copy-to-clipboard code for the Fast Light Tool Kit (FLTK).
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
#include <FL/Fl_Copy_Surface.H>
#include <FL/Fl_Image_Surface.H>
#include "Fl_Wayland_Graphics_Driver.H"
#include "Fl_Wayland_Screen_Driver.H"
#include "Fl_Wayland_Window_Driver.H"
#include <FL/platform.H>

class Fl_Wayland_Copy_Surface_Driver : public Fl_Copy_Surface_Driver {
  friend class Fl_Copy_Surface_Driver;
  Fl_Image_Surface *img_surf;
protected:
  Fl_Wayland_Copy_Surface_Driver(int w, int h);
  ~Fl_Wayland_Copy_Surface_Driver();
  void set_current();
  void translate(int x, int y);
  void untranslate();
};


Fl_Copy_Surface_Driver *Fl_Copy_Surface_Driver::newCopySurfaceDriver(int w, int h)
{
  return new Fl_Wayland_Copy_Surface_Driver(w, h);
}


Fl_Wayland_Copy_Surface_Driver::Fl_Wayland_Copy_Surface_Driver(int w, int h) : Fl_Copy_Surface_Driver(w, h) {
  img_surf = new Fl_Image_Surface(w, h);
  driver(img_surf->driver());
}


Fl_Wayland_Copy_Surface_Driver::~Fl_Wayland_Copy_Surface_Driver() {
  Fl_RGB_Image *rgb = img_surf->image();
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  scr_driver->copy_image(rgb->array, rgb->data_w(), rgb->data_h());
  delete rgb;
  delete img_surf;
  driver(NULL);
}


void Fl_Wayland_Copy_Surface_Driver::set_current() {
  Fl_Surface_Device::set_current();
  ((Fl_Wayland_Graphics_Driver*)driver())->activate(img_surf->offscreen(), fl_window?fl_window->scale:1);
}


void Fl_Wayland_Copy_Surface_Driver::translate(int x, int y) {
  ((Fl_Wayland_Graphics_Driver*)driver())->ps_translate(x, y);
}


void Fl_Wayland_Copy_Surface_Driver::untranslate() {
  ((Fl_Wayland_Graphics_Driver*)driver())->ps_untranslate();
}