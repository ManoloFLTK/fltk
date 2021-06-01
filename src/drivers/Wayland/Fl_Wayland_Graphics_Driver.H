//
// Definition of class Fl_Wayland_Graphics_Driver.
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

/**
 \file Fl_Wayland_Graphics_Driver.H
 \brief Definition of Wayland graphics driver.
 */

#ifndef FL_WAYLAND_GRAPHICS_DRIVER_H
#define FL_WAYLAND_GRAPHICS_DRIVER_H

#include "../Cairo/Fl_Cairo_Graphics_Driver.H"
#include <cairo/cairo.h>
#include <wayland-client.h>
typedef struct _PangoLayout  PangoLayout;

struct buffer {
  struct wl_buffer *wl_buffer;
  void *data;
  size_t data_size; // of wl_buffer and draw_buffer
  int stride;
  int width;
  unsigned char *draw_buffer;
  bool wl_buffer_ready;
  bool draw_buffer_needs_commit;
  cairo_surface_t *cairo_surface_;
  cairo_t *cairo_;
  PangoLayout *pango_layout_;
};
struct wld_window;

class Fl_Wayland_Graphics_Driver : public Fl_Cairo_Graphics_Driver {
private:
  cairo_surface_t *cairo_surface_;
  PangoLayout *dummy_pango_layout_; // used to measure text width before showing a window
  void draw_cached_pattern_(Fl_Image *img, cairo_pattern_t *pat, int X, int Y, int W, int H, int cx, int cy);
public:
  Fl_Wayland_Graphics_Driver();
  ~Fl_Wayland_Graphics_Driver();
  void activate(struct buffer *buffer, int scale);
  void font(Fl_Font fnum, Fl_Fontsize s);
  Fl_Font font() { return Fl_Graphics_Driver::font(); }
  void draw(const char* s, int nBytes, int x, int y) { draw(s, nBytes, float(x), float(y)); }
  void draw(const char* s, int nBytes, float x, float y);
  void draw(int angle, const char *str, int n, int x, int y);
  void rtl_draw(const char* str, int n, int x, int y);
  int height();
  int descent();
  double width(const char *str, int n);
  double width(unsigned c);
  void text_extents(const char* txt, int n, int& dx, int& dy, int& w, int& h);
  int not_clipped(int x, int y, int w, int h);
  int clip_box(int x, int y, int w, int h, int &X, int &Y, int &W, int &H);
  void restore_clip();
  Fl_Region XRectangleRegion(int x, int y, int w, int h);
  void add_rectangle_to_region(Fl_Region r, int X, int Y, int W, int H);
  void XDestroyRegion(Fl_Region r);
  void set_color(Fl_Color i, unsigned c);
  Fl_Font set_fonts(const char* pattern_name);
  const char *font_name(int num);
  void font_name(int num, const char *name);
  const char* get_font_name(Fl_Font fnum, int* ap);
  int get_font_sizes(Fl_Font fnum, int*& sizep);
  void point(int x, int y);
  void copy_offscreen(int x, int y, int w, int h, Fl_Offscreen osrc, int srcx, int srcy);
  void draw_image(const uchar *data, int ix, int iy, int iw, int ih, int D, int LD);
  void curve(double x, double y, double x1, double y1, double x2, double y2, double x3, double y3);
  void begin_points();
  void end_points();
  void transformed_vertex(double x, double y);
  void draw_rgb(Fl_RGB_Image *rgb,int XP, int YP, int WP, int HP, int cx, int cy);
  void cache(Fl_RGB_Image *rgb);
  void uncache(Fl_RGB_Image *img, fl_uintptr_t &id_, fl_uintptr_t &mask_);
  void draw_bitmap(Fl_Bitmap *bm,int XP, int YP, int WP, int HP, int cx, int cy);
  void cache(Fl_Bitmap *img);
  void delete_bitmask(Fl_Bitmask bm);
  void cache(Fl_Pixmap *pxm);
  void draw_pixmap(Fl_Pixmap *rgb,int XP, int YP, int WP, int HP, int cx, int cy);
  void uncache_pixmap(fl_uintptr_t p);
  static void init_built_in_fonts();
  static struct buffer *create_shm_buffer(int width, int height, uint32_t format, struct wld_window *window);
  static void buffer_release(struct wld_window *window);
  static void buffer_commit(struct wld_window *window);
  static void cairo_init(struct buffer *buffer, int width, int height, int stride);
void line(int x1, int y1, int x2, int y2);
void line(int x1, int y1, int x2, int y2, int x3, int y3);
void xyline(int x, int y, int x1);
void xyline(int x, int y, int x1, int y2);
void xyline(int x, int y, int x1, int y2, int x3);
void yxline(int x, int y, int y1);
void yxline(int x, int y, int y1, int x2);
void yxline(int x, int y, int y1, int x2, int y3);
void loop(int x0, int y0, int x1, int y1, int x2, int y2);
void loop(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3);
void rect(int x, int y, int w, int h);
void rectf(int x, int y, int w, int h);
void polygon(int x0, int y0, int x1, int y1, int x2, int y2);
void polygon(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3);
void end_loop();
void end_line();
void end_polygon();
};

#endif // FL_WAYLAND_GRAPHICS_DRIVER_H
