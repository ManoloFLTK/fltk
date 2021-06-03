//
// Class Fl_Wayland_Gl_Window_Driver for the Fast Light Tool Kit (FLTK).
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
#if HAVE_GL
#include <FL/platform.H>
#include <FL/Fl_Image_Surface.H>
#include "../../Fl_Gl_Choice.H"
#include "../../Fl_Screen_Driver.H"
#include "Fl_Wayland_Window_Driver.H"
#include "Fl_Wayland_Graphics_Driver.H"
#include "../../Fl_Gl_Window_Driver.H"
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <FL/gl.h>


class Fl_Wayland_Gl_Window_Driver : public Fl_Gl_Window_Driver {
  friend class Fl_Gl_Window_Driver;
protected:
  Fl_Wayland_Gl_Window_Driver(Fl_Gl_Window *win);
  virtual float pixels_per_unit();
  virtual void make_current_before();
  virtual int mode_(int m, const int *a);
  virtual void swap_buffers();
  virtual void resize(int is_a_resize, int w, int h);
  virtual char swap_type();
  virtual Fl_Gl_Choice *find(int m, const int *alistp);
  virtual GLContext create_gl_context(Fl_Window* window, const Fl_Gl_Choice* g, int layer = 0);
  virtual void set_gl_context(Fl_Window* w, GLContext context);
  virtual void delete_gl_context(GLContext);
  virtual void make_overlay_current();
  virtual void redraw_overlay();
  virtual void waitGL();
  virtual void gl_visual(Fl_Gl_Choice*); // support for Fl::gl_visual()
  virtual void gl_start();
  virtual Fl_RGB_Image* capture_gl_rectangle(int x, int y, int w, int h);
  char *alpha_mask_for_string(const char *str, int n, int w, int h, Fl_Fontsize fs);
  virtual int flush_begin(char& valid_f_);
public:
  //static GLContext create_gl_context(XVisualInfo* vis);
  static EGLDisplay egl_display;
  static EGLConfig egl_conf;
  void init();
  struct wl_egl_window *egl_window;
  EGLSurface egl_surface;
};

// Describes crap needed to create a GLContext.
class Fl_Wayland_Gl_Choice : public Fl_Gl_Choice {
  friend class Fl_Wayland_Gl_Window_Driver;
private:
  /*XVisualInfo *vis;
  Colormap colormap;
  GLXFBConfig best_fb;*/
public:
  Fl_Wayland_Gl_Choice(int m, const int *alistp, Fl_Gl_Choice *n) : Fl_Gl_Choice(m, alistp, n) {
    /*vis = NULL;
    colormap = 0;
    best_fb = NULL;*/
  }
};

EGLDisplay Fl_Wayland_Gl_Window_Driver::egl_display = EGL_NO_DISPLAY;
EGLConfig Fl_Wayland_Gl_Window_Driver::egl_conf = 0;

Fl_Wayland_Gl_Window_Driver::Fl_Wayland_Gl_Window_Driver(Fl_Gl_Window *win) : Fl_Gl_Window_Driver(win) {
  if (egl_display == EGL_NO_DISPLAY) init();
  egl_window = NULL;
  egl_surface = NULL;
}


void Fl_Wayland_Gl_Window_Driver::init() {
  EGLint major, minor, count, n, size;
  EGLConfig *configs;
  int i;
  EGLint config_attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_NONE
  };
  
  /*static const EGLint context_attribs[] = {
    EGL_CONTEXT_MAJOR_VERSION, 2,
    EGL_CONTEXT_OPENGL_PROFILE_MASK,  EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,//for OpenGL 1 et 2
    EGL_NONE
  };*/
  
  if (!fl_display) Fl::screen_driver()->open_display();
  egl_display = eglGetDisplay((EGLNativeDisplayType) fl_display);
  if (egl_display == EGL_NO_DISPLAY) {
    fprintf(stderr, "Can't create egl display\n");
    exit(1);
  } else {
    //fprintf(stderr, "Created egl display\n");
  }
  
  if (eglInitialize(egl_display, &major, &minor) != EGL_TRUE) {
    fprintf(stderr, "Can't initialise egl display\n");
    exit(1);
  }
  printf("EGL major: %d, minor %d\n", major, minor);
  
  eglGetConfigs(egl_display, NULL, 0, &count);
  printf("EGL has %d configs\n", count);
  
  configs = (void**)calloc(count, sizeof *configs);
  
  eglChooseConfig(egl_display, config_attribs,
                  configs, count, &n);
  if (n == 0) {
    fprintf(stderr, "failed to choose an EGL config\n");
    exit(1);
  }
  eglBindAPI(EGL_OPENGL_API); //necessary
  for (i = 0; i < n; i++) {
    eglGetConfigAttrib(egl_display,
                       configs[i], EGL_BUFFER_SIZE, &size);
    printf("Buffer size for config %d is %d\n", i, size);
    eglGetConfigAttrib(egl_display,
                       configs[i], EGL_RED_SIZE, &size);
    printf("Red size for config %d is %d\n", i, size);
    
    // just choose the first one
    egl_conf = configs[i];
    break;
  }
}


char *Fl_Wayland_Gl_Window_Driver::alpha_mask_for_string(const char *str, int n, int w, int h, Fl_Fontsize fs)
{
  // write str to a bitmap just big enough
  Window save_win = fl_window;
  fl_window = NULL;
  Fl_Image_Surface *surf = new Fl_Image_Surface(w, h);
  fl_window = save_win;
  Fl_Font f=fl_font();
  Fl_Surface_Device::push_current(surf);
  fl_color(FL_BLACK);
  fl_rectf(0, 0, w, h);
  fl_color(FL_WHITE);
  fl_font(f, fs);
  fl_draw(str, n, 0, fl_height() - fl_descent());
  // get the R channel only of the bitmap
  char *alpha_buf = new char[w*h], *r = alpha_buf, *q;
  for (int i = 0; i < h; i++) {
    q = (char*)surf->offscreen()->draw_buffer + i * surf->offscreen()->stride;
    for (int j = 0; j < w; j++) {
      *r++ = *q;
      q += 4;
    }
  }
  Fl_Surface_Device::pop_current();
  delete surf;
  return alpha_buf;
}


/*static XVisualInfo *gl3_getvisual(const int *blist, GLXFBConfig *pbestFB)
{
  int glx_major, glx_minor;

  // FBConfigs were added in GLX version 1.3.
  if ( !glXQueryVersion(fl_display, &glx_major, &glx_minor) ||
      ( ( glx_major == 1 ) && ( glx_minor < 3 ) ) || ( glx_major < 1 ) ) {
    return NULL;
  }

  //printf( "Getting matching framebuffer configs\n" );
  int fbcount;
  GLXFBConfig* fbc = glXChooseFBConfig(fl_display, DefaultScreen(fl_display), blist, &fbcount);
  if (!fbc) {
    //printf( "Failed to retrieve a framebuffer config\n" );
    return NULL;
  }
  //printf( "Found %d matching FB configs.\n", fbcount );

  // Pick the FB config/visual with the most samples per pixel
  int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;
  for (int i = 0; i < fbcount; ++i)
  {
    XVisualInfo *vi = glXGetVisualFromFBConfig( fl_display, fbc[i] );
    if (vi) {
      int samp_buf, samples;
      glXGetFBConfigAttrib(fl_display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
      glXGetFBConfigAttrib(fl_display, fbc[i], GLX_SAMPLES       , &samples );
      if ( best_fbc < 0 || (samp_buf && samples > best_num_samp) )
        best_fbc = i, best_num_samp = samples;
      if ( worst_fbc < 0 || !samp_buf || samples < worst_num_samp )
        worst_fbc = i, worst_num_samp = samples;
    }
    XFree(vi);
  }

  GLXFBConfig bestFbc = fbc[ best_fbc ];
  // Be sure to free the FBConfig list allocated by glXChooseFBConfig()
  XFree(fbc);
  // Get a visual
  XVisualInfo *vi = glXGetVisualFromFBConfig(fl_display, bestFbc);
  *pbestFB = bestFbc;
  return vi;
}*/

Fl_Gl_Choice *Fl_Wayland_Gl_Window_Driver::find(int m, const int *alistp)
{
  Fl_Wayland_Gl_Choice *g = (Fl_Wayland_Gl_Choice*)Fl_Gl_Window_Driver::find_begin(m, alistp);
  if (g) return g;

/*  const int *blist;
  int list[32];

  if (alistp)
    blist = alistp;
  else {
    int n = 0;
    if (m & FL_INDEX) {
      list[n++] = GLX_BUFFER_SIZE;
      list[n++] = 8; // glut tries many sizes, but this should work...
    } else {
      list[n++] = GLX_RGBA;
      list[n++] = GLX_GREEN_SIZE;
      list[n++] = (m & FL_RGB8) ? 8 : 1;
      if (m & FL_ALPHA) {
        list[n++] = GLX_ALPHA_SIZE;
        list[n++] = (m & FL_RGB8) ? 8 : 1;
      }
      if (m & FL_ACCUM) {
        list[n++] = GLX_ACCUM_GREEN_SIZE;
        list[n++] = 1;
        if (m & FL_ALPHA) {
          list[n++] = GLX_ACCUM_ALPHA_SIZE;
          list[n++] = 1;
        }
      }
    }
    if (m & FL_DOUBLE) {
      list[n++] = GLX_DOUBLEBUFFER;
    }
    if (m & FL_DEPTH) {
      list[n++] = GLX_DEPTH_SIZE; list[n++] = 1;
    }
    if (m & FL_STENCIL) {
      list[n++] = GLX_STENCIL_SIZE; list[n++] = 1;
    }
    if (m & FL_STEREO) {
      list[n++] = GLX_STEREO;
    }
#    if defined(GLX_VERSION_1_1) && defined(GLX_SGIS_multisample)
    if (m & FL_MULTISAMPLE) {
      list[n++] = GLX_SAMPLES_SGIS;
      list[n++] = 4; // value Glut uses
    }
#    endif
    list[n] = 0;
    blist = list;
  }

  fl_open_display();
  XVisualInfo *visp = NULL;
  GLXFBConfig best_fb = NULL;
  if (m & FL_OPENGL3) {
    visp = gl3_getvisual((const int *)blist, &best_fb);
  }
  if (!visp) {
    visp = glXChooseVisual(fl_display, fl_screen, (int *)blist);
    if (!visp) {
#     if defined(GLX_VERSION_1_1) && defined(GLX_SGIS_multisample)
        if (m&FL_MULTISAMPLE) return find(m&~FL_MULTISAMPLE, 0);
#     endif
      return 0;
    }
  }*/

  g = new Fl_Wayland_Gl_Choice(m, alistp, first);
  first = g;

/*  g->vis = visp;
  g->best_fb = best_fb;

  if (visp->visualid == fl_visual->visualid &&
      !fl_getenv("MESA_PRIVATE_CMAP"))
    g->colormap = fl_colormap;
  else
    g->colormap = XCreateColormap(fl_display, RootWindow(fl_display,fl_screen),
                                  visp->visual, AllocNone);*/
  return g;
}


GLContext Fl_Wayland_Gl_Window_Driver::create_gl_context(Fl_Window* window, const Fl_Gl_Choice* g, int layer) {
  GLContext shared_ctx = 0;
  if (context_list && nContext) shared_ctx = context_list[0];

  static const EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  GLContext ctx = (GLContext)eglCreateContext(egl_display, egl_conf, shared_ctx?shared_ctx:EGL_NO_CONTEXT, context_attribs);
//fprintf(stderr, "eglCreateContext=%p shared_ctx=%p\n", ctx, shared_ctx);
  if (ctx)
    add_context(ctx);
  return ctx;
}

/*GLContext Fl_Wayland_Gl_Window_Driver::create_gl_context(XVisualInfo *vis) {
  GLContext shared_ctx = 0;
  if (context_list && nContext) shared_ctx = context_list[0];
  GLContext context = glXCreateContext(fl_display, vis, shared_ctx, 1);
  if (context)
    add_context(context);
  return context;
}*/


void Fl_Wayland_Gl_Window_Driver::set_gl_context(Fl_Window* w, GLContext context) {
  struct wld_window *win = fl_xid(w);
  if (!win || !egl_surface) return;
  if (context != cached_context || w != cached_window) {
    cached_context = context;
    cached_window = w;
    if (eglMakeCurrent(egl_display, egl_surface, egl_surface, (EGLContext)context)) {
//fprintf(stderr, "EGLContext %p made current\n", context);
    } else {
      fprintf(stderr, "Made current failed\n");
    }
  }
}

void Fl_Wayland_Gl_Window_Driver::delete_gl_context(GLContext context) {
  if (cached_context == context) {
    cached_context = 0;
    cached_window = 0;
  }
  EGLBoolean b = eglDestroyContext(egl_display, context);
fprintf(stderr,"EGL context %p destroyed %s\n", context, b==EGL_TRUE?"successfully":"w/ error");
  b = eglDestroySurface(egl_display, egl_surface);
fprintf(stderr,"EGLSurface %p destroyed %s\n", egl_surface, b==EGL_TRUE?"successfully":"w/ error");
  egl_surface = NULL;
  wl_egl_window_destroy(egl_window);
  egl_window = NULL;
  del_context(context);
}


void Fl_Wayland_Gl_Window_Driver::make_overlay_current() {
//fprintf(stderr, "make_overlay_current\n");
  glDrawBuffer(GL_FRONT);
}

void Fl_Wayland_Gl_Window_Driver::redraw_overlay() {
//fprintf(stderr, "redraw_overlay\n");
  pWindow->redraw();
}


Fl_Gl_Window_Driver *Fl_Gl_Window_Driver::newGlWindowDriver(Fl_Gl_Window *w)
{
  return new Fl_Wayland_Gl_Window_Driver(w);
}


void Fl_Wayland_Gl_Window_Driver::make_current_before() {
  if (!egl_window) {
    struct wld_window *win = fl_xid(pWindow);
    struct wl_surface *surface = pWindow->parent() ? win->wl_surface : win->gl_wl_surface;
    egl_window = wl_egl_window_create(surface, pWindow->pixel_w(), pWindow->pixel_h());
    if (egl_window == EGL_NO_SURFACE) {
      fprintf(stderr, "Can't create egl window\n");
      exit(1);
    } else {
      //fprintf(stderr, "Created egl window=%p\n", egl_window);
    }
    egl_surface = eglCreateWindowSurface(egl_display, egl_conf, egl_window, NULL);
//fprintf(stderr, "Created egl surface=%p at scale=%d\n", egl_surface, win->scale);
    wl_surface_set_buffer_scale(surface, win->scale);
    wl_surface_commit(surface);
    wl_display_roundtrip(fl_display);
  }
}

float Fl_Wayland_Gl_Window_Driver::pixels_per_unit()
{
  int ns = Fl_Window_Driver::driver(pWindow)->screen_num();
  int wld_scale = pWindow->shown() ? fl_xid(pWindow)->scale : 1;
  return wld_scale * Fl::screen_driver()->scale(ns);
}


int Fl_Wayland_Gl_Window_Driver::mode_(int m, const int *a) {
  /*int oldmode = mode();
  if (a) { // when the mode is set using the a array of system-dependent values, and if asking for double buffer,
    // the FL_DOUBLE flag must be set in the mode_ member variable
    const int *aa = a;
    while (*aa) {
      if (*(aa++) ==
          GLX_DOUBLEBUFFER
          ) { m |= FL_DOUBLE; break; }
    }
  }
  Fl_Wayland_Gl_Choice* oldg = (Fl_Wayland_Gl_Choice*)g();
  pWindow->context(0);
  mode(m); alist(a);
  if (pWindow->shown()) {
    g( find(m, a) );
    // under X, if the visual changes we must make a new X window (yuck!):
    Fl_Wayland_Gl_Choice* g = (Fl_Wayland_Gl_Choice*)this->g();
    if (!g || g->vis->visualid != oldg->vis->visualid || (oldmode^m)&FL_DOUBLE) {
      pWindow->hide();
      pWindow->show();
    }
  } else {
    g(0);
  }*/
  return 1;
}


static void surface_frame_done(void *data, struct wl_callback *cb, uint32_t time) {
  wl_callback_destroy(cb);
  *(bool*)data = true;
}

static const struct wl_callback_listener surface_frame_listener = {
  .done = surface_frame_done,
};


void Fl_Wayland_Gl_Window_Driver::swap_buffers() {
  if (overlay()) {
    static bool overlay_buffer = true;
    int wo = pWindow->pixel_w(), ho = pWindow->pixel_h();
    GLint matrixmode;
    GLfloat pos[4];
    glGetIntegerv(GL_MATRIX_MODE, &matrixmode);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);       // save original glRasterPos
    glMatrixMode(GL_PROJECTION);                        // save proj/model matrices
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glScalef(2.0f/wo, 2.0f/ho, 1.0f);
    glTranslatef(-wo/2.0f, -ho/2.0f, 0.0f);         // set transform so 0,0 is bottom/left of Gl_Window
    glRasterPos2i(0,0);                             // set glRasterPos to bottom left corner
    {
      // Emulate overlay by doing copypixels
      glReadBuffer(overlay_buffer?GL_BACK:GL_FRONT);
      glDrawBuffer(overlay_buffer?GL_FRONT:GL_BACK);
      overlay_buffer = ! overlay_buffer;
      glCopyPixels(0, 0, wo, ho, GL_COLOR);
    }
    glPopMatrix(); // GL_MODELVIEW                  // restore model/proj matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(matrixmode);
    glRasterPos3f(pos[0], pos[1], pos[2]);              // restore original glRasterPos
    if (!overlay_buffer) return; // don't call eglSwapBuffers until overlay has been drawn
  }
  if (egl_surface) {
    // Make eglSwapBuffers non-blocking, we manage frame callbacks manually
    eglSwapInterval(egl_display, 0);
    // Register a frame callback to know when we can draw the next frame
    Window xid = fl_xid(pWindow);
    struct wl_surface *surf = xid->gl_wl_surface ? xid->gl_wl_surface : xid->wl_surface;
    struct wl_callback *callback = wl_surface_frame(surf);
    wl_surface_commit(surf);
    bool done = false;
    wl_callback_add_listener(callback, &surface_frame_listener, &done);
    while (!done) wl_display_dispatch(fl_display); // wait for arrival of frame event
    if (eglSwapBuffers(egl_display, egl_surface)) {
      //fprintf(stderr, "Swapped buffers for surface=%p display=%p\n", egl_surface, egl_display);
      cached_context = 0;
    } else {
      fprintf(stderr, "Swapped buffers failed\n");
    }
  }
}


class Fl_Gl_Overlay_Plugin : public Fl_Wayland_Plugin {
public:
  Fl_Gl_Overlay_Plugin() : Fl_Wayland_Plugin(name()) { }
  virtual const char *name() { return "gl_overlay.wayland.fltk.org"; }
  virtual void do_swap(Fl_Window *w) {
    Fl_Gl_Window_Driver *gldr = Fl_Gl_Window_Driver::driver(w->as_gl_window());
    if (gldr->overlay() == w) gldr->swap_buffers();
  }
};

static Fl_Gl_Overlay_Plugin Gl_Overlay_Plugin;


void Fl_Wayland_Gl_Window_Driver::resize(int is_a_resize, int W, int H) {
  if (egl_window) {
    struct wld_window *win = fl_xid(pWindow);
    int wld_scale = win->scale;
    wl_egl_window_resize(egl_window, W * wld_scale, H * wld_scale, 0, 0);
//fprintf(stderr, "Fl_Wayland_Gl_Window_Driver::resize to %dx%d\n", W * wld_scale, H * wld_scale);
  }
}

char Fl_Wayland_Gl_Window_Driver::swap_type() {
  return copy;
}

void Fl_Wayland_Gl_Window_Driver::waitGL() {
  //glXWaitGL();
}


void Fl_Wayland_Gl_Window_Driver::gl_visual(Fl_Gl_Choice *c) {
  /*Fl_Gl_Window_Driver::gl_visual(c);
  fl_visual = ((Fl_Wayland_Gl_Choice*)c)->vis;
  fl_colormap = ((Fl_Wayland_Gl_Choice*)c)->colormap;*/
}

void Fl_Wayland_Gl_Window_Driver::gl_start() {
  //glXWaitX();
}


Fl_RGB_Image* Fl_Wayland_Gl_Window_Driver::capture_gl_rectangle(int x, int y, int w, int h) {
  Fl_Surface_Device::push_current(Fl_Display_Device::display_device());
  Fl_RGB_Image *rgb = Fl_Gl_Window_Driver::capture_gl_rectangle(x, y, w, h);
  Fl_Surface_Device::pop_current();
  return rgb;
}


int Fl_Wayland_Gl_Window_Driver::flush_begin(char& valid_f_) {
  if (!pWindow->parent()) {
    Window window = fl_xid(pWindow);
    if (!window->buffer) { // a fresh top-level GL window: give it its buffer
      window->buffer = Fl_Wayland_Graphics_Driver::create_shm_buffer(
                    pWindow->w() * window->scale, pWindow->h() * window->scale,
                    WL_SHM_FORMAT_ARGB8888, window);
      wl_surface_damage_buffer(window->wl_surface, 0, 0, pWindow->w() * window->scale, pWindow->h() * window->scale);
      Fl_Wayland_Graphics_Driver::buffer_commit(window);
    }
  }
  return 0;
}

#endif // HAVE_GL