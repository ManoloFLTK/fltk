//
// Definition of X11 Screen interface
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

/**
 \file Fl_Wayland_Screen_Driver.H
 \brief Definition of Wayland Screen interface
 */

#ifndef FL_WAYLAND_SCREEN_DRIVER_H
#define FL_WAYLAND_SCREEN_DRIVER_H

#include "../../Fl_Screen_Driver.H"
#include <wayland-cursor.h>
#include <wayland-client.h>


class Fl_Window;

struct seat {
  struct wl_seat *wl_seat;
  struct wl_pointer *wl_pointer;
  struct wl_keyboard *wl_keyboard;
  uint32_t keyboard_enter_serial;
  struct wl_surface *keyboard_surface;
  struct wl_list link;
  struct wl_list pointer_outputs;
  struct wl_cursor_theme *cursor_theme;
  struct wl_cursor *default_cursor;
  struct wl_surface *cursor_surface;
  struct wl_surface *pointer_focus;
  int pointer_scale;
  uint32_t serial;
  struct wl_data_device_manager *data_device_manager;
  struct wl_data_device *data_device;
  struct wl_data_source *data_source;
  struct xkb_state *xkb_state;
  struct xkb_context *xkb_context;
  struct xkb_keymap *xkb_keymap;
  struct xkb_compose_state *xkb_compose_state;
  char *name;
};

class FL_EXPORT Fl_Wayland_Screen_Driver : public Fl_Screen_Driver
{
  friend class Fl_Screen_Driver;
public:
  typedef struct {
    short x_org;
    short y_org;
    short width;
    short height;
    float scale;
  } FLScreenInfo;
  FLScreenInfo screens[MAX_SCREENS];
  float dpi[MAX_SCREENS][2];
  int poll_or_select();
  int poll_or_select_with_delay(double time_to_wait);
  int get_mouse_unscaled(int &xx, int &yy);
  void screen_count(int count) {num_screens = count;}
  
  void reset_cursor();
  struct wl_cursor *xc_arrow;
  struct wl_cursor *xc_ns;
  struct wl_cursor *xc_wait;
  struct wl_cursor *xc_insert;
  struct wl_cursor *xc_hand;
  struct wl_cursor *xc_help;
  struct wl_cursor *xc_cross;
  struct wl_cursor *xc_move;
  struct wl_cursor *xc_north;
  struct wl_cursor *xc_south;
  struct wl_cursor *xc_west;
  struct wl_cursor *xc_east;
  struct wl_cursor *xc_we;
  struct wl_cursor *xc_nesw;
  struct wl_cursor *xc_nwse;
  struct wl_cursor *xc_sw;
  struct wl_cursor *xc_se;
  struct wl_cursor *xc_ne;
  struct wl_cursor *xc_nw;
  static const struct wl_data_device_listener *p_data_device_listener;

public:
  struct wl_compositor *wl_compositor;
  struct wl_subcompositor *wl_subcompositor;
  struct wl_shm *wl_shm;
  struct wl_list seats;
  struct seat *seat;
  struct wl_list outputs;
  struct output {
    uint32_t id;
    struct wl_output *wl_output;
    int scale;
    struct wl_list link;
  };
  struct libdecor *libdecor_context;
  struct xdg_wm_base *xdg_wm_base;
  
  Fl_Wayland_Screen_Driver();
  virtual APP_SCALING_CAPABILITY rescalable() { return PER_SCREEN_APP_SCALING; }
  virtual float scale(int n) {return screens[n].scale;}
  virtual void scale(int n, float f) { screens[n].scale = f;}
  virtual void desktop_scale_factor();
  int screen_num_unscaled(int x, int y);

  void copy_image(const unsigned char* data, int W, int H);
  // --- screen configuration
  void init_workarea();
  virtual void init();
  virtual int x();
  virtual int y();
  virtual int w();
  virtual int h();
  virtual void screen_xywh(int &X, int &Y, int &W, int &H, int n);
  virtual void screen_dpi(float &h, float &v, int n=0);
  virtual void screen_work_area(int &X, int &Y, int &W, int &H, int n);
  // --- audible output
  virtual void beep(int type);
  // --- global events
  virtual void flush();
  virtual double wait(double time_to_wait);
  virtual int ready();
  virtual void grab(Fl_Window* win);
  // --- global colors
  virtual void get_system_colors();
  virtual const char *get_system_scheme();
  // --- global timers
  virtual void add_timeout(double time, Fl_Timeout_Handler cb, void *argp);
  virtual void repeat_timeout(double time, Fl_Timeout_Handler cb, void *argp);
  virtual int has_timeout(Fl_Timeout_Handler cb, void *argp);
  virtual void remove_timeout(Fl_Timeout_Handler cb, void *argp);
  virtual int dnd(int unused);
  virtual int compose(int &del);
  virtual void compose_reset();
  virtual int text_display_can_leak();
  virtual Fl_RGB_Image *read_win_rectangle(int X, int Y, int w, int h, Fl_Window *win, bool may_capture_subwins, bool *did_capture_subwins);
  virtual int get_mouse(int &x, int &y);
  virtual void enable_im();
  virtual void disable_im();
  virtual void open_display_platform();
  virtual void close_display();
  // --- compute dimensions of an Fl_Offscreen
  virtual void offscreen_size(Fl_Offscreen o, int &width, int &height);
  virtual void default_icons(const Fl_RGB_Image *icons[], int count);
  virtual int has_marked_text();
  virtual void reset_marked_text();
  static int next_marked_length; // next length of marked text after current marked text will have been replaced
  // --- Wayland-special
  void set_cursor();
  struct wl_cursor *default_cursor();
  void default_cursor(struct wl_cursor *cursor);
  struct wl_cursor *cache_cursor(const char *cursor_name);
uint32_t get_serial();
struct wl_seat *get_wl_seat();
//char  *get_seat_name();
  struct xkb_keymap *get_xkb_keymap();
  static bool own_output(struct wl_output *output);
};


#endif // FL_WAYLAND_SCREEN_DRIVER_H
