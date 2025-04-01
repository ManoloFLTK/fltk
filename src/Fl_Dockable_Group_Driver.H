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
#include <FL/Fl_Dockable_Group.H>

class Fl_Dockable_Group_Driver {
  friend class Fl_Dockable_Group;
private:
  Fl_Color undock_color_, drag_color_, dock_color_, docked_color_;
  const char *undock_label_, *drag_label_, *dock_label_, *docked_label_;
protected:
  static void delete_win_cb(Fl_Window *);
  Fl_Dockable_Group *dockable_;
public:
  class drag_box_out : public Fl_Box {
  public:
    drag_box_out(Fl_Boxtype b, int x, int y, int w, int h, const char *l) : Fl_Box(b, x, y, w, h, l) {
      labelsize(12);
    }
    int handle(int event) FL_OVERRIDE;
  };
  enum target_states {INACTIVE, MAY_RECEIVE, DOCK_HERE};
  class target_box_class : public Fl_Box {
    public:
      enum target_states state_;
      target_box_class(Fl_Boxtype bt, int x, int y, int w, int h) : Fl_Box(bt,x,y,w,h,NULL) {
        labelfont(FL_ITALIC);
        state(MAY_RECEIVE);
      }
    void state(enum target_states);
  };
  Fl_Dockable_Group_Driver(Fl_Dockable_Group *from) { dockable_ = from; }
  void store_docked_position(Fl_Dockable_Group *dock);
  virtual Fl_Box *new_target_box(Fl_Boxtype bt, int x, int y, int w, int h, Fl_Dockable_Group*) {
    return new target_box_class(bt, x, y, w, h);
  }
  virtual int handle(drag_box_out *, int event);
  static Fl_Dockable_Group_Driver *driver(Fl_Dockable_Group *dock) { return dock->driver_; }
  void state(enum Fl_Dockable_Group::states);
};


#ifdef FLTK_USE_WAYLAND

class Fl_Wayland_Dockable_Group_Driver : public Fl_Dockable_Group_Driver {
private:
  struct xdg_toplevel_drag_v1 *drag_;
  Fl_Window *copy_(drag_box_out *box, const char *t);
public:
  class wld_target_box_class : public target_box_class {
    public:
      wld_target_box_class(Fl_Boxtype bt, int x, int y, int w, int h) : target_box_class(bt,x,y,w,h) {}
      int handle(int event) FL_OVERRIDE;
  };
  Fl_Wayland_Dockable_Group_Driver(Fl_Dockable_Group *from);
  static const char *xdg_toplevel_drag_pseudo_mime;
  static Fl_Dockable_Group *active_dock;
  Fl_Box *new_target_box(Fl_Boxtype bt, int x, int y, int w, int h, Fl_Dockable_Group*) FL_OVERRIDE;
  int handle(drag_box_out *, int event) FL_OVERRIDE;
};

#endif // FLTK_USE_WAYLAND
