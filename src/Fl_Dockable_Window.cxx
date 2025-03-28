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
/** \file
 Fl_Dockable_Window implementation.
 */


#include <FL/platform.H>
#include <FL/Fl_Dockable_Window.H>
#ifdef FLTK_USE_WAYLAND
#  include "../src/drivers/Wayland/Fl_Wayland_Screen_Driver.H"
#  include "../src/drivers/Wayland/Fl_Wayland_Window_Driver.H"
#  include "../src/xdg-shell-client-protocol.h"
#  include "../src/xdg-toplevel-drag-client-protocol.h"
#endif // FLTK_USE_WAYLAND


void Fl_Dockable_Window::target_box(Fl_Boxtype bt, int x, int y, int w, int h, const char *t, Fl_Group *g) {
  if (w == 0 || h == 0) target_box_ = NULL;
  else {
    Fl_Group *save = Fl_Group::current();
    Fl_Group::current(NULL);
#ifdef FLTK_USE_WAYLAND
    if (fl_wl_display()) target_box_ = new target_box_class(bt, x, y, w, h, t);
    else
#endif
    target_box_ = new Fl_Box(bt, x, y, w, h, t);
    g->add(target_box_);
    Fl_Group::current(save);
  }
}


void Fl_Dockable_Window::delete_win(Fl_Dockable_Window *dock, void *) {
#ifdef FLTK_USE_WAYLAND
  delete dock->old_dock_;
  if (dock->drag_) xdg_toplevel_drag_v1_destroy(dock->drag_);
#endif
  delete dock;
}


int Fl_Dockable_Window::drag_box_out::handle(int event) {
  static int fromx, fromy, winx, winy;
  if (event == FL_PUSH && (Fl::event_state() & FL_BUTTON1) && window()->parent()) {
    Fl_Dockable_Window *dock = (Fl_Dockable_Window*)window();
    Fl_Window *top = top_window();
#ifdef FLTK_USE_WAYLAND
    if (fl_wl_display()) {
      struct wld_window *xid = fl_wl_xid(dock);
      Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
      xdg_toplevel_set_parent(xid->xdg_toplevel, Fl_Wayland_Window_Driver::driver(top)->xdg_toplevel());
      scr_driver->seat->data_source = wl_data_device_manager_create_data_source(scr_driver->seat->data_device_manager);
      wl_data_source_add_listener(scr_driver->seat->data_source, Fl_Wayland_Screen_Driver::p_data_source_listener, (void*)0);
      wl_data_source_offer(scr_driver->seat->data_source, "xdg_toplevel_drag_manager");
      wl_data_source_set_actions(scr_driver->seat->data_source, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
      dock->drag_ =
        xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(scr_driver->xdg_toplevel_drag, scr_driver->seat->data_source);
      //printf("start_drag surface=%p serial=%u\n",xid->wl_surface, scr_driver->seat->serial);
      wl_data_device_start_drag(scr_driver->seat->data_device, scr_driver->seat->data_source, xid->wl_surface, NULL,
                                scr_driver->seat->serial);
      Fl_Wayland_Window_Driver::driver(dock)->unmap();
      top->remove(dock);
      Fl_Dockable_Window *old = dock;
      dock = dock->copy("dragged");
    } else
#endif
    {
      dock->hide();
      top->remove(dock);
      dock->position(top->x() + dock->x() , top->y() + dock->y() );
    }
    dock->callback((Fl_Callback*)delete_win);
    label("Drag");
    dock->border(0);
    can_dock_ = false;
    dock->show();
#ifdef FLTK_USE_WAYLAND
    if (fl_wl_display()) {
      struct wld_window *xid = fl_wl_xid(dock);
      //xdg_toplevel_set_parent(xid->xdg_toplevel, Fl_Wayland_Window_Driver::driver(top)->xdg_toplevel());
      xdg_toplevel_drag_v1_attach(dock->drag_, xid->xdg_toplevel, Fl::event_x() - x(), Fl::event_y() - y());
      //printf("xdg_toplevel_drag_v1_attach to toplevel=%p\n",xid->xdg_toplevel);
    }
#endif
    return 1;
#ifdef FLTK_USE_WAYLAND
  } else if (event == FL_PUSH && fl_wl_display() && (Fl::event_state() & FL_BUTTON1)) {
    // catch again a draggable window
    Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
    Fl_Dockable_Window *dock = (Fl_Dockable_Window*)window();
    if (dock->drag_) xdg_toplevel_drag_v1_destroy(dock->drag_);
    scr_driver->seat->data_source = wl_data_device_manager_create_data_source(scr_driver->seat->data_device_manager);
    wl_data_source_add_listener(scr_driver->seat->data_source, Fl_Wayland_Screen_Driver::p_data_source_listener, (void*)0);
    wl_data_source_offer(scr_driver->seat->data_source, "xdg_toplevel_drag_manager");
    wl_data_source_set_actions(scr_driver->seat->data_source, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
    dock->drag_ = xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(scr_driver->xdg_toplevel_drag, scr_driver->seat->data_source);
    struct wld_window *xid = fl_wl_xid(dock);
    //printf("start_drag surface=%p serial=%u\n",scr_driver->seat->pointer_focus, scr_driver->seat->serial);
    wl_data_device_start_drag(scr_driver->seat->data_device, scr_driver->seat->data_source,
                              scr_driver->seat->pointer_focus, NULL, scr_driver->seat->serial);
    //printf("xdg_toplevel_drag_v1_attach to toplevel=%p\n",xid->xdg_toplevel);
    // need to attach AFTER start_drag even though xdg_toplevel_drag protocol doc says opposite!
    xdg_toplevel_drag_v1_attach(dock->drag_, xid->xdg_toplevel, Fl::event_x() - x(), Fl::event_y() - y());
    return 1;
#endif
  } else if (event == FL_PUSH && (Fl::event_state() & FL_BUTTON1)) {
    fromx = Fl::event_x_root();
    fromy = Fl::event_y_root();
    winx = window()->x_root();
    winy = window()->y_root();
    return 1;
  } else if (event == FL_DRAG && !window()->parent()
#ifdef FLTK_USE_WAYLAND
             && !fl_wl_display()
#endif
             ) {
    int deltax = Fl::event_x_root() - fromx;
    int deltay = Fl::event_y_root() - fromy;
    window()->position(winx + deltax, winy + deltay);
    Fl_Dockable_Window *dock = (Fl_Dockable_Window*)window();
    Fl_Box *target = dock->target_box();
    if (target) {
      int target_x = target->window()->x() + target->x();
      int target_y = target->window()->y() + target->y();
      can_dock_ = (window()->x() + window()->w() >= target_x &&
                   window()->x() < target_x + target->w() &&
                   window()->y() >= target_y && window()->y() < target_y + target->h());
      if (can_dock_){
        label("Dock");
        target->color(FL_RED);
        target->redraw();
      } else {
        label("Drag");
        target->color(FL_BACKGROUND_COLOR);
        target->redraw();
      }
    }
    return 1;
  } else if (event == FL_RELEASE && !window()->parent() && can_dock_
#ifdef FLTK_USE_WAYLAND
             && !fl_wl_display()
#endif
             ) {
    Fl_Dockable_Window *dock = (Fl_Dockable_Window*)window();
    Fl_Box *target = dock->target_box();
    Fl_Window *parent = target->window();
    target->hide();
    Fl_Window *win = window();
    win->hide();
    parent->add(win);
    win->position(target->x(), target->y());
    label("Undock");
    dock->target_box(FL_NO_BOX, 0, 0, 0, 0, NULL, NULL);
    delete target;
    win->show();
    return 1;
  }
  return Fl_Box::handle(event);
}


void Fl_Dockable_Window::command_box(int x, int y, int w, int h, const char *t) {
  drag_box_out *drag = new drag_box_out(FL_DOWN_BOX, x,y,w,h, t);
  this->add(drag);
#ifdef FLTK_USE_WAYLAND
  command_box_ = drag;
#endif
}


#ifdef FLTK_USE_WAYLAND

Fl_Dockable_Window *Fl_Dockable_Window::copy(const char *t) {
  Fl_Dockable_Window *dock2 = new Fl_Dockable_Window(x(), y(), w(), h(), t);
  while (children()) {
    Fl_Widget *wid = child(0);
    remove(wid);
    dock2->add(wid);
  }
  target_box()->user_data(dock2);
  dock2->resizable( resizable() == this ? dock2 : resizable() );
  dock2->target_box_ = target_box_;
  dock2->drag_ = drag_;
  dock2->command_box_ = command_box_;
  dock2->old_dock_ = this;
  dock2->user_data( user_data() );
  user_data(NULL);
  dock2->color( color() );
  dock2->box( box() );
  return dock2;
}


int Fl_Dockable_Window::target_box_class::handle(int event) {
  Fl_Dockable_Window *dock = (Fl_Dockable_Window*)user_data();
  if (event == FL_DND_ENTER) {
    //puts("FL_DND_ENTER");
    dock->command_box_->label("Dock"); dock->command_box_->redraw_label();
    color(FL_RED); redraw();
    return 1;
  } else if (event == FL_DND_DRAG) {
    //puts("FL_DND_DRAG");
    return 1;
  } else if (event == FL_DND_LEAVE) {
    //puts("FL_DND_LEAVE");
    dock->command_box_->label("Drag"); dock->command_box_->redraw_label();
    color(FL_BACKGROUND_COLOR); redraw();
    return 1;
  } else if (event == FL_DND_RELEASE) {
    //puts("FL_DND_RELEASE");
    xdg_toplevel_drag_v1_destroy(dock->drag_);
    Fl_Box *target = this;
    Fl_Window *parent = target->window();
    target->hide();
    user_data(NULL);
    dock->hide();
    parent->add(dock);
    dock->position(target->x(), target->y());
    label("Undock");
    delete target;
    dock->show();
    dock->command_box_->label("Docked");
    dock->command_box_->color(FL_BACKGROUND_COLOR);
    delete dock->old_dock_;
    dock->old_dock_ = NULL;
    return 0; // not to generate FL_PASTE event
  }
  return Fl_Box::handle(event);
}

#endif // FLTK_USE_WAYLAND

