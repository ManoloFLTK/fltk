//
//  Fl_Cairo_Text_Widget_Driver.cxx
//

#include "../../Fl_Text_Widget_Driver.H"
#include <FL/Fl_Native_Text_Widget.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_Scrollbar.H>
#include "../Cairo/Fl_Cairo_Graphics_Driver.H"
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>          // fl_beep()
#include "../../Fl_Screen_Driver.H"
#include <FL/fl_callback_macros.H>

#include <gtk/gtk.h>

/* TODO
 - implement undo/redo
 */

class Fl_Cairo_Text_Widget_Driver : public Fl_Text_Widget_Driver {
private:
  GtkWidget *scrolled;
  GtkWidget *v_bar, *h_bar;
  GtkAdjustment *v_adjust, *h_adjust;
  GtkWidget *text_view;
  GtkTextBuffer *buffer;
  GtkWidget *window;
  GtkTextTag* font_size_tag;
  GtkAllocation allocation;
  //PangoLayout *layout;
  Fl_Scrollbar *v_fl_scrollbar;
  Fl_Slider *h_fl_slider;
  char *text_before_show;
  double upper;
  int lineheight; // approx lineheight which exact value varies somewhat along text
  int need_allocate; // Number of times gtk_widget_size_allocate() needs be called before
                     // drawing GTK objects
  int insert_offset; // offset in drawing units from left margin to insertion point
  static const int h_slider_height = 10;
  static void textbuffer_changed(GtkTextBuffer *buffer);
  void text_view_scroll_mark_onscreen();
  void text_view_scroll_mark_h(GtkTextIter *before);
  void scan_all_paragraphs();
  static void scan_single_line(Fl_Cairo_Text_Widget_Driver *o);
  static void put_back_newlines(char *text);
  int byte_pos_to_char_pos(int pos);
  int char_pos_to_byte_pos(int pos);
  void show_widget() FL_OVERRIDE;
  void value(const char *t, int len) FL_OVERRIDE;
  void replace(int from, int to, const char *text, int len) FL_OVERRIDE;
  void replace_selection(const char *text, int len); // all changes to text go through this
  void resize(int x, int y, int w, int h) FL_OVERRIDE;
  void focus() FL_OVERRIDE;
  void unfocus() FL_OVERRIDE;
  void draw() FL_OVERRIDE;
  unsigned index(int i) const FL_OVERRIDE;
  int handle_keyboard() FL_OVERRIDE;
  int handle_mouse(int event) FL_OVERRIDE;
  int handle_paste() FL_OVERRIDE;
  int handle_dnd(int event) FL_OVERRIDE;
  int insert_position() FL_OVERRIDE;
  void insert_position(int pos, int mark) FL_OVERRIDE;
  int mark() FL_OVERRIDE;
public:
  Fl_Cairo_Text_Widget_Driver();
  ~Fl_Cairo_Text_Widget_Driver();
  const char *value() FL_OVERRIDE;
};


Fl_Text_Widget_Driver *Fl_Text_Widget_Driver::newTextWidgetDriver(Fl_Native_Text_Widget *native) {
  Fl_Text_Widget_Driver *retval = (Fl_Text_Widget_Driver*)new Fl_Cairo_Text_Widget_Driver();
  retval->widget = native;
  return retval;
}


Fl_Cairo_Text_Widget_Driver::Fl_Cairo_Text_Widget_Driver() : Fl_Text_Widget_Driver() {
  text_before_show = NULL;
  text_view = NULL;
  font_size_tag = NULL;
  memset(&allocation, 0, sizeof(GtkAllocation));
  need_allocate = 0;
  v_bar = h_bar = NULL;
  v_adjust = h_adjust = NULL;
  v_fl_scrollbar = NULL;
  h_fl_slider = NULL;
  lineheight = 0;
  upper = 0;
  insert_offset = -1; // means undefined
}

Fl_Cairo_Text_Widget_Driver::~Fl_Cairo_Text_Widget_Driver() {
  delete[] text_before_show;
  if (text_view) {
    gtk_widget_destroy(window);
  }
}


static void mytagf(GtkTextTag *tag, void *data) {
  GtkTextTag **val = (GtkTextTag**)data;
  *val = tag;
}


// compute the full width of the newly changed single-line text and adjust h_fl_slider to it
void Fl_Cairo_Text_Widget_Driver::scan_single_line(Fl_Cairo_Text_Widget_Driver *o) {
  Fl::remove_check((Fl_Timeout_Handler)scan_single_line, o);
  while (o->need_allocate > 0) o->draw();
  int val = gtk_adjustment_get_value(o->h_adjust);
  o->h_fl_slider->scrollvalue(val, o->h_fl_slider->w(), 0, gtk_adjustment_get_upper(o->h_adjust));
}


static void (*old_changed_f)(GtkTextBuffer*) = NULL;

void Fl_Cairo_Text_Widget_Driver::textbuffer_changed(GtkTextBuffer *buffer) {
  static bool busy = false;
  if (busy) return;
  GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
//printf("gtk_text_tag_table_get_size=%d\n",gtk_text_tag_table_get_size(table));
  GtkTextTag *tag;
  gtk_text_tag_table_foreach(table, mytagf, &tag); // there's only one element in the table
  gchar *strval;
  g_object_get(G_OBJECT(tag), "name", &strval, NULL);
  Fl_Cairo_Text_Widget_Driver *dr;
  sscanf(strval, "%p", &dr);
  free(strval);
  if (dr->kind == SINGLE_LINE) Fl::add_check((Fl_Timeout_Handler)scan_single_line, dr);
  dr->need_allocate = 2;
}


static void scroll_cb(Fl_Slider *sb, GtkAdjustment *adjust, int *p_need_allocate) {
  int v = sb->value();
  gtk_adjustment_set_value(adjust, v);
  *p_need_allocate = 1;
  sb->parent()->redraw();
}


void Fl_Cairo_Text_Widget_Driver::show_widget()  {
  static bool first = true;
  if (first) {
    gtk_init(NULL,NULL);
    first = false;
  }
  if (widget->window()->shown() && !text_view) {
    window = gtk_offscreen_window_new();
    widget->begin();
    if (kind == Fl_Text_Widget_Driver::MULTIPLE_LINES) {
      v_fl_scrollbar = new Fl_Scrollbar(0, 0, 1, 1, NULL);
    }
    if (kind == Fl_Text_Widget_Driver::SINGLE_LINE || !widget->wrap()) {
      h_fl_slider = new Fl_Slider(0, 0, 1, 1, NULL);
      h_fl_slider->type(FL_HORIZONTAL);
    }
    widget->end();
    if (v_fl_scrollbar) {
      v_adjust = gtk_adjustment_new(0, 0, 10, 1, 1, 5); // temp values
      FL_FUNCTION_CALLBACK_3(v_fl_scrollbar, scroll_cb,
                             Fl_Slider*, v_fl_scrollbar,
                             GtkAdjustment*, v_adjust,
                             int*, &need_allocate);
    }
    if (h_fl_slider) {
      h_adjust = gtk_adjustment_new(0, 0, 10, 1, 1, 5);
      FL_FUNCTION_CALLBACK_3(h_fl_slider, scroll_cb,
                             Fl_Slider*, h_fl_slider,
                             GtkAdjustment*, h_adjust,
                             int*, &need_allocate);
    }
    scrolled = gtk_scrolled_window_new(h_adjust, v_adjust);
    text_view = gtk_text_view_new();
    if (v_fl_scrollbar) v_bar = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(scrolled));
    if (h_fl_slider) h_bar = gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(scrolled));
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view), 3);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view), 3);
    //layout = gtk_widget_create_pango_layout(text_view, NULL);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), !widget->readonly() );
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), true);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view),
      (kind == Fl_Text_Widget_Driver::SINGLE_LINE || !widget->wrap() ? GTK_WRAP_NONE: GTK_WRAP_WORD));
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(text_view), false);
    gtk_widget_set_can_focus(text_view, !widget->readonly());
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    gtk_container_add(GTK_CONTAINER(window), scrolled);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
      (kind == Fl_Text_Widget_Driver::MULTIPLE_LINES && widget->wrap() ? GTK_POLICY_NEVER: GTK_POLICY_ALWAYS), //H
          (kind == Fl_Text_Widget_Driver::SINGLE_LINE ? GTK_POLICY_NEVER : GTK_POLICY_ALWAYS) //V
                                   );
    gtk_widget_show_all(window);
    
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextBufferClass *tbc = GTK_TEXT_BUFFER_GET_CLASS(buffer);
    if (!old_changed_f) {
      old_changed_f = tbc->changed;
      tbc->changed = textbuffer_changed;
    }
    // Create a GtkTextTag to set the font+size for the text_view and attach it to the GtkTextBuffer
    // Name this tag with the address of corresponding Fl_Cairo_Text_Widget_Driver
    // This will allow textbuffer_changed() to recover the Fl_Cairo_Text_Widget_Driver from the buffer
    char text_tag_name[20];
    snprintf(text_tag_name, 20, "%p", this);
    char font_str[88];
    extern Fl_Fontdesc* fl_fonts;
    if (!fl_fonts) fl_fonts = Fl_Graphics_Driver::default_driver().calc_fl_fonts();
    const char *fname = (fl_fonts + widget->textfont())->name;
    snprintf(font_str, sizeof(font_str), "%s %dpx", fname, widget->textsize());
    font_size_tag = gtk_text_buffer_create_tag(buffer, text_tag_name, "font", font_str, NULL);
    
    // use css to set all colors
    GtkStyleContext *style_context = gtk_widget_get_style_context(text_view);
    gtk_style_context_add_class(style_context, GTK_STYLE_CLASS_VIEW);
    uchar r,g,b;
    char bg_color_str[8];
    Fl::get_color(widget->color(),r,g,b);
    snprintf(bg_color_str, sizeof(bg_color_str), "#%2.2x%2.2x%2.2x", r, g, b);
    char color_str[8];
    Fl::get_color(widget->textcolor(), r, g, b);
    snprintf(color_str, sizeof(color_str), "#%2.2x%2.2x%2.2x", r, g, b);
    char sel_color_str[8];
    Fl::get_color(widget->selection_color(), r, g, b);
    snprintf(sel_color_str, sizeof(sel_color_str), "#%2.2x%2.2x%2.2x", r, g, b);
    char text_sel_color_str[8];
    Fl::get_color(fl_contrast(widget->textcolor(), widget->selection_color()), r, g, b);
    snprintf(text_sel_color_str, sizeof(text_sel_color_str), "#%2.2x%2.2x%2.2x", r, g, b);
    char line[200];
    snprintf(line, sizeof(line), 
             //"textview text { background-color: %s; color: %s; }"
             ".view text { background-color: %s; color: %s; }"
             ".view text selection { background-color: %s;  color: %s; }",
             bg_color_str, color_str, sel_color_str, text_sel_color_str);
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider, line, -1, NULL);
    gtk_style_context_add_provider (style_context, GTK_STYLE_PROVIDER(css_provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    if (text_before_show) {
      widget->value(text_before_show);
      delete text_before_show;
      text_before_show = NULL;
    }
    GtkTextIter iter;
    if (kind == Fl_Text_Widget_Driver::MULTIPLE_LINES) gtk_text_buffer_get_start_iter(buffer, &iter);
    else gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_place_cursor(buffer, &iter);
    lineheight = widget->textsize() * 1.2;
    draw();
  }
}


void Fl_Cairo_Text_Widget_Driver::draw()  {
  if (!widget->visible()) return;
  if (Fl_Window::current() != widget->window()) widget->window()->make_current();
  int text_width = widget->w() - Fl::box_dw(widget->box()) - (v_bar ? Fl::scrollbar_size() : 0);
  int text_height = widget->h() - Fl::box_dh(widget->box()) - (h_bar ? h_slider_height : 0);
  if (need_allocate || text_width != allocation.width || text_height != allocation.height) {
    allocation.width = text_width;
    allocation.height = text_height;
    gtk_widget_size_allocate(scrolled, &allocation);
    GtkAllocation alloc_v_scroll, alloc_h_scroll;
    if (v_fl_scrollbar) gtk_widget_get_allocated_size(v_bar, &alloc_v_scroll, NULL);
    if (h_fl_slider) gtk_widget_get_allocated_size(h_bar, &alloc_h_scroll, NULL);
    if (v_fl_scrollbar)  {
      v_fl_scrollbar->resize(widget->x() + Fl::box_dx(widget->box()) +
                             (widget->right_to_left() ? 0 : allocation.width),
                             widget->y() + Fl::box_dy(widget->box()),
                             Fl::scrollbar_size(), allocation.height);
      v_fl_scrollbar->parent()->init_sizes();
      if (need_allocate == 1) upper = gtk_adjustment_get_upper(v_adjust);
      v_fl_scrollbar->value(v_fl_scrollbar->value(), allocation.height, 1, int (upper));
      v_fl_scrollbar->linesize(lineheight);
    }
    if (h_fl_slider)  {
      h_fl_slider->resize(widget->x() + Fl::box_dx(widget->box()) ,
                             widget->y() + Fl::box_dy(widget->box()) + allocation.height,
                             allocation.width, h_slider_height);
      h_fl_slider->parent()->init_sizes();
      gdouble d = gtk_adjustment_get_upper(h_adjust);
      h_fl_slider->scrollvalue(h_fl_slider->value(), allocation.width, 0, int(d));
    }
    if (need_allocate > 0) need_allocate--;
  }
  Fl_Image_Surface *surface = new Fl_Image_Surface(allocation.width, allocation.height, 1);
  Fl_Surface_Device::push_current(surface);
  Fl_Cairo_Graphics_Driver *dr = (Fl_Cairo_Graphics_Driver*)surface->driver();
  GtkTextIter insert, sel_end;
  bool selection_active = gtk_text_buffer_get_selection_bounds(buffer, &insert, &sel_end);
  if (Fl::focus() != widget) { // temporarily hide selection when widget not focused
    gtk_text_buffer_select_range(buffer, &insert, &insert);
  }
  gtk_widget_draw(scrolled, dr->cr());
  if (Fl::focus() != widget) {
    gtk_text_buffer_select_range(buffer, &insert, &sel_end);
  } else if (!selection_active) { // draw insertion cursor
    GdkRectangle strong;
    gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), NULL, &strong, NULL);
    gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(text_view),
                                          GTK_TEXT_WINDOW_TEXT, strong.x, strong.y, &strong.x, &strong.y);
    // the native insertion cursor, but how to set its color?
    //GtkStyleContext *style = gtk_widget_get_style_context(text_view);
    //gtk_render_insertion_cursor(style, dr->cr(), strong.x, strong.y, layout, 0, PANGO_DIRECTION_RTL);
    fl_color(widget->cursor_color()); dr->line_style(FL_SOLID, 2);
    dr->yxline(strong.x, strong.y, strong.y + lineheight);
  }
  Fl_Surface_Device::pop_current();
  fl_copy_offscreen(widget->x() + Fl::box_dx(widget->box()) +
                    (v_fl_scrollbar && widget->right_to_left() ? v_fl_scrollbar->w() : 0),
                    widget->y() + Fl::box_dy(widget->box()), text_width, text_height, surface->offscreen(), 0, 0);
  delete surface;
  widget->damage(FL_DAMAGE_CHILD); // important
}


const char *Fl_Cairo_Text_Widget_Driver::value() {
  if (!text_view) return strdup(text_before_show ? text_before_show : "");
  else {
    GtkTextIter first, last;
    gtk_text_buffer_get_start_iter(buffer, &first);
    gtk_text_buffer_get_end_iter(buffer, &last);
    return (const char *)gtk_text_buffer_get_text(buffer, &first, &last, true);
  }
}


/*
 Called when the text buffer's content is fully changed.
 Moves across all text, paragraph by paragraph, and compute position of its beginning;
 sets v_adjust and v_fl_scrollbar parameters accordingly.
 This makes GTK and FLTK aware of the full size of the widget's formatted text.
 */
void Fl_Cairo_Text_Widget_Driver::scan_all_paragraphs() {
  GtkTextIter start, end, current;
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_adjustment_set_upper(v_adjust, 0);
  while (need_allocate) draw();
  upper = 0;
//printf("end=%d chars upper=%.1f\n",gtk_text_iter_get_offset(&end), upper);
  current = start;
  do {
//printf("current=%d\n",gtk_text_iter_get_offset(&current));
    /*gboolean b =*/ gtk_text_iter_forward_line(&current);
//printf("next line=%d b=%d\n",gtk_text_iter_get_offset(&current),b);
    GdkRectangle strong;
    gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), &current, &strong, NULL);
    if (strong.y > upper) {
      upper = strong.y + 1;
//printf("upper=%.1f\n",upper);
      gtk_adjustment_set_upper(v_adjust, upper);
      gtk_adjustment_set_value(v_adjust, upper);
    }
  }  while(gtk_text_iter_compare(&current, &end) < 0);
  v_fl_scrollbar->maximum(upper);
  gtk_adjustment_set_value(v_adjust, 0);
  v_fl_scrollbar->value(0);
  gtk_text_buffer_place_cursor(buffer, &start);
  need_allocate = 1;
  draw();
//printf("upper=%.1f v_fl_scrollbar=%d\n",gtk_adjustment_get_upper(v_adjust), v_fl_scrollbar->value());
}


void Fl_Cairo_Text_Widget_Driver::value(const char *t, int len) {
  if (!text_view) {
    char *new_text = new char[len+1];
    memcpy(new_text, t, len);
    new_text[len] = 0;
    if (text_before_show) delete[] text_before_show;
    text_before_show = new_text;
  } else {
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_select_range(buffer, &start, &end);
    replace_selection(t, len);
  }
}


static char *substitute_with_cr_ht(const char *text, int &len) {
  // Replace \n and \t in text by ␍ and ␉. Return modified string
  int new_len = len + 1000;
  char *new_text = (char*)malloc(new_len);
  const char *p = text - 1, *end = text + len;
  char *q = new_text;
  while (++p < end) {
    if (q - new_text + 4 >= new_len) {
      new_len += 100;
      new_text = (char*)realloc(new_text, new_len);
    }
    if (*p == '\n') {
      memcpy(q, "␍", 3);
      q += 3;
    } else if (*p == '\t') {
      memcpy(q, "␉", 3);
      q += 3;
    } else *q++ = *p;
  }
  *q = 0;
  len = int(q - new_text);
  return new_text;
}


// ALL changes to the widget's text go through this function
void Fl_Cairo_Text_Widget_Driver::replace_selection(const char *text, int len) {
  if (gtk_text_buffer_get_has_selection(buffer)) {
    gtk_text_buffer_delete_selection(buffer, true, true);
  }
  bool full_replace = gtk_text_buffer_get_char_count(buffer) == 0;
  if (len > 0) {
    GtkTextIter iter, start, end;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    bool need_apply_tag = gtk_text_iter_equal(&iter, &start) || gtk_text_iter_equal(&iter, &end);
    char *new_text = (char*)text;
    if (kind == SINGLE_LINE) new_text = substitute_with_cr_ht(text, len);
    gtk_text_buffer_insert(buffer, &iter, new_text, len);
    if (kind == SINGLE_LINE) free(new_text);
    if (need_apply_tag) {
      gtk_text_buffer_get_start_iter(buffer, &start);
      gtk_text_buffer_get_end_iter(buffer, &end);
      gtk_text_buffer_apply_tag(buffer, font_size_tag, &start, &end);
    }
  }
  if (full_replace && kind == Fl_Text_Widget_Driver::MULTIPLE_LINES) scan_all_paragraphs();
  widget->redraw();
}


void Fl_Cairo_Text_Widget_Driver::replace(int from, int to, const char *text, int len) {
  insert_position(from, to);
  replace_selection(text, len);
}


void Fl_Cairo_Text_Widget_Driver::resize(int x, int y, int w, int h) {
  draw();
}


void Fl_Cairo_Text_Widget_Driver::focus() {
  if (!widget->readonly()) {
    //printf("focus(%s)\n",widget->label());
    widget->redraw();
  }
}


void Fl_Cairo_Text_Widget_Driver::unfocus() {
  //printf("unfocus(%s)\n",widget->label());
  widget->redraw();
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
      else {
        if (del) {
          int insert = insert_position();
          replace(insert, insert - del, NULL, 0);
        } else {
          replace_selection(Fl::event_text(), Fl::event_length());
        }
      }
    }
    /*if (Fl::screen_driver()->has_marked_text() && Fl::compose_state) {
     this->mark( this->insert_position() - Fl::compose_state );
     }*/
    return 1;
  }
  int mods = Fl::event_state() & (FL_META|FL_CTRL|FL_ALT);
  unsigned int shift = Fl::event_state() & FL_SHIFT;
  int retval = 0;
  if (Fl::event_key() == FL_Delete) {
    if (widget->readonly()) { fl_beep(); return 1; }
    int selected = gtk_text_buffer_get_has_selection(buffer);
    if (mods == 0 && shift && selected) {
      GtkTextIter start, end; // copy_cut(): Shift-Delete with selection (WP,NP,WOW,GE,KE,OF)
      gtk_text_buffer_get_iter_at_mark(buffer, &start, gtk_text_buffer_get_insert(buffer));
      gtk_text_buffer_get_iter_at_mark(buffer, &end, gtk_text_buffer_get_selection_bound(buffer));
      char *buf = gtk_text_buffer_get_text(buffer, &start, &end, false);
      if (kind == SINGLE_LINE) put_back_newlines(buf);
      Fl::copy(buf, (int)strlen(buf), 1);
      delete[] buf;
      replace_selection(NULL, 0);
      return 1;
    } else if (mods == 0 && ((shift && !selected) || !shift)) {
       // Delete or Shift-Delete no selection (WP,NP,WOW,GE,KE,!OF)
      GtkTextIter iter, next;
      gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
      gtk_text_iter_assign(&next, &iter);
      gtk_text_iter_forward_char(&next);
      gtk_text_buffer_select_range(buffer, &iter, &next);
      replace_selection(NULL, 0);
      return 1;
    } else if (mods == FL_CTRL) {// kf_delete_word_right()  Ctrl-Delete    (WP,!NP,WOW,GE,KE,!OF)
      GtkTextIter iter, next;
      gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
      gtk_text_iter_assign(&next, &iter);
      gtk_text_iter_forward_word_end(&next);
      gtk_text_buffer_select_range(buffer, &iter, &next);
      replace_selection(NULL, 0);
      return 1;
    }
  } else if (Fl::event_key() == FL_BackSpace) {
    if (widget->readonly()) { fl_beep(); return 1; }
    if (mods == 0) { // Backspace      (WP,NP,WOW,GE,KE,OF)
      if (gtk_text_buffer_get_has_selection(buffer)) {
        replace_selection(NULL, 0);
      } else {
        GtkTextIter iter, previous;
        gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
        gtk_text_iter_assign(&previous, &iter);
        gtk_text_iter_backward_cursor_position(&previous);
        gtk_text_buffer_select_range(buffer, &previous, &iter);
        replace_selection(NULL, 0);
      }
      retval = 1;
    }
    else if (mods == FL_CTRL) {//   kf_delete_word_left()  Ctrl-Backspace (WP,!NP,WOW,GE,KE,!OF)
      GtkTextIter iter, next;
      gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
      gtk_text_iter_assign(&next, &iter);
      gtk_text_iter_backward_word_start(&next);
      gtk_text_buffer_select_range(buffer, &next, &iter);
      replace_selection(NULL, 0);
      return 1;
    }
  } else if (Fl::event_key() == 'a' && Fl::event_ctrl() && widget->selectable()) { // select All
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_move_mark(buffer, gtk_text_buffer_get_insert(buffer), &start);
    gtk_text_buffer_move_mark(buffer, gtk_text_buffer_get_selection_bound(buffer), &end);
    widget->redraw();
    return 1;
  } else if (Fl::event_key() ==
#if __APPLE_CC__
             '7'
#else
             'c'
#endif
             && Fl::event_ctrl()) { // copy
    GtkTextIter start, end;
    gtk_text_buffer_get_iter_at_mark(buffer, &start, gtk_text_buffer_get_insert(buffer));
    gtk_text_buffer_get_iter_at_mark(buffer, &end, gtk_text_buffer_get_selection_bound(buffer));
    gtk_text_buffer_select_range(buffer, &start, &end);
    char *buf = gtk_text_buffer_get_text(buffer, &start, &end, false);
    if (kind == SINGLE_LINE) put_back_newlines(buf);
    Fl::copy(buf, (int)strlen(buf), 1);
    delete[] buf;
    return 1;
  } else if (Fl::event_key() ==
#if __APPLE_CC__
             '8'
#else
             'v'
#endif
             && Fl::event_ctrl()) { // paste
    if (widget->readonly()) fl_beep();
    else Fl::paste(*widget, 1);
    return 1;
  } else if (Fl::event_key() ==
#if __APPLE_CC__
             '6'
#else
             'x'
#endif
             && Fl::event_ctrl()) { // cut
    if (widget->readonly()) { fl_beep(); return 1; }
    GtkTextIter start, end;
    gtk_text_buffer_get_iter_at_mark(buffer, &start, gtk_text_buffer_get_insert(buffer));
    gtk_text_buffer_get_iter_at_mark(buffer, &end, gtk_text_buffer_get_selection_bound(buffer));
    gtk_text_buffer_select_range(buffer, &start, &end);
    char *buf = gtk_text_buffer_get_text(buffer, &start, &end, false);
    if (kind == SINGLE_LINE) put_back_newlines(buf);
    Fl::copy(buf, (int)strlen(buf), 1);
    delete[] buf;
    replace_selection(NULL, 0);
    return 1;
  }
  if (retval >= 1) return retval;
  
  if (Fl::event_key() == FL_Right || Fl::event_key() == FL_Left) {
    GtkTextIter before, after;
    gtk_text_buffer_get_iter_at_mark(buffer, &before, gtk_text_buffer_get_insert(buffer));
    after = before;
    insert_offset = -1;
    if (Fl::event_key() == (widget->right_to_left() ? FL_Right : FL_Left)) {
      gtk_text_iter_backward_char(&after);
    } else     {
      gtk_text_iter_forward_char(&after);
    }
    gtk_text_buffer_place_cursor(buffer, &after);
    if (v_fl_scrollbar) {
      text_view_scroll_mark_onscreen();
    }
    if (h_fl_slider) {
      text_view_scroll_mark_h(&before);
    }
    widget->redraw();
    return 1;
  } else if (Fl::event_key() == FL_Down || Fl::event_key() == FL_Up) {
    if (kind == SINGLE_LINE) return 1;
    GtkTextIter where;
    GdkRectangle strong;
    gtk_text_buffer_get_iter_at_mark(buffer, &where, gtk_text_buffer_get_insert(buffer));
    if (insert_offset < 0) {
      gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), &where, &strong, NULL);
      insert_offset = strong.x;
    }
    if (Fl::event_key() == FL_Down)
      gtk_text_view_forward_display_line(GTK_TEXT_VIEW(text_view), &where);
    else
      gtk_text_view_backward_display_line(GTK_TEXT_VIEW(text_view), &where);
    gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), &where, &strong, NULL);
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view), &where, insert_offset, strong.y);
    gtk_text_buffer_place_cursor(buffer, &where);
    // after the cursor moved
    text_view_scroll_mark_onscreen();
    widget->redraw();
    return 1;
  } else if (Fl::event_key() == FL_Enter && kind == MULTIPLE_LINES && !widget->readonly()) {
    replace_selection("\n", 1);
    text_view_scroll_mark_onscreen();
    return 1;
  }
  return 0;
}


// Scrolls the view vertically so the insertion cursor is visible
void Fl_Cairo_Text_Widget_Driver::text_view_scroll_mark_onscreen() {
  GdkRectangle strong, strong2;
  gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), NULL, &strong, NULL);
  gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(text_view),
                                        GTK_TEXT_WINDOW_TEXT,
                                        strong.x, strong.y, &strong2.x, &strong2.y);
  if (strong2.y > allocation.height - lineheight || strong2.y < 0) {
    if (strong2.y > allocation.height - lineheight && strong.y+ lineheight/2 > upper) {
      upper = strong.y + lineheight/2;
      gtk_adjustment_set_upper(v_adjust, upper);
    }
    int val = gtk_adjustment_get_value(v_adjust);
    if (strong2.y < 0) val += strong2.y;
    else val += strong2.y + lineheight - allocation.height;
    gtk_adjustment_set_value(v_adjust, val);
    v_fl_scrollbar->value( (int)gtk_adjustment_get_value(v_adjust) );
    if (!need_allocate) {
      gtk_widget_size_allocate(scrolled, &allocation);
      upper = gtk_adjustment_get_upper(v_adjust);
      v_fl_scrollbar->value(v_fl_scrollbar->value(), allocation.height, 1, int (upper));
    }
  }
}


// Scrolls the view horizontally after left or right arrow key was pushed
// so the insertion cursor is visible
void Fl_Cairo_Text_Widget_Driver::text_view_scroll_mark_h(GtkTextIter *before) {
  GdkRectangle strong, before_rect;
  gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), NULL, &strong, NULL);
  gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), before, &before_rect, NULL);
  int charwidth = strong.x - before_rect.x;
  gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(text_view),
                                        GTK_TEXT_WINDOW_TEXT,
                                        strong.x, strong.y, &strong.x, &strong.y);
  if (strong.x > allocation.width || strong.x < 0) {
    double d = gtk_adjustment_get_value(h_adjust);
    gtk_adjustment_set_value(h_adjust, d + charwidth);
    h_fl_slider->value( (int)gtk_adjustment_get_value(h_adjust) );
    if (!need_allocate) need_allocate = 1; // important
  }
}


static GtkTextIter dnd_iter_position, dnd_iter_mark, newpos;
static Fl_Widget *dnd_save_focus = NULL;
static int drag_start = -1;

int Fl_Cairo_Text_Widget_Driver::handle_mouse(int event) {
  if (v_fl_scrollbar && Fl::event_inside(v_fl_scrollbar)) return v_fl_scrollbar->handle(event);
  if (h_fl_slider && Fl::event_inside(h_fl_slider)) return h_fl_slider->handle(event);
  double dv = (v_adjust ? gtk_adjustment_get_value(v_adjust) : 0);
  double dh = (h_adjust ? gtk_adjustment_get_value(h_adjust) : 0);
  int v_scroll_w = (v_fl_scrollbar && widget->right_to_left() ? v_fl_scrollbar->w() : 0);

  if (event == FL_PUSH) {
    if (Fl::dnd_text_ops() && (Fl::event_button() != FL_RIGHT_MOUSE) && widget->selectable()) {
      GtkTextIter oldpos, oldmark;
      gtk_text_buffer_get_selection_bounds(buffer, &oldpos, &oldmark);
      // move insertion point to Fl::event_x(), Fl::event_y()
      GtkTextIter where;
      double dv = (v_adjust ? gtk_adjustment_get_value(v_adjust) : 0);
      double dh = (h_adjust ? gtk_adjustment_get_value(h_adjust) : 0);
      gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view), &where,
        Fl::event_x() - (widget->x() + Fl::box_dx(widget->box()) + v_scroll_w) + dh,
        Fl::event_y() - (widget->y() + Fl::box_dy(widget->box())) + dv);
      gtk_text_buffer_place_cursor(buffer, &where);

      newpos = where;
      gtk_text_buffer_select_range(buffer, &oldpos, &oldmark);
      GtkTextIter current_insert, current_mark;
      gtk_text_buffer_get_selection_bounds(buffer, &current_insert, &current_mark);
      if (Fl::focus()==widget && !Fl::event_state(FL_SHIFT)  &&
          ( (gtk_text_iter_compare(&newpos, &current_mark)>=0 && gtk_text_iter_compare(&newpos, &current_insert)<0) ||
           (gtk_text_iter_compare(&newpos, &current_insert)>=0 && gtk_text_iter_compare(&newpos, &current_mark)<0)
           ) ) {
        // user clicked in the selection, may be trying to drag
        drag_start = gtk_text_iter_get_offset(&newpos);
        return 1;
      }
      drag_start = -1;
    }
    
    GtkTextIter where, insert;
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view), &where,
      Fl::event_x() - (widget->x() + Fl::box_dx(widget->box()) + v_scroll_w) + dh,
      Fl::event_y() - (widget->y() + Fl::box_dy(widget->box())) + dv);
    gtk_text_buffer_get_iter_at_mark(buffer, &insert, gtk_text_buffer_get_insert(buffer));
    if (Fl::event_shift() && widget->selectable()) {
      gtk_text_buffer_select_range(buffer, &insert, &where);
    } else if (Fl::event_clicks() && widget->selectable()) {
      gtk_text_buffer_place_cursor(buffer, &where);
      if (Fl::event_clicks() >= 2) {
        gtk_text_iter_forward_line(&where);
        GtkTextIter begin_line;
        gtk_text_iter_assign(&begin_line, &where);
        gtk_text_iter_backward_line(&where);
        gtk_text_buffer_select_range(buffer, &begin_line, &where);
      } else {
        gtk_text_iter_backward_word_start(&where);
        GtkTextIter word_end;
        gtk_text_iter_assign(&word_end, &where);
        gtk_text_iter_forward_word_end(&where);
        gtk_text_buffer_select_range(buffer, &where, &word_end);
      }
    } else {
      gtk_text_buffer_place_cursor(buffer, &where);
      insert_offset = -1;
    }
    widget->redraw();
    return 1;
  } else if(event == FL_DRAG && widget->selectable()) {
    if (drag_start >= 0) {
      if (Fl::event_is_click()) return 1; // debounce the mouse
      // save the position because sometimes we don't get DND_ENTER:
      gtk_text_buffer_get_iter_at_mark(buffer, &dnd_iter_position,
                                       gtk_text_buffer_get_insert(buffer));
      gtk_text_buffer_get_iter_at_mark(buffer, &dnd_iter_mark,
                                       gtk_text_buffer_get_selection_bound(buffer));
      dnd_save_focus = widget;
      // drag the data:
      const char *buf = gtk_text_buffer_get_text(buffer, &dnd_iter_position, &dnd_iter_mark, false);
      Fl::copy(buf, (int)strlen(buf), 0);
      delete[] buf;
      Fl::screen_driver()->dnd(1);
      return 1;
    }
    // make selection follow the pointer
    GtkTextIter where, insert;
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view), &where,
      Fl::event_x() - (widget->x() + Fl::box_dx(widget->box()) + v_scroll_w) + dh,
      Fl::event_y() - (widget->y() + Fl::box_dy(widget->box())) + dv);
    gtk_text_buffer_get_iter_at_mark(buffer, &insert, gtk_text_buffer_get_insert(buffer));
    gtk_text_buffer_select_range(buffer, &insert, &where);
    widget->redraw();
    return 1;
    
  } else if(event == FL_RELEASE) {
    if (Fl::event_is_click() && drag_start >= 0) {
      // user clicked in the field and wants to reset the cursor position...
      GtkTextIter current;
      gtk_text_buffer_get_iter_at_offset(buffer, &current, drag_start);
      gtk_text_buffer_select_range(buffer, &current, &current);
      drag_start = -1;
      widget->window()->cursor(FL_CURSOR_INSERT);
      return 1;
    }
  } else if((event == FL_ENTER || event == FL_MOVE) && !widget->readonly()) {
    bool b = Fl::event_inside(
            widget->x() + Fl::box_dx(widget->box()), widget->y() + Fl::box_dy(widget->box()),
            widget->w() - Fl::box_dw(widget->box()), widget->h() - Fl::box_dh(widget->box()));
    if (b && v_fl_scrollbar && Fl::event_inside(v_fl_scrollbar)) b = false;
    if (b && h_fl_slider && Fl::event_inside(h_fl_slider)) b = false;
    widget->window()->cursor(b ? FL_CURSOR_INSERT : FL_CURSOR_DEFAULT);
    return 1;
  } else if(event == FL_LEAVE) {
    widget->window()->cursor(FL_CURSOR_DEFAULT);
    return 1;
  } else if(event == FL_MOUSEWHEEL) {
    if (v_fl_scrollbar) {
      int val = v_fl_scrollbar->value() + lineheight * Fl::event_dy();
      int changed = v_fl_scrollbar->value( v_fl_scrollbar->clamp(val) );
      if (changed) v_fl_scrollbar->do_callback();
    }
    if (h_fl_slider) {
      int val = h_fl_slider->value() + widget->textsize() * Fl::event_dx();
      int changed = h_fl_slider->value( h_fl_slider->clamp(val) );
      if (changed) h_fl_slider->do_callback();
    }
    return 1;
  }
  return 0;
}


int Fl_Cairo_Text_Widget_Driver::handle_paste() {
  if (widget->readonly()) fl_beep();
  else replace_selection(Fl::event_text(), Fl::event_length());
  return 1;
}


// Given a position in byte units in the text, returns the corresponding
// position in character units of the character containing said byte.
int Fl_Cairo_Text_Widget_Driver::byte_pos_to_char_pos(int pos) {
  const char *text = value(), *p = text, *end = text + strlen(text);
  int len, char_count = 0;
  while (true) {
    fl_utf8decode(p , end, &len);
    p += len;
    if (p - text > pos) break;
    char_count++;
  }
  delete[] text;
  return char_count;
}


// Given a position in character units in the text, returns the corresponding
// position in byte units.
int Fl_Cairo_Text_Widget_Driver::char_pos_to_byte_pos(int char_count) {
  const char *text;
  if (!text_view) {
    text = text_before_show;
    if (!text) return 0;
    const char *p = text, *end = text + strlen(text);
    int len;
    while (char_count > 0) {
      fl_utf8decode(p , end, &len);
      char_count--;
      p += len;
    }
    return int(p - text);
  } else if (!char_count) {
    return 0;
  } else {
    GtkTextIter first, last;
    gtk_text_buffer_get_start_iter(buffer, &first);
    gtk_text_buffer_get_iter_at_offset(buffer, &last, char_count);
    text = gtk_text_buffer_get_text(buffer, &first, &last, true);
    int l = (int)strlen(text);
    delete[] text;
    return l;
  }
}


int Fl_Cairo_Text_Widget_Driver::insert_position() {
  GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
  int pos = gtk_text_iter_get_offset(&iter);
  return char_pos_to_byte_pos(pos);
}


void Fl_Cairo_Text_Widget_Driver::insert_position(int pos, int mark) {
  bool need_selection = (pos != mark);
  pos = byte_pos_to_char_pos(pos);
  GtkTextMark *gtk_mark = gtk_text_buffer_get_insert(buffer);
  GtkTextIter where;
  gtk_text_iter_set_offset(&where, pos);
  gtk_text_buffer_move_mark(buffer, gtk_mark, &where);
  if (!need_selection) {
    gtk_mark = gtk_text_buffer_get_selection_bound(buffer);
    gtk_text_buffer_move_mark(buffer, gtk_mark, &where);
    return;
  }
  mark = byte_pos_to_char_pos(mark);
  gtk_text_iter_set_offset(&where, mark);
  gtk_mark = gtk_text_buffer_get_selection_bound(buffer);
  gtk_text_buffer_move_mark(buffer, gtk_mark, &where);
}


int Fl_Cairo_Text_Widget_Driver::mark() {
  GtkTextMark *mark = gtk_text_buffer_get_selection_bound(buffer);
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
  int pos = gtk_text_iter_get_offset(&iter);
  return char_pos_to_byte_pos(pos);
}


int Fl_Cairo_Text_Widget_Driver::handle_dnd(int event) {
  switch (event) {
    case FL_DND_ENTER:
      Fl::belowmouse(widget); // send the leave events first
      if (dnd_save_focus != widget) {
        gtk_text_buffer_get_iter_at_mark(buffer, &dnd_iter_position,
                                         gtk_text_buffer_get_insert(buffer));
        gtk_text_buffer_get_iter_at_mark(buffer, &dnd_iter_mark,
                                         gtk_text_buffer_get_selection_bound(buffer));
        dnd_save_focus = Fl::focus();
        Fl::focus(widget);
        widget->handle(FL_FOCUS);
      }
      // fall through:
    case FL_DND_DRAG:
      if (Fl::event_inside(widget)) {
        // move insertion point to Fl::event_x(), Fl::event_y()
        GtkTextIter where;
        double dv = (v_adjust ? gtk_adjustment_get_value(v_adjust) : 0);
        double dh = (h_adjust ? gtk_adjustment_get_value(h_adjust) : 0);
        int v_scroll_w = (v_fl_scrollbar && widget->right_to_left() ? v_fl_scrollbar->w() : 0);
        bool b = gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view), &where,
          Fl::event_x() - (widget->x() + Fl::box_dx(widget->box()) + v_scroll_w) + dh,
          Fl::event_y() - (widget->y() + Fl::box_dy(widget->box())) + dv);
        if (b) {
          gtk_text_buffer_place_cursor(buffer, &where);
          widget->redraw();
        }
      }
      return 1;
      
    case FL_DND_LEAVE:
      GtkTextIter start, end;
      gtk_text_buffer_get_start_iter(buffer, &start);
      gtk_text_buffer_get_end_iter(buffer, &end);
      if (gtk_text_iter_in_range(&dnd_iter_position, &start, &end) &&
          gtk_text_iter_in_range(&dnd_iter_mark, &start, &end)) {
        gtk_text_buffer_select_range(buffer, &dnd_iter_position, &dnd_iter_mark);
      }
      if (dnd_save_focus && dnd_save_focus != widget) {
        Fl::focus(dnd_save_focus);
        widget->handle(FL_UNFOCUS);
      }
      Fl::first_window()->cursor(FL_CURSOR_MOVE);
      dnd_save_focus = NULL;
      return 1;
      
    case FL_DND_RELEASE:
      if (dnd_save_focus == widget) {
        if (!widget->readonly()) {
          // remove the selected text
          GtkTextIter old_iter;
          gtk_text_buffer_get_iter_at_mark(buffer, &old_iter, gtk_text_buffer_get_insert(buffer));
          if (gtk_text_iter_compare(&dnd_iter_mark, &dnd_iter_position) > 0) {
            GtkTextIter tmp_iter = dnd_iter_mark;
            dnd_iter_mark = dnd_iter_position;
            dnd_iter_position = tmp_iter;
          }
          gtk_text_buffer_select_range(buffer, &dnd_iter_mark, &dnd_iter_position);
          replace_selection(NULL, 0);
        }
      } else if (dnd_save_focus) {
        dnd_save_focus->handle(FL_UNFOCUS);
      }
      dnd_save_focus = NULL;
      widget->take_focus();
      widget->window()->cursor(FL_CURSOR_INSERT);
      return 1;
  }
  return 0;
}


// Replace ␍ (U+240d) characters in text by \n  and ␉ characters by \t.
// Done only when performing a copy operation from a single-line widget.
void Fl_Cairo_Text_Widget_Driver::put_back_newlines(char *text) {
  // ␍ or runs of ␍ are found bracketted by U+202c / U+202b
  // we also remove these
  const int lcr = 3; // number of bytes to encode U+240D, U+202c, or U+202b
  char RLE_utf8[4];
  fl_utf8encode(0x202b, RLE_utf8); // U+202b = right-to-left embedding (RLE)
  char PDF_utf8[4];
  fl_utf8encode(0x202c, PDF_utf8); // U+202c = pop directional formatting (PDF)
  char *p;
  int lskip;
  long l = strlen(text);
  char *save_text = text;
  while ( l > 0 && (p = strstr(text, "␍"))) { // search for ␍
    lskip = lcr;
    char *q = p + lcr;
    int newlines = 1; // number of successive ␍ characters
    while (memcmp(q, "␍", lcr) == 0) { q += lcr; lskip += lcr; newlines++; }
    if (memcmp(q, RLE_utf8, lcr) == 0) { lskip += lcr; } // remove U+202b after
    if (p-3 >= text && memcmp(p-3, PDF_utf8, lcr) == 0) { // remove U+202c before
      p -= lcr;
      lskip += lcr;
    }
    memset(p, '\n', newlines);
    memmove(p+newlines, p + lskip, l - (p + lskip - text) + 1);
    long offset = p + newlines - text;
    text += offset;
    l -= offset;
  }
  
  text = save_text;
  l = strlen(text);
  while ( l > 0 && (p = strstr(text, "␉"))) { // search for ␉
    *p = '\t';
    memmove(p + 1, p + lcr, l - (p + lcr - text) + 1);
    long offset = p + 1 - text;
    text += offset;
    l -= offset;
  }
}
