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

#include <FL/platform.H>

class Fl_Dockable_Group_Driver {
protected:
  static void delete_win_cb(Fl_Window *);
public:
  class drag_box_out : public Fl_Box {
  public:
    drag_box_out(Fl_Boxtype b, int x, int y, int w, int h, const char *l) : Fl_Box(b, x, y, w, h, l) {
      labelsize(12);
    }
    int handle(int event) FL_OVERRIDE;
  };
  virtual void delete_win(Fl_Dockable_Group *dock);
  virtual Fl_Box *new_target_box(Fl_Boxtype bt, int x, int y, int w, int h, const char *t, Fl_Dockable_Group*) {
    return new Fl_Box(bt, x, y, w, h, t);
  }
  virtual void command_box(Fl_Box *) {}
  virtual int handle(drag_box_out *, int event);
  static Fl_Dockable_Group_Driver *driver(Fl_Dockable_Group *dock) { return dock->driver_; }
};


#ifdef FLTK_USE_WAYLAND

class Fl_Wayland_Dockable_Group_Driver : public Fl_Dockable_Group_Driver {
private:
  Fl_Box *command_box_;
  struct xdg_toplevel_drag_v1 *drag_;
  static Fl_Window *copy_(Fl_Dockable_Group *from, drag_box_out *box, const char *t);
public:
  class target_box_class : public Fl_Box {
    public:
      target_box_class(Fl_Boxtype bt, int x, int y, int w, int h, const char *t) : Fl_Box(bt,x,y,w,h,t) {}
      int handle(int event) FL_OVERRIDE;
  };
  Fl_Wayland_Dockable_Group_Driver();
  void delete_win(Fl_Dockable_Group *dock) FL_OVERRIDE;
  Fl_Box *new_target_box(Fl_Boxtype bt, int x, int y, int w, int h, const char *t, Fl_Dockable_Group*) FL_OVERRIDE;
  void command_box(Fl_Box *b) FL_OVERRIDE;
  int handle(drag_box_out *, int event) FL_OVERRIDE;
};

#endif // FLTK_USE_WAYLAND
