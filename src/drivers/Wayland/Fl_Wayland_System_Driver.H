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

#include <config.h>
#include "../Posix/Fl_Posix_System_Driver.H"

class FL_EXPORT Fl_Wayland_System_Driver : public Fl_Posix_System_Driver {
public:
  Fl_Wayland_System_Driver() : Fl_Posix_System_Driver() {
    // Wayland system driver uses the default key table
  }
  virtual void display_arg(const char *arg);
  virtual int clocale_printf(FILE *output, const char *format, va_list args);
  // these 2 are in Fl_get_key.cxx
  virtual int event_key(int k);
  virtual int get_key(int k);
  virtual int filename_list(const char *d, dirent ***list,
                            int (*sort)(struct dirent **, struct dirent **),
                            char *errmsg=NULL, int errmsg_sz=0);
  virtual int open_uri(const char *uri, char *msg, int msglen);
  virtual int use_tooltip_timeout_condition() {return 1;}
  virtual int file_browser_load_filesystem(Fl_File_Browser *browser, char *filename, int lname, Fl_File_Icon *icon);
  virtual void newUUID(char *uuidBuffer);
  virtual char *preference_rootnode(Fl_Preferences *prefs, Fl_Preferences::Root root, const char *vendor,
                                    const char *application);
  virtual int preferences_need_protection_check() {return 1;}
  virtual int utf8locale();
  // this one is in Fl_own_colormap.cxx
  virtual void own_colormap();
  // this one is in Fl_x.cxx
  virtual const char *filename_name(const char *buf);
  // this one is in Fl_x.cxx
  virtual void copy(const char *stuff, int len, int clipboard, const char *type);
  // this one is in Fl_x.cxx
  virtual void paste(Fl_Widget &receiver, int clipboard, const char *type);
  // this one is in Fl_wayland.cxx
  virtual int clipboard_contains(const char *type);
  // this one is in Fl_wayland.cxx
  //virtual void clipboard_notify_change();
  virtual void add_fd(int fd, int when, Fl_FD_Handler cb, void* = 0);
  virtual void add_fd(int fd, Fl_FD_Handler cb, void* = 0);
  virtual void remove_fd(int, int when);
  virtual void remove_fd(int);
  virtual void make_transient(void *ptr_gtk, void*, Fl_Window*);
  virtual int need_menu_handle_part2() {return 0;}
  
  char *get_prog_name();
};

#endif /* FL_X11_SYSTEM_DRIVER_H */
