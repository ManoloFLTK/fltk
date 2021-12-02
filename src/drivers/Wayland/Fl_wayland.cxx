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

#  define CONSOLIDATE_MOTION 1

#  include <config.h>
#  include <FL/Fl.H>
#  include <FL/platform.H>
#  include "../../Fl_Window_Driver.H"
#  include <FL/Fl_Window.H>
#  include <FL/fl_utf8.h>
#  include <FL/Fl_Tooltip.H>
#  include <FL/fl_draw.H>
#  include <FL/Fl_Paged_Device.H>
#  include <FL/Fl_Shared_Image.H>
#  include <FL/fl_ask.H>
#  include <FL/filename.H>
#  include <FL/Fl_Image_Surface.H>
#  include <stdio.h>
#  include <stdlib.h>
#  include "../../flstring.h"
#  include "Fl_Wayland_Screen_Driver.H"
#  include "Fl_Wayland_Window_Driver.H"
#  include "Fl_Wayland_System_Driver.H"
#  include "Fl_Wayland_Graphics_Driver.H"
#  include "../../../libdecor/src/libdecor.h"
#  include "xdg-shell-client-protocol.h"
#  include <unistd.h>
#  include <time.h>
#  include <sys/time.h>
#  include <math.h>
#  include <errno.h>

////////////////////////////////////////////////////////////////
// interface to poll/select call:

#  if USE_POLL

#    include <poll.h>
static pollfd *pollfds = 0;

#  else
#    if HAVE_SYS_SELECT_H
#      include <sys/select.h>
#    endif /* HAVE_SYS_SELECT_H */


static fd_set fdsets[3];
static int maxfd;
#    define POLLIN 1
#    define POLLOUT 4
#    define POLLERR 8

#  endif /* USE_POLL */

static int nfds = 0;
static int fd_array_size = 0;
struct FD {
#  if !USE_POLL
  int fd;
  short events;
#  endif
  void (*cb)(int, void*);
  void* arg;
};

static FD *fd = 0;

void Fl_Wayland_System_Driver::add_fd(int n, int events, void (*cb)(int, void*), void *v) {
  remove_fd(n,events);
  int i = nfds++;
  if (i >= fd_array_size) {
    FD *temp;
    fd_array_size = 2*fd_array_size+1;

    if (!fd) temp = (FD*)malloc(fd_array_size*sizeof(FD));
    else temp = (FD*)realloc(fd, fd_array_size*sizeof(FD));

    if (!temp) return;
    fd = temp;

#  if USE_POLL
    pollfd *tpoll;

    if (!pollfds) tpoll = (pollfd*)malloc(fd_array_size*sizeof(pollfd));
    else tpoll = (pollfd*)realloc(pollfds, fd_array_size*sizeof(pollfd));

    if (!tpoll) return;
    pollfds = tpoll;
#  endif
  }
  fd[i].cb = cb;
  fd[i].arg = v;
#  if USE_POLL
  pollfds[i].fd = n;
  pollfds[i].events = events;
#  else
  fd[i].fd = n;
  fd[i].events = events;
  if (events & POLLIN) FD_SET(n, &fdsets[0]);
  if (events & POLLOUT) FD_SET(n, &fdsets[1]);
  if (events & POLLERR) FD_SET(n, &fdsets[2]);
  if (n > maxfd) maxfd = n;
#  endif
}

void Fl_Wayland_System_Driver::add_fd(int n, void (*cb)(int, void*), void* v) {
  add_fd(n, POLLIN, cb, v);
}

void Fl_Wayland_System_Driver::remove_fd(int n, int events) {
  int i,j;
# if !USE_POLL
  maxfd = -1; // recalculate maxfd on the fly
# endif
  for (i=j=0; i<nfds; i++) {
#  if USE_POLL
    if (pollfds[i].fd == n) {
      int e = pollfds[i].events & ~events;
      if (!e) continue; // if no events left, delete this fd
      pollfds[j].events = e;
    }
#  else
    if (fd[i].fd == n) {
      int e = fd[i].events & ~events;
      if (!e) continue; // if no events left, delete this fd
      fd[i].events = e;
    }
    if (fd[i].fd > maxfd) maxfd = fd[i].fd;
#  endif
    // move it down in the array if necessary:
    if (j<i) {
      fd[j] = fd[i];
#  if USE_POLL
      pollfds[j] = pollfds[i];
#  endif
    }
    j++;
  }
  nfds = j;
#  if !USE_POLL
  if (events & POLLIN) FD_CLR(n, &fdsets[0]);
  if (events & POLLOUT) FD_CLR(n, &fdsets[1]);
  if (events & POLLERR) FD_CLR(n, &fdsets[2]);
#  endif
}

void Fl_Wayland_System_Driver::remove_fd(int n) {
  remove_fd(n, -1);
}

extern int fl_send_system_handlers(void *e);

#if CONSOLIDATE_MOTION
//static Fl_Window* send_motion;
extern Fl_Window* fl_xmousewin;
#endif
//static bool in_a_window; // true if in any of our windows, even destroyed ones

// these pointers are set by the Fl::lock() function:
static void nothing() {}
void (*fl_lock_function)() = nothing;
void (*fl_unlock_function)() = nothing;


// This is never called with time_to_wait < 0.0:
// It should return negative on error, 0 if nothing happens before
// timeout, and >0 if any callbacks were done.
int Fl_Wayland_Screen_Driver::poll_or_select_with_delay(double time_to_wait) {
  
  if (fl_display) wl_display_flush(fl_display);
  
#  if !USE_POLL
    fd_set fdt[3];
    fdt[0] = fdsets[0];
    fdt[1] = fdsets[1];
    fdt[2] = fdsets[2];
#  endif
    int n;
    
    fl_unlock_function();
    
    if (time_to_wait < 2147483.648) {
#  if USE_POLL
      n = ::poll(pollfds, nfds, int(time_to_wait*1000 + .5));
#  else
      timeval t;
      t.tv_sec = int(time_to_wait);
      t.tv_usec = int(1000000 * (time_to_wait-t.tv_sec));
      n = ::select(maxfd+1,&fdt[0],&fdt[1],&fdt[2],&t);
#  endif
    } else {
#  if USE_POLL
      n = ::poll(pollfds, nfds, -1);
#  else
      n = ::select(maxfd+1,&fdt[0],&fdt[1],&fdt[2],0);
#  endif
    }
    fl_lock_function();
    
    if (n > 0) {
      for (int i=0; i<nfds; i++) {
#  if USE_POLL
        if (pollfds[i].revents) fd[i].cb(pollfds[i].fd, fd[i].arg);
#  else
        int f = fd[i].fd;
        short revents = 0;
        if (FD_ISSET(f,&fdt[0])) revents |= POLLIN;
        if (FD_ISSET(f,&fdt[1])) revents |= POLLOUT;
        if (FD_ISSET(f,&fdt[2])) revents |= POLLERR;
        if (fd[i].events & revents) fd[i].cb(f, fd[i].arg);
#  endif
      }
    }
    return n;
}

int Fl_Wayland_Screen_Driver::poll_or_select() {
  if (fl_display) wl_display_flush(fl_display);

  if (!nfds) return 0; // nothing to select or poll
#  if USE_POLL
  return ::poll(pollfds, nfds, 0);
#  else
  timeval t;
  t.tv_sec = 0;
  t.tv_usec = 0;
  fd_set fdt[3];
  fdt[0] = fdsets[0];
  fdt[1] = fdsets[1];
  fdt[2] = fdsets[2];
  return ::select(maxfd+1,&fdt[0],&fdt[1],&fdt[2],&t);
#  endif
}

////////////////////////////////////////////////////////////////

Window fl_message_window = 0;
int fl_screen;
Window fl_xim_win = 0;
char fl_is_over_the_spot = 0;


extern char *fl_get_font_xfld(int fnum, int size);


void fl_set_status(int x, int y, int w, int h)
{
 }

//extern XRectangle fl_spot;
extern int fl_spotf;
extern int fl_spots;

void Fl_Wayland_Screen_Driver::enable_im() {
}

void Fl_Wayland_Screen_Driver::disable_im() {
}


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


static void read_int(uchar *c, int& i) {
  i = *c;
  i |= (*(++c))<<8;
  i |= (*(++c))<<16;
  i |= (*(++c))<<24;
}

// turn BMP image FLTK produced by create_bmp() back to Fl_RGB_Image
static Fl_RGB_Image *own_bmp_to_RGB(char *bmp) {
  int w, h;
  read_int((uchar*)bmp + 18, w);
  read_int((uchar*)bmp + 22, h);
  int R=((3*w+3)/4) * 4; // the number of bytes per row, rounded up to multiple of 4
  bmp +=  54;
  uchar *data = new uchar[w*h*3];
  uchar *p = data;
  for (int i = h-1; i >= 0; i--) {
    char *s = bmp + i * R;
    for (int j = 0; j < w; j++) {
      *p++=s[2];
      *p++=s[1];
      *p++=s[0];
      s+=3;
    }
  }
  Fl_RGB_Image *img = new Fl_RGB_Image(data, w, h, 3);
  img->alloc_array = 1;
  return img;
}


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

static void data_source_handle_cancelled(void *data, struct wl_data_source *source) {
  // An application has replaced the clipboard contents or DnD finished
//fprintf(stderr, "data_source_handle_cancelled: %p\n", source);
  wl_data_source_destroy(source);
  fl_i_own_selection[1] = 0;
  if (data == 0) { // at end of DnD
    Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
    scr_driver->xc_arrow = scr_driver->cache_cursor("left_ptr");
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

static bool doing_dnd = false; // helps identify DnD within the app itself

int Fl_Wayland_Screen_Driver::dnd(int unused) {
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();

  struct wl_data_source *source =
    wl_data_device_manager_create_data_source(scr_driver->seat->data_device_manager);
  // we transmit the adequate value of index in fl_selection_buffer[index]
  wl_data_source_add_listener(source, &data_source_listener, (void*)0);
  wl_data_source_offer(source, wld_plain_text_clipboard);
  wl_data_source_set_actions(source, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);

  struct wl_surface *icon = NULL;
  scr_driver->xc_arrow = scr_driver->cache_cursor("dnd-copy");
  wl_data_device_start_drag(scr_driver->seat->data_device, source, scr_driver->seat->pointer_focus, icon, scr_driver->seat->serial);
  doing_dnd = true;
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
  if (ret) wl_data_offer_accept(current_drag_offer, fl_dnd_serial, "text/plain");
}

static void data_device_handle_leave(void *data, struct wl_data_device *data_device) {
//printf("Drag left our surface\n");
  doing_dnd = false;
}


static void data_device_handle_drop(void *data, struct wl_data_device *data_device) {
  if (!current_drag_offer) return;
  int ret = Fl::handle(FL_DND_RELEASE, fl_dnd_target_window);
//printf("data_device_handle_drop ret=%d\n", ret);

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
    if (!rest) Fl::e_clipboard_data = own_bmp_to_RGB(bmp);
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


static void write_short(unsigned char **cp, short i) {
  unsigned char *c = *cp;
  *c++ = i & 0xFF; i >>= 8;
  *c++ = i & 0xFF;
  *cp = c;
}

static void write_int(unsigned char **cp, int i) {
  unsigned char *c = *cp;
  *c++ = i & 0xFF; i >>= 8;
  *c++ = i & 0xFF; i >>= 8;
  *c++ = i & 0xFF; i >>= 8;
  *c++ = i & 0xFF;
  *cp = c;
}

static unsigned char *create_bmp(const unsigned char *data, int W, int H, int *return_size){
  int R = ((3*W+3)/4) * 4; // the number of bytes per row, rounded up to multiple of 4
  int s=H*R;
  int fs=14+40+s;
  unsigned char *b=new unsigned char[fs];
  unsigned char *c=b;
  // BMP header
  *c++='B';
  *c++='M';
  write_int(&c,fs);
  write_int(&c,0);
  write_int(&c,14+40);
  // DIB header:
  write_int(&c,40);
  write_int(&c,W);
  write_int(&c,H);
  write_short(&c,1);
  write_short(&c,24);//bits ber pixel
  write_int(&c,0);//RGB
  write_int(&c,s);
  write_int(&c,0);// horizontal resolution
  write_int(&c,0);// vertical resolution
  write_int(&c,0);//number of colors. 0 -> 1<<bits_per_pixel
  write_int(&c,0);
  // Pixel data
  data+=3*W*H;
  for (int y=0;y<H;++y){
    data-=3*W;
    const unsigned char *s=data;
    unsigned char *p=c;
    for (int x=0;x<W;++x){
      *p++=s[2];
      *p++=s[1];
      *p++=s[0];
      s+=3;
    }
    c+=R;
  }
  *return_size = fs;
  return b;
}


// takes a raw RGB image and puts it in the copy/paste buffer
void Fl_Wayland_Screen_Driver::copy_image(const unsigned char *data, int W, int H){
  if (!data || W <= 0 || H <= 0) return;
  delete[] fl_selection_buffer[1];
  fl_selection_buffer[1] = (char *)create_bmp(data,W,H,&fl_selection_length[1]);
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


void Fl_Wayland_Window_Driver::resize(int X, int Y, int W, int H) {
  int is_a_move = (X != x() || Y != y() || Fl_Window::is_a_rescale());
  int is_a_resize = (W != w() || H != h() || Fl_Window::is_a_rescale());
  if (is_a_move) force_position(1);
  else if (!is_a_resize && !is_a_move) return;
  if (is_a_resize) {
    pWindow->Fl_Group::resize(X,Y,W,H);
//fprintf(stderr, "resize: win=%p to %dx%d\n", pWindow, W, H);
    if (shown()) {pWindow->redraw();}
  } else {
    if (pWindow->parent() || pWindow->menu_window() || pWindow->tooltip_window()) {
      x(X); y(Y);
//fprintf(stderr, "move menuwin=%p x()=%d\n", pWindow, X);
    } else {
      //"a deliberate design trait of Wayland makes application windows ignorant of their exact placement on screen"
      x(0); y(0);
    }
  }
  if (is_a_resize && !pWindow->resizable() && !shown()) {
    pWindow->size_range(w(), h(), w(), h());
  }
    
  if (shown()) {
    struct wld_window *fl_win = fl_xid(pWindow);
    if (is_a_resize) {
      float f = Fl::screen_scale(pWindow->screen_num());
      if (!pWindow->resizable() && !fl_win->frame) pWindow->size_range(w(), h(), w(), h());
      if (fl_win->frame) { // a decorated window
        if (fl_win->buffer) {
          Fl_Wayland_Graphics_Driver::buffer_release(fl_win);
        }
        fl_win->configured_width = W;
        fl_win->configured_height = H;
        if (!in_handle_configure && fl_win->xdg_toplevel) {
          struct libdecor_state *state = libdecor_state_new(int(W * f), int(H * f));
          libdecor_frame_commit(fl_win->frame, state, NULL); // necessary only if resize is initiated by prog
          libdecor_state_free(state);
          if (libdecor_frame_is_floating(fl_win->frame)) {
            fl_win->floating_width = int(W*f);
            fl_win->floating_height = int(H*f);
          }
        }
      } else if (fl_win->subsurface) { // a subwindow
        wl_subsurface_set_position(fl_win->subsurface, X * f, Y * f);
        if (!pWindow->as_gl_window()) Fl_Wayland_Graphics_Driver::buffer_release(fl_win);
        fl_win->configured_width = W;
        fl_win->configured_height = H;
      } else if (fl_win->xdg_surface) { // a window without border
        if (!pWindow->as_gl_window()) Fl_Wayland_Graphics_Driver::buffer_release(fl_win);
        fl_win->configured_width = W;
        fl_win->configured_height = H;
        xdg_surface_set_window_geometry(fl_win->xdg_surface, 0, 0, W * f, H * f);
      }
    } else {
     if (!in_handle_configure && fl_win->xdg_toplevel) {
      // Wayland doesn't seem to provide a reliable way for the app to set the window position on screen.
      // This is functional when the move is mouse-driven.
      Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
      xdg_toplevel_move(fl_win->xdg_toplevel, scr_driver->seat->wl_seat, scr_driver->seat->serial);
      }
    }
  }
}

////////////////////////////////////////////////////////////////


extern Fl_Window *fl_xfocus;


/* Change an existing window to fullscreen */
void Fl_Wayland_Window_Driver::fullscreen_on() {
    int top, bottom, left, right;

    top = fullscreen_screen_top();
    bottom = fullscreen_screen_bottom();
    left = fullscreen_screen_left();
    right = fullscreen_screen_right();

    if ((top < 0) || (bottom < 0) || (left < 0) || (right < 0)) {
      top = screen_num();
      bottom = top;
      left = top;
      right = top;
    }
  pWindow->wait_for_expose(); // make sure ->xdg_toplevel is initialized
  if (fl_xid(pWindow)->xdg_toplevel) {
    xdg_toplevel_set_fullscreen(fl_xid(pWindow)->xdg_toplevel, NULL);
    pWindow->_set_fullscreen();
    Fl::handle(FL_FULLSCREEN, pWindow);
  }
}

void Fl_Wayland_Window_Driver::fullscreen_off(int X, int Y, int W, int H) {
  if (!border()) pWindow->Fl_Group::resize(X, Y, W, H);
  xdg_toplevel_unset_fullscreen(fl_xid(pWindow)->xdg_toplevel);
  pWindow->_clear_fullscreen();
  Fl::handle(FL_FULLSCREEN, pWindow);
}

////////////////////////////////////////////////////////////////

void Fl_Wayland_Screen_Driver::default_icons(const Fl_RGB_Image *icons[], int count) {
}

void Fl_Wayland_Window_Driver::set_icons() {
}

////////////////////////////////////////////////////////////////


int Fl_Wayland_Window_Driver::set_cursor(const Fl_RGB_Image *rgb, int hotx, int hoty) {
  static struct wl_cursor *previous_custom_cursor = NULL;
  static struct buffer *previous_offscreen = NULL;
  struct cursor_image { // as in wayland-cursor.c of the Wayland project source code
    struct wl_cursor_image image;
    struct wl_cursor_theme *theme;
    struct wl_buffer *buffer;
    int offset; /* data offset of this image in the shm pool */
  };
// build a new wl_cursor and its image
  struct wl_cursor *new_cursor = (struct wl_cursor*)malloc(sizeof(struct wl_cursor));
  struct cursor_image *new_image = (struct cursor_image*)calloc(1, sizeof(struct cursor_image));
  int scale = fl_xid(pWindow)->scale;
  new_image->image.width = rgb->w() * scale;
  new_image->image.height = rgb->h() * scale;
  new_image->image.hotspot_x = hotx * scale;
  new_image->image.hotspot_y = hoty * scale;
  new_image->image.delay = 0;
  new_image->offset = 0;
  //create a Wayland buffer and have it used as an image of the new cursor
  struct buffer *offscreen = Fl_Wayland_Graphics_Driver::create_shm_buffer(new_image->image.width, new_image->image.height, WL_SHM_FORMAT_ARGB8888, NULL);
  new_image->buffer = offscreen->wl_buffer;
  new_cursor->image_count = 1;
  new_cursor->images = (struct wl_cursor_image**)malloc(sizeof(struct wl_cursor_image*));
  new_cursor->images[0] = (struct wl_cursor_image*)new_image;
  new_cursor->name = strdup("custom cursor");
  // draw the rgb image to the cursor's drawing buffer
  Fl_Image_Surface *img_surf = new Fl_Image_Surface(new_image->image.width, new_image->image.height, 0, offscreen);
  Fl_Surface_Device::push_current(img_surf);
  Fl_Wayland_Graphics_Driver *driver = (Fl_Wayland_Graphics_Driver*)img_surf->driver();
  cairo_scale(driver->cr(), scale, scale);
  memset(offscreen->draw_buffer, 0, offscreen->data_size);
  ((Fl_RGB_Image*)rgb)->draw(0, 0);
  Fl_Surface_Device::pop_current();
  delete img_surf;
  memcpy(offscreen->data, offscreen->draw_buffer, offscreen->data_size);
  if (new_image->image.width <= 64 && new_image->image.height <= 64) {
    // for some mysterious reason, small cursor images want RGBA whereas big ones want BGRA !!!
    //fprintf(stderr, "exchange R and B\n");
    char *to = (char*)offscreen->data, *last = to + offscreen->data_size, xchg;
    while (to < last) {
      xchg = *to;
      *to = *(to+2);
      *(to+2) = xchg;
      to += 4;
    }
  }
  //have this new cursor used
  this->cursor = new_cursor;
  //memorize new cursor
  if (previous_custom_cursor) {
    struct wld_window fake_xid;
    new_image = (struct cursor_image*)previous_custom_cursor->images[0];
    fake_xid.buffer = previous_offscreen;
    Fl_Wayland_Graphics_Driver::buffer_release(&fake_xid);
    free(new_image);
    free(previous_custom_cursor->images);
    free(previous_custom_cursor->name);
    free(previous_custom_cursor);
  }
  previous_custom_cursor = new_cursor;
  previous_offscreen = offscreen;
  return 1;
}

////////////////////////////////////////////////////////////////

// returns pointer to the filename, or null if name ends with '/'
const char *Fl_Wayland_System_Driver::filename_name(const char *name) {
  const char *p,*q;
  if (!name) return (0);
  for (p=q=name; *p;) if (*p++ == '/') q = p;
  return q;
}

void Fl_Wayland_Window_Driver::label(const char *name, const char *iname) {
  if (shown() && !parent()) {
    if (!name) name = "";
    if (!iname) iname = fl_filename_name(name);
    libdecor_frame_set_title(fl_xid(pWindow)->frame, name);
  }
}


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
