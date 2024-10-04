//
//  Fl_Wayland_Text_Widget_Drver.cxx
//

#include <config.h> // for BORDER_WIDTH
#include "../../Fl_Text_Widget_Driver.H"
#include <FL/Fl_Native_Text_Widget.H>
#include <FL/Fl_Image_Surface.H>
#include "../Cairo/Fl_Cairo_Graphics_Driver.H"
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>          // fl_beep()

#include <gtk/gtk.h>

class Fl_Cairo_Text_Widget_Driver : public Fl_Text_Widget_Driver {
  GtkWidget *scrolled;
  GtkWidget *text_view;
  GtkTextBuffer *buffer;
  //PangoLayout *layout;
  GtkWidget *window;
  GtkTextTag* font_size_color_tag;
  GtkAllocation allocation;
public:
  char *text_before_show;
  Fl_Cairo_Text_Widget_Driver();
  ~Fl_Cairo_Text_Widget_Driver();
  void show_widget() FL_OVERRIDE;
  void value(const char *t, int len) FL_OVERRIDE;
  void replace(int from, int to, const char *text, int len) FL_OVERRIDE;
  const char *value() FL_OVERRIDE;
  void resize(int x, int y, int w, int h) FL_OVERRIDE;
  void focus() FL_OVERRIDE;
  void unfocus() FL_OVERRIDE;
  void draw() FL_OVERRIDE;
  unsigned index(int i) const FL_OVERRIDE;
  int handle_keyboard() FL_OVERRIDE;
  int insert_position() FL_OVERRIDE;
  void insert_position(int pos, int mark) FL_OVERRIDE;
  int mark() FL_OVERRIDE;
};


Fl_Text_Widget_Driver *Fl_Text_Widget_Driver::newTextWidgetDriver(Fl_Native_Text_Widget *native) {
  Fl_Text_Widget_Driver *retval = (Fl_Text_Widget_Driver*)new Fl_Cairo_Text_Widget_Driver();
  retval->widget = native;
  return retval;
}


Fl_Cairo_Text_Widget_Driver::Fl_Cairo_Text_Widget_Driver() : Fl_Text_Widget_Driver() {
  text_before_show = NULL;
  text_view = NULL;
  font_size_color_tag = NULL;
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
    //layout = gtk_widget_create_pango_layout(text_view, NULL);
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
    
    /* Use a tag to change the color for all the widget */
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    char color_str[8];
    Fl::get_color(widget->textcolor(), r, g, b);
    snprintf(color_str, sizeof(color_str), "#%2.2x%2.2x%2.2x", r, g, b);
    char font_str[88];
    extern Fl_Fontdesc* fl_fonts;
    if (!fl_fonts) fl_fonts = Fl_Graphics_Driver::default_driver().calc_fl_fonts();
    const char *fname = (fl_fonts + widget->textfont())->name;
    snprintf(font_str, sizeof(font_str), "%s %dpx", fname, widget->textsize());
    font_size_color_tag = gtk_text_buffer_create_tag(buffer, NULL,
                      "foreground", color_str, "font", font_str, NULL);
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_apply_tag(buffer, font_size_color_tag, &start, &end);
    
    if (text_before_show) widget->value(text_before_show);
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_place_cursor(buffer, &start);
    resize(widget->x(), widget->y(), widget->w(), widget->h());
  }
}

/* attempt for provider
 GtkCssProvider  *cssProvider = gtk_css_provider_new();
 const char css[] = "FlTextView { color: red; }";
 gtk_css_provider_load_from_data(cssProvider, css, -1, NULL);
 gtk_style_context_add_provider(style, GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
 */

void Fl_Cairo_Text_Widget_Driver::draw()  {
  int tmp_width = widget->w() - 2 * BORDER_WIDTH;
  int tmp_height = widget->h() - 2 * BORDER_WIDTH;
  if (1|| tmp_width != allocation.width || tmp_height != allocation.height) {//TODO improve test?
    allocation.width = tmp_width;
    allocation.height = tmp_height;
    gtk_widget_size_allocate(scrolled, &allocation);
  }
  Fl_Image_Surface *surface = new Fl_Image_Surface(tmp_width, tmp_height, 1);
  Fl_Surface_Device::push_current(surface);
  Fl_Cairo_Graphics_Driver *dr = (Fl_Cairo_Graphics_Driver*)surface->driver();
  //gtk_render_background(style, dr->cr(), 0, 0, allocation.width, allocation.height);
  gtk_widget_draw(scrolled, dr->cr());
  if (Fl::focus() == widget) {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
    GdkRectangle strong, weak;
    gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), &iter, &strong, &weak);
    // the native insertion crusor, but how to set its color?
    //GtkStyleContext *style = gtk_widget_get_style_context(text_view);
    //gtk_render_insertion_cursor(style, dr->cr(), strong.x, strong.y, layout, 0, PANGO_DIRECTION_RTL);
    fl_color(widget->cursor_color()); dr->line_style(FL_SOLID, 2);
    dr->yxline(strong.x, strong.y, strong.y + widget->textsize());
  }
  Fl_Surface_Device::pop_current();
  fl_copy_offscreen(widget->x() + BORDER_WIDTH, widget->y() + BORDER_WIDTH, tmp_width, tmp_height, surface->offscreen(), 0, 0);
  delete surface;
  widget->damage(FL_DAMAGE_CHILD); // important
}


const char *Fl_Cairo_Text_Widget_Driver::value() {
  if (!text_view) return text_before_show ? text_before_show : "";
  else {
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
    gtk_text_buffer_set_text(buffer, t, len);
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter (buffer, &start);
    gtk_text_buffer_get_end_iter (buffer, &end);
    gtk_text_buffer_apply_tag(buffer, font_size_color_tag, &start, &end);
    if (text_before_show) {
      delete[] text_before_show;
      text_before_show = NULL;
    }
  }
}


void Fl_Cairo_Text_Widget_Driver::replace(int from, int to, const char *text, int len) {
  insert_position(from, to);
  gtk_text_buffer_delete_selection(buffer, true, true);
  GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
  gtk_text_buffer_insert(buffer, &iter, text, len);
  if (from < to || len > 0) {
    draw();
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
//printf("gtk_widget_has_focus=%d gtk_widget_is_focus=%d\n",
//           gtk_widget_has_focus(text_view), gtk_widget_is_focus(text_view));
  }
}


void Fl_Cairo_Text_Widget_Driver::unfocus() {
  printf("unfocus(%s)\n",widget->label());
  draw();
}


unsigned Fl_Cairo_Text_Widget_Driver::index(int i) const {
  const char *s = widget->value();
  int len = (int)strlen(s);
  unsigned r = (i >= 0 && i < len ? fl_utf8decode(s + i, s + len, &len) : 0);
  delete[] s;
  return r;
}


int Fl_Cairo_Text_Widget_Driver::handle_keyboard() {
  int del;
  if (Fl::compose(del)) {
    if (del || Fl::event_length()) {
      if (widget->readonly()) fl_beep();
      else replace(insert_position(), del ? insert_position()-del : mark(),
                   Fl::event_text(), Fl::event_length());
    }
    /*if (Fl::screen_driver()->has_marked_text() && Fl::compose_state) {
     this->mark( this->insert_position() - Fl::compose_state );
     }*/
    return 1;
  }
  if (Fl::e_keysym == FL_Right || Fl::e_keysym == FL_Left) {
    GtkTextIter where;
    gtk_text_buffer_get_iter_at_mark(buffer, &where, gtk_text_buffer_get_insert(buffer));
    if (Fl::e_keysym == FL_Right) {
      gtk_text_iter_backward_char(&where);
    } else     {
      gtk_text_iter_forward_char(&where);
    }
    gtk_text_buffer_place_cursor(buffer, &where);
    draw();
    return 1;
  }
  return 0;
}


static int byte_pos_to_char_pos(Fl_Cairo_Text_Widget_Driver *dr, int pos) {
  const char *text = dr->value(), *p = text, *end = text + strlen(text);
  int len, char_count = 0;
  while (p - text < pos) {
    fl_utf8decode(p , end, &len);
    char_count++;
    p += len;
  }
  delete[] text;
  return char_count;
}


static int char_pos_to_byte_pos(Fl_Cairo_Text_Widget_Driver *dr, int char_count) {
  const char *text = dr->value(), *p = text, *end = text + strlen(text);
  int len;
  while (char_count > 0) {
    fl_utf8decode(p , end, &len);
    char_count--;
    p += len;
  }
  delete[] text;
  return int(p - text);
}


int Fl_Cairo_Text_Widget_Driver::insert_position() {
  GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
  int pos = gtk_text_iter_get_offset(&iter);
  return char_pos_to_byte_pos(this, pos);
}


void Fl_Cairo_Text_Widget_Driver::insert_position(int pos, int mark) {
  pos = byte_pos_to_char_pos(this, pos);
  GtkTextMark *gtk_mark = gtk_text_buffer_get_insert(buffer);
  GtkTextIter where;
  gtk_text_buffer_get_start_iter (buffer, &where);
  gtk_text_iter_set_offset(&where, pos);
  gtk_text_buffer_move_mark(buffer, gtk_mark, &where);
  
  mark = byte_pos_to_char_pos(this, mark);
  gtk_text_iter_set_offset(&where, mark);
  gtk_mark = gtk_text_buffer_get_selection_bound(buffer);
  gtk_text_buffer_move_mark(buffer, gtk_mark, &where);
}


int Fl_Cairo_Text_Widget_Driver::mark() {
  GtkTextMark *mark = gtk_text_buffer_get_selection_bound(buffer);
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
  int pos = gtk_text_iter_get_offset(&iter);
  return char_pos_to_byte_pos(this, pos);
}

