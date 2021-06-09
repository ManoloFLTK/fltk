//
// Implementation of the Wayland graphics driver.
//
// Copyright 2021 by Bill Spitzak and others.
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
#include "Fl_Wayland_Graphics_Driver.H"
#include "Fl_Wayland_Screen_Driver.H"
#include "Fl_Wayland_Window_Driver.H"
#include "Fl_Font.H"
#include <pango/pangocairo.h>
#if ! PANGO_VERSION_CHECK(1,22,0)
#  error "Requires Pango 1.22 or higher"
#endif
#define _GNU_SOURCE 1
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

extern unsigned fl_cmap[256]; // defined in fl_color.cxx


static int create_anonymous_file(off_t size)
{
  int ret;
  int fd = memfd_create("FLTK-for-Wayland", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) return -1;
  fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK);
  do {
    ret = posix_fallocate(fd, 0, size);
  } while (ret == EINTR);
  if (ret != 0) {
    close(fd);
    errno = ret;
    return -1;
  }
  return fd;
}


static void buffer_gets_ready(void *user_data, struct wl_buffer *unused)
{
  struct wld_window *window = (struct wld_window *)user_data;
  window->buffer->wl_buffer_ready = true;
//fprintf(stderr, "buffer_gets_ready: needs_commit=%d\n", window->buffer->draw_buffer_needs_commit);
  if (window->buffer->draw_buffer_needs_commit) {
    Fl_Wayland_Graphics_Driver::buffer_commit(window);
  }
}

static const struct wl_buffer_listener buffer_listener = {
  buffer_gets_ready
};


struct buffer *Fl_Wayland_Graphics_Driver::create_shm_buffer(int width, int height, uint32_t format, struct wld_window *window)
{
  struct buffer *buffer;

  int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);
  int size = stride * height;
  int fd = create_anonymous_file(size);
  if (fd < 0) {
    fprintf(stderr, "creating a buffer file for %d B failed: %s\n",
      size, strerror(errno));
    return NULL;
  }
  void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    fprintf(stderr, "mmap failed: %s\n", strerror(errno));
    close(fd);
    return NULL;
  }
  buffer = (struct buffer*)calloc(1, sizeof *buffer);
  buffer->stride = stride;
  Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
  struct wl_shm_pool *pool = wl_shm_create_pool(scr_driver->wl_shm, fd, size);
  buffer->wl_buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
  wl_buffer_add_listener(buffer->wl_buffer, &buffer_listener, window);
  wl_shm_pool_destroy(pool);
  close(fd);
  buffer->data = data;
  buffer->data_size = size;
  buffer->width = width;
  buffer->draw_buffer = new uchar[buffer->data_size];
  buffer->wl_buffer_ready = true;
  buffer->draw_buffer_needs_commit = false;
//fprintf(stderr, "create_shm_buffer: %dx%d\n", width, height);
  cairo_init(buffer, width, height, stride);
  return buffer;
}


void Fl_Wayland_Graphics_Driver::buffer_commit(struct wld_window *window) {
  cairo_surface_t *surf = cairo_get_target(window->buffer->cairo_);
  cairo_surface_flush(surf);
  memcpy(window->buffer->data, window->buffer->draw_buffer, window->buffer->data_size);
  wl_surface_attach(window->wl_surface, window->buffer->wl_buffer, 0, 0);
  wl_surface_set_buffer_scale(window->wl_surface, window->scale);
  wl_surface_commit(window->wl_surface);
  window->buffer->draw_buffer_needs_commit = false;
  window->buffer->wl_buffer_ready = false;
}


void Fl_Wayland_Graphics_Driver::cairo_init(struct buffer *buffer, int width, int height, int stride) {
  cairo_surface_t *surf = cairo_image_surface_create_for_data (buffer->draw_buffer, CAIRO_FORMAT_ARGB32,
                                                        width, height, stride);
  if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
    fprintf(stderr, "Can't create Cairo surface\n");
    return;
  }
  buffer->cairo_ = cairo_create(surf);
  cairo_status_t err;
  if ((err = cairo_status(buffer->cairo_)) != CAIRO_STATUS_SUCCESS) {
    fprintf(stderr, "Cairo error on create %s\n", cairo_status_to_string(err));
    return;
  }
  cairo_set_source_rgba(buffer->cairo_, 1.0, 1.0, 1.0, 0.);
  cairo_paint(buffer->cairo_);
  cairo_set_source_rgba(buffer->cairo_, .0, .0, .0, 1.0); // Black default color
  buffer->pango_layout_ = pango_cairo_create_layout(buffer->cairo_);
  cairo_save(buffer->cairo_);
}


void Fl_Wayland_Graphics_Driver::buffer_release(struct wld_window *window)
{
  if (window->buffer) {
    wl_buffer_destroy(window->buffer->wl_buffer);
    munmap(window->buffer->data, window->buffer->data_size);
    delete[] window->buffer->draw_buffer;
    window->buffer->draw_buffer = NULL;
    cairo_surface_t *surf = cairo_get_target(window->buffer->cairo_);
    cairo_destroy(window->buffer->cairo_);
    cairo_surface_destroy(surf);
    g_object_unref(window->buffer->pango_layout_);
    free(window->buffer);
    window->buffer = NULL;
  }
}


Fl_Wayland_Graphics_Driver::Fl_Wayland_Graphics_Driver () : Fl_Cairo_Graphics_Driver() {
  clip_ = NULL;
  dummy_pango_layout_ = NULL;
  pango_layout_ = NULL;
}


Fl_Graphics_Driver *Fl_Graphics_Driver::newMainGraphicsDriver()
{
  fl_graphics_driver = new Fl_Wayland_Graphics_Driver();
  return fl_graphics_driver;
}


Fl_Wayland_Graphics_Driver::~Fl_Wayland_Graphics_Driver() {
  if (pango_layout_) g_object_unref(pango_layout_);
}


void Fl_Wayland_Graphics_Driver::activate(struct buffer *buffer, int scale) {
  if (dummy_pango_layout_) {
    cairo_surface_t *surf = cairo_get_target(cairo_);
    cairo_destroy(cairo_);
    cairo_surface_destroy(surf);
    g_object_unref(dummy_pango_layout_);
    dummy_pango_layout_ = NULL;
    pango_layout_ = NULL;
  }
  cairo_ = buffer->cairo_;
  if (pango_layout_ != buffer->pango_layout_) {
    if (pango_layout_) g_object_unref(pango_layout_);
    pango_layout_ = buffer->pango_layout_;
    g_object_ref(pango_layout_);
    Fl_Graphics_Driver::font(-1, -1); // signal that no font is current yet
  }
  this->buffer = buffer;
  cairo_restore(cairo_);
  cairo_save(cairo_);
  cairo_scale(cairo_, scale, scale);
  cairo_translate(cairo_, 0.5, 0.5);
  line_style(0);
}


static Fl_Fontdesc built_in_table[] = {  // Pango font names
  {"Sans"},
  {"Sans Bold"},
  {"Sans Italic"},
  {"Sans Bold Italic"},
  {"Monospace"},
  {"Monospace Bold"},
  {"Monospace Italic"},
  {"Monospace Bold Italic"},
  {"Serif"},
  {"Serif Bold"},
  {"Serif Italic"},
  {"Serif Bold Italic"},
  {"Standard Symbols PS"}, // FL_SYMBOL
  {"Monospace"},           // FL_SCREEN
  {"Monospace Bold"},      // FL_SCREEN_BOLD
  {"D050000L"},            // FL_ZAPF_DINGBATS
};

FL_EXPORT Fl_Fontdesc *fl_fonts = built_in_table;


static Fl_Font_Descriptor* find(Fl_Font fnum, Fl_Fontsize size) {
  Fl_Fontdesc* s = fl_fonts+fnum;
  if (!s->name) s = fl_fonts; // use 0 if fnum undefined
  Fl_Font_Descriptor* f;
  for (f = s->first; f; f = f->next)
    if (f->size == size) return f;
  f = new Fl_Wayland_Font_Descriptor(s->name, size);
  f->next = s->first;
  s->first = f;
  return f;
}


Fl_Wayland_Font_Descriptor::Fl_Wayland_Font_Descriptor(const char* name, Fl_Fontsize size) : Fl_Font_Descriptor(name, size) {
  char string[70];
  strcpy(string, name);
  sprintf(string + strlen(string), " %d", int(size * 0.7 + 0.5) ); // why reduce size?
  fontref = pango_font_description_from_string(string);
  width = NULL;
  static PangoFontMap *def_font_map = pango_cairo_font_map_get_default(); // 1.10
  static PangoContext *pango_context = pango_font_map_create_context(def_font_map); // 1.22
  static PangoLanguage *language = pango_language_get_default(); // 1.16
  PangoFontset *fontset = pango_font_map_load_fontset(def_font_map, pango_context, fontref, language);
  PangoFontMetrics *metrics = pango_fontset_get_metrics(fontset);
  ascent = pango_font_metrics_get_ascent(metrics)/PANGO_SCALE;
  descent = pango_font_metrics_get_descent(metrics)/PANGO_SCALE;
  q_width = pango_font_metrics_get_approximate_char_width(metrics)/PANGO_SCALE;
  pango_font_metrics_unref(metrics);
  g_object_unref(fontset);
//fprintf(stderr, "[%s](%d) ascent=%d descent=%d q_width=%d\n", name, size, ascent, descent, q_width);
}


Fl_Wayland_Font_Descriptor::~Fl_Wayland_Font_Descriptor() {
  pango_font_description_free(fontref);
  if (width) {
    for (int i = 0; i < 64; i++) delete[] width[i];
  }
  delete[] width;
}


int Fl_Wayland_Graphics_Driver::height() {
  return (font_descriptor()->ascent + font_descriptor()->descent)*1.1  /*1.15 scale=1*/;
}


int Fl_Wayland_Graphics_Driver::descent() {
  return font_descriptor()->descent;
}


void Fl_Wayland_Graphics_Driver::font(Fl_Font fnum, Fl_Fontsize s) {
  if (font() == fnum && size() == s) return;
  if (!font_descriptor()) fl_open_display();
  if (!pango_layout_) {
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 100);
    cairo_ = cairo_create(surf);
    dummy_pango_layout_ = pango_cairo_create_layout(cairo_);
    pango_layout_ = dummy_pango_layout_;
  }
  if (fnum == -1) {
    Fl_Graphics_Driver::font(0, 0);
    return;
  }
  Fl_Graphics_Driver::font(fnum, s);
  font_descriptor( find(fnum, s) );
  pango_layout_set_font_description(pango_layout_, ((Fl_Wayland_Font_Descriptor*)font_descriptor())->fontref);
}


static int font_name_process(const char *name, char &face) {
  int l = strlen(name);
  face = ' ';
  if (!memcmp(name + l - 8, " Regular", 8)) l -= 8;
  else if (!memcmp(name + l - 6, " Plain", 6)) l -= 6;
  else if (!memcmp(name + l - 12, " Bold Italic", 12)) {l -= 12; face='P';}
  else if (!memcmp(name + l - 7, " Italic", 7)) {l -= 7; face='I';}
  else if (!memcmp(name + l - 5, " Bold", 5)) {l -= 5; face='B';}
  return l;
}

typedef int (*sort_f_type)(const void *aa, const void *bb);


static int font_sort(Fl_Fontdesc *fa, Fl_Fontdesc *fb) {
  char face_a, face_b;
  int la = font_name_process(fa->name, face_a);
  int lb = font_name_process(fb->name, face_b);
  int c = strncasecmp(fa->name, fb->name, la >= lb ? lb : la);
  return (c == 0 ? face_a - face_b : c);
}


Fl_Font Fl_Wayland_Graphics_Driver::set_fonts(const char* pattern_name)
{
  fl_open_display();
  int n_families, count = 0;
  PangoFontFamily **families;
  static PangoFontMap *pfmap_ = pango_cairo_font_map_get_default(); // 1.10
  Fl_Wayland_Graphics_Driver::init_built_in_fonts();
  pango_font_map_list_families(pfmap_, &families, &n_families);
  for (int fam = 0; fam < n_families; fam++) {
    PangoFontFace **faces;
    int n_faces;
    const char *fam_name = pango_font_family_get_name (families[fam]);
    int l = strlen(fam_name);
    pango_font_family_list_faces(families[fam], &faces, &n_faces);
    for (int j = 0; j < n_faces; j++) {
      const char *p = pango_font_face_get_face_name(faces[j]);
      // build the font's FLTK name
      l += strlen(p) + 2;
      char *q = new char[l];
      sprintf(q, "%s %s", fam_name, p);
      Fl::set_font((Fl_Font)(count++ + FL_FREE_FONT), q);
    }
    /*g_*/free(faces); // glib source code shows that g_free is equivalent to free
  }
  /*g_*/free(families);
  // Sort the list into alphabetic order
  qsort(fl_fonts + FL_FREE_FONT, count, sizeof(Fl_Fontdesc), (sort_f_type)font_sort);
  return FL_FREE_FONT + count;
}


void Fl_Wayland_Graphics_Driver::init_built_in_fonts() {
  static int i = 0;
  if (!i) {
    while (i < FL_FREE_FONT) {
      i++;
      Fl::set_font((Fl_Font)i-1, built_in_table[i-1].name);
    }
  }
}


const char *Fl_Wayland_Graphics_Driver::font_name(int num) {
  return fl_fonts[num].name;
}


void Fl_Wayland_Graphics_Driver::font_name(int num, const char *name) {
  Fl_Fontdesc *s = fl_fonts + num;
  if (s->name) {
    if (!strcmp(s->name, name)) {s->name = name; return;}
    for (Fl_Font_Descriptor* f = s->first; f;) {
      Fl_Font_Descriptor* n = f->next; delete f; f = n;
    }
    s->first = 0;
  }
  s->name = name;
  s->fontname[0] = 0;
  s->first = 0;
}

#define ENDOFBUFFER  sizeof(fl_fonts->fontname)-1

// turn a stored font name into a pretty name:
const char* Fl_Wayland_Graphics_Driver::get_font_name(Fl_Font fnum, int* ap) {
  Fl_Fontdesc *f = fl_fonts + fnum;
  if (!f->fontname[0]) {
    strcpy(f->fontname, f->name); // to check
    const char* thisFont = f->name;
    if (!thisFont || !*thisFont) {if (ap) *ap = 0; return "";}
    int type = 0;
    if (strstr(f->name, "Bold")) type |= FL_BOLD;
    if (strstr(f->name, "Italic") || strstr(f->name, "Oblique")) type |= FL_ITALIC;
    f->fontname[ENDOFBUFFER] = (char)type;
  }
  if (ap) *ap = f->fontname[ENDOFBUFFER];
  return f->fontname;
}


int Fl_Wayland_Graphics_Driver::get_font_sizes(Fl_Font fnum, int*& sizep) {
  static int array[128];
  if (!fl_fonts) fl_fonts = calc_fl_fonts();
  Fl_Fontdesc *s = fl_fonts+fnum;
  if (!s->name) s = fl_fonts; // empty slot in table, use entry 0
  int cnt = 0;

  array[0] = 0;
  sizep = array;
  cnt = 1;

  return cnt;
}


void Fl_Wayland_Graphics_Driver::draw(const char* str, int n, float x, float y) {
  if (!n) return;
  cairo_save(cairo_);
  cairo_translate(cairo_, x, y - height() + descent() -1);
  pango_layout_set_text(pango_layout_, str, n);
  pango_cairo_show_layout(cairo_, pango_layout_);
  cairo_restore(cairo_);
  buffer->draw_buffer_needs_commit = true;
}


void Fl_Wayland_Graphics_Driver::draw(int rotation, const char *str, int n, int x, int y)
{
  cairo_save(cairo_);
  cairo_translate(cairo_, x, y);
  cairo_rotate(cairo_, -rotation * M_PI / 180);
  this->draw(str, n, 0, 0);
  cairo_restore(cairo_);
}


void Fl_Wayland_Graphics_Driver::rtl_draw(const char* str, int n, int x, int y) {
  int w = (int)width(str, n);
  draw(str, n, x - w, y);
}


double Fl_Wayland_Graphics_Driver::width(const char* c, int n) {
  if (!font_descriptor()) return -1.0;
  int i = 0, w = 0, l;
  const char *end = c + n;
  unsigned int ucs;
  while (i < n) {
    ucs = fl_utf8decode(c + i, end, &l);
    i += l;
    w += width(ucs);
  }
  return (double)w;
}


double Fl_Wayland_Graphics_Driver::width(unsigned int c) {
  unsigned int r = 0;
  Fl_Wayland_Font_Descriptor *desc = NULL;
  if (c <= 0xFFFF) { // when inside basic multilingual plane
    desc = (Fl_Wayland_Font_Descriptor*)font_descriptor();
    r = (c & 0xFC00) >> 10;
    if (!desc->width) {
      desc->width = (int**)new int*[64];
      memset(desc->width, 0, 64*sizeof(int*));
    }
    if (!desc->width[r]) {
      desc->width[r] = (int*)new int[0x0400];
      for (int i = 0; i < 0x0400; i++) desc->width[r][i] = -1;
    } else {
      if ( desc->width[r][c & 0x03FF] >= 0 ) { // already cached
        return (double) desc->width[r][c & 0x03FF];
      }
    }
  }
  char buf[4];
  int n = fl_utf8encode(c, buf);
  pango_layout_set_text(pango_layout_, buf, n);
  int  W = 0, H;
  pango_layout_get_pixel_size(pango_layout_, &W, &H);
  if (c <= 0xFFFF) desc->width[r][c & 0x03FF] = W;
  return (double)W;
}


void Fl_Wayland_Graphics_Driver::text_extents(const char* txt, int n, int& dx, int& dy, int& w, int& h) {
  pango_layout_set_text(pango_layout_, txt, n);
  PangoRectangle ink_rect;
  pango_layout_get_pixel_extents(pango_layout_, &ink_rect, NULL);
  dx = ink_rect.x;
  dy = ink_rect.y - height() + descent();
  w = ink_rect.width;
  h = ink_rect.height;
}


int Fl_Wayland_Graphics_Driver::not_clipped(int x, int y, int w, int h) {
  if (!clip_) return 1;
  if (clip_->w < 0) return 1;
  int X = 0, Y = 0, W = 0, H = 0;
  clip_box(x, y, w, h, X, Y, W, H);
  if (W) return 1;
  return 0;
}

int Fl_Wayland_Graphics_Driver::clip_box(int x, int y, int w, int h, int &X, int &Y, int &W, int &H) {
  if (!clip_) {
    X = x; Y = y; W = w; H = h;
    return 0;
  }
  if (clip_->w < 0) {
    X = x; Y = y; W = w; H = h;
    return 1;
  }
  int ret = 0;
  if (x > (X=clip_->x)) {X=x; ret=1;}
  if (y > (Y=clip_->y)) {Y=y; ret=1;}
  if ((x+w) < (clip_->x+clip_->w)) {
    W=x+w-X;

    ret=1;

  }else
    W = clip_->x + clip_->w - X;
  if(W<0){
    W=0;
    return 1;
  }
  if ((y+h) < (clip_->y+clip_->h)) {
    H=y+h-Y;
    ret=1;
  }else
    H = clip_->y + clip_->h - Y;
  if(H<0){
    W=0;
    H=0;
    return 1;
  }
  return ret;
}

void Fl_Wayland_Graphics_Driver::restore_clip() {
  //TODO
}


Fl_Region Fl_Wayland_Graphics_Driver::XRectangleRegion(int x, int y, int w, int h) {
  Fl_Region R = (Fl_Region)malloc(sizeof(*R));
  R->count = 1;
  R->rects = (cairo_rectangle_t *)malloc(sizeof(cairo_rectangle_t));
  R->rects->x=x, R->rects->y=y, R->rects->width=w; R->rects->height=h;
  return R;
}


// r1 ⊂ r2
static bool CairoRectContainsRect(cairo_rectangle_t *r1, cairo_rectangle_t *r2) {
  return r1->x >= r2->x && r1->y >= r2->y && r1->x+r1->width <= r2->x+r2->width &&
    r1->y+r1->height <= r2->y+r2->height;
}


void Fl_Wayland_Graphics_Driver::add_rectangle_to_region(Fl_Region r, int X, int Y, int W, int H) {
  cairo_rectangle_t arg = {double(X), double(Y), double(W), double(H)};
  int j; // don't add a rectangle totally inside the Fl_Region
  for (j = 0; j < r->count; j++) {
    if (CairoRectContainsRect(&arg, &(r->rects[j]))) break;
  }
  if (j >= r->count) {
    r->rects = (cairo_rectangle_t*)realloc(r->rects, (++(r->count)) * sizeof(cairo_rectangle_t));
    r->rects[r->count - 1] = arg;
  }
}


void Fl_Wayland_Graphics_Driver::XDestroyRegion(Fl_Region r) {
  if (r) {
    free(r->rects);
    free(r);
  }
}


void Fl_Wayland_Graphics_Driver::set_color(Fl_Color i, unsigned c) {
  if (fl_cmap[i] != c) {
    fl_cmap[i] = c;
  }
}


void Fl_Wayland_Graphics_Driver::point(int x, int y) {
  rectf(x, y, 1, 1);
}


void Fl_Wayland_Graphics_Driver::copy_offscreen(int x, int y, int w, int h, Fl_Offscreen osrc, int srcx, int srcy) {
  // draw portion srcx,srcy,w,h of osrc to position x,y (top-left) of the graphics driver's surface
  int height = osrc->data_size / osrc->stride;
  cairo_matrix_t matrix;
  cairo_get_matrix(cairo_, &matrix);
  double s = matrix.xx;
  cairo_save(cairo_);
  cairo_rectangle(cairo_, x, y, w, h);
  cairo_clip(cairo_);
  cairo_surface_t *surf = cairo_image_surface_create_for_data(osrc->draw_buffer, CAIRO_FORMAT_ARGB32, osrc->width, height, osrc->stride);
  cairo_pattern_t *pat = cairo_pattern_create_for_surface(surf);
  cairo_set_source(cairo_, pat);
  cairo_matrix_init_scale(&matrix, s, s);
  cairo_matrix_translate(&matrix, -(x - srcx), -(y - srcy));
  cairo_pattern_set_matrix(pat, &matrix);
  cairo_mask(cairo_, pat);
  cairo_pattern_destroy(pat);
  cairo_surface_destroy(surf);
  cairo_restore(cairo_);
}


struct callback_data {
  const uchar *data;
  int D, LD;
};


static void draw_image_cb(void *data, int x, int y, int w, uchar *buf) {
  struct callback_data *cb_data;
  const uchar *curdata;

  cb_data = (struct callback_data*)data;
  int last = x+w;
  const size_t aD = abs(cb_data->D);
  curdata = cb_data->data + x*cb_data->D + y*cb_data->LD;
  for (; x<last; x++) {
    memcpy(buf, curdata, aD);
    buf += aD;
    curdata += cb_data->D;
  }
}


void Fl_Wayland_Graphics_Driver::draw_image(const uchar *data, int ix, int iy, int iw, int ih, int D, int LD) {
  if (abs(D)<3){ //mono
    draw_image_mono(data, ix, iy, iw, ih, D, LD);
    return;
  }
  struct callback_data cb_data;
  if (!LD) LD = iw*abs(D);
  if (D<0) data += iw*abs(D);
  cb_data.data = data;
  cb_data.D = D;
  cb_data.LD = LD;
  Fl_Cairo_Graphics_Driver::draw_image(draw_image_cb, &cb_data, ix, iy, iw, ih, abs(D));
}


void Fl_Wayland_Graphics_Driver::curve(double x, double y, double x1, double y1, double x2, double y2, double x3, double y3) {
  if (shape_ == POINTS) Fl_Graphics_Driver::curve(x, y, x1, y1, x2, y2, x3, y3);
  else Fl_Cairo_Graphics_Driver::curve(x, y, x1, y1, x2, y2, x3, y3);
}


void Fl_Wayland_Graphics_Driver::begin_points() {
  cairo_save(cairo_);
  gap_=1;
  shape_=POINTS;
}


void Fl_Wayland_Graphics_Driver::end_points() {
  cairo_restore(cairo_);
}


void Fl_Wayland_Graphics_Driver::transformed_vertex(double x, double y) {
  if (shape_ == POINTS){
    cairo_move_to(cairo_, x, y);
    point(x, y);
    gap_ = 1;
  } else {
    Fl_Cairo_Graphics_Driver::transformed_vertex(x, y);
  }
}


void Fl_Wayland_Graphics_Driver::draw_cached_pattern_(Fl_Image *img, cairo_pattern_t *pat, int X, int Y, int W, int H, int cx, int cy) {
  cairo_save(cairo_);
  cairo_rectangle(cairo_, X-0.5, Y-0.5, W+1, H+1);
  cairo_clip(cairo_);
  if (img->d() >= 1) cairo_set_source(cairo_, pat);
  cairo_matrix_t matrix;
  cairo_matrix_init_scale(&matrix, double(img->data_w())/(img->w()+1), double(img->data_h())/(img->h()+1));
  cairo_matrix_translate(&matrix, -X+0.5+cx, -Y+0.5+cy);
  cairo_pattern_set_matrix(pat, &matrix);
  cairo_mask(cairo_, pat);
  cairo_restore(cairo_);
  buffer->draw_buffer_needs_commit = true;
}


void Fl_Wayland_Graphics_Driver::draw_rgb(Fl_RGB_Image *rgb,int XP, int YP, int WP, int HP, int cx, int cy) {
  int X, Y, W, H;
  // Don't draw an empty image...
  if (!rgb->d() || !rgb->array) {
    Fl_Graphics_Driver::draw_empty(rgb, XP, YP);
    return;
  }
  if (start_image(rgb, XP, YP, WP, HP, cx, cy, X, Y, W, H)) {
    return;
  }
  cairo_pattern_t *pat = (cairo_pattern_t*)*Fl_Graphics_Driver::id(rgb);
  if (!pat) {
    cache(rgb);
    pat = (cairo_pattern_t*)*Fl_Graphics_Driver::id(rgb);
  }
  draw_cached_pattern_(rgb, pat, X, Y, W, H, cx, cy);
}


static cairo_user_data_key_t data_key_for_surface = {};

static void dealloc_surface_data(void *data) {
  delete[] (uchar*)data;
}


void Fl_Wayland_Graphics_Driver::cache(Fl_RGB_Image *rgb) {
  int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, rgb->data_w());
  uchar *BGRA = new uchar[stride * rgb->data_h()];
  memset(BGRA, 0, stride * rgb->data_h());
  int lrgb = rgb->ld() ? rgb->ld() : rgb->data_w() * rgb->d();
  uchar A = 0xff, R,G,B, *q;
  const uchar *r;
  float f = 1;
  if (rgb->d() >= 3) { // color images
    for (int j = 0; j < rgb->data_h(); j++) {
      r = rgb->array + j * lrgb;
      q = BGRA + j * stride;
      for (int i = 0; i < rgb->data_w(); i++) {
        R = *r;
        G = *(r+1);
        B = *(r+2);
        if (rgb->d() == 4) {
          A = *(r+3);
          f = float(A)/0xff;
        }
        *q =  B * f;
        *(q+1) =  G * f;
        *(q+2) =  R * f;
        *(q+3) =  A;
        r += rgb->d(); q += 4;
      }
    }
  } else if (rgb->d() == 1 || rgb->d() == 2) { // B&W
    for (int j = 0; j < rgb->data_h(); j++) {
      r = rgb->array + j * lrgb;
      q = BGRA + j * stride;
      for (int i = 0; i < rgb->data_w(); i++) {
        G = *r;
        if (rgb->d() == 2) {
          A = *(r+1);
          f = float(A)/0xff;
        }
        *(q) =  G * f;
        *(q+1) =  G * f;
        *(q+2) =  G * f;
        *(q+3) =  A;
        r += rgb->d(); q += 4;
      }
    }
  }
  cairo_surface_t *surf = cairo_image_surface_create_for_data(BGRA, CAIRO_FORMAT_ARGB32, rgb->data_w(), rgb->data_h(), stride);
  if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) return;
  (void)cairo_surface_set_user_data(surf, &data_key_for_surface, BGRA, dealloc_surface_data);
  cairo_pattern_t *pat = cairo_pattern_create_for_surface(surf);
  *Fl_Graphics_Driver::id(rgb) = (fl_uintptr_t)pat;
}


void Fl_Wayland_Graphics_Driver::uncache(Fl_RGB_Image *img, fl_uintptr_t &id_, fl_uintptr_t &mask_) {
  cairo_pattern_t *pat = (cairo_pattern_t*)id_;
  if (pat) {
    cairo_surface_t *surf;
    cairo_pattern_get_surface(pat, &surf);
    cairo_pattern_destroy(pat);
    cairo_surface_destroy(surf);
    id_ = 0;
  }
}


void Fl_Wayland_Graphics_Driver::draw_bitmap(Fl_Bitmap *bm,int XP, int YP, int WP, int HP, int cx, int cy) {
  int X, Y, W, H;
  if (!bm->array) {
    draw_empty(bm, XP, YP);
    return;
  }
  if (start_image(bm, XP,YP,WP,HP,cx,cy,X,Y,W,H)) return;
  cairo_pattern_t *pat = (cairo_pattern_t*)*Fl_Graphics_Driver::id(bm);
  if (!pat) {
    cache(bm);
    pat = (cairo_pattern_t*)*Fl_Graphics_Driver::id(bm);
  }
  if (pat) {
    draw_cached_pattern_(bm, pat, X, Y, W, H, cx, cy);
  }
}


void Fl_Wayland_Graphics_Driver::cache(Fl_Bitmap *bm) {
  int stride = cairo_format_stride_for_width(CAIRO_FORMAT_A1, bm->data_w());
  uchar *BGRA = new uchar[stride * bm->data_h()];
  memset(BGRA, 0, stride * bm->data_h());
    uchar  *r, p;
    unsigned *q;
    for (int j = 0; j < bm->data_h(); j++) {
      r = (uchar*)bm->array + j * ((bm->data_w() + 7)/8);
      q = (unsigned*)(BGRA + j * stride);
      unsigned k = 0, mask32 = 1;
      p = *r;
      for (int i = 0; i < bm->data_w(); i++) {
        if (p&1) (*q) |= mask32;
        k++;
        if (k % 8 != 0) p >>= 1; else p = *(++r);
        if (k % 32 != 0) mask32 <<= 1; else {q++; mask32 = 1;}
      }
    }
  cairo_surface_t *surf = cairo_image_surface_create_for_data(BGRA, CAIRO_FORMAT_A1, bm->data_w(), bm->data_h(), stride);
  if (cairo_surface_status(surf) == CAIRO_STATUS_SUCCESS) {
    (void)cairo_surface_set_user_data(surf, &data_key_for_surface, BGRA, dealloc_surface_data);
    cairo_pattern_t *pat = cairo_pattern_create_for_surface(surf);
    *Fl_Graphics_Driver::id(bm) = (fl_uintptr_t)pat;
  }
}


void Fl_Wayland_Graphics_Driver::delete_bitmask(Fl_Bitmask bm) {
  cairo_pattern_t *pat = (cairo_pattern_t*)bm;
  if (pat) {
    cairo_surface_t *surf;
    cairo_pattern_get_surface(pat, &surf);
    cairo_pattern_destroy(pat);
    cairo_surface_destroy(surf);
  }
}


void Fl_Wayland_Graphics_Driver::draw_pixmap(Fl_Pixmap *pxm,int XP, int YP, int WP, int HP, int cx, int cy) {
  int X, Y, W, H;
  // Don't draw an empty image...
  if (!pxm->data() || !pxm->w()) {
    Fl_Graphics_Driver::draw_empty(pxm, XP, YP);
    return;
  }
  if (start_image(pxm, XP, YP, WP, HP, cx, cy, X, Y, W, H)) {
    return;
  }
  cairo_pattern_t *pat = (cairo_pattern_t*)*Fl_Graphics_Driver::id(pxm);
  if (!pat) {
    cache(pxm);
    pat = (cairo_pattern_t*)*Fl_Graphics_Driver::id(pxm);
  }
  draw_cached_pattern_(pxm, pat, X, Y, W, H, cx, cy);
}


void Fl_Wayland_Graphics_Driver::cache(Fl_Pixmap *pxm) {
  Fl_RGB_Image *rgb = new Fl_RGB_Image(pxm);
  cache(rgb);
  *Fl_Graphics_Driver::id(pxm) = *Fl_Graphics_Driver::id(rgb);
  *Fl_Graphics_Driver::id(rgb) = 0;
  delete rgb;
}


void Fl_Wayland_Graphics_Driver::uncache_pixmap(fl_uintptr_t p) {
  cairo_pattern_t *pat = (cairo_pattern_t*)p;
  if (pat) {
    cairo_surface_t *surf;
    cairo_pattern_get_surface(pat, &surf);
    cairo_pattern_destroy(pat);
    cairo_surface_destroy(surf);
  }
}


void Fl_Wayland_Graphics_Driver::line(int x1, int y1, int x2, int y2) {
  Fl_Cairo_Graphics_Driver::line(x1, y1, x2, y2);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::line(int x1, int y1, int x2, int y2, int x3, int y3) {
  Fl_Cairo_Graphics_Driver::line(x1, y1, x2, y2, x3, y3);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::xyline(int x, int y, int x1) {
  Fl_Cairo_Graphics_Driver::xyline(x, y, x1);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::xyline(int x, int y, int x1, int y2) {
  Fl_Cairo_Graphics_Driver::xyline(x, y, x1, y2);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::xyline(int x, int y, int x1, int y2, int x3) {
  Fl_Cairo_Graphics_Driver::xyline(x, y, x1, y2, x3);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::yxline(int x, int y, int y1) {
  Fl_Cairo_Graphics_Driver::yxline(x, y, y1);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::yxline(int x, int y, int y1, int x2) {
  Fl_Cairo_Graphics_Driver::yxline(x, y, y1, x2);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::yxline(int x, int y, int y1, int x2, int y3) {
  Fl_Cairo_Graphics_Driver::yxline(x, y, y1, x2, y3);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::loop(int x0, int y0, int x1, int y1, int x2, int y2) {
  Fl_Cairo_Graphics_Driver::loop(x0, y0, x1, y1, x2, y2);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::loop(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
  Fl_Cairo_Graphics_Driver::loop(x0, y0, x1, y1, x2, y2, x3, y3);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::rectf(int x, int y, int w, int h) {
  Fl_Cairo_Graphics_Driver::rectf(x, y, w, h);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::rect(int x, int y, int w, int h) {
  Fl_Cairo_Graphics_Driver::rect(x, y, w, h);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::polygon(int x0, int y0, int x1, int y1, int x2, int y2) {
  Fl_Cairo_Graphics_Driver::polygon(x0, y0, x1, y1, x2, y2);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::polygon(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
  Fl_Cairo_Graphics_Driver::polygon(x0, y0, x1, y1, x2, y2, x3, y3);
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::end_line() {
  Fl_Cairo_Graphics_Driver::end_line();
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::end_loop(){
  Fl_Cairo_Graphics_Driver::end_loop();
  buffer->draw_buffer_needs_commit = true;
}

void Fl_Wayland_Graphics_Driver::end_polygon() {
  Fl_Cairo_Graphics_Driver::end_polygon();
  buffer->draw_buffer_needs_commit = true;
}