//
// Implementation of the Wayland window driver.
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
#include <FL/platform.H>
#include "Fl_Wayland_Window_Driver.H"
#include "Fl_Wayland_Screen_Driver.H"
#include "Fl_Wayland_Graphics_Driver.H"
#include "Fl_Wayland_System_Driver.H"
#include "../../../libdecor/src/libdecor.h"
#include "../../../libdecor/build/xdg-shell-client-protocol.h"
#include <pango/pangocairo.h>
#include <FL/Fl_Overlay_Window.H>
#include <FL/Fl_Menu_Window.H>
#include <FL/Fl_Tooltip.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <FL/Fl.H>
#include <FL/Fl_Image_Surface.H>
#include <string.h>
#include <sys/mman.h>

#define fl_max(a,b) ((a) > (b) ? (a) : (b))

Window fl_window;


void Fl_Wayland_Window_Driver::destroy_double_buffer() {
  if (pWindow->as_overlay_window()) fl_delete_offscreen(other_xid);
  other_xid = 0;
}


Fl_Window_Driver *Fl_Window_Driver::newWindowDriver(Fl_Window *w)
{
  return new Fl_Wayland_Window_Driver(w);
}


Fl_Wayland_Window_Driver::Fl_Wayland_Window_Driver(Fl_Window *win) : Fl_Window_Driver(win)
{
  icon_ = new icon_data;
  memset(icon_, 0, sizeof(icon_data));
#if USE_XFT
  screen_num_ = -1;
#endif
}


Fl_Wayland_Window_Driver::~Fl_Wayland_Window_Driver()
{
  if (shape_data_) {
    cairo_surface_t *surface;
    cairo_pattern_get_surface(shape_data_->mask_pattern_, &surface);
    cairo_pattern_destroy(shape_data_->mask_pattern_);
    uchar *data = cairo_image_surface_get_data(surface);
    cairo_surface_destroy(surface);
    delete[] data;
    delete shape_data_;
  }
  delete icon_;
}


// --- private

void Fl_Wayland_Window_Driver::decorated_win_size(int &w, int &h)
{
  Fl_Window *win = pWindow;
  w = win->w();
  h = win->h();
  if (!win->shown() || win->parent() || !win->border() || !win->visible()) return;
  h = fl_xid(win)->decorated_height;
}


// --- window data

int Fl_Wayland_Window_Driver::decorated_h()
{
  int w, h;
  decorated_win_size(w, h);
  return h;
}

int Fl_Wayland_Window_Driver::decorated_w()
{
  int w, h;
  decorated_win_size(w, h);
  return w;
}


void Fl_Wayland_Window_Driver::take_focus()
{
  Window w = fl_xid(pWindow);
  if (w) {
    Fl_Window *old_first = Fl::first_window();
    Window first_xid = (old_first ? fl_xid(old_first->top_window()) : NULL);
    if (first_xid && first_xid != w && w->xdg_toplevel) {
      // this will move the target window to the front
      xdg_toplevel_set_parent(w->xdg_toplevel, first_xid->xdg_toplevel);
      // this will remove the parent-child relationship
      old_first->wait_for_expose();
      xdg_toplevel_set_parent(w->xdg_toplevel, NULL);
    }
    // this sets the first window
    fl_find(w);
  }
}


void Fl_Wayland_Window_Driver::flush_overlay()
{
  if (!shown()) return;
  Fl_Overlay_Window *oWindow = pWindow->as_overlay_window();
  int erase_overlay = (pWindow->damage()&FL_DAMAGE_OVERLAY) | (overlay() == oWindow);
  pWindow->clear_damage((uchar)(pWindow->damage()&~FL_DAMAGE_OVERLAY));
  pWindow->make_current();
  if (!other_xid) {
    other_xid = fl_create_offscreen(oWindow->w(), oWindow->h());
    oWindow->clear_damage(FL_DAMAGE_ALL);
  }
  if (oWindow->damage() & ~FL_DAMAGE_EXPOSE) {
    Fl_X *myi = Fl_X::i(pWindow);
    fl_clip_region(myi->region); myi->region = 0;
    fl_begin_offscreen(other_xid);
    draw();
    fl_end_offscreen();
  }
  if (erase_overlay) fl_clip_region(0);
  if (other_xid) {
    fl_copy_offscreen(0, 0, oWindow->w(), oWindow->h(), other_xid, 0, 0);
  }
  if (overlay() == oWindow) oWindow->draw_overlay();
  Window xid = fl_xid(pWindow);
  wl_surface_damage_buffer(xid->wl_surface, 0, 0, pWindow->w() * xid->scale, pWindow->h() * xid->scale);
}


const Fl_Image* Fl_Wayland_Window_Driver::shape() {
  return shape_data_ ? shape_data_->shape_ : NULL;
}

void Fl_Wayland_Window_Driver::shape_bitmap_(Fl_Image* b) { // needs testing
  // complement the bits of the Fl_Bitmap and control its stride too
  int i, j, w = b->w(), h = b->h();
  int bytesperrow = cairo_format_stride_for_width(CAIRO_FORMAT_A1, w);
  uchar* bits = new uchar[h * bytesperrow];
  const uchar *q = ((Fl_Bitmap*)b)->array;
  for (i = 0; i < h; i++) {
    uchar *p = bits + i * bytesperrow;
    for (j = 0; j < w; j++) {
      *p++ = ~*q++;
    }
  }
  cairo_surface_t *mask_surf = cairo_image_surface_create_for_data(bits, CAIRO_FORMAT_A1, w, h, bytesperrow);
  shape_data_->mask_pattern_ = cairo_pattern_create_for_surface(mask_surf);
  shape_data_->shape_ = b;
  shape_data_->lw_ = w;
  shape_data_->lh_ = h;
}

void Fl_Wayland_Window_Driver::shape_alpha_(Fl_Image* img, int offset) {
  int i, j, d = img->d(), w = img->w(), h = img->h();
  int bytesperrow = cairo_format_stride_for_width(CAIRO_FORMAT_A1, w);
  unsigned u;
  uchar byte, onebit;
  // build a CAIRO_FORMAT_A1 surface covering the non-fully transparent/black part of the image
  uchar* bits = new uchar[h*bytesperrow]; // to store the surface data
  const uchar* alpha = (const uchar*)*img->data() + offset; // points to alpha value of rgba pixels
  for (i = 0; i < h; i++) {
    uchar *p = (uchar*)bits + i * bytesperrow;
    byte = 0;
    onebit = 1;
    for (j = 0; j < w; j++) {
      if (d == 3) {
        u = *alpha;
        u += *(alpha+1);
        u += *(alpha+2);
      }
      else u = *alpha;
      if (u > 0) { // if the pixel is not fully transparent/black
        byte |= onebit; // turn on the corresponding bit of the bitmap
      }
      onebit = onebit << 1; // move the single set bit one position to the left
      if (onebit == 0 || j == w-1) {
        onebit = 1;
        *p++ = ~byte; // store in bitmap one pack of bits, complemented
        byte = 0;
      }
      alpha += d; // point to alpha value of next img pixel
    }
  }
  cairo_surface_t *mask_surf = cairo_image_surface_create_for_data(bits, CAIRO_FORMAT_A1, w, h, bytesperrow);
  shape_data_->mask_pattern_ = cairo_pattern_create_for_surface(mask_surf);
  shape_data_->shape_ = img;
  shape_data_->lw_ = w;
  shape_data_->lh_ = h;
}

void Fl_Wayland_Window_Driver::shape(const Fl_Image* img) {
  if (shape_data_) {
    if (shape_data_->mask_pattern_) {
      cairo_surface_t *surface;
      cairo_pattern_get_surface(shape_data_->mask_pattern_, &surface);
      cairo_pattern_destroy(shape_data_->mask_pattern_);
      uchar *data = cairo_image_surface_get_data(surface);
      cairo_surface_destroy(surface);
      delete[] data;
    }
  }
  else {
    shape_data_ = new shape_data_type;
  }
  memset(shape_data_, 0, sizeof(shape_data_type));
  pWindow->border(false);
  int d = img->d();
  if (d && img->count() >= 2) {
    shape_pixmap_((Fl_Image*)img);
    shape_data_->shape_ = (Fl_Image*)img;
  }
  else if (d == 0) shape_bitmap_((Fl_Image*)img);
  else if (d == 2 || d == 4) shape_alpha_((Fl_Image*)img, d - 1);
  else if ((d == 1 || d == 3) && img->count() == 1) shape_alpha_((Fl_Image*)img, 0);
}

void Fl_Wayland_Window_Driver::draw_end()
{
  if (shape_data_ && shape_data_->mask_pattern_) {
    Fl_Wayland_Graphics_Driver *gr_dr = (Fl_Wayland_Graphics_Driver*)fl_graphics_driver;
    cairo_t *cr = gr_dr->cr();
    cairo_matrix_t matrix;
    cairo_matrix_init_scale(&matrix, double(shape_data_->lw_)/pWindow->w() , double(shape_data_->lh_)/pWindow->h());
    cairo_pattern_set_matrix(shape_data_->mask_pattern_, &matrix);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_mask(cr, shape_data_->mask_pattern_);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  }
}


void Fl_Wayland_Window_Driver::icons(const Fl_RGB_Image *icons[], int count) {
  free_icons();

  if (count > 0) {
    icon_->icons = new Fl_RGB_Image*[count];
    icon_->count = count;
    // FIXME: Fl_RGB_Image lacks const modifiers on methods
    for (int i = 0;i < count;i++) {
      icon_->icons[i] = (Fl_RGB_Image*)((Fl_RGB_Image*)icons[i])->copy();
      icon_->icons[i]->normalize();
    }
  }

  if (Fl_X::i(pWindow))
    set_icons();
}

const void *Fl_Wayland_Window_Driver::icon() const {
  return icon_->legacy_icon;
}

void Fl_Wayland_Window_Driver::icon(const void * ic) {
  free_icons();
  icon_->legacy_icon = ic;
}

void Fl_Wayland_Window_Driver::free_icons() {
  int i;
  icon_->legacy_icon = 0L;
  if (icon_->icons) {
    for (i = 0;i < icon_->count;i++)
      delete icon_->icons[i];
    delete [] icon_->icons;
    icon_->icons = 0L;
  }
  icon_->count = 0;
}


/* Returns images of the captures of the window title-bar, and the left, bottom and right window borders
 (or NULL if a particular border is absent).
 Returned images can be deleted after use. Their depth and size may be platform-dependent.
 The top and bottom images extend from left of the left border to right of the right border.
 */
void Fl_Wayland_Window_Driver::capture_titlebar_and_borders(Fl_RGB_Image*& top, Fl_RGB_Image*& left, Fl_RGB_Image*& bottom, Fl_RGB_Image*& right)
{
  top = left = bottom = right = NULL;
  if (pWindow->decorated_h() == h()) return;
  int htop = pWindow->decorated_h() - pWindow->h();
// reproduce the target window's titlebar
  Fl_Image_Surface *surf = new Fl_Image_Surface(pWindow->w(), htop, 1);
  Fl_Surface_Device::push_current(surf);
  fl_color(FL_BLACK);
  fl_rectf(0, 0, pWindow->w(), htop);
  fl_color(FL_WHITE);
  Fl::set_font(FL_SCREEN_BOLD, "sans Bold");
  fl_font(FL_SCREEN_BOLD, 20);
  double w = fl_width(pWindow->label());
  fl_draw(pWindow->label(), pWindow->w()/2 - w/2, htop - fl_descent() - 1);
  int X = pWindow->w()-1.1*htop;
  fl_line(X, htop-5, X+htop-10, 5);
  fl_line(X,5, X+htop-10,htop-5);
  X -= 1.2*htop;
  if (!pWindow->resizable()) fl_color(fl_gray_ramp(4));
  fl_rect(X, 5, htop-10,htop-10);
  fl_color(FL_WHITE);
  X -= 1.2*htop;
  fl_xyline(X, htop-5, X+htop-10);
  top = surf->image();
  Fl_Surface_Device::pop_current();
  delete surf;
  top->scale(pWindow->w(), htop);
}


// make drawing go into this window (called by subclass flush() impl.)
void Fl_Wayland_Window_Driver::make_current() {
  if (!shown()) {
    static const char err_message[] = "Fl_Window::make_current(), but window is not shown().";
    fl_alert(err_message);
    Fl::fatal(err_message);
  }
  
  struct wld_window *window = fl_xid(pWindow);
  // to support progressive drawing
  if ( (!Fl_Wayland_Window_Driver::in_flush) && window && window->buffer) {
    if (!window->buffer->draw_buffer_needs_commit) {
      wl_surface_damage_buffer(window->wl_surface, 0, 0, pWindow->w() * window->scale, pWindow->h() * window->scale);
//fprintf(stderr, "direct make_current calls damage_buffer\n");
    } else if (window->buffer->wl_buffer_ready) {
//fprintf(stderr, "direct make_current calls buffer_commit\n");
      Fl_Wayland_Graphics_Driver::buffer_commit(window);
    }
  }

  fl_graphics_driver->clip_region(0);
  fl_window = window;
  if (!window->buffer) window->buffer = Fl_Wayland_Graphics_Driver::create_shm_buffer(
          pWindow->w() * window->scale, pWindow->h() * window->scale, WL_SHM_FORMAT_ARGB8888, window);
  ((Fl_Wayland_Graphics_Driver*)fl_graphics_driver)->activate(window->buffer, window->scale);

#ifdef FLTK_USE_CAIRO
  // update the cairo_t context
  if (Fl::cairo_autolink_context()) Fl::cairo_make_current(pWindow);
#endif
}


void Fl_Wayland_Window_Driver::flush() {
  if (!pWindow->damage()) return;
  if (pWindow->as_gl_window()) {
    Fl_Window_Driver::flush();
    static Fl_Wayland_Plugin *plugin = NULL;
    if (!plugin) {
      Fl_Plugin_Manager pm("fltk:wayland");
      plugin = (Fl_Wayland_Plugin*)pm.plugin("gl_overlay.wayland.fltk.org");
    }
    if (plugin) plugin->do_swap(pWindow); // useful only for GL win with overlay
    return;
  }
  struct wld_window *window = fl_xid(pWindow);
  if (!window || !window->configured_width) return;
  
  Fl_X *i = Fl_X::i(pWindow);
  Fl_Region r = i->region;
  if (r && window->buffer && window->buffer->wl_buffer_ready) {
    for (int i = 0; i < r->count; i++) {
      int left = r->rects[i].x * window->scale;
      int top = r->rects[i].y * window->scale;
      int width = r->rects[i].width * window->scale;
      int height = r->rects[i].height * window->scale;
      wl_surface_damage_buffer(window->wl_surface, left, top, width, height);
//fprintf(stderr, "damage %dx%d %dx%d\n", left, top, width, height);
    }
  } else {
    wl_surface_damage_buffer(window->wl_surface, 0, 0, pWindow->w() * window->scale, pWindow->h() * window->scale);
//fprintf(stderr, "damage 0x0 %dx%d\n", pWindow->w() * window->scale, pWindow->h() * window->scale);
  }

  Fl_Wayland_Window_Driver::in_flush = true;
  Fl_Window_Driver::flush();
  Fl_Wayland_Window_Driver::in_flush = false;
  pWindow->clear_damage();
  if (window->buffer->wl_buffer_ready) {
    Fl_Wayland_Graphics_Driver::buffer_commit(window);
  }
}


void Fl_Wayland_Window_Driver::show() {
  if (!shown()) {
    fl_open_display();
    makeWindow();
  } else {
    //XMapRaised(fl_display, fl_xid(pWindow));
    //TODO
    Fl::handle(FL_SHOW, pWindow);
  }
}


void Fl_Wayland_Window_Driver::show_menu()
{
    pWindow->Fl_Window::show();
}


static void popup_done(void *data, struct xdg_popup *xdg_popup);

void Fl_Wayland_Window_Driver::hide() {
  Fl_X* ip = Fl_X::i(pWindow);
  if (hide_common()) return;
  if (ip->region) Fl_Graphics_Driver::default_driver().XDestroyRegion(ip->region);
#if USE_XFT
  screen_num_ = -1;
#endif
  struct wld_window *wld_win = ip->xid;
  if (wld_win) { // this test makes sure ip->xid has not been destroyed already
    Fl_Wayland_Graphics_Driver::buffer_release(wld_win);
//fprintf(stderr, "Before hide: sub=%p gl=%p frame=%p xdg=%p top=%p pop=%p surf=%p\n", wld_win->subsurface, wld_win->gl_wl_surface, wld_win->frame, wld_win->xdg_surface, wld_win->xdg_toplevel, wld_win->xdg_popup, wld_win->wl_surface);
    if (wld_win->subsurface) {
      wl_subsurface_destroy(wld_win->subsurface);
      wld_win->subsurface = NULL;
    }
    if (wld_win->gl_wl_surface) {
      wl_surface_destroy(wld_win->gl_wl_surface);
      wld_win->gl_wl_surface = NULL;
    }
    if (wld_win->frame) {
      libdecor_frame_unref(wld_win->frame);
      wld_win->frame = NULL;
      wld_win->xdg_surface = NULL;
      wld_win->xdg_toplevel = NULL;
    } else {
      if (wld_win->xdg_popup) {
        popup_done(wld_win, wld_win->xdg_popup);
        wld_win->xdg_popup = NULL;
      }
      if (wld_win->xdg_toplevel) {
        xdg_toplevel_destroy(wld_win->xdg_toplevel);
        wld_win->xdg_toplevel = NULL;
      }
      if (wld_win->xdg_surface) {
        xdg_surface_destroy(wld_win->xdg_surface);
        wld_win->xdg_surface = NULL;
      }
    }
    if (wld_win->wl_surface) {
      wl_surface_destroy(wld_win->wl_surface);
      wld_win->wl_surface = NULL;
    }
//fprintf(stderr, "After hide: sub=%p gl=%p frame=%p xdg=%p top=%p pop=%p surf=%p\n", wld_win->subsurface, wld_win->gl_wl_surface, wld_win->frame, wld_win->xdg_surface, wld_win->xdg_toplevel, wld_win->xdg_popup, wld_win->wl_surface);
  }
  delete ip;
}


void Fl_Wayland_Window_Driver::map() {
  Fl_X* ip = Fl_X::i(pWindow);
  struct wld_window *wl_win = ip->xid;
  if (wl_win->frame) libdecor_frame_map(wl_win->frame);//needs checking
  else if (pWindow->parent() && !wl_win->subsurface) {
    struct wld_window *parent = fl_xid(pWindow->window());
    if (parent) {
      Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
      wl_win->subsurface = wl_subcompositor_get_subsurface(scr_driver->wl_subcompositor, wl_win->wl_surface, parent->wl_surface);
      wl_subsurface_set_position(wl_win->subsurface, pWindow->x(), pWindow->y());
      wl_subsurface_set_desync(wl_win->subsurface); // important
      wl_subsurface_place_above(wl_win->subsurface, parent->wl_surface);
      wl_win->configured_width = pWindow->w();
      wl_win->configured_height = pWindow->h();
      wl_win->scale = parent->scale;
      wait_for_expose_value = 0;
    }
  }
}


void Fl_Wayland_Window_Driver::unmap() {
  Fl_X* ip = Fl_X::i(pWindow);
  struct wld_window *wl_win = ip->xid;
  if (wl_win->frame) libdecor_frame_close(wl_win->frame);//needs checking
  else if(wl_win->subsurface) {
    wl_surface_attach(wl_win->wl_surface, NULL, 0, 0);
    Fl_Wayland_Graphics_Driver::buffer_release(wl_win);
    wl_subsurface_destroy(wl_win->subsurface);
    wl_win->subsurface = NULL;
  }
}


void Fl_Wayland_Window_Driver::size_range() {
  Fl_Window_Driver::size_range();
  if (shown()) {
    Fl_X* ip = Fl_X::i(pWindow);
    struct wld_window *wl_win = ip->xid;
    if (wl_win->frame) {
      libdecor_frame_set_min_content_size(wl_win->frame, minw(), minh());
      if (maxw() && maxh()) {
        libdecor_frame_set_max_content_size(wl_win->frame, maxw(), maxh());
        libdecor_frame_unset_capabilities(wl_win->frame, LIBDECOR_ACTION_FULLSCREEN);
        if (minw() >= maxw() || minh() >= maxh()) {
          libdecor_frame_unset_capabilities(wl_win->frame, LIBDECOR_ACTION_RESIZE);
        }
      }
    } else if (wl_win->xdg_toplevel) {
      xdg_toplevel_set_min_size(wl_win->xdg_toplevel, minw(), minh());
      if (maxw() && maxh())
          xdg_toplevel_set_max_size(wl_win->xdg_toplevel, maxw(), maxh());
    }
  }
}


void Fl_Wayland_Window_Driver::iconize() {
  Fl_X* ip = Fl_X::i(pWindow);
  struct wld_window *wl_win = ip->xid;
  if (wl_win->frame) {
    libdecor_frame_set_minimized(wl_win->frame);
    Fl::handle(FL_HIDE, pWindow);
  }
  else if (wl_win->xdg_toplevel) xdg_toplevel_set_minimized(wl_win->xdg_toplevel);
}


void Fl_Wayland_Window_Driver::decoration_sizes(int *top, int *left,  int *right, int *bottom) {
  // Ensure border is on screen; these values are generic enough
  // to work with many window managers, and are based on KDE defaults.
  *top = 20;
  *left = 4;
  *right = 4;
  *bottom = 8;
}

void Fl_Wayland_Window_Driver::show_with_args_begin() {
  // Get defaults for drag-n-drop and focus...
  const char *key = 0;

  if (Fl::first_window()) key = Fl::first_window()->xclass();
  if (!key) key = "fltk";

  /*const char *val = XGetDefault(fl_display, key, "dndTextOps");
  if (val) Fl::dnd_text_ops(strcasecmp(val, "true") == 0 ||
                            strcasecmp(val, "on") == 0 ||
                            strcasecmp(val, "yes") == 0);

  val = XGetDefault(fl_display, key, "tooltips");
  if (val) Fl_Tooltip::enable(strcasecmp(val, "true") == 0 ||
                              strcasecmp(val, "on") == 0 ||
                              strcasecmp(val, "yes") == 0);

  val = XGetDefault(fl_display, key, "visibleFocus");
  if (val) Fl::visible_focus(strcasecmp(val, "true") == 0 ||
                             strcasecmp(val, "on") == 0 ||
                             strcasecmp(val, "yes") == 0);*/
}


void Fl_Wayland_Window_Driver::show_with_args_end(int argc, char **argv) {
  if (argc) {
    // set the command string, used by state-saving window managers:
    int j;
    int n=0; for (j=0; j<argc; j++) n += strlen(argv[j])+1;
    char *buffer = new char[n];
    char *p = buffer;
    for (j=0; j<argc; j++) for (const char *q = argv[j]; (*p++ = *q++););
    //XChangeProperty(fl_display, fl_xid(pWindow), XA_WM_COMMAND, XA_STRING, 8, 0,
    //                (unsigned char *)buffer, p-buffer-1);
    delete[] buffer;
  }
}


void Fl_Wayland_Window_Driver::flush_menu() {
   flush_Fl_Window();
}


int Fl_Wayland_Window_Driver::scroll(int src_x, int src_y, int src_w, int src_h, int dest_x, int dest_y,
                                 void (*draw_area)(void*, int,int,int,int), void* data)
{
  Window xid = fl_xid(pWindow);
  struct buffer *buffer = xid->buffer;
  int s = xid->scale;
  if (s != 1) {
    src_x *= s; src_y *= s; src_w *= s; src_h *= s; dest_x *= s; dest_y *= s;
  }
  if (src_x == dest_x) { // vertical scroll
    int i, to, step;
    if (src_y > dest_y) {
      i = 0; to = src_h; step = 1;
    } else {
      i = src_h - 1; to = -1; step = -1;
    }
    while (i != to) {
      memcpy(buffer->draw_buffer + (dest_y + i) * buffer->stride + 4 * dest_x,
             buffer->draw_buffer + (src_y + i) * buffer->stride + 4 * src_x, 4 * src_w);
      i += step;
    }
  } else { // horizontal scroll
    int i, to, step;
    if (src_x > dest_x) {
      i = 0; to = src_h; step = 1;
    } else {
      i = src_h - 1; to = -1; step = -1;
    }
    while (i != to) {
      memmove(buffer->draw_buffer + (src_y + i) * buffer->stride + 4 * dest_x,
             buffer->draw_buffer + (src_y + i) * buffer->stride + 4 * src_x, 4 * src_w);
      i += step;
    }
  }
  return 0;
}


static void handle_error(struct libdecor *libdecor_context, enum libdecor_error error, const char *message)
{
  fprintf(stderr, "Caught error (%d): %s\n", error, message);
  exit(EXIT_FAILURE);
}

static struct libdecor_interface libdecor_iface = {
  .error = handle_error,
};

static void surface_enter(void *data, struct wl_surface *wl_surface, struct wl_output *wl_output)
{
  struct wld_window *window = (struct wld_window*)data;
  Fl_Wayland_Window_Driver::window_output *window_output;

  if (!Fl_Wayland_Screen_Driver::own_output(wl_output))
    return;

  Fl_Wayland_Screen_Driver::output *output = (Fl_Wayland_Screen_Driver::output*)wl_output_get_user_data(wl_output);
  if (output == NULL)
    return;

  window_output = (Fl_Wayland_Window_Driver::window_output*)calloc(1, sizeof *window_output);
  window_output->output = output;
  wl_list_insert(&window->outputs, &window_output->link);
  Fl_Wayland_Window_Driver *win_driver = (Fl_Wayland_Window_Driver*)Fl_Window_Driver::driver(window->fl_win);
  win_driver->update_scale();
}

static void surface_leave(void *data, struct wl_surface *wl_surface, struct wl_output *wl_output)
{
  struct wld_window *window = (struct wld_window*)data;
  if (! window->wl_surface) return;
  Fl_Wayland_Window_Driver::window_output *window_output;

  wl_list_for_each(window_output, &window->outputs, link) {
    if (window_output->output->wl_output == wl_output) {
      wl_list_remove(&window_output->link);
      free(window_output);
      Fl_Wayland_Window_Driver *win_driver = (Fl_Wayland_Window_Driver*)Fl_Window_Driver::driver(window->fl_win);
      win_driver->update_scale();
      break;
    }
  }
}

static struct wl_surface_listener surface_listener = {
  surface_enter,
  surface_leave,
};

bool Fl_Wayland_Window_Driver::in_handle_configure = false;
bool not_using_weston = false; //TODO: try to get rid of that

static void handle_configure(struct libdecor_frame *frame,
     struct libdecor_configuration *configuration, void *user_data)
{
  struct wld_window *window = (struct wld_window*)user_data;
  if (!window->wl_surface) return;
  int width, height;
  enum libdecor_window_state window_state;
  struct libdecor_state *state;
  Fl_Window_Driver *driver = Fl_Window_Driver::driver(window->fl_win);

  if (!window->xdg_toplevel) window->xdg_toplevel = libdecor_frame_get_xdg_toplevel(frame);
  if (!window->xdg_surface) window->xdg_surface = libdecor_frame_get_xdg_surface(frame);
  if (!libdecor_configuration_get_content_size(configuration, frame, &width, &height)) {
    width = 0;
    height = 0;
    // With Weston, this doesn't allow to distinguish the 1st from the 2nd run of handle_configure
    if (!window->fl_win->parent() && window->fl_win->as_gl_window())
      driver->wait_for_expose_value = 0;
  } else {
    not_using_weston = true;
    if (driver->size_range_set()) {
      if (width < driver->minw() || height < driver->minh()) return;
    }
  }

  int tmp;
  if (libdecor_configuration_get_window_size(configuration, &tmp, &window->decorated_height) ) {
    driver->wait_for_expose_value = 0;
//    fprintf(stderr, "decorated size=%dx%d ", tmp, window->decorated_height);
  }
  if (width == 0) {
    width = window->fl_win->w();
    height = window->fl_win->h();
    driver->wait_for_expose_value = 0;// necessary for Weston
  }
  if (width < 128) width = 128; // enforce minimal size of decorated windows for libdecor
  if (height < 56) height = 56;
  Fl_Wayland_Window_Driver::in_handle_configure = true;
  window->fl_win->resize(0, 0, width, height);
  Fl_Wayland_Window_Driver::in_handle_configure = false;
  
  if (width != window->configured_width || height != window->configured_height) {
    if (window->buffer) {
      Fl_Wayland_Graphics_Driver::buffer_release(window);
    }
  }
  window->configured_width = width;
  window->configured_height = height;

  if (!libdecor_configuration_get_window_state(configuration, &window_state))
    window_state = LIBDECOR_WINDOW_STATE_NONE;

//fprintf(stderr, "handle_configure fl_win=%p pos:%dx%d size:%dx%d state=%x wait_for_expose_value=%d not_using_weston=%d\n", window->fl_win, window->fl_win->x(), window->fl_win->y(), width,height,window_state,driver->wait_for_expose_value,not_using_weston);

/* We would like to do FL_HIDE when window is minimized but :
 "There is no way to know if the surface is currently minimized, nor is there any way to
 unset minimization on this surface. If you are looking to throttle redrawing when minimized,
 please instead use the wl_surface.frame event" */
  if (window_state == LIBDECOR_WINDOW_STATE_NONE) Fl::handle(FL_UNFOCUS, window->fl_win);
  else if (window_state == LIBDECOR_WINDOW_STATE_ACTIVE) Fl::handle(FL_FOCUS, window->fl_win);

  state = libdecor_state_new(width, height);
  libdecor_frame_commit(frame, state, configuration);
  libdecor_state_free(state);
  window->fl_win->redraw();
  
  if (window->buffer) window->buffer->wl_buffer_ready = true; // dirty hack necessary for Weston
  if (!window->fl_win->as_gl_window()) {
    driver->flush();
  } else if (window->fl_win->parent()) {
    driver->Fl_Window_Driver::flush(); // GL subwindow
  } else {
    Fl_Wayland_Window_Driver::in_handle_configure = true;
    driver->Fl_Window_Driver::flush(); // top-level GL window
    Fl_Wayland_Window_Driver::in_handle_configure = false;
  }
}


static void delayed_close(Fl_Window *win) {
  Fl::handle(FL_CLOSE, win);
}

static void handle_close(struct libdecor_frame *frame, void *user_data)
{
  struct wld_window* wl_win = (struct wld_window*)user_data;
  // This may be called while in Fl::flush() when GL windows are involved.
  // Thus, have the FL_CLOSE event sent by a timeout to leave Fl::flush().
  Fl::add_timeout(0.001, (Fl_Timeout_Handler)delayed_close, wl_win->fl_win);
}


static void handle_commit(struct libdecor_frame *frame, void *user_data)
{
  struct wld_window* wl_win = (struct wld_window*)user_data;
  if (wl_win->wl_surface) wl_surface_commit(wl_win->wl_surface);
}

static void handle_dismiss_popup(struct libdecor_frame *frame, const char *seat_name, void *user_data)
{
}

static struct libdecor_frame_interface libdecor_frame_iface = {
  handle_configure,
  handle_close,
  handle_commit,
  handle_dismiss_popup,
};


static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
  // runs for borderless windows and popup (menu,tooltip) windows
  struct wld_window *window = (struct wld_window*)data;
  xdg_surface_ack_configure(xdg_surface, serial);
//fprintf(stderr, "xdg_surface_configure: surface=%p\n", window->wl_surface);
  Fl_Window_Driver::driver(window->fl_win)->wait_for_expose_value = 0;
  
  if (window->fl_win->w() != window->configured_width || window->fl_win->h() != window->configured_height) {
    if (window->buffer) {
      Fl_Wayland_Graphics_Driver::buffer_release(window);
    }
  }
  window->configured_width = window->fl_win->w();
  window->configured_height = window->fl_win->h();
  window->fl_win->redraw();
  Fl_Window_Driver::driver(window->fl_win)->flush();
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};


static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                                   int32_t width, int32_t height, struct wl_array *states)
{
  // runs for borderless top-level windows
  struct wld_window *window = (struct wld_window*)data;
//fprintf(stderr, "xdg_toplevel_configure: surface=%p size: %dx%d\n", window->wl_surface, width, height);
  if (width == 0 || height == 0) {
    width = window->fl_win->w();
    height = window->fl_win->h();
  }
  window->fl_win->size(width, height);
  if (window->buffer && (width != window->configured_width || height != window->configured_height)) {
    Fl_Wayland_Graphics_Driver::buffer_release(window);
  }
  window->configured_width = width;
  window->configured_height = height;
  Fl_Window_Driver::driver(window->fl_win)->wait_for_expose_value = 0;
  /*if (window->fl_win->as_gl_window()) {
    Fl_Window_Driver::driver(window->fl_win)->Fl_Window_Driver::flush();
  } else {
    Fl_Window_Driver::driver(window->fl_win)->flush();
  }*/
}


static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
  .configure = xdg_toplevel_configure,
  .close = xdg_toplevel_close,
};


static void popup_configure(void *data, struct xdg_popup *xdg_popup, int32_t x, int32_t y, int32_t width, int32_t height) {
}

static void popup_done(void *data, struct xdg_popup *xdg_popup) {
   //input_ungrab(menu->input); //to be tested
//fprintf(stderr, "popup_done: popup=%p \n", xdg_popup);
  xdg_popup_destroy(xdg_popup);
}

static const struct xdg_popup_listener popup_listener = {
  .configure = popup_configure,
  .popup_done = popup_done,
};

bool Fl_Wayland_Window_Driver::in_flush = false;


Fl_X *Fl_Wayland_Window_Driver::makeWindow()
{
  struct wld_window *new_window;
  Fl_Wayland_Screen_Driver::output *output;
  wait_for_expose_value = 1;
  
  if (pWindow->parent() && !pWindow->window()->shown()) return NULL;

  new_window = (struct wld_window *)calloc(1, sizeof *new_window);
  new_window->fl_win = pWindow;
  new_window->scale = 1;
  // for Weston, pre-estimate decorated_height
  new_window->decorated_height = pWindow->h();
  if (!pWindow->parent()) new_window->decorated_height += 24; // can be changed later
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  wl_list_for_each(output, &(scr_driver->outputs), link) {
    new_window->scale = MAX(new_window->scale, output->scale);
  }
  wl_list_init(&new_window->outputs);

  new_window->wl_surface = wl_compositor_create_surface(scr_driver->wl_compositor);
fprintf(stderr, "makeWindow:%p wl_compositor_create_surface=%p scale=%d\n", pWindow, new_window->wl_surface, new_window->scale);
  wl_surface_add_listener(new_window->wl_surface, &surface_listener, new_window);
  
  if (pWindow->menu_window() || pWindow->tooltip_window()) { // a menu window or tooltip
    new_window->xdg_surface = xdg_wm_base_get_xdg_surface(scr_driver->xdg_wm_base, new_window->wl_surface);
    xdg_surface_add_listener(new_window->xdg_surface, &xdg_surface_listener, new_window);
    struct xdg_positioner *positioner = xdg_wm_base_create_positioner(scr_driver->xdg_wm_base);
    //xdg_positioner_get_version(positioner) <== gives 1 under Debian
    Fl_Widget *target = pWindow->tooltip_window() ? Fl_Tooltip::current() : Fl::pushed();
    if (!target) {
      target = Fl::belowmouse()->top_window();
    }
    Fl_Window *parent_win = target->top_window();
    struct xdg_surface *parent = fl_xid(parent_win)->xdg_surface;
    int y_offset = parent_win->decorated_h() - parent_win->h();
//fprintf(stderr, "menu parent_win=%p pos:%dx%d size:%dx%d y_offset=%d\n", parent_win, pWindow->x(), pWindow->y(), pWindow->w(), pWindow->h(), y_offset);
    xdg_positioner_set_anchor_rect(positioner, pWindow->x(), pWindow->y() + y_offset, 1, 1);
    xdg_positioner_set_size(positioner, pWindow->w() , pWindow->h() );
    xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP_LEFT);
    xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    new_window->xdg_popup = xdg_surface_get_popup(new_window->xdg_surface, parent, positioner);
    xdg_positioner_destroy(positioner);
    xdg_popup_add_listener(new_window->xdg_popup, &popup_listener, new_window);
//xdg_popup_grab(new_window->xdg_popup, scr_driver->get_wl_seat(), scr_driver->get_serial());//useful?
    wl_surface_commit(new_window->wl_surface);
//libdecor_frame_popup_grab(fl_xid(parent_win)->frame, scr_driver->get_seat_name());//useful?

  } else if ( pWindow->border() && !pWindow->parent() ) { // a decorated window
    if (!scr_driver->libdecor_context) scr_driver->libdecor_context = libdecor_new(fl_display, &libdecor_iface);
    new_window->frame = libdecor_decorate(scr_driver->libdecor_context, new_window->wl_surface,
                                              &libdecor_frame_iface, new_window);
//fprintf(stderr, "makeWindow: libdecor_decorate=%p pos:%dx%d\n", new_window->frame, pWindow->x(), pWindow->y());
    libdecor_frame_set_app_id(new_window->frame, ((Fl_Wayland_System_Driver*)Fl::system_driver())->get_prog_name()); // appears in the Gnome desktop menu bar
    libdecor_frame_set_title(new_window->frame, pWindow->label()?pWindow->label():"");
    if (!pWindow->resizable()) {
      libdecor_frame_unset_capabilities(new_window->frame, LIBDECOR_ACTION_RESIZE);
      libdecor_frame_unset_capabilities(new_window->frame, LIBDECOR_ACTION_FULLSCREEN);
    }
    libdecor_frame_set_min_content_size(new_window->frame, 128, 56);// libdecor wants width ≥ 128 & height ≥ 56
    libdecor_frame_map(new_window->frame);
    if (pWindow->as_gl_window()) { // a top-level GL window: create a subsurface for the GL part
      new_window->gl_wl_surface = wl_compositor_create_surface(scr_driver->wl_compositor);
      new_window->subsurface = wl_subcompositor_get_subsurface(scr_driver->wl_subcompositor, new_window->gl_wl_surface, new_window->wl_surface);
      wl_subsurface_set_position(new_window->subsurface, 0, 0);
      wl_subsurface_set_desync(new_window->subsurface);
      wl_subsurface_place_above(new_window->subsurface, new_window->wl_surface);
    }

  } else if (pWindow->parent()) { // for subwindows (GL or non-GL)
    struct wld_window *parent = fl_xid(pWindow->window());
    new_window->subsurface = wl_subcompositor_get_subsurface(scr_driver->wl_subcompositor, new_window->wl_surface, parent->wl_surface);
fprintf(stderr, "makeWindow: subsurface=%p\n", new_window->subsurface);
    wl_subsurface_set_position(new_window->subsurface, pWindow->x(), pWindow->y());
    wl_subsurface_set_desync(new_window->subsurface); // important
    wl_subsurface_place_above(new_window->subsurface, parent->wl_surface);
    // next 3 statements ensure the subsurface will be mapped because:
    // "A sub-surface becomes mapped, when a non-NULL wl_buffer is applied and the parent surface is mapped."
    new_window->configured_width = pWindow->w();
    new_window->configured_height = pWindow->h();
    wait_for_expose_value = 0;
    pWindow->border(0);

  } else { // a window without decoration
    new_window->xdg_surface = xdg_wm_base_get_xdg_surface(scr_driver->xdg_wm_base, new_window->wl_surface);
fprintf(stderr, "makeWindow: xdg_wm_base_get_xdg_surface=%p\n", new_window->xdg_surface);
    xdg_surface_add_listener(new_window->xdg_surface, &xdg_surface_listener, new_window);
    new_window->xdg_toplevel = xdg_surface_get_toplevel(new_window->xdg_surface);
    xdg_toplevel_add_listener(new_window->xdg_toplevel, &xdg_toplevel_listener, new_window);
    if (pWindow->label()) xdg_toplevel_set_title(new_window->xdg_toplevel, pWindow->label());
    wl_surface_commit(new_window->wl_surface);
    pWindow->border(0);
  }
    
  Fl_Window *old_first = Fl::first_window();
  Window first_xid = (old_first ? fl_xid(old_first) : NULL);
  Fl_X *xp = new Fl_X;
  xp->xid = new_window;
  other_xid = 0;
  xp->w = pWindow;
  i(xp);
  xp->region = 0;
  if (!pWindow->parent()) {
    xp->next = Fl_X::first;
    Fl_X::first = xp;
  } else if (Fl_X::first) {
    xp->next = Fl_X::first->next;
    Fl_X::first->next = xp;
  } else {
    xp->next = NULL;
    Fl_X::first = xp;
  }
  
  if (pWindow->modal()) {
    Fl::modal_ = pWindow; /*fl_fix_focus();*/
    if (new_window->frame && first_xid && first_xid->frame) {
     libdecor_frame_set_parent(new_window->frame, first_xid->frame);
    } else if (new_window->xdg_toplevel && first_xid && first_xid->xdg_toplevel) {
      xdg_toplevel_set_parent(new_window->xdg_toplevel, first_xid->xdg_toplevel);
    }
  }
  
  if (size_range_set()) size_range();
  pWindow->set_visible();
  int old_event = Fl::e_number;
  pWindow->handle(Fl::e_number = FL_SHOW); // get child windows to appear
  Fl::e_number = old_event;
  pWindow->redraw();
  
  return xp;
}

Fl_Wayland_Window_Driver::type_for_resize_window_between_screens Fl_Wayland_Window_Driver::data_for_resize_window_between_screens_ = {0, false};

void Fl_Wayland_Window_Driver::resize_after_screen_change(void *data) {
  Fl_Window *win = (Fl_Window*)data;
  float f = Fl::screen_driver()->scale(data_for_resize_window_between_screens_.screen);
  Fl_Window_Driver::driver(win)->resize_after_scale_change(data_for_resize_window_between_screens_.screen, f, f);
  data_for_resize_window_between_screens_.busy = false;
}


#if USE_XFT
int Fl_Wayland_Window_Driver::screen_num() {
  if (pWindow->parent()) {
    screen_num_ = Fl_Window_Driver::driver(pWindow->top_window())->screen_num();
  }
  return screen_num_ >= 0 ? screen_num_ : 0;
}
#endif


int Fl_Wayland_Window_Driver::set_cursor(Fl_Cursor c) {
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();

  //cursor names seem to be the files of directory /usr/share/icons/Adwaita/cursors/
  switch (c) {
    case FL_CURSOR_ARROW:
      if (!scr_driver->xc_arrow) scr_driver->xc_arrow = scr_driver->cache_cursor("left_ptr");
      scr_driver->default_cursor(scr_driver->xc_arrow);
      break;
    case FL_CURSOR_NS:
      if (!scr_driver->xc_ns) scr_driver->xc_ns = scr_driver->cache_cursor("ns-resize");
      if (!scr_driver->xc_ns) return 0;
      scr_driver->default_cursor(scr_driver->xc_ns);
      break;
    case FL_CURSOR_CROSS:
      if (!scr_driver->xc_cross) scr_driver->xc_cross = scr_driver->cache_cursor("cross");
      if (!scr_driver->xc_cross) return 0;
      scr_driver->default_cursor(scr_driver->xc_cross);
      break;
    case FL_CURSOR_WAIT:
      if (!scr_driver->xc_wait) scr_driver->xc_wait = scr_driver->cache_cursor("wait");
      if (!scr_driver->xc_wait) scr_driver->xc_wait = scr_driver->cache_cursor("watch");
      if (!scr_driver->xc_wait) return 0;
      scr_driver->default_cursor(scr_driver->xc_wait);
      break;
    case FL_CURSOR_INSERT:
      if (!scr_driver->xc_insert) scr_driver->xc_insert = scr_driver->cache_cursor("xterm");
      if (!scr_driver->xc_insert) return 0;
      scr_driver->default_cursor(scr_driver->xc_insert);
      break;
    case FL_CURSOR_HAND:
      if (!scr_driver->xc_hand) scr_driver->xc_hand = scr_driver->cache_cursor("hand");
      if (!scr_driver->xc_hand) scr_driver->xc_hand = scr_driver->cache_cursor("hand1");
      if (!scr_driver->xc_hand) return 0;
      scr_driver->default_cursor(scr_driver->xc_hand);
      break;
    case FL_CURSOR_HELP:
      if (!scr_driver->xc_help) scr_driver->xc_help = scr_driver->cache_cursor("help");
      if (!scr_driver->xc_help) return 0;
      scr_driver->default_cursor(scr_driver->xc_help);
      break;
    case FL_CURSOR_MOVE:
      if (!scr_driver->xc_move) scr_driver->xc_move = scr_driver->cache_cursor("move");
      if (!scr_driver->xc_move) return 0;
      scr_driver->default_cursor(scr_driver->xc_move);
      break;
    case FL_CURSOR_WE:
      if (!scr_driver->xc_we) scr_driver->xc_we = scr_driver->cache_cursor("sb_h_double_arrow");
      if (!scr_driver->xc_we) return 0;
      scr_driver->default_cursor(scr_driver->xc_we);
      break;
    case FL_CURSOR_N:
      if (!scr_driver->xc_north) scr_driver->xc_north = scr_driver->cache_cursor("top_side");
      if (!scr_driver->xc_north) return 0;
      scr_driver->default_cursor(scr_driver->xc_north);
      break;
    case FL_CURSOR_E:
      if (!scr_driver->xc_east) scr_driver->xc_east = scr_driver->cache_cursor("right_side");
      if (!scr_driver->xc_east) return 0;
      scr_driver->default_cursor(scr_driver->xc_east);
      break;
    case FL_CURSOR_W:
      if (!scr_driver->xc_west) scr_driver->xc_west = scr_driver->cache_cursor("left_side");
      if (!scr_driver->xc_west) return 0;
      scr_driver->default_cursor(scr_driver->xc_west);
      break;
    case FL_CURSOR_S:
      if (!scr_driver->xc_south) scr_driver->xc_south = scr_driver->cache_cursor("bottom_side");
      if (!scr_driver->xc_south) return 0;
      scr_driver->default_cursor(scr_driver->xc_south);
      break;
    case FL_CURSOR_NESW:
      if (!scr_driver->xc_nesw) scr_driver->xc_nesw = scr_driver->cache_cursor("fd_double_arrow");
      if (!scr_driver->xc_nesw) return 0;
      scr_driver->default_cursor(scr_driver->xc_nesw);
      break;
    case FL_CURSOR_NWSE:
      if (!scr_driver->xc_nwse) scr_driver->xc_nwse = scr_driver->cache_cursor("bd_double_arrow");
      if (!scr_driver->xc_nwse) return 0;
      scr_driver->default_cursor(scr_driver->xc_nwse);
      break;
    case FL_CURSOR_SW:
      if (!scr_driver->xc_sw) scr_driver->xc_sw = scr_driver->cache_cursor("bottom_left_corner");
      if (!scr_driver->xc_sw) return 0;
      scr_driver->default_cursor(scr_driver->xc_sw);
      break;
    case FL_CURSOR_SE:
      if (!scr_driver->xc_se) scr_driver->xc_se = scr_driver->cache_cursor("bottom_right_corner");
      if (!scr_driver->xc_se) return 0;
      scr_driver->default_cursor(scr_driver->xc_se);
      break;
    case FL_CURSOR_NE:
      if (!scr_driver->xc_ne) scr_driver->xc_ne = scr_driver->cache_cursor("top_right_corner");
      if (!scr_driver->xc_ne) return 0;
      scr_driver->default_cursor(scr_driver->xc_ne);
      break;
    case FL_CURSOR_NW:
      if (!scr_driver->xc_nw) scr_driver->xc_nw = scr_driver->cache_cursor("top_left_corner");
      if (!scr_driver->xc_nw) return 0;
      scr_driver->default_cursor(scr_driver->xc_nw);
      break;

    default:
      return 0;
  }
  scr_driver->set_cursor();
  return 1;
}


void Fl_Wayland_Window_Driver::update_scale()
{
  struct wld_window *window = fl_xid(pWindow);
  int scale = 1;
  Fl_Wayland_Window_Driver::window_output *window_output;

  wl_list_for_each(window_output, &window->outputs, link) {
    scale = fl_max(scale, window_output->output->scale);
  }
  if (scale != window->scale) {
    window->scale = scale;
    if (window->buffer || window->fl_win->as_gl_window()) {
      window->fl_win->damage(FL_DAMAGE_ALL);
      Fl_Window_Driver::driver(window->fl_win)->flush();
    }
  }
}
