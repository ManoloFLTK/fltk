//
//  Fl_Wayland_Text_Widget_Driver.cxx
//

#include <config.h> // for BORDER_WIDTH
#include "../../Fl_Text_Widget_Driver.H"
#include <FL/Fl_Native_Text_Widget.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_Scrollbar.H>
#include "../Cairo/Fl_Cairo_Graphics_Driver.H"
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>          // fl_beep()

#include <gtk/gtk.h>

/* TODO
 - simplifier compute_lineheight() avec gtk_text_view_forward_display_line()
 - improve code to compute location of Fl_Scrollbar's in scene
 - handle wheel events
 - transmit Fl_Scrollbar changes to GtkScroller
 - finalize parameters of Fl_Scrollbar and GtkScroller
 - drag to select
 - drag-and-drop
 */

class Fl_Cairo_Text_Widget_Driver : public Fl_Text_Widget_Driver {
  GtkWidget *scrolled;
  GtkWidget *v_bar, *h_bar;
  Fl_Scrollbar *v_fl_scrollbar, *h_fl_scrollbar;
  GtkAdjustment *v_adjust, *h_adjust;
  GtkWidget *text_view;
  GtkTextBuffer *buffer;
  //PangoLayout *layout;
  GtkWidget *window;
  GtkTextTag* font_size_tag;
  GtkAllocation allocation;
  int lineheight;
  void text_view_scroll_mark_onscreen();
  void compute_lineheight();
  void text_view_scroll_mark_h(GtkTextIter *before);
public:
  char *text_before_show;
  int need_allocate;
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
  int handle_push() FL_OVERRIDE;
  int handle_paste() FL_OVERRIDE;
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
  font_size_tag = NULL;
  memset(&allocation, 0, sizeof(GtkAllocation));
  need_allocate = 0;
  v_bar = h_bar = NULL;
  v_adjust = h_adjust = NULL;
  v_fl_scrollbar = h_fl_scrollbar = NULL;
  lineheight = 0;
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

typedef void (*changed_f_t)(GtkTextBuffer*);
changed_f_t old_changed_f = NULL;
static void fl_textbuffer_changed(GtkTextBuffer *buffer) {
  //old_changed_f(buffer); //utile?
  GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
//printf("gtk_text_tag_table_get_size=%d\n",gtk_text_tag_table_get_size(table));
  GtkTextTag *tag;
  gtk_text_tag_table_foreach(table, mytagf, &tag); // there's only one element in the table
  gchar *strval;
  g_object_get(G_OBJECT(tag), "name", &strval, NULL);
  Fl_Cairo_Text_Widget_Driver *dr;
  sscanf(strval, "%p", &dr);
  free(strval);
  dr->need_allocate = 2;
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
      h_fl_scrollbar = new Fl_Scrollbar(0, 0, 1, 1, NULL);
      h_fl_scrollbar->type(FL_HORIZONTAL);
    }
    widget->end();
    if (v_fl_scrollbar) v_adjust = gtk_adjustment_new(0, 0, 10, 1, 1, 5); // temp values
    if (h_fl_scrollbar) h_adjust = gtk_adjustment_new(0, 0, 10, 1, 1, 5);
    scrolled = gtk_scrolled_window_new(h_adjust, v_adjust);
    text_view = gtk_text_view_new();
    if (v_fl_scrollbar) v_bar = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(scrolled));
    if (h_fl_scrollbar) h_bar = gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(scrolled));
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
      tbc->changed = fl_textbuffer_changed;
    }
    // Create a GtkTextTag to set the font+size for the text_view and attach it to the GtkTextBuffer
    // Name this tag with the address of corresponding Fl_Cairo_Text_Widget_Driver
    // This will allow fl_textbuffer_changed() to recover the Fl_Cairo_Text_Widget_Driver from the buffer
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
    
    if (text_before_show) widget->value(text_before_show);
    GtkTextIter iter;
    if (kind == Fl_Text_Widget_Driver::MULTIPLE_LINES) gtk_text_buffer_get_start_iter(buffer, &iter);
    else gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_place_cursor(buffer, &iter);
    draw();
  }
}


void Fl_Cairo_Text_Widget_Driver::draw()  {
  if (Fl_Window::current() != widget->window()) widget->window()->make_current();
  int tmp_width = widget->w() - 2 * BORDER_WIDTH - (v_bar ? gtk_widget_get_allocated_width(v_bar) : 0);
  // gtk_widget_get_allocated_height(h_bar) peut être incorrect ici et devenir valable apres
  // gtk_widget_size_allocate(scrolled,..)
  int tmp_height = widget->h() - 2 * BORDER_WIDTH - (h_bar ? gtk_widget_get_allocated_height(h_bar) : 0);
  if (h_bar && gtk_widget_get_allocated_height(h_bar) <= 1) h_fl_scrollbar->clear_visible();
  if (need_allocate || tmp_width != allocation.width || tmp_height != allocation.height) {
    allocation.width = tmp_width;
    allocation.height = tmp_height;
    gtk_widget_size_allocate(scrolled, &allocation);
    GtkAllocation alloc2;
    if (v_fl_scrollbar)  {
      gtk_widget_get_allocated_size(v_bar, &alloc2, NULL);
      if (alloc2.width > 1) {
        v_fl_scrollbar->resize(widget->x() + BORDER_WIDTH +
                                 (widget->right_to_left() ? 0 : allocation.width),
                               widget->y() + BORDER_WIDTH,
                               alloc2.width, allocation.height);
        v_fl_scrollbar->parent()->init_sizes();
        gdouble d = gtk_adjustment_get_upper(v_adjust);
        v_fl_scrollbar->value(v_fl_scrollbar->value(), allocation.height, 1, int (d));
      }
    }
    if (h_fl_scrollbar)  {
      gtk_widget_get_allocated_size(h_bar, &alloc2, NULL);
      if (alloc2.width > 1) {
        h_fl_scrollbar->resize(widget->x() + BORDER_WIDTH ,
                               widget->y() + BORDER_WIDTH + allocation.height,
                               allocation.width, alloc2.height);
//printf("%dx%d %dx%d widget->h()=%d allocation.height=%d tmp_height=%d\n",widget->x() + BORDER_WIDTH , widget->y() + BORDER_WIDTH + allocation.height,   allocation.width, alloc2.height, widget->h(), allocation.height,tmp_height);
        h_fl_scrollbar->parent()->init_sizes();
        gdouble d = gtk_adjustment_get_upper(h_adjust);
        h_fl_scrollbar->value(h_fl_scrollbar->value(), allocation.width, 1, int (d));
        if (need_allocate == 1) {
          h_fl_scrollbar->set_visible();
          h_fl_scrollbar->redraw();
          ((Fl_Widget*)h_fl_scrollbar)->draw();
        }
      }
    }
    gtk_widget_get_allocated_size(scrolled, &alloc2, NULL);
    if (need_allocate > 0) need_allocate--;
  }
  Fl_Image_Surface *surface = new Fl_Image_Surface(tmp_width, tmp_height, 1);
  Fl_Surface_Device::push_current(surface);
  Fl_Cairo_Graphics_Driver *dr = (Fl_Cairo_Graphics_Driver*)surface->driver();
  //gtk_render_background(style, dr->cr(), 0, 0, allocation.width, allocation.height);
  GtkTextIter insert, sel_end;
  bool selection_active = gtk_text_buffer_get_selection_bounds(buffer, &insert, &sel_end);
  if (Fl::focus() != widget) { // temporarily hide selection when widget not focused
    gtk_text_buffer_select_range(buffer, &insert, &insert);
  }
  gtk_widget_draw(scrolled, dr->cr());
  if (Fl::focus() != widget) {
    gtk_text_buffer_select_range(buffer, &insert, &sel_end);
  } else if (!selection_active) {
    GdkRectangle strong;
    gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), NULL, &strong, NULL);
    gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(text_view),
                                          GTK_TEXT_WINDOW_TEXT, strong.x, strong.y, &strong.x, &strong.y);
    // the native insertion cursor, but how to set its color?
    //GtkStyleContext *style = gtk_widget_get_style_context(text_view);
    //gtk_render_insertion_cursor(style, dr->cr(), strong.x, strong.y, layout, 0, PANGO_DIRECTION_RTL);
    fl_color(widget->cursor_color()); dr->line_style(FL_SOLID, 2);
    dr->yxline(strong.x, strong.y, strong.y + widget->textsize());
  }
  Fl_Surface_Device::pop_current();
  fl_copy_offscreen(widget->x() + BORDER_WIDTH +
                    (v_fl_scrollbar && widget->right_to_left() ? v_fl_scrollbar->w() : 0),
                    widget->y() + BORDER_WIDTH, tmp_width, tmp_height, surface->offscreen(), 0, 0);
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
    gtk_text_buffer_apply_tag(buffer, font_size_tag, &start, &end);
    if (text_before_show) {
      delete[] text_before_show;
      text_before_show = NULL;
    }
  }
}


void Fl_Cairo_Text_Widget_Driver::replace(int from, int to, const char *text, int len) {
  insert_position(from, to);
  if (from != to) gtk_text_buffer_delete_selection(buffer, true, true);
  if (!len) return;
  GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  bool need_apply_tag = gtk_text_iter_equal(&iter, &start) || !gtk_text_iter_in_range(&iter, &start, &end);
  gtk_text_buffer_insert(buffer, &iter, text, len);
  if (need_apply_tag) {
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_apply_tag(buffer, font_size_tag, &start, &end);
  }
  if (from < to || len > 0) {
    draw();
  }
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
      else replace(insert_position(), del ? insert_position()-del : mark(),
                   Fl::event_text(), Fl::event_length());
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
    int selected = (insert_position() != mark()) ? 1 : 0;
    if (mods == 0 && shift && selected) {
      GtkTextIter start, end; // copy_cut(): Shift-Delete with selection (WP,NP,WOW,GE,KE,OF)
      gtk_text_buffer_get_iter_at_mark(buffer, &start, gtk_text_buffer_get_insert(buffer));
      gtk_text_buffer_get_iter_at_mark(buffer, &end, gtk_text_buffer_get_selection_bound(buffer));
      gtk_text_buffer_select_range(buffer, &start, &end);
      const char *buf = gtk_text_buffer_get_text(buffer, &start, &end, false);
      Fl::copy(buf, (int)strlen(buf), 1);
      delete[] buf;
      gtk_text_buffer_delete(buffer, &start, &end);
      draw();
      return 1;
    } else if (mods == 0 && ((shift && !selected) || !shift)) {
       // Delete or Shift-Delete no selection (WP,NP,WOW,GE,KE,!OF)
      GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
      GtkTextIter iter, next;
      gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
      gtk_text_iter_assign(&next, &iter);
      gtk_text_iter_forward_char(&next);
      gtk_text_buffer_delete(buffer, &iter, &next);
      draw();
      return 1;
    } else if (mods == FL_CTRL) {// kf_delete_word_right()  Ctrl-Delete    (WP,!NP,WOW,GE,KE,!OF)
      GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
      GtkTextIter iter, next;
      gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
      gtk_text_iter_assign(&next, &iter);
      gtk_text_iter_forward_word_end(&next);
      gtk_text_buffer_delete(buffer, &iter, &next);
      draw();
      return 1;
    }
  } else if (Fl::event_key() == FL_BackSpace) {
    if (mods == 0) { // Backspace      (WP,NP,WOW,GE,KE,OF)
      if (gtk_text_buffer_get_has_selection(buffer)) {
        gtk_text_buffer_delete_selection(buffer, true, true);
      } else {
        GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
        gtk_text_buffer_backspace (buffer, &iter, true, true);
      }
      draw();
      retval = 1;
    }
    else if (mods == FL_CTRL) {//   kf_delete_word_left()  Ctrl-Backspace (WP,!NP,WOW,GE,KE,!OF)
      GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
      GtkTextIter iter, next;
      gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
      gtk_text_iter_assign(&next, &iter);
      gtk_text_iter_backward_word_start(&next);
      gtk_text_buffer_delete(buffer, &iter, &next);
      draw();
      return 1;
    }
  } else if (Fl::event_key() == 'a' && Fl::event_ctrl()) { // select All
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_move_mark(buffer, gtk_text_buffer_get_insert(buffer), &start);
    gtk_text_buffer_move_mark(buffer, gtk_text_buffer_get_selection_bound(buffer), &end);
    draw();
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
    const char *buf = gtk_text_buffer_get_text(buffer, &start, &end, false);
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
    Fl::paste(*widget, 1);
    return 1;
  } else if (Fl::event_key() ==
#if __APPLE_CC__
             '6'
#else
             'x'
#endif
             && Fl::event_ctrl()) { // cut
    GtkTextIter start, end;
    gtk_text_buffer_get_iter_at_mark(buffer, &start, gtk_text_buffer_get_insert(buffer));
    gtk_text_buffer_get_iter_at_mark(buffer, &end, gtk_text_buffer_get_selection_bound(buffer));
    gtk_text_buffer_select_range(buffer, &start, &end);
    const char *buf = gtk_text_buffer_get_text(buffer, &start, &end, false);
    Fl::copy(buf, (int)strlen(buf), 1);
    delete[] buf;
    gtk_text_buffer_delete_selection(buffer, true, !widget->readonly());
    draw();
    return 1;
  }
  if (retval >= 1) return retval;
  
  if (Fl::event_key() == FL_Right || Fl::event_key() == FL_Left) {
    GtkTextIter before, after;
    gtk_text_buffer_get_iter_at_mark(buffer, &before, gtk_text_buffer_get_insert(buffer));
    after = before;
    if (Fl::event_key() == (widget->right_to_left() ? FL_Right : FL_Left)) {
      gtk_text_iter_backward_char(&after);
    } else     {
      gtk_text_iter_forward_char(&after);
    }
    gtk_text_buffer_place_cursor(buffer, &after);
    if (v_fl_scrollbar) {
      if (!lineheight) compute_lineheight();
      text_view_scroll_mark_onscreen();
    }
    if (h_fl_scrollbar) {
      text_view_scroll_mark_h(&before);
    }
    draw();
    return 1;
  } else if (kind == MULTIPLE_LINES && (Fl::event_key() == FL_Down || Fl::event_key() == FL_Up)) {
    GtkTextIter where;
    GdkRectangle strong;
    if (!lineheight) compute_lineheight();
    gtk_text_buffer_get_iter_at_mark(buffer, &where, gtk_text_buffer_get_insert(buffer));
    gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), NULL, &strong, NULL);
    strong.y += lineheight * (Fl::event_key() == FL_Down ? +1 : -1);
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view), &where, strong.x-1, strong.y);
    gtk_text_buffer_place_cursor(buffer, &where);
    // after the cursor moved
    text_view_scroll_mark_onscreen();
    draw();
    return 1;
  }
  return 0;
}


void Fl_Cairo_Text_Widget_Driver::text_view_scroll_mark_onscreen() {
  GdkRectangle strong;
  gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), NULL, &strong, NULL);
  gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(text_view),
                                        GTK_TEXT_WINDOW_TEXT,
                                        strong.x, strong.y, &strong.x, &strong.y);
  if (strong.y > widget->h() - lineheight || strong.y < 0) {
    double d = gtk_adjustment_get_value(v_adjust);
    gtk_adjustment_set_value(v_adjust, d + lineheight * (strong.y < 0 ? -1 : +1));
    v_fl_scrollbar->value( (int)gtk_adjustment_get_value(v_adjust) );
    if (!need_allocate) need_allocate = 1; // important
  }
}


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
    h_fl_scrollbar->value( (int)gtk_adjustment_get_value(h_adjust) );
    if (!need_allocate) need_allocate = 1; // important
  }
}


void Fl_Cairo_Text_Widget_Driver::compute_lineheight() {
  if (!lineheight) {
    GtkTextIter keep, where;
    GdkRectangle strong, strong2;
    double factor = 1.3;
    bool b = false;
    gtk_text_buffer_get_iter_at_mark(buffer, &where, gtk_text_buffer_get_insert(buffer));
    keep = where;
    gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), NULL, &strong, NULL);
    do {
      strong2.y = strong.y + widget->textsize() * factor;
      b = gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view), &where, strong.x-1, strong2.y);
//printf("f=%.2f b=%d ",factor, b);
      gtk_text_buffer_place_cursor(buffer, &where);
      gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view), NULL, &strong2, NULL);
      lineheight = strong2.y - strong.y;
      factor *= 1.1;
    } while (b && !lineheight);
    gtk_text_buffer_place_cursor(buffer, &keep);
//printf("fontsize=%d lineheight=%d\n",widget->textsize(),lineheight);
  }
}


int Fl_Cairo_Text_Widget_Driver::handle_push() {
  GtkTextIter where, insert;
  double dv = (v_adjust ? gtk_adjustment_get_value(v_adjust) : 0);
  double dh = (h_adjust ? gtk_adjustment_get_value(h_adjust) : 0);
  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW(text_view), &where,
                                      Fl::event_x() - widget->x() + dh,
                                      Fl::event_y() - widget->y() + dv);
  gtk_text_buffer_get_iter_at_mark(buffer, &insert, gtk_text_buffer_get_insert(buffer));
  if (Fl::event_shift()) {
    gtk_text_buffer_select_range(buffer, &insert, &where);
  } else if (Fl::event_clicks()) {
    gtk_text_buffer_place_cursor(buffer, &where);
    gtk_text_iter_backward_word_start(&where);
    GtkTextIter word_end;
    gtk_text_iter_assign(&word_end, &where);
    gtk_text_iter_forward_word_end(&where);
    gtk_text_buffer_select_range(buffer, &where, &word_end);
  } else {
    gtk_text_buffer_place_cursor(buffer, &where);
  }
  draw();
  return 1;
}


int Fl_Cairo_Text_Widget_Driver::handle_paste() {
  GtkTextIter insert, start, end;
  gtk_text_buffer_delete_selection (buffer, true, !widget->readonly());
  gtk_text_buffer_get_iter_at_mark(buffer, &insert, gtk_text_buffer_get_insert(buffer));
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  bool need_apply_tag = gtk_text_iter_equal(&insert, &start) ||
    !gtk_text_iter_in_range(&insert, &start, &end);
  gtk_text_buffer_insert_interactive(buffer, &insert,
                                     Fl::event_text(), (gint)strlen(Fl::event_text()),
                                     !widget->readonly());
  if (need_apply_tag) {
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_apply_tag(buffer, font_size_tag, &start, &end);
  }
  draw();
  return 1;
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
  bool need_selection = (pos != mark);
  pos = byte_pos_to_char_pos(this, pos);
  GtkTextMark *gtk_mark = gtk_text_buffer_get_insert(buffer);
  GtkTextIter where;
  gtk_text_buffer_get_start_iter (buffer, &where);
  gtk_text_iter_set_offset(&where, pos);
  gtk_text_buffer_move_mark(buffer, gtk_mark, &where);
  if (!need_selection) {
    gtk_mark = gtk_text_buffer_get_selection_bound(buffer);
    gtk_text_buffer_move_mark(buffer, gtk_mark, &where);
    return;
  }
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

