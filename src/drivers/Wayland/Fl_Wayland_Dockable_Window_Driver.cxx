//
// Wayland-specific dockable window code for the Fast Light Tool Kit (FLTK).
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
 Fl_Wayland_Dockable_Window_Driver implementation.
 */


#include <FL/Fl_Dockable_Window.H>
#include "../../Fl_Dockable_Window_Driver.H"
#include "Fl_Wayland_Screen_Driver.H"
#include "Fl_Wayland_Window_Driver.H"
#include "xdg-shell-client-protocol.h"
#include "xdg-toplevel-drag-client-protocol.h"


Fl_Wayland_Dockable_Window_Driver::Fl_Wayland_Dockable_Window_Driver() {
  old_dock_ = NULL;
  drag_ = NULL;
  command_box_ = NULL;
}


void Fl_Wayland_Dockable_Window_Driver::delete_win(Fl_Dockable_Window *dock) {
  Fl_Wayland_Dockable_Window_Driver *dr = (Fl_Wayland_Dockable_Window_Driver*)Fl_Dockable_Window_Driver::driver(dock);
  delete dr->old_dock_;
  if (dr->drag_) xdg_toplevel_drag_v1_destroy(dr->drag_);
  delete dock;
}


Fl_Box *Fl_Wayland_Dockable_Window_Driver::new_target_box(Fl_Boxtype bt,
                                                          int x, int y, int w, int h, const char *t) {
  return new target_box_class(bt, x, y, w, h, t);
}


void Fl_Wayland_Dockable_Window_Driver::command_box(Fl_Box *b) {
  command_box_ = b;
}


Fl_Dockable_Window *Fl_Wayland_Dockable_Window_Driver::copy(Fl_Dockable_Window *from, const char *t) {
  Fl_Dockable_Window *dock2 = new Fl_Dockable_Window(from->x(), from->y(), from->w(), from->h(), t);
  while (from->children()) {
    Fl_Widget *wid = from->child(0);
    from->remove(wid);
    dock2->add(wid);
  }
  from->target_box()->user_data(dock2);
  dock2->resizable( from->resizable() == from ? dock2 : from->resizable() );
  *target_box_address(dock2) = from->target_box();
  Fl_Wayland_Dockable_Window_Driver *dr = (Fl_Wayland_Dockable_Window_Driver*)Fl_Dockable_Window_Driver::driver(from);
  Fl_Wayland_Dockable_Window_Driver *dr2 = (Fl_Wayland_Dockable_Window_Driver*)Fl_Dockable_Window_Driver::driver(dock2);
  dr2->drag_ = dr->drag_;
  dr2->command_box_ = dr->command_box_;
  dr2->old_dock_ = from;
  dock2->user_data( from->user_data() );
  from->user_data(NULL);
  dock2->color( from->color() );
  dock2->box( from->box() );
  return dock2;
}


int Fl_Wayland_Dockable_Window_Driver::target_box_class::handle(int event) {
  Fl_Dockable_Window *dock = (Fl_Dockable_Window*)user_data();
  if (!dock) return 0;
  Fl_Wayland_Dockable_Window_Driver *dr = (Fl_Wayland_Dockable_Window_Driver*)Fl_Dockable_Window_Driver::driver(dock);
  if (event == FL_DND_ENTER) {
    //puts("FL_DND_ENTER");
    dr->command_box_->label("Dock"); dr->command_box_->redraw_label();
    color(FL_RED); redraw();
    return 1;
  } else if (event == FL_DND_DRAG) {
    //puts("FL_DND_DRAG");
    return 1;
  } else if (event == FL_DND_LEAVE) {
    //puts("FL_DND_LEAVE");
    dr->command_box_->label("Drag"); dr->command_box_->redraw_label();
    color(FL_BACKGROUND_COLOR); redraw();
    return 1;
  } else if (event == FL_DND_RELEASE) {
    //puts("FL_DND_RELEASE");
    Fl_Wayland_Dockable_Window_Driver *dr = (Fl_Wayland_Dockable_Window_Driver*)Fl_Dockable_Window_Driver::driver(dock);
    xdg_toplevel_drag_v1_destroy(dr->drag_);
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
    dr->command_box_->label("Docked");
    dr->command_box_->color(FL_BACKGROUND_COLOR);
    delete dr->old_dock_;
    dr->old_dock_ = NULL;
    return 0; // not to generate FL_PASTE event
  }
  return Fl_Box::handle(event);
}


int Fl_Wayland_Dockable_Window_Driver::handle(Fl_Dockable_Window_Driver::drag_box_out *box, int event) {
  Fl_Dockable_Window *dock = (Fl_Dockable_Window*)box->window();
  if (event == FL_PUSH && (Fl::event_state() & FL_BUTTON1) && dock->parent()) {
    Fl_Window *top = dock->top_window();
    struct wld_window *xid = fl_wl_xid(dock);
    Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
    xdg_toplevel_set_parent(xid->xdg_toplevel, Fl_Wayland_Window_Driver::driver(top)->xdg_toplevel());
    scr_driver->seat->data_source = wl_data_device_manager_create_data_source(scr_driver->seat->data_device_manager);
    wl_data_source_add_listener(scr_driver->seat->data_source, Fl_Wayland_Screen_Driver::p_data_source_listener, (void*)0);
    wl_data_source_offer(scr_driver->seat->data_source, "xdg_toplevel_drag_manager");
    wl_data_source_set_actions(scr_driver->seat->data_source, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
    Fl_Wayland_Dockable_Window_Driver *dr = (Fl_Wayland_Dockable_Window_Driver*)Fl_Dockable_Window_Driver::driver(dock);
    dr->drag_ =
    xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(scr_driver->xdg_toplevel_drag, scr_driver->seat->data_source);
    //printf("start_drag surface=%p serial=%u\n",xid->wl_surface, scr_driver->seat->serial);
    wl_data_device_start_drag(scr_driver->seat->data_device, scr_driver->seat->data_source, xid->wl_surface, NULL,
                              scr_driver->seat->serial);
    Fl_Wayland_Window_Driver::driver(dock)->unmap();
    top->remove(dock);
    Fl_Dockable_Window *old = dock;
    dock = Fl_Wayland_Dockable_Window_Driver::copy(dock, "dragged");
    dock->callback((Fl_Callback*)Fl_Dockable_Window_Driver::delete_win_cb);
    box->label("Drag");
    dock->border(0);
    box->can_dock_ = false;
    dock->show();
    xid = fl_wl_xid(dock);
    //xdg_toplevel_set_parent(xid->xdg_toplevel, Fl_Wayland_Window_Driver::driver(top)->xdg_toplevel());
    dr = (Fl_Wayland_Dockable_Window_Driver*)Fl_Dockable_Window_Driver::driver(dock);
    xdg_toplevel_drag_v1_attach(dr->drag_, xid->xdg_toplevel, Fl::event_x() - box->x(), Fl::event_y() - box->y());
    //printf("xdg_toplevel_drag_v1_attach to toplevel=%p\n",xid->xdg_toplevel);
    return 1;
  } else if (event == FL_PUSH && (Fl::event_state() & FL_BUTTON1)) {
    // catch again a draggable window
    Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
    Fl_Wayland_Dockable_Window_Driver *dr = (Fl_Wayland_Dockable_Window_Driver*)Fl_Dockable_Window_Driver::driver(dock);
    if (dr->drag_) xdg_toplevel_drag_v1_destroy(dr->drag_);
    scr_driver->seat->data_source = wl_data_device_manager_create_data_source(scr_driver->seat->data_device_manager);
    wl_data_source_add_listener(scr_driver->seat->data_source, Fl_Wayland_Screen_Driver::p_data_source_listener, (void*)0);
    wl_data_source_offer(scr_driver->seat->data_source, "xdg_toplevel_drag_manager");
    wl_data_source_set_actions(scr_driver->seat->data_source, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
    dr->drag_ = xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(scr_driver->xdg_toplevel_drag, scr_driver->seat->data_source);
    struct wld_window *xid = fl_wl_xid(dock);
    //printf("start_drag surface=%p serial=%u\n",scr_driver->seat->pointer_focus, scr_driver->seat->serial);
    wl_data_device_start_drag(scr_driver->seat->data_device, scr_driver->seat->data_source,
                              scr_driver->seat->pointer_focus, NULL, scr_driver->seat->serial);
    //printf("xdg_toplevel_drag_v1_attach to toplevel=%p\n",xid->xdg_toplevel);
    // need to attach AFTER start_drag even though xdg_toplevel_drag protocol doc says opposite!
    xdg_toplevel_drag_v1_attach(dr->drag_, xid->xdg_toplevel, Fl::event_x() - box->x(), Fl::event_y() - box->y());
    return 1;
  }
  return box->Fl_Box::handle(event);
}
