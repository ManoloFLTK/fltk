//
//  Fl_Wayland_Text_Widget_Drver.cxx
//

#include <config.h> // for BORDER_WIDTH
#include "../../Fl_Text_Widget_Driver.H"
#include <FL/Fl_Native_Text_Widget.H>
#include <FL/Fl_Image_Surface.H>
#include "../Cairo/Fl_Cairo_Graphics_Driver.H"
#include <FL/fl_draw.H>

#include <gtk/gtk.h>

class Fl_Cairo_Text_Widget_Driver : public Fl_Text_Widget_Driver {
  GtkWidget *scrolled;
  GtkWidget *text_view;
  GtkWidget *window;
  GtkAllocation allocation;
public:
  char *text_before_show;
  Fl_Cairo_Text_Widget_Driver();
  ~Fl_Cairo_Text_Widget_Driver();
  Fl_Native_Widget *as_native_widget() FL_OVERRIDE { return NULL; }
  void show_widget() FL_OVERRIDE;
  void value(const char *t, int len) FL_OVERRIDE;
  const char *value() FL_OVERRIDE;
  void resize(int x, int y, int w, int h) FL_OVERRIDE;
  void focus() FL_OVERRIDE;
  void unfocus() FL_OVERRIDE;
  void draw() FL_OVERRIDE;
};


Fl_Text_Widget_Driver *Fl_Text_Widget_Driver::newTextWidgetDriver(Fl_Native_Text_Widget *native) {
  Fl_Text_Widget_Driver *retval = (Fl_Text_Widget_Driver*)new Fl_Cairo_Text_Widget_Driver();
  retval->widget = native;
  return retval;
}


Fl_Cairo_Text_Widget_Driver::Fl_Cairo_Text_Widget_Driver() : Fl_Text_Widget_Driver() {
  text_before_show = NULL;
  text_view = NULL;
  memset(&allocation, 0, sizeof(GtkAllocation));
}

Fl_Cairo_Text_Widget_Driver::~Fl_Cairo_Text_Widget_Driver() {
  delete[] text_before_show;
  if (text_view) {
    gtk_widget_destroy(window);
  }
}


void Fl_Cairo_Text_Widget_Driver::show_widget()  {
  static bool first = true;
  if (first) {
    gtk_init(NULL,NULL);
    first = false;
  }
  if (widget->window()->shown() && !text_view) {
    window = gtk_offscreen_window_new();
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, (kind == Fl_Text_Widget_Driver::SINGLE_LINE || !widget->wrap()));
    gtk_widget_set_hexpand(scrolled, TRUE);
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), !widget->readonly() );
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), true);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view),
      (kind == Fl_Text_Widget_Driver::SINGLE_LINE || !widget->wrap() ? GTK_WRAP_NONE: GTK_WRAP_WORD));
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(text_view), false);
    gtk_widget_set_can_focus(text_view, !widget->readonly());
    uchar r,g,b;
    Fl::get_color(widget->color(),r,g,b);
    GdkRGBA bg = {r/255., g/255., b/255., 1.};
    gtk_widget_override_background_color(text_view, GTK_STATE_FLAG_NORMAL, &bg);
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    gtk_container_add(GTK_CONTAINER(window), scrolled);
    gtk_widget_show_all(window);
    if (text_before_show) widget->value(text_before_show);
    resize(widget->x(), widget->y(), widget->w(), widget->h());
  }
}


void Fl_Cairo_Text_Widget_Driver::draw()  {
  int tmp_width = widget->w() - 2 * BORDER_WIDTH;
  int tmp_height = widget->h() - 2 * BORDER_WIDTH;
  if (tmp_width != allocation.width || tmp_height != allocation.height) {
    allocation.width = tmp_width;
    allocation.height = tmp_height;
    gtk_widget_size_allocate(scrolled, &allocation);
  }
  Fl_Image_Surface *surface = new Fl_Image_Surface(tmp_width, tmp_height, 1);
  Fl_Surface_Device::push_current(surface);
  Fl_Cairo_Graphics_Driver *dr = (Fl_Cairo_Graphics_Driver*)surface->driver();
  //GtkStyleContext *style = gtk_widget_get_style_context(scrolled /*or window ?*/);
  //gtk_render_background(style, dr->cr(), 0, 0, allocation.width, allocation.height);
  gtk_widget_draw(scrolled, dr->cr());
  Fl_Surface_Device::pop_current();
  fl_copy_offscreen(widget->x() + BORDER_WIDTH, widget->y() + BORDER_WIDTH, tmp_width, tmp_height, surface->offscreen(), 0, 0);
  delete surface;
}


const char *Fl_Cairo_Text_Widget_Driver::value() {
  if (!text_view) return text_before_show ? text_before_show : "";
  else {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter first, last;
    gtk_text_buffer_get_start_iter(buffer, &first);
    gtk_text_buffer_get_end_iter(buffer, &last);
    return (const char *)gtk_text_buffer_get_text(buffer, &first, &last, true);
  }
}


void Fl_Cairo_Text_Widget_Driver::value(const char *t, int len) {
  if (!text_view) {
    char *new_text = new char[len+1];
    memcpy(new_text, t, len);
    new_text[len] = 0;
    if (text_before_show) delete[] text_before_show;
    text_before_show = new_text;
  } else {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    extern Fl_Fontdesc* fl_fonts;
    if (!fl_fonts) fl_fonts = Fl_Graphics_Driver::default_driver().calc_fl_fonts();
    const char *fname = (fl_fonts + widget->textfont())->name;
    int l = int(strlen(fname) + len + 100);
    char *tmp = new char[l];
    uchar r, g, b;
    Fl::get_color(widget->textcolor(), r, g, b);
    snprintf(tmp, l, "<span font='%s %d' fgcolor='#%.2X%.2X%.2X'>",
             fname, widget->textsize(), r, g, b);
    char *to = tmp + strlen(tmp);
    memcpy(to, t, len);
    to += len;
    strcpy(to, "</span>");
    gtk_text_buffer_set_text(buffer, "", 0);
    GtkTextIter first;
    gtk_text_buffer_get_start_iter(buffer, &first);
    gtk_text_buffer_insert_markup(buffer, &first, tmp, (int)strlen(tmp));
    delete[] tmp;
    if (text_before_show) {
      delete[] text_before_show;
      text_before_show = NULL;
    }
  }
}


void Fl_Cairo_Text_Widget_Driver::resize(int x, int y, int w, int h) {
  int new_width = (w - 2 * BORDER_WIDTH);
  int new_height = (h - 2 * BORDER_WIDTH);
  if (new_width != allocation.width || new_height != allocation.height) {
    allocation.width = new_width;
    allocation.height = new_height;
    gtk_widget_size_allocate(scrolled, &allocation);
  }
  widget->window()->make_current();
  draw();
}


void Fl_Cairo_Text_Widget_Driver::focus() {
  if (!widget->readonly() /*&& !gtk_widget_is_focus(text_view)*/) {
    printf("focus(%s)\n",widget->label());
    gtk_widget_grab_focus(text_view);
    widget->window()->make_current();
    draw();
printf("gtk_widget_has_focus=%d gtk_widget_is_focus=%d\n",
           gtk_widget_has_focus(text_view), gtk_widget_is_focus(text_view));
  }
}


void Fl_Cairo_Text_Widget_Driver::unfocus() {
  gtk_widget_set_can_focus(text_view, false);
printf("unfocus: gtk_widget_is_focus=%d\n",gtk_widget_is_focus(text_view));
}
