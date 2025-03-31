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
 Fl_Wayland_Dockable_Group_Driver implementation.
 */


#include <FL/Fl_Dockable_Group.H>
#include "../../Fl_Dockable_Group_Driver.H"
#include "Fl_Wayland_Screen_Driver.H"
#include "Fl_Wayland_Window_Driver.H"
#include "xdg-shell-client-protocol.h"
#include "xdg-toplevel-drag-client-protocol.h"


const char *Fl_Wayland_Dockable_Group_Driver::xdg_toplevel_drag_pseudo_mime = "xdg_toplevel_drag_manager";


Fl_Wayland_Dockable_Group_Driver::Fl_Wayland_Dockable_Group_Driver() {
  drag_ = NULL;
  command_box_ = NULL;
}


void Fl_Wayland_Dockable_Group_Driver::delete_win(Fl_Dockable_Group *dock) {
  if (drag_) xdg_toplevel_drag_v1_destroy(drag_);
  drag_ = NULL;
  delete dock->window();
}


Fl_Box *Fl_Wayland_Dockable_Group_Driver::new_target_box(Fl_Boxtype bt,
                                                          int x, int y, int w, int h, const char *t,
                                                          Fl_Dockable_Group *dock) {
  Fl_Box *box = new target_box_class(bt, x, y, w, h, t);
  box->user_data(dock);
  return box;
}


void Fl_Wayland_Dockable_Group_Driver::command_box(Fl_Box *b) {
  command_box_ = b;
}


Fl_Window *Fl_Wayland_Dockable_Group_Driver::copy_(Fl_Dockable_Group *from, drag_box_out *box, const char *t) {
  // transform the dockable group into a draggable, borderless toplevel window
  Fl_Group *top = from->parent();
  top->remove(from);
  top->redraw();
  Fl_Window *win = new Fl_Window(0, 0, from->w(), from->h(), t);
  // TODO fix position of from's widgets
  from->position(0,0);
  win->add(from);
  win->end();
  win->callback((Fl_Callback0*)Fl_Dockable_Group_Driver::delete_win_cb);
  from->state = Fl_Dockable_Group::DRAG;
  box->label("Drag");
  win->border(0);
  return win;
}


int Fl_Wayland_Dockable_Group_Driver::target_box_class::handle(int event) {
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)user_data();
  if (!dock) return 0;
  Fl_Wayland_Dockable_Group_Driver *dr = (Fl_Wayland_Dockable_Group_Driver*)Fl_Dockable_Group_Driver::driver(dock);
  if (event == FL_DND_ENTER) {
    //puts("FL_DND_ENTER");
    dock->state = Fl_Dockable_Group::DOCK;
    dr->command_box_->label("Dock");
    dr->command_box_->color(FL_RED);
    dr->command_box_->redraw();
    color(FL_RED); redraw();
    return 1;
  } else if (event == FL_DND_DRAG) {
    //puts("FL_DND_DRAG");
    return 1;
  } else if (event == FL_DND_LEAVE) {
    //puts("FL_DND_LEAVE");
    dock->state = Fl_Dockable_Group::DRAG;
    dr->command_box_->label("Drag");
    dr->command_box_->color(FL_BACKGROUND_COLOR);
    dr->command_box_->redraw();
    color(FL_BACKGROUND_COLOR); redraw();
    return 1;
  } else if (event == FL_DND_RELEASE && dock->state == Fl_Dockable_Group::DOCK) {
    //puts("FL_DND_RELEASE");
    Fl_Wayland_Dockable_Group_Driver *dr = (Fl_Wayland_Dockable_Group_Driver*)Fl_Dockable_Group_Driver::driver(dock);
    xdg_toplevel_drag_v1_destroy(dr->drag_);
    dr->drag_ = NULL;
    Fl_Box *target = this;
    Fl_Window *parent = target->window();
    target->hide();
    user_data(NULL);
    Fl_Window *top = dock->window();
    top->hide();
    top->remove(dock);
    delete top;
    parent->add(dock);
    dock->position(target->x(), target->y());
    dock->state = Fl_Dockable_Group::DOCKED;
    delete target;
    dock->clear_visible();
    dock->show();
    dr->command_box_->label("Docked");
    dr->command_box_->color(FL_BACKGROUND_COLOR);
    return 0; // not to generate FL_PASTE event
  }
  return Fl_Box::handle(event);
}


int Fl_Wayland_Dockable_Group_Driver::handle(Fl_Dockable_Group_Driver::drag_box_out *box, int event) {
  if (event != FL_PUSH || !(Fl::event_state() & FL_BUTTON1)) return box->Fl_Box::handle(event);
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)box->parent();
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  if (dock->state == Fl_Dockable_Group::UNDOCK) {
    Fl_Window *top = dock->top_window();
    // It seems that while MUTTER accepts to apply the xdg_toplevel_drag protocol
    // to a subwindow, KWIN doesn't accept it and works OK only when dragging inside a toplevel.
    struct wld_window *xid = fl_wl_xid(dock->window());
    scr_driver->seat->data_source = wl_data_device_manager_create_data_source(scr_driver->seat->data_device_manager);
    wl_data_source_add_listener(scr_driver->seat->data_source, Fl_Wayland_Screen_Driver::p_data_source_listener, (void*)0);
    wl_data_source_offer(scr_driver->seat->data_source, xdg_toplevel_drag_pseudo_mime);
    wl_data_source_set_actions(scr_driver->seat->data_source, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
    Fl_Wayland_Dockable_Group_Driver *dr = (Fl_Wayland_Dockable_Group_Driver*)Fl_Dockable_Group_Driver::driver(dock);
    dr->drag_ =
    xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(scr_driver->xdg_toplevel_drag, scr_driver->seat->data_source);
    printf("start_drag surface=%p serial=%u\n",xid->wl_surface, scr_driver->seat->serial);
    wl_data_device_start_drag(scr_driver->seat->data_device, scr_driver->seat->data_source,
                              xid->wl_surface, NULL, scr_driver->seat->serial);
    int dock_x = dock->x(), dock_y = dock->y();
    Fl_Window *new_win = Fl_Wayland_Dockable_Group_Driver::copy_(dock, box, "dragged");
    new_win->show();
    xid = fl_wl_xid(new_win);
    xdg_toplevel_set_parent(xid->xdg_toplevel, Fl_Wayland_Window_Driver::driver(top)->xdg_toplevel());
    xdg_toplevel_drag_v1_attach(dr->drag_, xid->xdg_toplevel, Fl::event_x() - dock_x, Fl::event_y() - dock_y);
    printf("xdg_toplevel_drag_v1_attach to toplevel=%p\n",xid->xdg_toplevel);
  } else if (dock->state == Fl_Dockable_Group::DRAG) {
    // catch again a draggable window
    Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
    Fl_Wayland_Dockable_Group_Driver *dr = (Fl_Wayland_Dockable_Group_Driver*)Fl_Dockable_Group_Driver::driver(dock);
    if (dr->drag_) xdg_toplevel_drag_v1_destroy(dr->drag_);
    scr_driver->seat->data_source = wl_data_device_manager_create_data_source(scr_driver->seat->data_device_manager);
    wl_data_source_add_listener(scr_driver->seat->data_source, Fl_Wayland_Screen_Driver::p_data_source_listener, (void*)0);
    wl_data_source_offer(scr_driver->seat->data_source, xdg_toplevel_drag_pseudo_mime);
    wl_data_source_set_actions(scr_driver->seat->data_source, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
    dr->drag_ = xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(scr_driver->xdg_toplevel_drag, scr_driver->seat->data_source);
    struct wld_window *xid = fl_wl_xid(dock->window());
    printf("start_drag surface=%p serial=%u\n",scr_driver->seat->pointer_focus, scr_driver->seat->serial);
    wl_data_device_start_drag(scr_driver->seat->data_device, scr_driver->seat->data_source,
                              scr_driver->seat->pointer_focus, NULL, scr_driver->seat->serial);
    printf("xdg_toplevel_drag_v1_attach to toplevel=%p\n",xid->xdg_toplevel);
    // need to attach AFTER start_drag even though xdg_toplevel_drag protocol doc says opposite!
    xdg_toplevel_drag_v1_attach(dr->drag_, xid->xdg_toplevel, Fl::event_x(), Fl::event_y());
  }
  return 1;
}
