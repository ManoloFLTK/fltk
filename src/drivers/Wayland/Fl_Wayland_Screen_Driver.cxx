//
// Implementation of Wayland Screen interface
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
#include "Fl_Wayland_Screen_Driver.H"
#include "Fl_Wayland_Window_Driver.H"
#include "Fl_Wayland_System_Driver.H"
#include "Fl_Wayland_Graphics_Driver.H"
#include "../../../libdecor/src/libdecor.h"
#include "../../../libdecor/build/xdg-shell-client-protocol.h"
#include "../Posix/Fl_Posix_System_Driver.H"
#include <FL/Fl.H>
#include <FL/platform.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tooltip.H>
#include <FL/filename.H>
#include <dlfcn.h>
#include <sys/time.h>
#include <linux/input.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <assert.h>
#include <sys/mman.h>
extern "C" {
  bool libdecor_get_cursor_settings(char **theme, int *size);
}


#define fl_max(a,b) ((a) > (b) ? (a) : (b))

struct pointer_output {
  Fl_Wayland_Screen_Driver::output* output;
  struct wl_list link;
};


/* Implementation note about screen-related information
 
 struct wl_output : Wayland-defined, contains info about a screen, one such record for each screen

 struct Fl_Wayland_Screen_Driver::output { // FLTK defined
   uint32_t id; // screen identification
   struct wl_output *wl_output;
   int scale;
   struct wl_list link;
 };

 struct Fl_Wayland_Window_Driver::window_output {  // FLTK defined
   Fl_Wayland_Screen_Driver::output* output;
   struct wl_list link;
 }

 The unique Fl_Wayland_Screen_Driver object contains a member
   "outputs" of type struct wl_list = list of Fl_Wayland_Screen_Driver::output records
   - this list is initialised by open-display
   - registry_handle_global() feeds the list with 1 record for each screen
   - registry_handle_global_remove() runs when a screen is removed. It removes
   output records that correspond to that screen from the unique list of screens
   (outputs member of the Fl_Wayland_Screen_Driver) and the list of struct output objects attached
   to each window.

 Each Fl_Wayland_Window_Driver object contains a member
   "outputs" of type struct wl_list = list of Fl_Wayland_Window_Driver::window_output records
   - this list is fed by surface_enter() (when a surface is mapped?)
   - these records contain:
   window_output->output = (Fl_Wayland_Screen_Driver::output*)wl_output_get_user_data(wl_output);
   where wl_output is received from OS by surface_enter()
   - surface_leave() removes the adequate record from the list
   - Fl_Wayland_Window_Driver::update_scale() sets the scale info of the records for a given window
 */

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};


// these are set by Fl::args() and override any system colors: from Fl_get_system_colors.cxx
extern const char *fl_fg;
extern const char *fl_bg;
extern const char *fl_bg2;
// end of extern additions workaround

//
//  timers
//


////////////////////////////////////////////////////////////////////////
// Timeouts are stored in a sorted list (*first_timeout), so only the
// first one needs to be checked to see if any should be called.
// Allocated, but unused (free) Timeout structs are stored in another
// linked list (*free_timeout).

struct Timeout {
  double time;
  void (*cb)(void*);
  void* arg;
  Timeout* next;
};
static Timeout* first_timeout, *free_timeout;

// I avoid the overhead of getting the current time when we have no
// timeouts by setting this flag instead of getting the time.
// In this case calling elapse_timeouts() does nothing, but records
// the current time, and the next call will actually elapse time.
static char reset_clock = 1;

static void elapse_timeouts() {
  static struct timeval prevclock;
  struct timeval newclock;
  gettimeofday(&newclock, NULL);
  double elapsed = newclock.tv_sec - prevclock.tv_sec +
    (newclock.tv_usec - prevclock.tv_usec)/1000000.0;
  prevclock.tv_sec = newclock.tv_sec;
  prevclock.tv_usec = newclock.tv_usec;
  if (reset_clock) {
    reset_clock = 0;
  } else if (elapsed > 0) {
    for (Timeout* t = first_timeout; t; t = t->next) t->time -= elapsed;
  }
}


// Continuously-adjusted error value, this is a number <= 0 for how late
// we were at calling the last timeout. This appears to make repeat_timeout
// very accurate even when processing takes a significant portion of the
// time interval:
static double missed_timeout_by;

/**
 Creates a driver that manages all screen and display related calls.

 This function must be implemented once for every platform.
 */
Fl_Screen_Driver *Fl_Screen_Driver::newScreenDriver()
{
  Fl_Wayland_Screen_Driver *d = new Fl_Wayland_Screen_Driver();
#if USE_XFT
  for (int i = 0;  i < MAX_SCREENS; i++) d->screens[i].scale = 1;
#endif
  return d;
}

FL_EXPORT struct wl_display *fl_display = NULL;

static bool has_xrgb = false;


static void shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
  if (format == WL_SHM_FORMAT_ARGB8888)
    has_xrgb = true;
}

static struct wl_shm_listener shm_listener = {
  shm_format
};

static void do_set_cursor(struct seat *seat)
{
  struct wl_cursor *wl_cursor;
  struct wl_cursor_image *image;
  struct wl_buffer *buffer;
  const int scale = seat->pointer_scale;

  if (!seat->cursor_theme)
    return;

  wl_cursor = seat->default_cursor;
  image = wl_cursor->images[0];
  buffer = wl_cursor_image_get_buffer(image);
  wl_pointer_set_cursor(seat->wl_pointer, seat->serial,
            seat->cursor_surface,
            image->hotspot_x / scale,
            image->hotspot_y / scale);
  wl_surface_attach(seat->cursor_surface, buffer, 0, 0);
  wl_surface_set_buffer_scale(seat->cursor_surface, scale);
  wl_surface_damage_buffer(seat->cursor_surface, 0, 0,
         image->width, image->height);
  wl_surface_commit(seat->cursor_surface);
}

static uint32_t ptime;
FL_EXPORT uint32_t fl_event_time;
static int px, py;


static void set_event_xy(Fl_Window *win) {
  // turn off is_click if enough time or mouse movement has passed:
  if (abs(Fl::e_x_root-px)+abs(Fl::e_y_root-py) > 3 ||
      fl_event_time >= ptime+1000) {
    Fl::e_is_click = 0;
//fprintf(stderr, "Fl::e_is_click = 0\n");
  }
}

// if this is same event as last && is_click, increment click count:
static inline void checkdouble() {
  if (Fl::e_is_click == Fl::e_keysym) {
    Fl::e_clicks++;
//fprintf(stderr, "Fl::e_clicks = %d\n", Fl::e_clicks);
  } else {
    Fl::e_clicks = 0;
    Fl::e_is_click = Fl::e_keysym;
//fprintf(stderr, "Fl::e_is_click = %d\n", Fl::e_is_click);
  }
  px = Fl::e_x_root;
  py = Fl::e_y_root;
  ptime = fl_event_time;
}


static Fl_Window *surface_to_window(struct wl_surface *surface) {
  Fl_X *xp = Fl_X::first;
  while (xp) {
    if (xp->xid->wl_surface == surface || xp->xid->gl_wl_surface == surface) return xp->w;
    xp = xp->next;
  }
  return NULL;
}


static void pointer_enter(void *data,
        struct wl_pointer *wl_pointer,
        uint32_t serial,
        struct wl_surface *surface,
        wl_fixed_t surface_x,
        wl_fixed_t surface_y)
{
  struct seat *seat = (struct seat*)data;
  do_set_cursor(seat);
  seat->serial = serial;
  Fl_Window *win = surface_to_window(surface);
  if (win) {
    Fl::e_x = wl_fixed_to_int(surface_x);
    Fl::e_x_root = Fl::e_x + win->x();
    Fl::e_y = wl_fixed_to_int(surface_y);
    Fl::e_y_root = Fl::e_y + win->y();
    set_event_xy(win);
    Fl::handle(FL_ENTER, win);
//fprintf(stderr, "pointer_enter window=%p\n", win);
  }
  seat->pointer_focus = surface;
}


static void pointer_leave(void *data,
        struct wl_pointer *wl_pointer,
        uint32_t serial,
        struct wl_surface *surface)
{
  struct seat *seat = (struct seat*)data;
  if (seat->pointer_focus == surface) seat->pointer_focus = NULL;
  Fl_Window *win = surface_to_window(surface);
  if (win) {
    Fl::belowmouse(0);
    set_event_xy(win);
  }
//fprintf(stderr, "pointer_leave surface=%p window=%p\n", surface, win);
}


static void pointer_motion(void *data,
         struct wl_pointer *wl_pointer,
         uint32_t time,
         wl_fixed_t surface_x,
         wl_fixed_t surface_y)
{
  struct seat *seat = (struct seat*)data;
  Fl_Window *win = surface_to_window(seat->pointer_focus);
  if (!win) return;
  Fl::e_x = wl_fixed_to_int(surface_x);
  Fl::e_x_root = Fl::e_x + win->x();
  // If there's an active grab() and the pointer is in a window other than the grab(),
  // make e_x_root too large to be in any window
  if (Fl::grab() && !Fl::grab()->menu_window() && Fl::grab() != win) {
    Fl::e_x_root = 1000000;
  }
  Fl::e_y = wl_fixed_to_int(surface_y);
  Fl::e_y_root = Fl::e_y + win->y();
//fprintf(stderr, "FL_MOVE on win=%p to x:%dx%d root:%dx%d\n", win, Fl::e_x, Fl::e_y, Fl::e_x_root, Fl::e_y_root);
  fl_event_time = time;
  set_event_xy(win);
  Fl::handle(FL_MOVE, win);
}


//#include <FL/names.h>
static void pointer_button(void *data,
         struct wl_pointer *wl_pointer,
         uint32_t serial,
         uint32_t time,
         uint32_t button,
         uint32_t state)
{
  struct seat *seat = (struct seat*)data;
  seat->serial = serial;
  int event = 0;
  Fl_Window *win = surface_to_window(seat->pointer_focus);
  if (!win) return;
  fl_event_time = time;
  if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED && seat->pointer_focus == NULL &&
      fl_xid(win)->frame) {
    // click on titlebar
    libdecor_frame_move(fl_xid(win)->frame, seat->wl_seat, serial);
    return;
  }
  int b = 0;
  Fl::e_state = 0;
  if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
    if (button == BTN_LEFT) {Fl::e_state = FL_BUTTON1; b = 1;}
    else if (button == BTN_RIGHT) {Fl::e_state = FL_BUTTON3; b = 3;}
    else if (button == BTN_MIDDLE) {Fl::e_state = FL_BUTTON2; b = 2;}
  }
  Fl::e_keysym = FL_Button + b;
  Fl::e_dx = Fl::e_dy = 0;

  if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
    event = FL_PUSH;
    checkdouble();
  } else if (state == WL_POINTER_BUTTON_STATE_RELEASED) event = FL_RELEASE;
  set_event_xy(win);
//fprintf(stderr, "%s %s\n", fl_eventnames[event], win->label() ? win->label():"[]");
  Fl::handle(event, win);
}

static void pointer_axis(void *data,
       struct wl_pointer *wl_pointer,
       uint32_t time,
       uint32_t axis,
       wl_fixed_t value)
{
  struct seat *seat = (struct seat*)data;
  Fl_Window *win = surface_to_window(seat->pointer_focus);
  if (!win) return;
  fl_event_time = time;
  int delta = wl_fixed_to_int(value) / 10;
//fprintf(stderr, "FL_MOUSEWHEEL: %c delta=%d\n", axis==WL_POINTER_AXIS_HORIZONTAL_SCROLL?'H':'V', delta);
  // allow both horizontal and vertical movements to be processed by the widget
  if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
    Fl::e_dx = delta;
    Fl::e_dy = 0;
    Fl::handle(FL_MOUSEWHEEL, win);
  }
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    Fl::e_dx = 0;
    Fl::e_dy = delta;
    Fl::handle(FL_MOUSEWHEEL, win);
  }
}

static struct wl_pointer_listener pointer_listener = {
  pointer_enter,
  pointer_leave,
  pointer_motion,
  pointer_button,
  pointer_axis
};

static const char *proxy_tag = "libdecor-client";//TODO: see what's the purpose of this

bool Fl_Wayland_Screen_Driver::own_output(struct wl_output *output)
{
  return wl_proxy_get_tag((struct wl_proxy *)output) == &proxy_tag;
}

static void init_cursors(struct seat *seat);

static void try_update_cursor(struct seat *seat)
{
  struct pointer_output *pointer_output;
  int scale = 1;

  wl_list_for_each(pointer_output, &seat->pointer_outputs, link) {
    scale = fl_max(scale, pointer_output->output->scale);
  }

  if (scale != seat->pointer_scale) {
    seat->pointer_scale = scale;
    init_cursors(seat);
    do_set_cursor(seat);
  }
}


static void cursor_surface_enter(void *data,
        struct wl_surface *wl_surface,
        struct wl_output *wl_output)
{
  struct seat *seat = (struct seat*)data;
  struct pointer_output *pointer_output;

  if (!Fl_Wayland_Screen_Driver::own_output(wl_output))
    return;

  pointer_output = (struct pointer_output *)calloc(1, sizeof(struct pointer_output));
  pointer_output->output = (Fl_Wayland_Screen_Driver::output *)wl_output_get_user_data(wl_output);
//fprintf(stderr, "cursor_surface_enter: wl_output_get_user_data(%p)=%p\n", wl_output, pointer_output->output);
  wl_list_insert(&seat->pointer_outputs, &pointer_output->link);
  try_update_cursor(seat);
}

static void cursor_surface_leave(void *data,
        struct wl_surface *wl_surface,
        struct wl_output *wl_output)
{
  struct seat *seat = (struct seat*)data;
  struct pointer_output *pointer_output, *tmp;

  wl_list_for_each_safe(pointer_output, tmp, &seat->pointer_outputs, link) {
    if (pointer_output->output->wl_output == wl_output) {
      wl_list_remove(&pointer_output->link);
      free(pointer_output);
    }
  }
}

static struct wl_surface_listener cursor_surface_listener = {
  cursor_surface_enter,
  cursor_surface_leave,
};


static void init_cursors(struct seat *seat)
{
  char *name;
  int size;
  struct wl_cursor_theme *theme;

  if (!libdecor_get_cursor_settings(&name, &size)) {
    name = NULL;
    size = 24;
  }
  size *= seat->pointer_scale;
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  theme = wl_cursor_theme_load(name, size, scr_driver->wl_shm);
  free(name);
  //struct wl_cursor_theme *old_theme = seat->cursor_theme;
  if (theme != NULL) {
    if (seat->cursor_theme) {
     // caution to destroy theme because Fl_Wayland_Window_Driver::set_cursor(Fl_Cursor) caches used cursors
      scr_driver->reset_cursor();
      wl_cursor_theme_destroy(seat->cursor_theme);
    }
    seat->cursor_theme = theme;
  }
  if (seat->cursor_theme)
    seat->default_cursor = scr_driver->xc_arrow = wl_cursor_theme_get_cursor(seat->cursor_theme, "left_ptr");
  if (!seat->cursor_surface) {
    seat->cursor_surface = wl_compositor_create_surface(scr_driver->wl_compositor);
    wl_surface_add_listener(seat->cursor_surface, &cursor_surface_listener, seat);
  }
}


static void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t format, int32_t fd, uint32_t size)
{
  struct seat *seat = (struct seat*)data;
  assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);
  
  char *map_shm = (char*)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  assert(map_shm != MAP_FAILED);
  
  struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(seat->xkb_context, map_shm,
                                                             XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap(map_shm, size);
  close(fd);
  
  struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);
  xkb_keymap_unref(seat->xkb_keymap);
  xkb_state_unref(seat->xkb_state);
  seat->xkb_keymap = xkb_keymap;
  seat->xkb_state = xkb_state;
}

static void wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, struct wl_surface *surface,
               struct wl_array *keys)
{
  struct seat *seat = (struct seat*)data;
//fprintf(stderr, "keyboard enter fl_win=%p; keys pressed are:\n", surface_to_window(surface));
  seat->keyboard_surface = surface;
  seat->keyboard_enter_serial = serial;
}

struct key_repeat_data_t {
  uint32_t time;
  Fl_Window *window;
};

#define KEY_REPEAT_DELAY 0.5 // sec
#define KEY_REPEAT_INTERVAL 0.05 // sec

static void key_repeat_timer_cb(key_repeat_data_t *key_repeat_data) {
  if (Fl::event() == FL_KEYDOWN && fl_event_time == key_repeat_data->time) {
    Fl::handle(FL_KEYDOWN, key_repeat_data->window);
    Fl::add_timeout(KEY_REPEAT_INTERVAL, (Fl_Timeout_Handler)key_repeat_timer_cb, key_repeat_data);
  }
  else delete key_repeat_data;
}

int Fl_Wayland_Screen_Driver::next_marked_length = 0;

int Fl_Wayland_Screen_Driver::has_marked_text() {
  return true;
}

void Fl_Wayland_Screen_Driver::reset_marked_text() {
  Fl::compose_state = 0;
  next_marked_length = 0;
}

int Fl_Wayland_Screen_Driver::compose(int& del) {
  unsigned char ascii = (unsigned char)Fl::e_text[0];
  int condition = (Fl::e_state & (FL_ALT | FL_META | FL_CTRL)) && ascii < 128 ; // letter+modifier key
  condition |= (Fl::e_keysym >= FL_Shift_L && Fl::e_keysym <= FL_Alt_R); // pressing modifier key
//fprintf(stderr, "compose: condition=%d e_state=%x ascii=%d\n", condition, Fl::e_state, ascii);
  if (condition) { del = 0; return 0;}
//fprintf(stderr, "compose: del=%d compose_state=%d next_marked_length=%d \n", del, Fl::compose_state, next_marked_length);
  del = Fl::compose_state;
  Fl::compose_state = next_marked_length;
  // no-underlined-text && (ascii non-printable || ascii == delete)
  if ( (!Fl::compose_state) && (ascii <= 31 || ascii == 127)) { del = 0; return 0; }
  return 1;
}

void Fl_Wayland_Screen_Driver::compose_reset()
{
  Fl::compose_state = 0;
  next_marked_length = 0;
  xkb_compose_state_reset(seat->xkb_compose_state);
}

struct dead_key_struct {
  xkb_keysym_t keysym; // the keysym obtained when hitting a dead key
  const char *marked_text; // the temporary text to display for that dead key
};

static dead_key_struct dead_keys[] = {
  {XKB_KEY_dead_grave, "`"},
  {XKB_KEY_dead_acute, "´"},
  {XKB_KEY_dead_circumflex, "^"},
  {XKB_KEY_dead_tilde, "~"},
  {XKB_KEY_dead_perispomeni, "~"}, /* alias for dead_tilde */
  {XKB_KEY_dead_macron, "¯"},
  {XKB_KEY_dead_breve, "˘"},
  {XKB_KEY_dead_abovedot, "˙"},
  {XKB_KEY_dead_diaeresis, "¨"},
  {XKB_KEY_dead_abovering, "˚"},
  {XKB_KEY_dead_doubleacute, "˝"},
  {XKB_KEY_dead_caron, "ˇ"},
  {XKB_KEY_dead_cedilla, "¸"},
  {XKB_KEY_dead_ogonek, "˛"},
  {XKB_KEY_dead_iota, "ι"},
  {XKB_KEY_dead_doublegrave, " ̏"},
};

const int dead_key_count = sizeof(dead_keys)/sizeof(struct dead_key_struct);


static void wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
  struct seat *seat = (struct seat*)data;
  seat->serial = serial;
  static char buf[128];
  uint32_t keycode = key + 8;
  xkb_keysym_t sym = xkb_state_key_get_one_sym(seat->xkb_state, keycode);
/*xkb_keysym_get_name(sym, buf, sizeof(buf));
const char *action = (state == WL_KEYBOARD_KEY_STATE_PRESSED ? "press" : "release");
fprintf(stderr, "key %s: sym: %-12s(%d) code:%u fl_win=%p, ", action, buf, sym, keycode, surface_to_window(seat->keyboard_surface));*/
  xkb_state_key_get_utf8(seat->xkb_state, keycode, buf, sizeof(buf));
//fprintf(stderr, "utf8: '%s' e_length=%d\n", buf, (int)strlen(buf));
  Fl::e_keysym = sym;
  // special processing for number keys == keycodes 10-19 :
  if (keycode >= 10 && keycode <= 18) Fl::e_keysym = keycode + 39;
  else if (keycode == 19) Fl::e_keysym = 48;
  Fl::e_text = buf;
  Fl::e_length = strlen(buf);
  // Process dead keys and compose sequences :
  enum xkb_compose_status status = XKB_COMPOSE_NOTHING;
  Fl::compose_state = 0;
  if (state == WL_KEYBOARD_KEY_STATE_PRESSED && !(sym >= FL_Shift_L && sym <= FL_Alt_R) &&
      sym != XKB_KEY_ISO_Level3_Shift) {
    xkb_compose_state_feed(seat->xkb_compose_state, sym);
    status = xkb_compose_state_get_status(seat->xkb_compose_state);
    if (status == XKB_COMPOSE_COMPOSING) {
      if (Fl::e_length == 0) { // dead keys produce e_length = 0
        int i;
        for (i = 0; i < dead_key_count; i++) {
          if (dead_keys[i].keysym == sym) break;
        }
        if (i < dead_key_count) strcpy(buf, dead_keys[i].marked_text);
        else buf[0] = 0;
        Fl::e_length = strlen(buf);
        Fl::compose_state = 0;
      }
      Fl_Wayland_Screen_Driver::next_marked_length = Fl::e_length;
    } else if (status == XKB_COMPOSE_COMPOSED) {
      Fl::e_length = xkb_compose_state_get_utf8(seat->xkb_compose_state, buf, sizeof(buf));
      Fl::compose_state = Fl_Wayland_Screen_Driver::next_marked_length;
      Fl_Wayland_Screen_Driver::next_marked_length = 0;
    } else if (status == XKB_COMPOSE_CANCELLED) {
      Fl::e_length = 0;
      Fl::compose_state = Fl_Wayland_Screen_Driver::next_marked_length;
      Fl_Wayland_Screen_Driver::next_marked_length = 0;
    }
//fprintf(stderr, "xkb_compose_status=%d ctxt=%p state=%p l=%d[%s]\n", status, seat->xkb_context, seat->xkb_compose_state, Fl::e_length, buf);
  }
  
  fl_event_time = time;
  int event = (state == WL_KEYBOARD_KEY_STATE_PRESSED ? FL_KEYDOWN : FL_KEYUP);
  // Send event to focus-containing top window as defined by FLTK,
  // otherwise send it to Wayland-defined focus window
  Fl_Window *win = ( Fl::focus() ? Fl::focus()->top_window() : surface_to_window(seat->keyboard_surface) );
  set_event_xy(win);
  Fl::e_is_click = 0;
  Fl::handle(event, win);
  key_repeat_data_t *key_repeat_data = new key_repeat_data_t;
  key_repeat_data->time = time;
  key_repeat_data->window = win;
  if (event == FL_KEYDOWN && status == XKB_COMPOSE_NOTHING && !(sym >= FL_Shift_L && sym <= FL_Alt_R))
    Fl::add_timeout(KEY_REPEAT_DELAY, (Fl_Timeout_Handler)key_repeat_timer_cb, key_repeat_data);
}

static void wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, struct wl_surface *surface)
{
  struct seat *seat = (struct seat*)data;
//fprintf(stderr, "keyboard leave fl_win=%p\n", surface_to_window(surface));
  seat->keyboard_surface = NULL;
}

static void wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, uint32_t mods_depressed,
               uint32_t mods_latched, uint32_t mods_locked,
               uint32_t group)
{
  struct seat *seat = (struct seat*)data;
  xkb_state_update_mask(seat->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
  Fl::e_state = 0;
  if (xkb_state_mod_name_is_active(seat->xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_DEPRESSED))
    Fl::e_state |= FL_SHIFT;
  if (xkb_state_mod_name_is_active(seat->xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_DEPRESSED))
    Fl::e_state |= FL_CTRL;
  if (xkb_state_mod_name_is_active(seat->xkb_state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_DEPRESSED))
    Fl::e_state |= FL_ALT;
  if (xkb_state_mod_name_is_active(seat->xkb_state, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_LOCKED))
    Fl::e_state |= FL_CAPS_LOCK;
//fprintf(stderr, "mods_depressed=%u Fl::e_state=%X\n", mods_depressed, Fl::e_state);
}

static void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay)
{
  // wl_keyboard is version 3 under Debian, but that event isn't sent until version 4
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
       .keymap = wl_keyboard_keymap,
       .enter = wl_keyboard_enter,
       .leave = wl_keyboard_leave,
       .key = wl_keyboard_key,
       .modifiers = wl_keyboard_modifiers,
       .repeat_info = wl_keyboard_repeat_info,
};

                                                
static void seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities)
{
  struct seat *seat = (struct seat*)data;
  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !seat->wl_pointer) {
    seat->wl_pointer = wl_seat_get_pointer(wl_seat);
    wl_pointer_add_listener(seat->wl_pointer, &pointer_listener, seat);
    seat->pointer_scale = 1;
    init_cursors(seat);
  } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && seat->wl_pointer) {
    wl_pointer_release(seat->wl_pointer);
    seat->wl_pointer = NULL;
  }
  
  bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
  if (have_keyboard && seat->wl_keyboard == NULL) {
          seat->wl_keyboard = wl_seat_get_keyboard(wl_seat);
          wl_keyboard_add_listener(seat->wl_keyboard,
                          &wl_keyboard_listener, seat);
//fprintf(stderr, "wl_keyboard version=%d\n", wl_keyboard_get_version(seat->wl_keyboard));

  } else if (!have_keyboard && seat->wl_keyboard != NULL) {
          wl_keyboard_release(seat->wl_keyboard);
          seat->wl_keyboard = NULL;
  }
}

static void seat_name(void *data, struct wl_seat *wl_seat, const char *name) {
  struct seat *seat = (struct seat*)data;
  seat->name = strdup(name);
}

static struct wl_seat_listener seat_listener = {
  seat_capabilities,
  seat_name
};

static void output_geometry(void *data,
    struct wl_output *wl_output,
    int32_t x,
    int32_t y,
    int32_t physical_width,
    int32_t physical_height,
    int32_t subpixel,
    const char *make,
    const char *model,
    int32_t transform)
{
  //fprintf(stderr, "output_geometry: x=%d y=%d physical=%dx%d\n",x,y,physical_width,physical_height);
}

static void output_mode(void *data, struct wl_output *wl_output, uint32_t flags,
      int32_t width, int32_t height, int32_t refresh)
{
  Fl_Wayland_Screen_Driver::output *output;
  bool found = false;
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  wl_list_for_each(output, &(scr_driver->outputs), link) { // all screens
    if (output->wl_output == wl_output) { // the screen involved in this call
      for (int i = 0; i < Fl::screen_count(); i++) {
        scr_driver->screens[i].x_org = 0;
        scr_driver->screens[i].y_org = 0;
        scr_driver->screens[i].width = width;
        scr_driver->screens[i].height = height;
        found = true;
      }
    }
  }
  if (!found) {
    int count = Fl::screen_count();
    if (count < 0) count = 0;
    scr_driver->screens[count].x_org = 0;
    scr_driver->screens[count].y_org = 0;
    scr_driver->screens[count].width = width;
    scr_driver->screens[count].height = height;
    scr_driver->screens[count].scale = 1.f;
    scr_driver->screen_count(count+1);
//fprintf(stderr, "output_mode: screen_count()=%d width=%d,height=%d\n",Fl::screen_count(),width,height);
  }
}

static void output_done(void *data, struct wl_output *wl_output)
{//TODO to be verified
  Fl_Wayland_Screen_Driver::output *output = (Fl_Wayland_Screen_Driver::output*)data;
  Fl_Wayland_Window_Driver::window_output *window_output;
  struct seat *seat;
//fprintf(stderr, "output_done output=%p\n",output);
  Fl_X *xp = Fl_X::first;
  while (xp) { // all mapped windows
    struct wld_window *win = xp->xid;
    wl_list_for_each(window_output, &(win->outputs), link) { // all Fl_Wayland_Window_Driver::window_output for this window
      if (window_output->output == output) {
        Fl_Wayland_Window_Driver *win_driver = (Fl_Wayland_Window_Driver*)Fl_Window_Driver::driver(win->fl_win);
        if (output->scale != win->scale) win_driver->update_scale();
      }
    }
    xp = xp->next;
  }

  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  wl_list_for_each(seat, &(scr_driver->seats), link) {
    try_update_cursor(seat);
  }
  scr_driver->init_workarea();
}


static void output_scale(void *data,
       struct wl_output *wl_output,
       int32_t factor)
{
  Fl_Wayland_Screen_Driver::output *output = (Fl_Wayland_Screen_Driver::output*)data;
  output->scale = factor;
}


static struct wl_output_listener output_listener = {
  output_geometry,
  output_mode,
  output_done,
  output_scale
};


static void registry_handle_global(void *user_data, struct wl_registry *wl_registry,
           uint32_t id, const char *interface, uint32_t version) {
//fprintf(stderr, "interface=%s\n", interface);
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  if (strcmp(interface, "wl_compositor") == 0) {
    if (version < 4) {
      fprintf(stderr, "wl_compositor version >= 4 required");
      exit(EXIT_FAILURE);
    }
    scr_driver->wl_compositor = (struct wl_compositor*)wl_registry_bind(wl_registry,
           id, &wl_compositor_interface, 4);
    
  } else if (strcmp(interface, "wl_subcompositor") == 0) {
    scr_driver->wl_subcompositor = (struct wl_subcompositor*)wl_registry_bind(wl_registry,
           id, &wl_subcompositor_interface, 1);    
    
  } else if (strcmp(interface, "wl_shm") == 0) {
    scr_driver->wl_shm = (struct wl_shm*)wl_registry_bind(wl_registry,
            id, &wl_shm_interface, 1);
    wl_shm_add_listener(scr_driver->wl_shm, &shm_listener, NULL);
    
  } else if (strcmp(interface, "wl_seat") == 0) {
    if (version < 3) {
      fprintf(stderr, "%s version 3 required but only version "
          "%i is available\n", interface, version);
      exit(EXIT_FAILURE);
    }
    if (!scr_driver->seat) scr_driver->seat = (struct seat*)calloc(1, sizeof(struct seat));
//fprintf(stderr, "registry_handle_global: seat=%p\n", scr_driver->seat);
    wl_list_init(&scr_driver->seat->pointer_outputs);
    scr_driver->seat->wl_seat = (wl_seat*)wl_registry_bind(wl_registry, id, &wl_seat_interface, 3);
    scr_driver->seat->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    const char *locale = getenv("LC_ALL");
    if (!locale || !*locale)
      locale = getenv("LC_CTYPE");
    if (!locale || !*locale)
      locale = getenv("LANG");
    if (!locale || !*locale)
      locale = "C";
    struct xkb_compose_table *table = xkb_compose_table_new_from_locale(scr_driver->seat->xkb_context, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
    scr_driver->seat->xkb_compose_state = xkb_compose_state_new(table, XKB_COMPOSE_STATE_NO_FLAGS);
    wl_seat_add_listener(scr_driver->seat->wl_seat, &seat_listener, scr_driver->seat);
    if (scr_driver->seat->data_device_manager) {
      scr_driver->seat->data_device = wl_data_device_manager_get_data_device(scr_driver->seat->data_device_manager, scr_driver->seat->wl_seat);
      wl_data_device_add_listener(scr_driver->seat->data_device, Fl_Wayland_Screen_Driver::p_data_device_listener, NULL);
    }
    
  } else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
    if (!scr_driver->seat) scr_driver->seat = (struct seat*)calloc(1, sizeof(struct seat));
    scr_driver->seat->data_device_manager = (struct wl_data_device_manager*)wl_registry_bind(wl_registry, id, &wl_data_device_manager_interface, 3);
    if (scr_driver->seat->wl_seat) {scr_driver->seat->data_device = wl_data_device_manager_get_data_device(scr_driver->seat->data_device_manager, scr_driver->seat->wl_seat);
      wl_data_device_add_listener(scr_driver->seat->data_device, Fl_Wayland_Screen_Driver::p_data_device_listener, NULL);
    }
//fprintf(stderr, "registry_handle_global: %s\n", interface);
    
  } else if (strcmp(interface, "wl_output") == 0) {
    if (version < 2) {
      fprintf(stderr, "%s version 3 required but only version "
          "%i is available\n", interface, version);
      exit(EXIT_FAILURE);
    }
    Fl_Wayland_Screen_Driver::output *output = (Fl_Wayland_Screen_Driver::output*)calloc(1, sizeof *output);
    output->id = id;
    output->scale = 1;
    output->wl_output = (struct wl_output*)wl_registry_bind(wl_registry,
                 id, &wl_output_interface, 2);
//fprintf(stderr, "wl_output: id=%d wl_output=%p\n", id, output->wl_output);
    wl_proxy_set_tag((struct wl_proxy *) output->wl_output, &proxy_tag);
    wl_output_add_listener(output->wl_output, &output_listener, output);
    wl_list_insert(&(scr_driver->outputs), &output->link);
    
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
//fprintf(stderr, "registry_handle_global interface=%s\n", interface);
    scr_driver->xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(wl_registry, id, &xdg_wm_base_interface, 1);
      xdg_wm_base_add_listener(scr_driver->xdg_wm_base, &xdg_wm_base_listener, NULL);
  }
}


static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{//TODO to be tested
  Fl_Wayland_Screen_Driver::output *output;
  Fl_Wayland_Window_Driver::window_output *window_output;
fprintf(stderr, "registry_handle_global_remove data=%p id=%u\n", data, name);
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  wl_list_for_each(output, &(scr_driver->outputs), link) { // all screens of the system
    if (output->id == name) { // the screen being removed
      Fl_X *xp = Fl_X::first;
      while (xp) { // all mapped windows
        struct wld_window *win = xp->xid;
        wl_list_for_each(window_output, &(win->outputs), link) { // all Fl_Wayland_Window_Driver::window_output for this window
          if (window_output->output == output) {
            wl_list_remove(&window_output->link);
            free(window_output);
          }
        }
        xp = xp->next;
      }
      wl_list_remove(&output->link);
      wl_output_destroy(output->wl_output);
      free(output);
      break;
    }
  }
}


static const struct wl_registry_listener registry_listener = {
  registry_handle_global,
  registry_handle_global_remove
};


static void fd_callback(int unused, struct wl_display *display) {
  wl_display_dispatch(display);
}


Fl_Wayland_Screen_Driver::Fl_Wayland_Screen_Driver() : Fl_Screen_Driver() {
  libdecor_context = NULL;
  seat = NULL;
  reset_cursor();
}

void Fl_Wayland_Screen_Driver::open_display_platform() {
  struct wl_display *wl_display;
  struct wl_registry *wl_registry;
  
  static bool beenHereDoneThat = false;
  if (beenHereDoneThat)
    return;

  beenHereDoneThat = true;
  wl_display = wl_display_connect(NULL);
  if (!wl_display) {
    fprintf(stderr, "No Wayland connection\n");
    exit(EXIT_FAILURE);
  }
  fl_display = wl_display;
  wl_list_init(&seats);
  wl_list_init(&outputs);

  wl_registry = wl_display_get_registry(wl_display);
  wl_registry_add_listener(wl_registry, &registry_listener, NULL);
  wl_display_dispatch(wl_display);
  wl_display_roundtrip(wl_display);
  if (!has_xrgb) {
    fprintf(stderr, "No WL_SHM_FORMAT_ARGB8888 shm format\n");
    exit( EXIT_FAILURE);
  }
  Fl::add_fd(wl_display_get_fd(wl_display), FL_READ, (Fl_FD_Handler)fd_callback, wl_display);
}

void Fl_Wayland_Screen_Driver::close_display() {
  Fl::remove_fd(wl_display_get_fd(fl_display));
  wl_display_disconnect(fl_display);
}


static int fl_workarea_xywh[4] = { -1, -1, -1, -1 };


void Fl_Wayland_Screen_Driver::init_workarea()
{
    fl_workarea_xywh[0] = 0;
    fl_workarea_xywh[1] = 0;
    fl_workarea_xywh[2] = screens[0].width;
    fl_workarea_xywh[3] = screens[0].height;
}


int Fl_Wayland_Screen_Driver::x() {
  if (!fl_display) open_display();
  return fl_workarea_xywh[0]
#if USE_XFT
  / screens[0].scale
#endif
  ;
}

int Fl_Wayland_Screen_Driver::y() {
  if (!fl_display) open_display();
  return fl_workarea_xywh[1]
#if USE_XFT
  / screens[0].scale
#endif
  ;
}

int Fl_Wayland_Screen_Driver::w() {
  if (!fl_display) open_display();
  return fl_workarea_xywh[2]
#if USE_XFT
      / screens[0].scale
#endif
  ;
}

int Fl_Wayland_Screen_Driver::h() {
  if (!fl_display) open_display();
  return fl_workarea_xywh[3]
#if USE_XFT
  / screens[0].scale
#endif
  ;
}


void Fl_Wayland_Screen_Driver::init() {
  if (!fl_display) open_display();
}


void Fl_Wayland_Screen_Driver::screen_work_area(int &X, int &Y, int &W, int &H, int n)
{
  if (num_screens < 0) init();
  if (n < 0 || n >= num_screens) n = 0;
  if (n == 0) { // for the main screen, these return the work area
    X = Fl::x();
    Y = Fl::y();
    W = Fl::w();
    H = Fl::h();
  } else { // for other screens, work area is full screen,
    screen_xywh(X, Y, W, H, n);
  }
}


void Fl_Wayland_Screen_Driver::screen_xywh(int &X, int &Y, int &W, int &H, int n)
{
  if (num_screens < 0) init();

  if ((n < 0) || (n >= num_screens))
    n = 0;

  if (num_screens > 0) {
#if USE_XFT
    float s = screens[n].scale;
#else
    float s = 1;
#endif
    X = screens[n].x_org / s;
    Y = screens[n].y_org / s;
    W = screens[n].width / s;
    H = screens[n].height / s;
  }
}


void Fl_Wayland_Screen_Driver::screen_dpi(float &h, float &v, int n)
{
  if (num_screens < 0) init();
  h = v = 0.0f;

  if (n >= 0 && n < num_screens) {
    h = dpi[n][0];
    v = dpi[n][1];
  }
}


void Fl_Wayland_Screen_Driver::beep(int type)
{ //TODO
  switch (type) {
    case FL_BEEP_DEFAULT :
    case FL_BEEP_ERROR :
      if (!fl_display) open_display();
      //XBell(fl_display, 100);
      break;
    default :
      if (!fl_display) open_display();
      //XBell(fl_display, 50);
      break;
  }
}


void Fl_Wayland_Screen_Driver::flush()
{
  if (fl_display) {
    wl_display_flush(fl_display);
  }
}


double Fl_Wayland_Screen_Driver::wait(double time_to_wait)
{
  static char in_idle;

  if (first_timeout) {
    elapse_timeouts();
    Timeout *t;
    while ((t = first_timeout)) {
      if (t->time > 0) break;
      // The first timeout in the array has expired.
      missed_timeout_by = t->time;
      // We must remove timeout from array before doing the callback:
      void (*cb)(void*) = t->cb;
      void *argp = t->arg;
      first_timeout = t->next;
      t->next = free_timeout;
      free_timeout = t;
      // Now it is safe for the callback to do add_timeout:
      cb(argp);
    }
  } else {
    reset_clock = 1; // we are not going to check the clock
  }
  Fl::run_checks();
  if (Fl::idle) {
    if (!in_idle) {
      in_idle = 1;
      Fl::idle();
      in_idle = 0;
    }
    // the idle function may turn off idle, we can then wait:
    if (Fl::idle) time_to_wait = 0.0;
  }
  if (first_timeout && first_timeout->time < time_to_wait)
    time_to_wait = first_timeout->time;
//fprintf(stderr,"time_to_wait=%g\n", time_to_wait);
  if (time_to_wait <= 0.0) {
    // do flush second so that the results of events are visible:
    int ret = this->poll_or_select_with_delay(0.0);
    Fl::flush();
    return ret;
  } else {
    // do flush first so that user sees the display:
    Fl::flush();
    if (Fl::idle && !in_idle) // 'idle' may have been set within flush()
      time_to_wait = 0.0;
    else if (first_timeout && first_timeout->time < time_to_wait) {
      // another timeout may have been queued within flush(), see STR #3188
      time_to_wait = first_timeout->time >= 0.0 ? first_timeout->time : 0.0;
    }
    return this->poll_or_select_with_delay(time_to_wait);
  }
}


int Fl_Wayland_Screen_Driver::ready()
{
  if (first_timeout) {
    elapse_timeouts();
    if (first_timeout->time <= 0) return 1;
  } else {
    reset_clock = 1;
  }
  return this->poll_or_select();
}


extern void fl_fix_focus(); // in Fl.cxx


void Fl_Wayland_Screen_Driver::grab(Fl_Window* win)
{
  Fl_Window *fullscreen_win = NULL;
  for (Fl_Window *W = Fl::first_window(); W; W = Fl::next_window(W)) {
    if (W->fullscreen_active()) {
      fullscreen_win = W;
      break;
    }
  }
  if (win) {
    if (!Fl::grab()) {
    }
    Fl::grab_ = win;    // FIXME: Fl::grab_ "should be private", but we need
                        // a way to *set* the variable from the driver!
  } else {
    if (Fl::grab()) {
      // We must keep the grab in the non-EWMH fullscreen case
      if (!fullscreen_win ) {
        //XUngrabKeyboard(fl_display, fl_event_time);
      }
      //XUngrabPointer(fl_display, fl_event_time);
      // this flush is done in case the picked menu item goes into
      // an infinite loop, so we don't leave the X server locked up:
      //XFlush(fl_display);
      Fl::grab_ = 0;    // FIXME: Fl::grab_ "should be private", but we need
                        // a way to *set* the variable from the driver!
      fl_fix_focus();
    }
  }
}


static void set_selection_color(uchar r, uchar g, uchar b)
{
  Fl::set_color(FL_SELECTION_COLOR,r,g,b);
}

static void getsyscolor(const char *key1, const char* key2, const char *arg, const char *defarg, void (*func)(uchar,uchar,uchar))
{
}


void Fl_Wayland_Screen_Driver::get_system_colors()
{
  open_display();
  const char* key1 = 0;
  if (Fl::first_window()) key1 = Fl::first_window()->xclass();
  if (!key1) key1 = "fltk";
  if (!bg2_set)
    getsyscolor("Text","background",    fl_bg2, "#ffffff", Fl::background2);
  if (!fg_set)
    getsyscolor(key1,  "foreground",    fl_fg,  "#000000", Fl::foreground);
  if (!bg_set)
    getsyscolor(key1,  "background",    fl_bg,  "#c0c0c0", Fl::background);
  getsyscolor("Text", "selectBackground", 0, "#000080", set_selection_color);
}


const char *Fl_Wayland_Screen_Driver::get_system_scheme()
{
  const char *s = 0L;
  /*if ((s = fl_getenv("FLTK_SCHEME")) == NULL) {
    const char* key = 0;
    if (Fl::first_window()) key = Fl::first_window()->xclass();
    if (!key) key = "fltk";
    open_display();
    s = XGetDefault(fl_display, key, "scheme");
  }*/
  return s;
}


void Fl_Wayland_Screen_Driver::add_timeout(double time, Fl_Timeout_Handler cb, void *argp) {
  elapse_timeouts();
  missed_timeout_by = 0;
  repeat_timeout(time, cb, argp);
}

void Fl_Wayland_Screen_Driver::repeat_timeout(double time, Fl_Timeout_Handler cb, void *argp) {
  time += missed_timeout_by; if (time < -.05) time = 0;
  Timeout* t = free_timeout;
  if (t) {
      free_timeout = t->next;
  } else {
      t = new Timeout;
  }
  t->time = time;
  t->cb = cb;
  t->arg = argp;
  // insert-sort the new timeout:
  Timeout** p = &first_timeout;
  while (*p && (*p)->time <= time) p = &((*p)->next);
  t->next = *p;
  *p = t;
}

/**
  Returns true if the timeout exists and has not been called yet.
*/
int Fl_Wayland_Screen_Driver::has_timeout(Fl_Timeout_Handler cb, void *argp) {
  for (Timeout* t = first_timeout; t; t = t->next)
    if (t->cb == cb && t->arg == argp) return 1;
  return 0;
}

/**
  Removes a timeout callback. It is harmless to remove a timeout
  callback that no longer exists.

  \note This version removes all matching timeouts, not just the first one.
        This may change in the future.
*/
void Fl_Wayland_Screen_Driver::remove_timeout(Fl_Timeout_Handler cb, void *argp) {
  for (Timeout** p = &first_timeout; *p;) {
    Timeout* t = *p;
    if (t->cb == cb && (t->arg == argp || !argp)) {
      *p = t->next;
      t->next = free_timeout;
      free_timeout = t;
    } else {
      p = &(t->next);
    }
  }
}


int Fl_Wayland_Screen_Driver::text_display_can_leak() {
#if USE_XFT
  return 1;
#else
  return 0;
#endif
}


Fl_RGB_Image *Fl_Wayland_Screen_Driver::read_win_rectangle(int X, int Y, int w, int h, Fl_Window *win,
                                                           bool ignore, bool *p_ignore) {
  Window xid = win ? fl_xid(win) : NULL;
  struct buffer *buffer = win ? xid->buffer : (Fl_Offscreen)Fl_Surface_Device::surface()->driver()->gc();
  int s = win ? xid->scale : 1; //TODO: check when win is NULL
  if (s != 1) {
    X *= s; Y *= s; w *= s; h *= s;
  }
  uchar *data = new uchar[w * h * 3];
  uchar *p = data, *q;
  for (int j = 0; j < h; j++) {
    q = buffer->draw_buffer + (j+Y) * buffer->stride + 4 * X;
    for (int i = 0; i < w; i++) {
      *p++ = *(q+2); // R
      *p++ = *(q+1); // G
      *p++ = *q;     // B
      q += 4;
    }
  }
  Fl_RGB_Image *rgb = new Fl_RGB_Image(data, w, h, 3);
  rgb->alloc_array = 1;
  return rgb;
}


void Fl_Wayland_Screen_Driver::offscreen_size(Fl_Offscreen off, int &width, int &height)
{
  width = off->width;
  height = off->data_size / off->stride;
}

#if USE_XFT
//NOTICE: returns -1 if x,y is not in any screen
int Fl_Wayland_Screen_Driver::screen_num_unscaled(int x, int y)
{
  int screen = -1;
  if (num_screens < 0) init();

  for (int i = 0; i < num_screens; i ++) {
    int sx = screens[i].x_org, sy = screens[i].y_org, sw = screens[i].width, sh = screens[i].height;
    if ((x >= sx) && (x < (sx+sw)) && (y >= sy) && (y < (sy+sh))) {
      screen = i;
      break;
    }
  }
  return screen;
}


// set the desktop's default scaling value
void Fl_Wayland_Screen_Driver::desktop_scale_factor()
{
}

#endif // USE_XFT

void Fl_Wayland_Screen_Driver::set_cursor() {
  do_set_cursor(seat);
}

struct wl_cursor *Fl_Wayland_Screen_Driver::default_cursor() {
  return seat->default_cursor;
}

void Fl_Wayland_Screen_Driver::default_cursor(struct wl_cursor *cursor) {
  seat->default_cursor = cursor;
}

struct wl_cursor *Fl_Wayland_Screen_Driver::cache_cursor(const char *cursor_name) {
  return wl_cursor_theme_get_cursor(seat->cursor_theme, cursor_name);
}

void Fl_Wayland_Screen_Driver::reset_cursor() {
  xc_arrow = xc_ns = xc_wait = xc_insert = xc_hand = xc_help = xc_cross = xc_move = xc_north = xc_south = xc_west = xc_east = xc_we = xc_nesw = xc_nwse = xc_sw = xc_se = xc_ne = xc_nw = NULL;
}

/*uint32_t Fl_Wayland_Screen_Driver::get_serial() {
  return seat->serial;
}

struct wl_seat*Fl_Wayland_Screen_Driver::get_wl_seat() {
  return seat->wl_seat;
}

char *Fl_Wayland_Screen_Driver::get_seat_name() {
  return seat->name;
}*/

struct xkb_keymap *Fl_Wayland_Screen_Driver::get_xkb_keymap() {
  return seat->xkb_keymap;
}
