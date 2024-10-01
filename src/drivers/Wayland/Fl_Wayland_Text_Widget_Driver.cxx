//
//  Fl_Wayland_Text_Widget_Drver.cxx
//

#include <config.h> // for BORDER_WIDTH
#include "../../Fl_Text_Widget_Driver.H"
#include <FL/platform.H>
#include <FL/Fl_Native_Text_Widget.H>
#include "Fl_Wayland_Window_Driver.H"
#include "Fl_Wayland_Screen_Driver.H"
#include "Fl_Wayland_Graphics_Driver.H"

#include <wayland-client.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>

class Fl_Wayland_Text_Widget_Driver : public Fl_Text_Widget_Driver {
  GtkWidget *scrolled;
  GtkWidget *text_view;
  GtkWidget *window;
  struct wl_surface *wl_surface;
  struct wl_subsurface *wl_subsurface;
  Fl_Wayland_Graphics_Driver::wld_buffer *buffer;
  struct wld_window *fake_window;
  void draw_widget();
public:
  char *text_before_show;
  Fl_Wayland_Text_Widget_Driver();
  ~Fl_Wayland_Text_Widget_Driver();
  void show_widget() FL_OVERRIDE;
  void value(const char *t, int len) FL_OVERRIDE;
  const char *value() FL_OVERRIDE;
  void resize(int x, int y, int w, int h) FL_OVERRIDE;
};


Fl_Text_Widget_Driver *Fl_Text_Widget_Driver::newTextWidgetDriver(Fl_Native_Text_Widget *n) {
  Fl_Text_Widget_Driver *retval = (Fl_Text_Widget_Driver*)new Fl_Wayland_Text_Widget_Driver();
  retval->widget = n;
  return retval;
}


Fl_Wayland_Text_Widget_Driver::Fl_Wayland_Text_Widget_Driver() : Fl_Text_Widget_Driver() {
  text_before_show = NULL;
  text_view = NULL;
}

Fl_Wayland_Text_Widget_Driver::~Fl_Wayland_Text_Widget_Driver() {
  delete[] text_before_show;
}


void Fl_Wayland_Text_Widget_Driver::show_widget()  {
  static bool first = true;
  if (first) {
    gtk_init(NULL,NULL);
    first = false;
  }
  if (widget->window()->shown() && !text_view) {
    Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
    wl_surface = wl_compositor_create_surface(fl_wl_compositor());
    struct wld_window *parent = fl_wl_xid(widget->window());
    int scale = Fl_Wayland_Window_Driver::driver(widget->window())->wld_scale();
    wl_surface_set_buffer_scale(wl_surface, scale);
    wl_subsurface = wl_subcompositor_get_subsurface(scr_driver->wl_subcompositor,
                wl_surface, parent->wl_surface);
    wl_subsurface_set_position(wl_subsurface, widget->x() + BORDER_WIDTH, widget->y() + BORDER_WIDTH);
    wl_subsurface_set_desync(wl_subsurface); // important
    wl_subsurface_place_above(wl_subsurface, parent->wl_surface);
    window = gtk_offscreen_window_new();
        
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, (kind == Fl_Text_Widget_Driver::SINGLE_LINE || !widget->wrap()));
    gtk_widget_set_hexpand(scrolled, TRUE);
    text_view = gtk_text_view_new();
printf("show_widget() text_view=%p scrolled=%p scale=%d\n",text_view,scrolled,scale);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW (text_view),
      (kind == Fl_Text_Widget_Driver::SINGLE_LINE || !widget->wrap() ? GTK_WRAP_NONE: GTK_WRAP_WORD));
    gtk_container_add (GTK_CONTAINER (scrolled), text_view);
    
    if (text_before_show) {
      widget->value(text_before_show);
      delete[] text_before_show;
      text_before_show = NULL;
    }
    gtk_container_add(GTK_CONTAINER(window), scrolled);
    GtkAllocation allocation = {0,0, scale * (widget->w() - BORDER_WIDTH), scale * (widget->h() - BORDER_WIDTH)};
    gtk_widget_show_all(window);
    gtk_widget_size_allocate(scrolled, &allocation);
    if (widget->readonly()) gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), false);
    fake_window = new struct wld_window;
    memset(fake_window, 0, sizeof(struct wld_window));
    fake_window->wl_surface = wl_surface;
    fake_window->subsurface = wl_subsurface;
    fake_window->kind = Fl_Wayland_Window_Driver::SUBWINDOW;
    fake_window->fl_win = widget->top_window();
    fake_window->configured_width = scale * (widget->w() - BORDER_WIDTH);
    fake_window->configured_height = scale * (widget->h() - BORDER_WIDTH);
    buffer = new Fl_Wayland_Graphics_Driver::wld_buffer;
    memset(buffer, 0, sizeof(Fl_Wayland_Graphics_Driver::wld_buffer));
    buffer->draw_buffer.width = fake_window->configured_width;
    buffer->draw_buffer.stride = cairo_format_stride_for_width(
                                 Fl_Cairo_Graphics_Driver::cairo_format, buffer->draw_buffer.width);
    buffer->draw_buffer.data_size = buffer->draw_buffer.stride * fake_window->configured_height;
    Fl_Wayland_Graphics_Driver::create_shm_buffer(buffer);
    fake_window->buffer = buffer;
    Fl_Wayland_Graphics_Driver::cairo_init(&buffer->draw_buffer,
                                           buffer->draw_buffer.width,
                                           fake_window->configured_height,
                                           buffer->draw_buffer.stride,
                                           Fl_Cairo_Graphics_Driver::cairo_format);
    draw_widget();
  }
}


void Fl_Wayland_Text_Widget_Driver::draw_widget()  {
  //GtkAllocation allocation;
  //gtk_widget_get_allocation(GTK_WIDGET(scrolled), &allocation);
  //GtkStyleContext *style = gtk_widget_get_style_context(scrolled);
  //gtk_render_background(style, fake_window->buffer->draw_buffer.cairo_, allocation.x, allocation.y, allocation.width, allocation.height);
  gtk_widget_draw(scrolled, fake_window->buffer->draw_buffer.cairo_);
  Fl_Wayland_Graphics_Driver::buffer_commit(fake_window, NULL);
}


const char *Fl_Wayland_Text_Widget_Driver::value() {
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
  GtkTextIter first, last;
  gtk_text_buffer_get_start_iter(buffer, &first);
  gtk_text_buffer_get_end_iter(buffer, &last);
  return (const char *)gtk_text_buffer_get_text(buffer, &first, &last,true);
}


void Fl_Wayland_Text_Widget_Driver::value(const char *t, int len) {
  if (text_view) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    //gtk_text_buffer_set_text(buffer, t, len);
    GtkTextIter first;
    gtk_text_buffer_get_start_iter(buffer, &first);
    extern Fl_Fontdesc* fl_fonts;
    if (!fl_fonts) fl_fonts = Fl_Graphics_Driver::default_driver().calc_fl_fonts();
    const char *fname = (fl_fonts + widget->textfont())->name;
    int l = strlen(fname) + strlen(text_before_show) + 100;
    char *tmp = new char[l];
    uchar r, g, b;
    Fl::get_color(widget->textcolor(), r, g, b);
    int scale = Fl_Wayland_Window_Driver::driver(widget->window())->wld_scale();
    snprintf(tmp, l, "<span font='%s %d' fgcolor='#%.2X%.2X%.2X'>%s</span>", fname,
             scale * widget->textsize(), r, g, b, text_before_show);
    gtk_text_buffer_insert_markup(buffer, &first, tmp, -1);
    delete[] tmp;
  } else {
    if (text_before_show) delete[] text_before_show;
    text_before_show = new char[len+1];
    memcpy(text_before_show, t, len);
    text_before_show[len] = 0;
  }
}


void Fl_Wayland_Text_Widget_Driver::resize(int x, int y, int w, int h) {
  int scale = Fl_Wayland_Window_Driver::driver(widget->window())->wld_scale();
  wl_subsurface_set_position(wl_subsurface, x + BORDER_WIDTH, y + BORDER_WIDTH);
  Fl_Wayland_Graphics_Driver::buffer_release(fake_window);
  fake_window->configured_width = scale * (w - BORDER_WIDTH);
  fake_window->configured_height = scale * (h - BORDER_WIDTH);
  GtkAllocation allocation = {0,0, fake_window->configured_width, fake_window->configured_height};
  gtk_widget_size_allocate(scrolled, &allocation);
  buffer = new Fl_Wayland_Graphics_Driver::wld_buffer;
  memset(buffer, 0, sizeof(Fl_Wayland_Graphics_Driver::wld_buffer));
  fake_window->buffer = buffer;
  buffer->draw_buffer.width = fake_window->configured_width;
  buffer->draw_buffer.stride = cairo_format_stride_for_width(
                               Fl_Cairo_Graphics_Driver::cairo_format, buffer->draw_buffer.width);
  buffer->draw_buffer.data_size = buffer->draw_buffer.stride * fake_window->configured_height;
  Fl_Wayland_Graphics_Driver::create_shm_buffer(fake_window->buffer);
  Fl_Wayland_Graphics_Driver::cairo_init(&buffer->draw_buffer,
                                         buffer->draw_buffer.width,
                                         fake_window->configured_height,
                                         buffer->draw_buffer.stride,
                                         Fl_Cairo_Graphics_Driver::cairo_format);
  draw_widget();
}

