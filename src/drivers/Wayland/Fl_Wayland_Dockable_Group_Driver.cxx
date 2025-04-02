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
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Image_Surface.H>
#include "../../Fl_Dockable_Group_Driver.H"
#include "Fl_Wayland_Screen_Driver.H"
#include "Fl_Wayland_Window_Driver.H"
#include "xdg-shell-client-protocol.h"
#ifdef HAVE_XDG_TOPLEVEL_DRAG
#  include "xdg-toplevel-drag-client-protocol.h"
#endif


const char *Fl_Wayland_Dockable_Group_Driver::xdg_toplevel_drag_pseudo_mime = "xdg_toplevel_drag_manager";


Fl_Wayland_Dockable_Group_Driver::Fl_Wayland_Dockable_Group_Driver(Fl_Dockable_Group *from) :
  Fl_Dockable_Group_Driver(from) {
#ifdef HAVE_XDG_TOPLEVEL_DRAG
  drag_ = NULL;
#endif
}


Fl_Box *Fl_Wayland_Dockable_Group_Driver::new_target_box(Fl_Boxtype bt,
                                                          int x, int y, int w, int h,
                                                          Fl_Dockable_Group *dock) {
  return new wld_target_box_class(bt, x, y, w, h);
}


Fl_Window *Fl_Wayland_Dockable_Group_Driver::copy_(cmd_box_class *box, const char *t) {
  // transform the dockable group into a draggable, borderless toplevel window
  Fl_Group *top = dockable_->parent();
  store_docked_position(dockable_);
  top->remove(dockable_);
  top->redraw();
  Fl_Window *win = new Fl_Window(dockable_->w(), dockable_->h(), t);
  dockable_->position(0,0);
  win->add(dockable_);
  win->end();
  win->callback((Fl_Callback0*)Fl_Dockable_Group_Driver::delete_win_cb);
  state(Fl_Dockable_Group::DRAG);
  win->border(0);
  return win;
}


void Fl_Wayland_Dockable_Group_Driver::handle_dnd_release(
                            Fl_Wayland_Dockable_Group_Driver::wld_target_box_class *box) {
#ifdef HAVE_XDG_TOPLEVEL_DRAG
  Fl_Dockable_Group *dock = this->dockable_;
  xdg_toplevel_drag_v1_destroy(this->drag_);
  this->drag_ = NULL;
  Fl_Dockable_Group::active_dockable = NULL;
  Fl_Group *parent = box->parent();
  parent->remove(box);
  box->state(Fl_Dockable_Group_Driver::INACTIVE);
  Fl_Dockable_Group::active_dockable = NULL;
  Fl_Window *top = dock->window();
  top->hide();
  top->remove(dock);
  delete top;
#endif
}


int Fl_Wayland_Dockable_Group_Driver::wld_target_box_class::handle(int event) {
  Fl_Dockable_Group *dock = Fl_Dockable_Group::active_dockable;
  if (!dock) return 0;
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  if (event == FL_DND_ENTER) {
    //puts("FL_DND_ENTER");
    if (scr_driver->xdg_toplevel_drag) Fl_Dockable_Group_Driver::driver(dock)->state(Fl_Dockable_Group::DOCK);
    state(DOCK_HERE);
    return 1;
  } else if (event == FL_DND_DRAG) {
    //puts("FL_DND_DRAG");
    return 1;
  } else if (event == FL_DND_LEAVE) {
    //puts("FL_DND_LEAVE");
    if (scr_driver->xdg_toplevel_drag) Fl_Dockable_Group_Driver::driver(dock)->state(Fl_Dockable_Group::DRAG);
    state(MAY_RECEIVE);
    return 1;
  } else if (event == FL_DND_RELEASE) {
    //puts("FL_DND_RELEASE");
    Fl_Group *parent = this->parent();
    ((Fl_Wayland_Dockable_Group_Driver*)driver(dock))->handle_dnd_release(this);
    parent->add(dock);
    dock->resize(x(), y(), w(), h());
    dock->clear_visible();
    Fl_Dockable_Group_Driver::driver(dock)->state(Fl_Dockable_Group::DOCKED);
    dock->show();
    Fl_Tabs *tabs = dynamic_cast<Fl_Tabs*>(parent);
    if (tabs) tabs->value(dock);
    return 0; // not to generate FL_PASTE event
  }
  return Fl_Box::handle(event);
}


int Fl_Wayland_Dockable_Group_Driver::handle(Fl_Dockable_Group_Driver::cmd_box_class *box, int event) {
#ifdef HAVE_XDG_TOPLEVEL_DRAG
  static int drag_count;
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)box->parent();
  if ((event != FL_PUSH && event != FL_DRAG) || !(Fl::event_state() & FL_BUTTON1))
    return box->Fl_Box::handle(event);
  if (event == FL_PUSH && dock->state == Fl_Dockable_Group::UNDOCK) {
    drag_count = 0;
  } else if (event == FL_DRAG && dock->state == Fl_Dockable_Group::UNDOCK && ++drag_count >= 5) {
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
    //printf("start_drag surface=%p serial=%u\n",xid->wl_surface, scr_driver->seat->serial);
    wl_data_device_start_drag(scr_driver->seat->data_device, scr_driver->seat->data_source,
                              xid->wl_surface, NULL, scr_driver->seat->serial);
    int dock_x = dock->x(), dock_y = dock->y();
    Fl_Window *new_win = copy_(box, "dragged");
    box->parent()->show(); // necessary for tabs
    new_win->show();
    Fl::pushed(dock->command_box()); // necessary for tabs
    xid = fl_wl_xid(new_win);
    xdg_toplevel_set_parent(xid->xdg_toplevel, Fl_Wayland_Window_Driver::driver(top)->xdg_toplevel());
    xdg_toplevel_drag_v1_attach(dr->drag_, xid->xdg_toplevel, Fl::event_x() - dock_x, Fl::event_y() - dock_y);
    //printf("xdg_toplevel_drag_v1_attach to toplevel=%p\n",xid->xdg_toplevel);
    Fl_Dockable_Group::active_dockable = dock;
  } else if (event == FL_PUSH && dock->state == Fl_Dockable_Group::DRAG) {
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
    //printf("start_drag surface=%p serial=%u\n",scr_driver->seat->pointer_focus, scr_driver->seat->serial);
    wl_data_device_start_drag(scr_driver->seat->data_device, scr_driver->seat->data_source,
                              scr_driver->seat->pointer_focus, NULL, scr_driver->seat->serial);
    //printf("xdg_toplevel_drag_v1_attach to toplevel=%p\n",xid->xdg_toplevel);
    // need to attach AFTER start_drag even though xdg_toplevel_drag protocol doc says opposite!
    xdg_toplevel_drag_v1_attach(dr->drag_, xid->xdg_toplevel, Fl::event_x(), Fl::event_y());
    Fl_Dockable_Group::active_dockable = dock;
  }
  return 1;
#endif // HAVE_XDG_TOPLEVEL_DRAG
  return box->Fl_Box::handle(event);
}


static struct Fl_Wayland_Graphics_Driver::wld_buffer *offscreen_from_group(Fl_Dockable_Group *dock,
                                                                           int scale) {
  int icon_size = 200; // max width or height of image for cursor in FLTK units
  struct Fl_Wayland_Graphics_Driver::wld_buffer *off;
  double r1 = double(icon_size) / dock->w();
  double r2 = double(icon_size) / dock->h();
  double r = (r1 < r2 ? r1 : r2);
  r = (r < 1 ? r : 1);
  int width = dock->w() * r;
  int height = dock->h() * r;
  Fl_Image_Surface *surf = Fl_Wayland_Graphics_Driver::custom_offscreen(width * scale, height * scale, &off);
  Fl_Surface_Device::push_current(surf);
  cairo_scale(off->draw_buffer.cairo_, r * scale, r * scale);
  surf->draw(dock);
  Fl_Surface_Device::pop_current();
  delete surf;
  cairo_surface_flush( cairo_get_target(off->draw_buffer.cairo_) );
  memcpy(off->data, off->draw_buffer.buffer, off->draw_buffer.data_size);
  return off;
}


int Fl_oldWayland_Dockable_Group_Driver::handle(Fl_Dockable_Group_Driver::cmd_box_class *box, int event) {
  static int drag_count;
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  Fl_Dockable_Group *dock = (Fl_Dockable_Group*)box->parent();
  if ((event != FL_PUSH && event != FL_DRAG) || !(Fl::event_state() & FL_BUTTON1))
    return box->Fl_Box::handle(event);
  if (event == FL_PUSH && dock->state == Fl_Dockable_Group::UNDOCK) {
    drag_count = 0;
    return 1;
  } else if (event == FL_DRAG && dock->state == Fl_Dockable_Group::UNDOCK && ++drag_count < 5) {
    return 1;
  } else if (event == FL_DRAG && dock->state == Fl_Dockable_Group::UNDOCK && drag_count >= 5) {
    Fl_Window *top = dock->top_window();
    struct wld_window *xid = fl_wl_xid(dock->window());
    state(Fl_Dockable_Group::DRAG);
    scr_driver->seat->data_source = wl_data_device_manager_create_data_source(scr_driver->seat->data_device_manager);
    wl_data_source_add_listener(scr_driver->seat->data_source, Fl_Wayland_Screen_Driver::p_data_source_listener, (void*)0);
    wl_data_source_offer(scr_driver->seat->data_source, xdg_toplevel_drag_pseudo_mime);
    wl_data_source_set_actions(scr_driver->seat->data_source, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
    Fl_Wayland_Dockable_Group_Driver *dr = (Fl_Wayland_Dockable_Group_Driver*)Fl_Dockable_Group_Driver::driver(dock);
    struct wl_surface *dnd_icon = wl_compositor_create_surface(scr_driver->wl_compositor);
    //printf("start_drag surface=%p serial=%u\n",xid->wl_surface, scr_driver->seat->serial);
    wl_data_device_start_drag(scr_driver->seat->data_device, scr_driver->seat->data_source,
                              xid->wl_surface, dnd_icon, scr_driver->seat->serial);
    int s = Fl_Wayland_Window_Driver::driver(box->top_window())->wld_scale();
    struct Fl_Wayland_Graphics_Driver::wld_buffer *off = (struct Fl_Wayland_Graphics_Driver::wld_buffer *)offscreen_from_group(dock, s);
    wl_surface_attach(dnd_icon, off->wl_buffer, 0, 0);
    wl_surface_set_buffer_scale(dnd_icon, s);
    wl_surface_damage(dnd_icon, 0, 0, 10000, 10000);
    wl_surface_commit(dnd_icon);
    wl_surface_set_user_data(dnd_icon, off);
    Fl_Dockable_Group::active_dockable = dock;
    state(Fl_Dockable_Group::UNDOCK);
    return 1;
  }
  return box->Fl_Box::handle(event);
}


void Fl_oldWayland_Dockable_Group_Driver::handle_dnd_release(
                    Fl_Wayland_Dockable_Group_Driver::wld_target_box_class *box) {
  Fl_Dockable_Group *dock = this->dockable_;
  Fl_Dockable_Group::active_dockable = NULL;
  Fl_Group *parent = box->parent();
  parent->remove(box);
  box->state(Fl_Dockable_Group_Driver::INACTIVE);
  Fl_Dockable_Group::active_dockable = NULL;
}
