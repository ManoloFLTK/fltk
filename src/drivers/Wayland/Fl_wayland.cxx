//
// Wayland specific code for the Fast Light Tool Kit (FLTK).
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

#if !defined(FL_DOXYGEN)

#  include <config.h>
#  include <FL/Fl.H>
#  include <FL/platform.H>
#  include <FL/Fl_Window.H>
#  include <FL/Fl_Shared_Image.H>
#  include <FL/Fl_Image_Surface.H>
#  include <stdio.h>
#  include <stdlib.h>
#  include "../../flstring.h"
#  include "Fl_Wayland_Screen_Driver.H"
#  include "Fl_Wayland_Window_Driver.H"
#  include "Fl_Wayland_System_Driver.H"
#  include "Fl_Wayland_Graphics_Driver.H"
#  include <errno.h>


////////////////////////////////////////////////////////////////

Window fl_message_window = 0;
int fl_screen;
Window fl_xim_win = 0;
char fl_is_over_the_spot = 0;


int Fl_Wayland_Screen_Driver::get_mouse_unscaled(int &mx, int &my) {
  open_display();
  mx = Fl::e_x_root; my = Fl::e_y_root;
  int screen = screen_num_unscaled(mx, my);
  return screen >= 0 ? screen : 0;
}


int Fl_Wayland_Screen_Driver::get_mouse(int &xx, int &yy) {
  int snum = get_mouse_unscaled(xx, yy);
  float s = scale(snum);
  xx = xx/s;
  yy = yy/s;
  return snum;
}

////////////////////////////////////////////////////////////////
// Code used for copy and paste and DnD into the program:
//static Window fl_dnd_source_window;

static char *fl_selection_buffer[2];
static int fl_selection_length[2];
static const char * fl_selection_type[2];
static int fl_selection_buffer_length[2];
static char fl_i_own_selection[2] = {0,0};
static struct wl_data_offer *fl_selection_offer = NULL;
static const char *fl_selection_offer_type = NULL;
// The MIME type Wayland uses for text-containing clipboard:
static const char wld_plain_text_clipboard[] = "text/plain;charset=utf-8";


int Fl_Wayland_System_Driver::clipboard_contains(const char *type)
{
  return fl_selection_type[1] == type;
}


struct data_source_write_struct {
  size_t rest;
  char *from;
};

void write_data_source_cb(FL_SOCKET fd, data_source_write_struct *data) {
  while (data->rest) {
    ssize_t n = write(fd, data->from, data->rest);
    if (n == -1) {
      if (errno == EAGAIN) return;
      Fl::error("write_data_source_cb: error while writing clipboard data\n");
      break;
    }
    data->from += n;
    data->rest -= n;
  }
  Fl::remove_fd(fd, FL_WRITE);
  delete data;
  close(fd);
}

static void data_source_handle_send(void *data, struct wl_data_source *source, const char *mime_type, int fd) {
  fl_intptr_t rank = (fl_intptr_t)data;
//fprintf(stderr, "data_source_handle_send: %s fd=%d l=%d\n", mime_type, fd, fl_selection_length[1]);
  if (strcmp(mime_type, wld_plain_text_clipboard) == 0 || strcmp(mime_type, "text/plain") == 0 || strcmp(mime_type, "image/bmp") == 0) {
    data_source_write_struct *write_data = new data_source_write_struct;
    write_data->rest = fl_selection_length[rank];
    write_data->from = fl_selection_buffer[rank];
    Fl::add_fd(fd, FL_WRITE, (Fl_FD_Handler)write_data_source_cb, write_data);
  } else {
    Fl::error("Destination client requested unsupported MIME type: %s\n", mime_type);
    close(fd);
  }
}

static Fl_Window *fl_dnd_target_window = 0;
static bool doing_dnd = false; // true when DnD is in action
static wl_surface *dnd_icon = NULL; // non null when DnD uses text as cursor
static wl_cursor* save_cursor = NULL; // non null when DnD uses "dnd-copy" cursor

static void data_source_handle_cancelled(void *data, struct wl_data_source *source) {
  // An application has replaced the clipboard contents or DnD finished
//fprintf(stderr, "data_source_handle_cancelled: %p\n", source);
  wl_data_source_destroy(source);
  doing_dnd = false;
  if (dnd_icon) {
    Fl_Offscreen off = (Fl_Offscreen)wl_surface_get_user_data(dnd_icon);
    struct wld_window fake_window;
    fake_window.buffer = off;
    Fl_Wayland_Graphics_Driver::buffer_release(&fake_window);
    wl_surface_destroy(dnd_icon);
    dnd_icon = NULL;
  }
  fl_i_own_selection[1] = 0;
  if (data == 0) { // at end of DnD
    if (save_cursor) {
      Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
      scr_driver->default_cursor(save_cursor);
      scr_driver->set_cursor();
      save_cursor = NULL;
    }
    if (fl_dnd_target_window) {
      Fl::handle(FL_DND_LEAVE, fl_dnd_target_window);
      fl_dnd_target_window = 0;
    }
    Fl::pushed(0);
  }
}


static void data_source_handle_target(void *data, struct wl_data_source *source, const char *mime_type) {
  if (mime_type != NULL) {
    //printf("Destination would accept MIME type if dropped: %s\n", mime_type);
  } else {
    //printf("Destination would reject if dropped\n");
  }
}

static uint32_t last_dnd_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

static void data_source_handle_action(void *data, struct wl_data_source *source, uint32_t dnd_action) {
  last_dnd_action = dnd_action;
  switch (dnd_action) {
  case WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY:
    //printf("Destination would perform a copy action if dropped\n");
    break;
  case WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE:
    //printf("Destination would reject the drag if dropped\n");
    break;
  }
}
  
static void data_source_handle_dnd_drop_performed(void *data, struct wl_data_source *source) {
  //printf("Drop performed\n");
}

static void data_source_handle_dnd_finished(void *data, struct wl_data_source *source) {
  switch (last_dnd_action) {
  case WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE:
    //printf("Destination has accepted the drop with a move action\n");
    break;
  case WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY:
    //printf("Destination has accepted the drop with a copy action\n");
    break;
  }
}

static const struct wl_data_source_listener data_source_listener = {
  .target = data_source_handle_target,
  .send = data_source_handle_send,
  .cancelled = data_source_handle_cancelled,
  .dnd_drop_performed = data_source_handle_dnd_drop_performed,
  .dnd_finished = data_source_handle_dnd_finished,
  .action = data_source_handle_action,
};


static Fl_Offscreen offscreen_from_text(const char *text, int scale) {
  const char *p, *q;
  int width = 0, height, w2, ltext = strlen(text);
  fl_font(FL_HELVETICA, 10 * scale);
  p = text;
  int nl = 0;
  while(nl < 20 && (q=strchr(p, '\n')) != NULL) {
    nl++;
    w2 = int(fl_width(p, q - p));
    if (w2 > width) width = w2;
    p = q + 1;
  }
  if (nl < 20 && text[ ltext - 1] != '\n') {
    nl++;
    w2 = int(fl_width(p));
    if (w2 > width) width = w2;
  }
  if (width > 300*scale) width = 300*scale;
  height = nl * fl_height() + 3;
  width += 6;
  Fl_Offscreen off = Fl_Wayland_Graphics_Driver::create_shm_buffer(width, height);
  memset(off->draw_buffer, 0, off->data_size);
  Fl_Image_Surface *surf = new Fl_Image_Surface(width, height, 0, off);
  Fl_Surface_Device::push_current(surf);
  p = text;
  fl_font(FL_HELVETICA, 10 * scale);
  int y = fl_height();
  while (nl > 0) {
    q = strchr(p, '\n');
    if (q) {
      fl_draw(p, q - p, 3, y);
    } else {
      fl_draw(p, 3, y);
      break;
    }
    y += fl_height();
    p = q + 1;
    nl--;
  }
  Fl_Surface_Device::pop_current();
  delete surf;
  cairo_surface_flush( cairo_get_target(off->cairo_) );
  memcpy(off->data, off->draw_buffer, off->data_size);
  return off;
}


int Fl_Wayland_Screen_Driver::dnd(int use_selection) {
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();

  struct wl_data_source *source =
    wl_data_device_manager_create_data_source(scr_driver->seat->data_device_manager);
  // we transmit the adequate value of index in fl_selection_buffer[index]
  wl_data_source_add_listener(source, &data_source_listener, (void*)0);
  wl_data_source_offer(source, wld_plain_text_clipboard);
  wl_data_source_set_actions(source, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
  Fl_Offscreen off = NULL;
  int s = 1;
  if (use_selection) {
    // use the text as dragging icon
    Fl_Widget *current = Fl::pushed() ? Fl::pushed() : Fl::first_window();
    s = fl_xid(current->top_window())->scale;
    off = offscreen_from_text(fl_selection_buffer[0], s);
    dnd_icon = wl_compositor_create_surface(scr_driver->wl_compositor);
  } else dnd_icon = NULL;
  doing_dnd = true;
  wl_data_device_start_drag(scr_driver->seat->data_device, source,
                            scr_driver->seat->pointer_focus, dnd_icon, scr_driver->seat->serial);
  if (use_selection) {
    wl_surface_attach(dnd_icon, off->wl_buffer, 0, 0);
    wl_surface_set_buffer_scale(dnd_icon, s);
    wl_surface_damage(dnd_icon, 0, 0, 10000, 10000);
    wl_surface_commit(dnd_icon);
    wl_surface_set_user_data(dnd_icon, off);
  } else {
    static struct wl_cursor *dnd_cursor = scr_driver->cache_cursor("dnd-copy");
    if (dnd_cursor) {
      save_cursor = scr_driver->default_cursor();
      scr_driver->default_cursor(dnd_cursor);
      scr_driver->set_cursor();
    } else save_cursor = NULL;
  }
  return 1;
}


static void data_offer_handle_offer(void *data, struct wl_data_offer *offer, const char *mime_type) {
  // runs when app becomes active and lists possible clipboard types
//fprintf(stderr, "Clipboard offer=%p supports MIME type: %s\n", offer, mime_type);
  if (strcmp(mime_type, "image/png") == 0) {
    fl_selection_type[1] = Fl::clipboard_image;
    fl_selection_offer_type = "image/png";
  } else if (strcmp(mime_type, "image/bmp") == 0 && (!fl_selection_offer_type || strcmp(fl_selection_offer_type, "image/png"))) {
    fl_selection_type[1] = Fl::clipboard_image;
    fl_selection_offer_type = "image/bmp";
  } else if (strcmp(mime_type, wld_plain_text_clipboard) == 0 && !fl_selection_type[1]) {
    fl_selection_type[1] = Fl::clipboard_plain_text;
  }
}


static void data_offer_handle_source_actions(void *data, struct wl_data_offer *offer, uint32_t actions) {
  if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY) {
    //printf("Drag supports the copy action\n");
  }
}

static void data_offer_handle_action(void *data, struct wl_data_offer *offer, uint32_t dnd_action) {
  switch (dnd_action) {
  case WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE:
    //printf("A move action would be performed if dropped\n");
    break;
  case WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY:
    //printf("A copy action would be performed if dropped\n");
    break;
  case WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE:
    //printf("The drag would be rejected if dropped\n");
    break;
  }
}

static const struct wl_data_offer_listener data_offer_listener = {
  .offer = data_offer_handle_offer,
  .source_actions = data_offer_handle_source_actions,
  .action = data_offer_handle_action,
};

static void data_device_handle_data_offer(void *data, struct wl_data_device *data_device, struct wl_data_offer *offer) {
  // An application has created a new data source
//fprintf(stderr, "data_device_handle_data_offer offer=%p\n", offer);
  fl_selection_type[1] = NULL;
  fl_selection_offer_type = NULL;
  wl_data_offer_add_listener(offer, &data_offer_listener, NULL);
}


static void data_device_handle_selection(void *data, struct wl_data_device *data_device, struct wl_data_offer *offer) {
  // An application has set the clipboard contents. W
//fprintf(stderr, "data_device_handle_selection\n");
  if (fl_selection_offer) wl_data_offer_destroy(fl_selection_offer);
  fl_selection_offer = offer;
//if (offer == NULL) fprintf(stderr, "Clipboard is empty\n");
}


static size_t convert_crlf(char *s, size_t len)
{ // turn \r characters into \n and "\r\n" sequences into \n:
  char *p;
  size_t l = len;
  while ((p = strchr(s, '\r'))) {
    if (*(p+1) == '\n') {
      memmove(p, p+1, l-(p-s));
      len--; l--;
    } else *p = '\n';
    l -= p-s;
    s = p + 1;
  }
  return len;
}


// Gets from the system the clipboard or dnd text and puts it in fl_selection_buffer[1]
// which is enlarged if necessary.
static void get_clipboard_or_dragged_text(struct wl_data_offer *offer) {
  int fds[2];
  if (pipe(fds)) return;
  wl_data_offer_receive(offer, wld_plain_text_clipboard, fds[1]);
  close(fds[1]);
  wl_display_flush(fl_display);
  // read in fl_selection_buffer
  char *to = fl_selection_buffer[1];
  ssize_t rest = fl_selection_buffer_length[1];
  while (rest) {
    ssize_t n = read(fds[0], to, rest);
    if (n <= 0) {
      close(fds[0]);
      fl_selection_length[1] = to - fl_selection_buffer[1];
      fl_selection_buffer[1][ fl_selection_length[1] ] = 0;
      return;
    }
    n = convert_crlf(to, n);
    to += n;
    rest -= n;
  }
  // compute size of unread clipboard data
  rest = fl_selection_buffer_length[1];
  while (true) {
    char buf[1000];
    ssize_t n = read(fds[0], buf, sizeof(buf));
    if (n <= 0) {
      close(fds[0]);
      break;
    }
    rest += n;
  }
//fprintf(stderr, "get_clipboard_or_dragged_text: size=%ld\n", rest);
  // read full clipboard data
  if (pipe(fds)) return;
  wl_data_offer_receive(offer, wld_plain_text_clipboard, fds[1]);
  close(fds[1]);
  wl_display_flush(fl_display);
  if (rest+1 > fl_selection_buffer_length[1]) {
    delete[] fl_selection_buffer[1];
    fl_selection_buffer[1] = new char[rest+1000+1];
    fl_selection_buffer_length[1] = rest+1000;
  }
  char *from = fl_selection_buffer[1];
  while (true) {
    ssize_t n = read(fds[0], from, rest);
    if (n <= 0) {
      close(fds[0]);
      break;
    }
    n = convert_crlf(from, n);
    from += n;
  }
  fl_selection_length[1] = from - fl_selection_buffer[1];;
  fl_selection_buffer[1][fl_selection_length[1]] = 0;
  Fl::e_clipboard_type = Fl::clipboard_plain_text;
}

static struct wl_data_offer *current_drag_offer = NULL;
static uint32_t fl_dnd_serial;


static void data_device_handle_enter(void *data, struct wl_data_device *data_device, uint32_t serial,
    struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *offer) {
  Fl_Window *win = Fl_Wayland_Screen_Driver::surface_to_window(surface);
//printf("Drag entered our surface %p(win=%p) at %dx%d\n", surface, win, wl_fixed_to_int(x), wl_fixed_to_int(y));
  if (win) {
    float f = Fl::screen_scale(win->screen_num());
    fl_dnd_target_window = win;
    Fl::e_x = wl_fixed_to_int(x) / f;
    Fl::e_x_root = Fl::e_x + fl_dnd_target_window->x();
    Fl::e_y = wl_fixed_to_int(y) / f;
    Fl::e_y_root = Fl::e_y + fl_dnd_target_window->y();
    Fl::handle(FL_DND_ENTER, fl_dnd_target_window);
    current_drag_offer = offer;
    fl_dnd_serial = serial;
  }
  uint32_t supported_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  uint32_t preferred_action = supported_actions;
  wl_data_offer_set_actions(offer, supported_actions, preferred_action);
}

static void data_device_handle_motion(void *data, struct wl_data_device *data_device, uint32_t time,
    wl_fixed_t x, wl_fixed_t y) {
  if (!current_drag_offer) return;
//printf("data_device_handle_motion fl_dnd_target_window=%p\n", fl_dnd_target_window);
  int ret = 0;
  if (fl_dnd_target_window) {
    float f = Fl::screen_scale(fl_dnd_target_window->screen_num());
    Fl::e_x = wl_fixed_to_int(x) / f;
    Fl::e_x_root = Fl::e_x + fl_dnd_target_window->x();
    Fl::e_y = wl_fixed_to_int(y) / f;
    Fl::e_y_root = Fl::e_y + fl_dnd_target_window->y();
    ret = Fl::handle(FL_DND_DRAG, fl_dnd_target_window);
  }
  uint32_t supported_actions =  ret ? WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY : WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
  uint32_t preferred_action = supported_actions;
  wl_data_offer_set_actions(current_drag_offer, supported_actions, preferred_action);
  wl_display_roundtrip(fl_display);
  if (ret && current_drag_offer) wl_data_offer_accept(current_drag_offer, fl_dnd_serial, "text/plain");
}

static void data_device_handle_leave(void *data, struct wl_data_device *data_device) {
//printf("Drag left our surface\n");
}


static void data_device_handle_drop(void *data, struct wl_data_device *data_device) {
  if (!current_drag_offer) return;
  int ret = Fl::handle(FL_DND_RELEASE, fl_dnd_target_window);
//printf("data_device_handle_drop ret=%d doing_dnd=%d\n", ret, doing_dnd);

  if (!ret) {
    wl_data_offer_destroy(current_drag_offer);
    current_drag_offer = NULL;
    return;
  }

  if (doing_dnd) {
    Fl::e_text = fl_selection_buffer[0];
    Fl::e_length = fl_selection_length[0];
  } else {
    get_clipboard_or_dragged_text(current_drag_offer);
    Fl::e_text = fl_selection_buffer[1];
    Fl::e_length = fl_selection_length[1];
  }
  int old_event = Fl::e_number;
  Fl::belowmouse()->handle(Fl::e_number = FL_PASTE);
  Fl::e_number = old_event;

  wl_data_offer_finish(current_drag_offer);
  wl_data_offer_destroy(current_drag_offer);
  current_drag_offer = NULL;
}

static const struct wl_data_device_listener data_device_listener = {
  .data_offer = data_device_handle_data_offer,
  .enter = data_device_handle_enter,
  .leave = data_device_handle_leave,
  .motion = data_device_handle_motion,
  .drop = data_device_handle_drop,
  .selection = data_device_handle_selection,
};


const struct wl_data_device_listener *Fl_Wayland_Screen_Driver::p_data_device_listener = &data_device_listener;


static void read_int(uchar *c, int& i) {
  i = *c;
  i |= (*(++c))<<8;
  i |= (*(++c))<<16;
  i |= (*(++c))<<24;
}


// Reads from the clipboard an image which can be in image/bmp or image/png MIME type.
// Returns 0 if OK, != 0 if error.
static int get_clipboard_image() {
  int fds[2];
  if (pipe(fds)) return 1;
  wl_data_offer_receive(fl_selection_offer, fl_selection_offer_type, fds[1]);
  close(fds[1]);
  wl_display_roundtrip(fl_display);
  if (strcmp(fl_selection_offer_type, "image/png") == 0) {
    char tmp_fname[21];
    Fl_Shared_Image *shared = 0;
    strcpy(tmp_fname, "/tmp/clipboardXXXXXX");
    int fd = mkstemp(tmp_fname);
    if (fd == -1) return 1;
    while (true) {
      char buf[10000];
      ssize_t n = read(fds[0], buf, sizeof(buf));
      if (n <= 0) {
        close(fds[0]);
        close(fd);
        break;
      }
      n = write(fd, buf, n);
    }
    shared = Fl_Shared_Image::get(tmp_fname);
    fl_unlink(tmp_fname);
    if (!shared) return 1;
    int ld = shared->ld() ? shared->ld() : shared->w() * shared->d();
    uchar *rgb = new uchar[shared->w() * shared->h() * shared->d()];
    memcpy(rgb, shared->data()[0], ld * shared->h() );
    Fl_RGB_Image *image = new Fl_RGB_Image(rgb, shared->w(), shared->h(), shared->d(), shared->ld());
    shared->release();
    image->alloc_array = 1;
    Fl::e_clipboard_data = (void*)image;
  } else { // process image/bmp
    uchar buf[54];
    size_t rest = 1;
    char *bmp = NULL;
    ssize_t n = read(fds[0], buf, sizeof(buf)); // read size info of the BMP image
    if (n == sizeof(buf)) {
      int w, h; // size of the BMP image
      read_int(buf + 18, w);
      read_int(buf + 22, h);
      int R = ((3*w+3)/4) * 4; // the number of bytes per row of BMP image, rounded up to multiple of 4
      bmp = new char[R * h + 54];
      memcpy(bmp, buf, 54);
      char *from = bmp + 54;
      rest = R * h;
      while (rest) {
        ssize_t n = read(fds[0], from, rest);
        if (n <= 0) break;
        from += n;
        rest -= n;
      }
//fprintf(stderr, "get_clipboard_image: image/bmp %dx%d rest=%lu\n", w,h,rest);
    }
    close(fds[0]);
    Fl_Nix_System_Driver *sys_dr = (Fl_Nix_System_Driver*)Fl::system_driver();
    if (!rest) Fl::e_clipboard_data = sys_dr->own_bmp_to_RGB(bmp);
    delete[] bmp;
    if (rest) return 1;
  }
  Fl::e_clipboard_type = Fl::clipboard_image;
  return 0;
}


void Fl_Wayland_System_Driver::paste(Fl_Widget &receiver, int clipboard, const char *type) {
  if (clipboard != 1) return;
  if (fl_i_own_selection[1]) {
    // We already have it, do it quickly without compositor.
    if (type == Fl::clipboard_plain_text && fl_selection_type[1] == type) {
      Fl::e_text = fl_selection_buffer[1];
      Fl::e_length = fl_selection_length[1];
      if (!Fl::e_text) Fl::e_text = (char *)"";
    } else if (type == Fl::clipboard_image && fl_selection_type[1] == type) {
      Fl::e_clipboard_data = own_bmp_to_RGB(fl_selection_buffer[1]);
      Fl::e_clipboard_type = Fl::clipboard_image;
    } else return;
    receiver.handle(FL_PASTE);
    return;
  }
  // otherwise get the compositor to return it:
  if (!fl_selection_offer) return;
  if (type == Fl::clipboard_plain_text && clipboard_contains(Fl::clipboard_plain_text)) {
    get_clipboard_or_dragged_text(fl_selection_offer);
    Fl::e_text = fl_selection_buffer[1];
    Fl::e_length = fl_selection_length[1];
    receiver.handle(FL_PASTE);
  } else if (type == Fl::clipboard_image && clipboard_contains(Fl::clipboard_image)) {
    if (get_clipboard_image()) return;
    Window xid = fl_xid(receiver.top_window());
    if (xid && xid->scale > 1) {
      Fl_RGB_Image *rgb = (Fl_RGB_Image*)Fl::e_clipboard_data;
      rgb->scale(rgb->data_w() / xid->scale, rgb->data_h() / xid->scale);
    }
    int done = receiver.handle(FL_PASTE);
    Fl::e_clipboard_type = "";
    if (done == 0) {
      delete (Fl_RGB_Image*)Fl::e_clipboard_data;
      Fl::e_clipboard_data = NULL;
    }
  }
}


void Fl_Wayland_System_Driver::copy(const char *stuff, int len, int clipboard, const char *type) {
  if (!stuff || len < 0) return;

  if (clipboard >= 2)
    clipboard = 1; // Only on X11 do multiple clipboards make sense.

  if (len+1 > fl_selection_buffer_length[clipboard]) {
    delete[] fl_selection_buffer[clipboard];
    fl_selection_buffer[clipboard] = new char[len+100];
    fl_selection_buffer_length[clipboard] = len+100;
  }
  memcpy(fl_selection_buffer[clipboard], stuff, len);
  fl_selection_buffer[clipboard][len] = 0; // needed for direct paste
  fl_selection_length[clipboard] = len;
  fl_i_own_selection[clipboard] = 1;
  fl_selection_type[clipboard] = Fl::clipboard_plain_text;
  if (clipboard == 1) {
    Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
    scr_driver->seat->data_source = wl_data_device_manager_create_data_source(scr_driver->seat->data_device_manager);
    // we transmit the adequate value of index in fl_selection_buffer[index]
    wl_data_source_add_listener(scr_driver->seat->data_source, &data_source_listener, (void*)1);
    wl_data_source_offer(scr_driver->seat->data_source, wld_plain_text_clipboard);
    wl_data_device_set_selection(scr_driver->seat->data_device, scr_driver->seat->data_source, scr_driver->seat->keyboard_enter_serial);
//fprintf(stderr, "wl_data_device_set_selection len=%d to %d\n", len, clipboard);
  }
}


// takes a raw RGB image and puts it in the copy/paste buffer
void Fl_Wayland_Screen_Driver::copy_image(const unsigned char *data, int W, int H){
  if (!data || W <= 0 || H <= 0) return;
  delete[] fl_selection_buffer[1];
  Fl_Nix_System_Driver *sys_dr = (Fl_Nix_System_Driver*)Fl::system_driver();
  fl_selection_buffer[1] = (char *)sys_dr->create_bmp(data,W,H,&fl_selection_length[1]);
  fl_selection_buffer_length[1] = fl_selection_length[1];
  fl_i_own_selection[1] = 1;
  fl_selection_type[1] = Fl::clipboard_image;
  seat->data_source = wl_data_device_manager_create_data_source(seat->data_device_manager);
  // we transmit the adequate value of index in fl_selection_buffer[index]
  wl_data_source_add_listener(seat->data_source, &data_source_listener, (void*)1);
  wl_data_source_offer(seat->data_source, "image/bmp");
  wl_data_device_set_selection(seat->data_device, seat->data_source, seat->keyboard_enter_serial);
//fprintf(stderr, "copy_image: len=%d\n", fl_selection_length[1]);
}

////////////////////////////////////////////////////////////////
// Code for tracking clipboard changes:

// is that possible with Wayland ?

////////////////////////////////////////////////////////////////

//#define USE_PRINT_BUTTON 1
#ifdef USE_PRINT_BUTTON

// to test the Fl_Printer class creating a "Print front window" button in a separate window
#include <FL/Fl_Printer.H>
#include <FL/Fl_Button.H>

void printFront(Fl_Widget *o, void *data)
{
  Fl_Printer printer;
  o->window()->hide();
  Fl_Window *win = Fl::first_window()->top_window();
  if(!win) return;
  int w, h;
  if( printer.begin_job(1) ) { o->window()->show(); return; }
  if( printer.begin_page() ) { o->window()->show(); return; }
  printer.printable_rect(&w,&h);
  // scale the printer device so that the window fits on the page
  float scale = 1;
  int ww = win->decorated_w();
  int wh = win->decorated_h();
  if (ww > w || wh > h) {
    scale = (float)w/ww;
    if ((float)h/wh < scale) scale = (float)h/wh;
    printer.scale(scale, scale);
    printer.printable_rect(&w, &h);
  }

// #define ROTATE 20.0
#ifdef ROTATE
  printer.scale(scale * 0.8, scale * 0.8);
  printer.printable_rect(&w, &h);
  printer.origin(w/2, h/2 );
  printer.rotate(ROTATE);
  printer.print_window( win, - win->w()/2, - win->h()/2);
  //printer.print_window_part( win, 0,0, win->w(), win->h(), - win->w()/2, - win->h()/2 );
#else
  printer.origin(w/2, h/2 );
  printer.print_window(win, -ww/2, -wh/2);
  //printer.print_window_part( win, 0,0, win->w(), win->h(), -ww/2, -wh/2 );
#endif

  printer.end_page();
  printer.end_job();
  o->window()->show();
}

#include <FL/Fl_Copy_Surface.H>
void copyFront(Fl_Widget *o, void *data)
{
  o->window()->hide();
  Fl_Window *win = Fl::first_window();
  if (!win) return;
  Fl_Copy_Surface *surf = new Fl_Copy_Surface(win->decorated_w(), win->decorated_h());
  Fl_Surface_Device::push_current(surf);
  surf->draw_decorated_window(win); // draw the window content
  Fl_Surface_Device::pop_current();
  delete surf; // put the window on the clipboard
  o->window()->show();
}

static int prepare_print_button() {
  static Fl_Window w(0,0,140,60);
  static Fl_Button bp(0,0,w.w(),30, "Print front window");
  bp.callback(printFront);
  static Fl_Button bc(0,30,w.w(),30, "Copy front window");
  bc.callback(copyFront);
  w.end();
  w.show();
  return 0;
}

static int unused = prepare_print_button();

#endif // USE_PRINT_BUTTON

#endif // !defined(FL_DOXYGEN)
