//
// Fl_Cairo_Native_Input_Driver for the Fast Light Tool Kit (FLTK).
//
// Copyright 2025 by Bill Spitzak and others.
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

#include "../../Fl_Native_Input_Driver.H"
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/fl_callback_macros.H>
#include "Fl_Cairo_Graphics_Driver.H"
#include <FL/fl_ask.H>          // fl_beep()
#include "../../Fl_Screen_Driver.H"

#include <gtk/gtk.h>

#if GTK_MAJOR_VERSION != 3 && GTK_MAJOR_VERSION != 4
# error GTK3 or GTK4 required
#endif

class Fl_GTK3_Text_Undo_Action;
class Fl_GTK3_Text_Undo_Action_List;

class Fl_Cairo_Native_Input_Driver : public Fl_Native_Input_Driver {
private:
  GtkWidget *scrolled_;
  GtkAdjustment *v_adjust_, *h_adjust_;
  GtkWidget *text_view_;
  GtkTextBuffer *buffer_;
  GtkWidget *window_;
  GtkTextTag* font_size_tag_;
  GtkAllocation allocation_;
  GtkCssProvider *css_provider_;
  Fl_Scrollbar *v_fl_scrollbar_;
  Fl_Slider *h_fl_slider_;
  Fl_GTK3_Text_Undo_Action* undo_;
  Fl_GTK3_Text_Undo_Action_List* undo_list_;
  Fl_GTK3_Text_Undo_Action_List* redo_list_;
  char *text_before_show_;
  char class_name_part_[60];
  double upper_;
  int lineheight_; // approx line height which exact value varies somewhat along text
  int insert_offset_; // offset in drawing units from left margin to insertion point
  static const int slider_thickness_ = 10;
  static void textbuffer_changed_(GtkTextBuffer *buffer_, Fl_Cairo_Native_Input_Driver*);
  static void adjustment_changed_(GtkAdjustment *adj, Fl_Cairo_Native_Input_Driver*);
  static void adjustment_value_changed_(GtkAdjustment *adj, Fl_Cairo_Native_Input_Driver*);
  static void draw_child(Fl_Widget& widget);
  static void update_child(Fl_Widget& widget);
#if GTK_MAJOR_VERSION == 4
  static void delayed_set_style_(Fl_Cairo_Native_Input_Driver *dr);
#endif
  void text_view_scroll_mark_onscreen_(bool relative_to_mark = false);
  void text_view_scroll_mark_h_(GtkTextIter *before);
  void scan_all_paragraphs_();
  static void put_back_newlines_(char *text);
  static void delayed_cursor_at_extremity_(Fl_Cairo_Native_Input_Driver *o);
  int char_pos_to_byte_pos_(int pos);
  int apply_undo_();
  void set_style_();
public:
  Fl_Cairo_Native_Input_Driver();
  ~Fl_Cairo_Native_Input_Driver();
  void show_widget() FL_OVERRIDE;
  void value(const char *t, int len) FL_OVERRIDE;
  void static_value(const char *t, int len) FL_OVERRIDE;
  void replace(int from, int to, const char *text, int len) FL_OVERRIDE;
  void replace_selection(const char *text, int len) FL_OVERRIDE;
  const char *value() FL_OVERRIDE;
  int handle_focus(int) FL_OVERRIDE;
  void resize() FL_OVERRIDE;
  void draw() FL_OVERRIDE;
  unsigned index(int i) const FL_OVERRIDE;
  int copy() FL_OVERRIDE;
  void paste() FL_OVERRIDE;
  int handle_keyboard() FL_OVERRIDE;
  int handle_mouse(int event) FL_OVERRIDE;
  int handle_paste() FL_OVERRIDE;
  int handle_dnd(int event) FL_OVERRIDE;
  int insert_position() FL_OVERRIDE;
  void insert_position(int pos, int mark) FL_OVERRIDE;
  int mark() FL_OVERRIDE;
  int undo() FL_OVERRIDE;
  int redo() FL_OVERRIDE;
  bool can_undo() const FL_OVERRIDE;
  bool can_redo() const FL_OVERRIDE;
  void right_to_left() FL_OVERRIDE;
  void hide_widget() FL_OVERRIDE;
  void select_all() FL_OVERRIDE;
  void textfontandsize() FL_OVERRIDE;
  void textcolor() FL_OVERRIDE;
  int copy_cuts() FL_OVERRIDE;
};


class Fl_GTK3_Text_Undo_Action {
public:
  char *undobuffer;
  int undobufferlength;    // in byte unit
  int undoat_chars;        // insertion position in GTKtext in char unit
  int undocut;             // number of bytes deleted there; these bytes are in undobuffer
  int undocut_chars;       // same in char unit
  int undoinsert;          // number of bytes inserted; these bytes are in undobuffer
  int undoinsert_chars;    // same in char unit
  int undoyankcut_chars;   // char length of valid contents of undobuffer, even if undocut=0
  Fl_GTK3_Text_Undo_Action();
  ~Fl_GTK3_Text_Undo_Action();
  void undobuffersize(int n);
  void clear();
};


class Fl_GTK3_Text_Undo_Action_List {
  Fl_GTK3_Text_Undo_Action** list_;
  int list_size_;
  int list_capacity_;
public:
  Fl_GTK3_Text_Undo_Action_List();
  ~Fl_GTK3_Text_Undo_Action_List();
  int size() const;
  void push(Fl_GTK3_Text_Undo_Action* action);
  Fl_GTK3_Text_Undo_Action* pop();
  void clear();
};


static int calc_char_length(const char *text, int len) {
  const char *end = text + len;
  int l, retval = 0;
  while (text < end) {
    fl_utf8decode(text, end, &l);
    text += l;
    retval++;
  }
  return retval;
}


Fl_Native_Input_Driver *Fl_Native_Input_Driver::newNativeInputDriver(Fl_Native_Input *native) {
  Fl_Native_Input_Driver *retval = (Fl_Native_Input_Driver*)new Fl_Cairo_Native_Input_Driver();
  retval->widget = native;
  retval->widget->maximum_size(INT32_MAX);
  return retval;
}


Fl_Cairo_Native_Input_Driver::Fl_Cairo_Native_Input_Driver() : Fl_Native_Input_Driver() {
  text_before_show_ = NULL;
  text_view_ = NULL;
  font_size_tag_ = NULL;
  css_provider_ = NULL;
  memset(&allocation_, 0, sizeof(GtkAllocation));
  v_adjust_ = h_adjust_ = NULL;
  v_fl_scrollbar_ = NULL;
  h_fl_slider_ = NULL;
  undo_list_ = new Fl_GTK3_Text_Undo_Action_List();
  redo_list_ = new Fl_GTK3_Text_Undo_Action_List();
  undo_ = new Fl_GTK3_Text_Undo_Action();
  lineheight_ = 0;
  upper_ = 0;
  insert_offset_ = -1; // means undefined
  strcpy(class_name_part_, "textview");
#if GTK_MAJOR_VERSION == 4
  strcat(class_name_part_,  ".Fl_Native_Input");
  snprintf(class_name_part_ + strlen(class_name_part_),
           sizeof(class_name_part_) - strlen(class_name_part_), "%p", this);
#endif
}

Fl_Cairo_Native_Input_Driver::~Fl_Cairo_Native_Input_Driver() {
  delete[] text_before_show_;
  if (text_view_) {
#if GTK_MAJOR_VERSION == 3
    if (css_provider_) {
      GtkStyleContext *style_context = gtk_widget_get_style_context(text_view_);
      gtk_style_context_remove_provider(style_context, GTK_STYLE_PROVIDER(css_provider_));
      g_object_unref(css_provider_);
    }
    gtk_widget_destroy(window_);
#else
    gtk_window_destroy((GtkWindow*)window_);
#endif
  }
  if (v_fl_scrollbar_) delete v_fl_scrollbar_;
  if (h_fl_slider_) delete h_fl_slider_;
}


void Fl_Cairo_Native_Input_Driver::textbuffer_changed_(GtkTextBuffer *buffer_, Fl_Cairo_Native_Input_Driver *dr) {
  if (dr->widget->changed()) {
    if (dr->widget->when() & FL_WHEN_CHANGED) {
      dr->widget->do_callback(FL_REASON_CHANGED);
    }
  }
}


void Fl_Cairo_Native_Input_Driver::adjustment_changed_(GtkAdjustment *adj, Fl_Cairo_Native_Input_Driver *dr) {
  if (adj == dr->h_adjust_)
    dr->h_fl_slider_->scrollvalue(gtk_adjustment_get_value(adj), dr->allocation_.width, 0,
                                  int(gtk_adjustment_get_upper(adj)));
  else if (adj == dr->v_adjust_) {
    //printf("adjustment_changed_ to upper %g value %g\n",gtk_adjustment_get_upper(adj),gtk_adjustment_get_value(adj));
    int maxi = int(gtk_adjustment_get_upper(adj)) - dr->allocation_.height;
    int val = gtk_adjustment_get_value(adj);
    if (maxi < val) val = maxi;
    dr->v_fl_scrollbar_->value(val, dr->allocation_.height, 0, int(gtk_adjustment_get_upper(adj)));
    dr->v_fl_scrollbar_->linesize(dr->lineheight_);
  }
}


void Fl_Cairo_Native_Input_Driver::adjustment_value_changed_(GtkAdjustment *adj, Fl_Cairo_Native_Input_Driver *dr) {
  if (adj == dr->h_adjust_)
    dr->h_fl_slider_->value(gtk_adjustment_get_value(adj));
  else if (adj == dr->v_adjust_) {
    //printf("adjustment_value_changed_ to %g\n",gtk_adjustment_get_value(adj));
    dr->v_fl_scrollbar_->value(gtk_adjustment_get_value(adj));
  }
}


static void scroll_cb(Fl_Slider *sb, GtkAdjustment *adjust, Fl_Native_Input *native) {
  int v = sb->value();
  gtk_adjustment_set_value(adjust, v);
  Fl::focus(native);
  native->redraw();
}


void Fl_Cairo_Native_Input_Driver::textfontandsize()  {
  if (text_view_) {
    const char *content = value();
#if GTK_MAJOR_VERSION == 3
    gtk_widget_destroy(window_);
#else
    gtk_window_destroy((GtkWindow*)window_);
#endif
    text_view_ = NULL;
    delete v_fl_scrollbar_;
    v_fl_scrollbar_ = NULL;
    delete h_fl_slider_;
    h_fl_slider_ = NULL;
    allocation_.width = 0; // important
    resize();
    value(content, (int)strlen(content));
    show_widget();
  }
}


void Fl_Cairo_Native_Input_Driver::textcolor()  {
  if (text_view_) {
    set_style_();
    widget->damage(FL_DAMAGE_CHILD);
  }
}


void Fl_Cairo_Native_Input_Driver::set_style_()  {
  // use css to set all colors
#if GTK_MAJOR_VERSION == 3
  GtkStyleContext *style_context = gtk_widget_get_style_context(text_view_);
  //gtk_style_context_add_class(style_context, GTK_STYLE_CLASS_VIEW);
#endif
  uchar r,g,b;
  char bg_color_str[8];
  Fl::get_color(widget->color(),r,g,b);
  snprintf(bg_color_str, sizeof(bg_color_str), "#%2.2x%2.2x%2.2x", r, g, b);
  char caret_color_str[10];
  Fl::get_color(widget->cursor_color(), r, g, b);
#if GTK_MAJOR_VERSION == 3
  snprintf(caret_color_str, sizeof(caret_color_str), "#%2.2x%2.2x%2.2x", r, g, b);
#else
  uchar a = (widget == Fl::focus() ? 0xFF : 0);
  snprintf(caret_color_str, sizeof(caret_color_str), "#%2.2x%2.2x%2.2x%2.2x", r, g, b, a);
#endif
  char sel_color_str[8];
  Fl::get_color(widget->selection_color(), r, g, b);
  snprintf(sel_color_str, sizeof(sel_color_str), "#%2.2x%2.2x%2.2x", r, g, b);
  char text_sel_color_str[8];
  Fl::get_color(fl_contrast(widget->textcolor(), widget->selection_color()), r, g, b);
  snprintf(text_sel_color_str, sizeof(text_sel_color_str), "#%2.2x%2.2x%2.2x", r, g, b);
  char line[400];
  snprintf(line, sizeof(line),
           "%s { caret-color: %s; } "
           "%s text { background-color: %s; } "
           "%s text selection { background-color: %s;  color: %s; }",
           class_name_part_, caret_color_str,
           class_name_part_, bg_color_str,
           class_name_part_, sel_color_str, text_sel_color_str);
#if GTK_MAJOR_VERSION == 3
  if (css_provider_) {
    gtk_style_context_remove_provider(style_context, GTK_STYLE_PROVIDER(css_provider_));
    g_object_unref(css_provider_);
  }
#endif
  css_provider_ = gtk_css_provider_new();
#if GTK_MAJOR_VERSION == 3 || (GTK_MAJOR_VERSION == 4 && GTK_MINOR_VERSION < 12)
  gtk_css_provider_load_from_data(css_provider_, line, -1 , NULL);
#else
  gtk_css_provider_load_from_string(css_provider_, line);
#endif
#if GTK_MAJOR_VERSION == 3
  gtk_style_context_add_provider(style_context, GTK_STYLE_PROVIDER(css_provider_),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#else
  gtk_style_context_add_provider_for_display(gdk_display_get_default(),
          GTK_STYLE_PROVIDER(css_provider_), GTK_STYLE_PROVIDER_PRIORITY_USER);
  //TODO shd we use gtk_style_context_remove_provider_for_display() ?
  g_object_unref(css_provider_);
  css_provider_ = NULL;
#endif
  // Set the text color
  Fl::get_color(widget->active() ? widget->textcolor() : fl_inactive(widget->textcolor()), r, g, b);
  GdkRGBA text_rgba = {r/255.f, g/255.f, b/255.f, 1.f};
  g_object_set(font_size_tag_, "foreground-rgba", &text_rgba, NULL);
}


void Fl_Cairo_Native_Input_Driver::show_widget()  {
  static bool first = true;
  if (first) {
#if GTK_MAJOR_VERSION == 3
    gtk_init(NULL, NULL);
#else
    gtk_init();
#endif
    first = false;
  }
  if (widget->window()->shown() && !text_view_) {
#if GTK_MAJOR_VERSION == 3
    window_ = gtk_offscreen_window_new();
#else
    window_ = gtk_window_new();
#endif
    Fl_Group *g = Fl_Group::current();
    Fl_Group::current(NULL);
    if (kind == Fl_Native_Input_Driver::MULTIPLE_LINES) {
      v_fl_scrollbar_ = new Fl_Scrollbar(0, 0, 1, 1, NULL);
    }
    if (kind == Fl_Native_Input_Driver::SINGLE_LINE || !widget->wrap()) {
      h_fl_slider_ = new Fl_Slider(0, 0, 1, 1, NULL);
      h_fl_slider_->type(FL_HORIZONTAL);
    }
    Fl_Group::current(g);
    if (v_fl_scrollbar_) {
      v_adjust_ = gtk_adjustment_new(0, 0, 10, 1, 1, 5); // temp values
      FL_FUNCTION_CALLBACK_3(v_fl_scrollbar_, scroll_cb, Fl_Slider*, v_fl_scrollbar_, GtkAdjustment*, v_adjust_, Fl_Native_Input*, widget);
      g_signal_connect(v_adjust_, "changed", G_CALLBACK(adjustment_changed_), this);
      g_signal_connect(v_adjust_, "value-changed", G_CALLBACK(adjustment_value_changed_), this);
    }
    if (h_fl_slider_) {
      h_adjust_ = gtk_adjustment_new(0, 0, 10, 1, 1, 5);
      FL_FUNCTION_CALLBACK_3(h_fl_slider_, scroll_cb, Fl_Slider*, h_fl_slider_, GtkAdjustment*, h_adjust_, Fl_Native_Input*, widget);
      g_signal_connect(h_adjust_, "changed", G_CALLBACK(adjustment_changed_), this);
      g_signal_connect(h_adjust_, "value-changed", G_CALLBACK(adjustment_value_changed_), this);
    }
#if GTK_MAJOR_VERSION == 3
    scrolled_ = gtk_scrolled_window_new(h_adjust_, v_adjust_);
#else
    scrolled_ = gtk_scrolled_window_new();
    if (h_adjust_) gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(scrolled_), h_adjust_);
    if (v_adjust_) gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scrolled_), v_adjust_);
    GtkWidget *scroll = gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(scrolled_));
    gtk_widget_set_visible(scroll, false);
    scroll = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(scrolled_));
    gtk_widget_set_visible(scroll, false);
#endif
    text_view_ = gtk_text_view_new();
#if GTK_MAJOR_VERSION == 4
    gtk_widget_add_css_class(text_view_, class_name_part_ + 9);
#endif
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view_), 3);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view_), 3);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view_), !widget->readonly() );
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view_), true);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view_),
      (kind == Fl_Native_Input_Driver::SINGLE_LINE || !widget->wrap() ? GTK_WRAP_NONE: GTK_WRAP_WORD));
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(text_view_), false);
    gtk_widget_set_can_focus(text_view_, !widget->readonly());
#if GTK_MAJOR_VERSION == 3
    gtk_container_add(GTK_CONTAINER(scrolled_), text_view_);
    gtk_container_add(GTK_CONTAINER(window_), scrolled_);
#else
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_), text_view_);
    gtk_window_set_child((GtkWindow*)window_, scrolled_);
#endif
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_), // important
        (kind == Fl_Native_Input_Driver::MULTIPLE_LINES && widget->wrap() ? GTK_POLICY_NEVER: GTK_POLICY_ALWAYS), //H
        (kind == Fl_Native_Input_Driver::SINGLE_LINE ? GTK_POLICY_NEVER : GTK_POLICY_ALWAYS) //V
                                   );
#if GTK_MAJOR_VERSION == 3
    gtk_widget_show_all(window_);
#else
    gtk_widget_realize(window_);
#endif

    
    buffer_ = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view_));
    g_signal_connect(buffer_, "changed", G_CALLBACK(textbuffer_changed_), this);
    // Create an anonymous GtkTextTag to set the font+size for the text_view_
    // and attach it to the GtkTextBuffer
    char font_str[88];
    extern Fl_Fontdesc* fl_fonts;
    if (!fl_fonts) fl_fonts = Fl_Graphics_Driver::default_driver().calc_fl_fonts();
    const char *fname = (fl_fonts + widget->textfont())->name;
    snprintf(font_str, sizeof(font_str), "%s %dpx", fname, widget->textsize());
    font_size_tag_ = gtk_text_buffer_create_tag(buffer_, NULL, "font", font_str,
                                                NULL);
    set_style_();
    
    if (text_before_show_) {
      static_value(text_before_show_, (int)strlen(text_before_show_));
      delete[] text_before_show_;
      text_before_show_ = NULL;
    }
    lineheight_ = widget->textsize() * 1.2;
    resize();
  } else {
    widget->set_visible();
    widget->parent()->redraw();
  }
  if (kind == Fl_Native_Input_Driver::SINGLE_LINE && h_fl_slider_) h_fl_slider_->hide();
}


void Fl_Cairo_Native_Input_Driver::resize()  {
  int text_width = widget->w() - Fl::box_dw(widget->box()) - (v_fl_scrollbar_ ? slider_thickness_ : 0);
  int text_height = widget->h() - Fl::box_dh(widget->box()) - (h_fl_slider_ && kind == Fl_Native_Input_Driver::MULTIPLE_LINES ? slider_thickness_ : 0);
  if (text_width < 10 || text_height < 10) return;
  if (text_width != allocation_.width || text_height != allocation_.height) {
    allocation_.width = text_width;
    allocation_.height = text_height;
    if (v_fl_scrollbar_)  {
      v_fl_scrollbar_->resize(widget->x() + Fl::box_dx(widget->box()) +
                             (widget->right_to_left() ? 0 : allocation_.width),
                             widget->y() + Fl::box_dy(widget->box()),
                              slider_thickness_, allocation_.height);
    }
    if (h_fl_slider_)  {
      h_fl_slider_->resize(widget->x() + Fl::box_dx(widget->box()) + (widget->right_to_left() ? slider_thickness_ : 0),
                             widget->y() + Fl::box_dy(widget->box()) + allocation_.height,
                             allocation_.width, slider_thickness_);
    }
  }
}


void Fl_Cairo_Native_Input_Driver::draw()  {
  if (!widget->visible()) return;
  gtk_widget_size_allocate(scrolled_, &allocation_
#if GTK_MAJOR_VERSION == 4
                           , -1
#endif
                           );
  bool to_display = true;
  Fl_Image_Surface *offscreen = NULL;
  Fl_Cairo_Graphics_Driver *dr;
  if (Fl_Surface_Device::surface() != Fl_Display_Device::display_device()) {
    to_display = false;
    offscreen = new Fl_Image_Surface(2 * allocation_.width, 2 * allocation_.height, 1);
    dr = (Fl_Cairo_Graphics_Driver*)offscreen->driver();
    Fl_Surface_Device::push_current(offscreen);
    cairo_scale(dr->cr(), 2, 2);
  } else {
    dr = (Fl_Cairo_Graphics_Driver*)Fl_Display_Device::display_device()->driver();
    if (Fl_Window::current() != widget->window()) widget->window()->make_current();
    if (!dr->cr()) return;
    cairo_save(dr->cr());
    cairo_translate(dr->cr(),widget->x() + Fl::box_dx(widget->box()) +
                    (v_fl_scrollbar_ && widget->right_to_left() ? v_fl_scrollbar_->w() : 0),
                    widget->y() + Fl::box_dy(widget->box()));
  }
  GtkTextIter insert, sel_end;
  bool selection_active = gtk_text_buffer_get_selection_bounds(buffer_, &insert, &sel_end);
  if (Fl::focus() != widget) { // temporarily hide selection when widget not focused
    gtk_text_buffer_select_range(buffer_, &insert, &insert);
  }
#if GTK_MAJOR_VERSION == 3
  gtk_widget_draw(scrolled_, dr->cr());
#endif
  GdkRectangle strong;
  if (Fl::focus() != widget) {
    gtk_text_buffer_select_range(buffer_, &insert, &sel_end);
  } else if (!selection_active) { // draw insertion cursor
    gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view_), NULL, &strong, NULL);
    gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(text_view_),
                                          GTK_TEXT_WINDOW_TEXT, strong.x, strong.y, &strong.x, &strong.y);
#if GTK_MAJOR_VERSION == 3
    // the native insertion cursor
    PangoLayout *layout = gtk_widget_create_pango_layout(text_view_, NULL);
    gtk_render_insertion_cursor(gtk_widget_get_style_context(text_view_),
                                dr->cr(), strong.x, strong.y + widget->textsize()/4, layout, 0, PANGO_DIRECTION_NEUTRAL);
    g_object_unref(layout);
#endif
  }
#if GTK_MAJOR_VERSION == 4
  GtkSnapshot *snapshot = gtk_snapshot_new();
  gtk_widget_set_visible(window_, true);
  gtk_widget_snapshot_child(scrolled_, text_view_, snapshot);
  gtk_widget_set_visible(window_, false);
  GskRenderNode *rendernode = gtk_snapshot_free_to_node(snapshot);
  gsk_render_node_draw(rendernode, dr->cr());
  gsk_render_node_unref(rendernode);
  g_main_context_iteration(NULL, false); /* force window hiding to be processed */
#endif
  if (to_display && Fl::focus() == widget && !selection_active) {
    fl_set_spot(widget->textfont(), widget->textsize(),
                (strong.x + widget->x()), (strong.y + widget->textsize() * 1.3 + widget->y()), 1, 0);
  }
  if (to_display) {
    cairo_new_path(dr->cr()); // sometimes necessary to restore cairo context OK
    cairo_restore(dr->cr());
  } else {
    Fl_RGB_Image *rgb = offscreen->image();
    rgb->scale(rgb->w()/2, rgb->h()/2);
    Fl_Surface_Device::pop_current();
    delete offscreen;
    rgb->draw(
              (widget->x() + Fl::box_dx(widget->box()) +
                                (v_fl_scrollbar_ && widget->right_to_left() ? v_fl_scrollbar_->w() : 0)),
              (widget->y() + Fl::box_dy(widget->box()))
              );
    delete rgb;
  }
  // draw the scrollbars
  if (widget->damage() & ~FL_DAMAGE_CHILD) { // redraw the entire thing:
    if (v_fl_scrollbar_) draw_child(*v_fl_scrollbar_);
    if (h_fl_slider_) draw_child(*h_fl_slider_);
  } else {      // only redraw the children that need it:
    if (v_fl_scrollbar_) update_child(*v_fl_scrollbar_);
    if (h_fl_slider_) update_child(*h_fl_slider_);
  }
}


void Fl_Cairo_Native_Input_Driver::draw_child(Fl_Widget& widget) {
  if (widget.visible() && fl_not_clipped(widget.x(), widget.y(), widget.w(), widget.h())) {
    widget.clear_damage(FL_DAMAGE_ALL);
    widget.draw();
    widget.clear_damage();
  }
}


void Fl_Cairo_Native_Input_Driver::update_child(Fl_Widget& widget) {
  if (widget.damage() && widget.visible() &&
      fl_not_clipped(widget.x(), widget.y(), widget.w(), widget.h())) {
    widget.draw();
    widget.clear_damage();
  }
}


const char *Fl_Cairo_Native_Input_Driver::value() {
  if (!text_view_) return (text_before_show_ ? text_before_show_ : "");
  else {
    static char *utf8_ = NULL;
    GtkTextIter first, last;
    gtk_text_buffer_get_start_iter(buffer_, &first);
    gtk_text_buffer_get_end_iter(buffer_, &last);
    if (utf8_) free(utf8_);
    utf8_ = gtk_text_buffer_get_text(buffer_, &first, &last, true);
    return utf8_;
  }
}


/*
 Called when the multiline widget buffer_'s content is fully changed.
 Moves across all text, paragraph by paragraph, and computes position of its beginning;
 sets v_adjust_ and v_fl_scrollbar_ parameters accordingly.
 This makes GTK and FLTK aware of the full size of the widget's formatted text.
 */
void Fl_Cairo_Native_Input_Driver::scan_all_paragraphs_() {
  GtkTextIter end, current;
  gtk_text_buffer_get_start_iter (buffer_, &current);
  gtk_text_buffer_get_end_iter (buffer_, &end);
  gtk_adjustment_set_upper(v_adjust_, 0);
  draw();
  upper_ = 0;
//printf("end=%d chars upper_=%.1f\n",gtk_text_iter_get_offset(&end), upper_);
  do {
//printf("current=%d\n",gtk_text_iter_get_offset(&current));
    /*gboolean b =*/ gtk_text_iter_forward_line(&current);
//printf("next line=%d b=%d\n",gtk_text_iter_get_offset(&current),b);
    GdkRectangle strong;
    gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view_), &current, &strong, NULL);
    if (strong.y > upper_) {
      upper_ = strong.y + 1;
//printf("upper_=%.1f\n",upper_);
      gtk_adjustment_set_upper(v_adjust_, upper_);
      gtk_adjustment_set_value(v_adjust_, upper_);
    }
  }  while(gtk_text_iter_compare(&current, &end) < 0);
  gtk_adjustment_set_value(v_adjust_, 0);
//printf("upper_=%.1f v_fl_scrollbar_=%d\n",gtk_adjustment_get_upper(v_adjust_), v_fl_scrollbar_->value());
}


void Fl_Cairo_Native_Input_Driver::value(const char *t, int len) {
  if (!text_view_) {
    char *new_text = new char[len+1];
    memcpy(new_text, t, len);
    new_text[len] = 0;
    if (text_before_show_) delete[] text_before_show_;
    text_before_show_ = new_text;
  } else {
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer_, &start);
    gtk_text_buffer_get_end_iter(buffer_, &end);
    gtk_text_buffer_select_range(buffer_, &start, &end);
    replace_selection(t, len);
  }
}


void Fl_Cairo_Native_Input_Driver::static_value(const char *t, int len) {
  value(t, len);
  undo_->undoinsert = 0; // prevent undoing initial text assignment
  undo_->undoinsert_chars = undo_->undoat_chars = 0;
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


void Fl_Cairo_Native_Input_Driver::delayed_cursor_at_extremity_(Fl_Cairo_Native_Input_Driver *o) {
  GtkTextIter iter;
  if (o->kind == MULTIPLE_LINES) gtk_text_buffer_get_start_iter(o->buffer_, &iter);
  else gtk_text_buffer_get_end_iter(o->buffer_, &iter);
  gtk_text_buffer_select_range(o->buffer_, &iter, &iter);
  if (o->kind == MULTIPLE_LINES && !o->widget->wrap()) {
    double d = (o->widget->right_to_left() ? gtk_adjustment_get_upper(o->h_adjust_) : 0);
    gtk_adjustment_set_value(o->h_adjust_, d);
  } else if (o->kind == SINGLE_LINE && !o->widget->right_to_left()) {
    double d = gtk_adjustment_get_upper(o->h_adjust_);
    gtk_adjustment_set_value(o->h_adjust_, d);
  }
  o->widget->redraw();
}


// ALL changes to the widget's text go through this function
void Fl_Cairo_Native_Input_Driver::replace_selection(const char *text, int len) {
  GtkTextIter start, end, first;
  gtk_text_buffer_get_selection_bounds(buffer_, &start, &end);
  int char_pos = gtk_text_iter_get_offset(&start); // char offset in text to insertion point
  gtk_text_buffer_get_start_iter(buffer_, &first);
  if (gtk_text_buffer_get_has_selection(buffer_)) {
    int e_chars = gtk_text_iter_get_offset(&end); // char offset in text to end of selection
    char *selection = gtk_text_buffer_get_text(buffer_, &start, &end, true);
    int lselection = (int)strlen(selection);
    if (char_pos == undo_->undoat_chars) {
      undo_->undobuffersize(undo_->undocut + lselection);
      memcpy(undo_->undobuffer + undo_->undocut, selection, lselection);
      undo_->undocut += lselection;
      undo_->undocut_chars += e_chars - char_pos;
    } else if (e_chars == undo_->undoat_chars && !undo_->undoinsert_chars) {
      undo_->undobuffersize(undo_->undocut + lselection);
      memmove(undo_->undobuffer + lselection, undo_->undobuffer, undo_->undocut);
      memcpy(undo_->undobuffer, selection, lselection);
      undo_->undocut += lselection;
      undo_->undocut_chars += e_chars - char_pos;
    } else if (e_chars == undo_->undoat_chars && (e_chars - char_pos) < undo_->undoinsert_chars) {
      undo_->undoinsert -= lselection;
      undo_->undoinsert_chars -= e_chars - char_pos;
    } else {
      redo_list_->clear();
      undo_list_->push(undo_);
      undo_ = new Fl_GTK3_Text_Undo_Action();
      undo_->undobuffersize(lselection);
      memcpy(undo_->undobuffer, selection, lselection);
      undo_->undocut = lselection;
      undo_->undocut_chars = e_chars - char_pos;
      undo_->undoinsert = 0;
      undo_->undoinsert_chars = 0;
    }
    undo_->undoat_chars = char_pos;
    undo_->undoyankcut_chars = undo_->undocut_chars;
    free(selection);
    widget->set_changed();
    gtk_text_buffer_delete_selection(buffer_, true, true);
  }
  bool full_replace = (gtk_text_buffer_get_char_count(buffer_) == 0);
  int len_chars = 0;
  if (len > 0) {
    len_chars = calc_char_length(text, len);
    GtkTextIter iter, start, end;
    gtk_text_buffer_get_selection_bounds(buffer_, &iter, NULL);
    gtk_text_buffer_get_start_iter(buffer_, &start);
    gtk_text_buffer_get_end_iter(buffer_, &end);
    bool need_apply_tag = gtk_text_iter_equal(&iter, &start) || gtk_text_iter_equal(&iter, &end);
    if (full_replace) widget->clear_changed();
    else widget->set_changed();
    gtk_text_buffer_insert(buffer_, &iter, text, len);
    if (need_apply_tag) {
      gtk_text_buffer_get_start_iter(buffer_, &start);
      gtk_text_buffer_get_end_iter(buffer_, &end);
      gtk_text_buffer_apply_tag(buffer_, font_size_tag_, &start, &end);
    }
    if (full_replace && len_chars > 1) { // for some reason, cursor positioning at extremity of text needs be delayed
      Fl::add_timeout(0, (Fl_Timeout_Handler)delayed_cursor_at_extremity_, this);
    }
    if (char_pos == undo_->undoat_chars) {
      undo_->undoinsert += len;
      undo_->undoinsert_chars += len_chars;
    } else {
      redo_list_->clear();
      undo_list_->push(undo_);
      undo_ = new Fl_GTK3_Text_Undo_Action();
      undo_->undocut = 0;
      undo_->undocut_chars = 0;
      undo_->undoinsert = len;
      undo_->undoinsert_chars = len_chars;
    }
  }
  undo_->undoat_chars = char_pos + len_chars;
  if (kind == MULTIPLE_LINES) {
    if (full_replace) scan_all_paragraphs_();
    else text_view_scroll_mark_onscreen_();
  }
  widget->redraw();
}


void Fl_Cairo_Native_Input_Driver::replace(int from, int to, const char *text, int len) {
  insert_position(from, to);
  replace_selection(text, len);
}


#if GTK_MAJOR_VERSION == 4
void Fl_Cairo_Native_Input_Driver::delayed_set_style_(Fl_Cairo_Native_Input_Driver *dr) {
  dr->set_style_();
}
#endif


int Fl_Cairo_Native_Input_Driver::handle_focus(int event) {
#if GTK_MAJOR_VERSION == 4
  Fl::add_timeout(0, (Fl_Timeout_Handler)delayed_set_style_, this);
#endif
  widget->damage(FL_DAMAGE_CHILD);
  return 1;
}


unsigned Fl_Cairo_Native_Input_Driver::index(int i) const {
  const char *s = widget->value();
  int len = (int)strlen(s);
  unsigned r = (i >= 0 && i < len ? fl_utf8decode(s + i, s + len, &len) : 0);
  return r;
}


int Fl_Cairo_Native_Input_Driver::copy() {
  GtkTextIter start, end;

  if (gtk_text_buffer_get_selection_bounds(buffer_, &start, &end)) {
    char *buf = gtk_text_buffer_get_text(buffer_, &start, &end, false);
    if (kind == SINGLE_LINE) put_back_newlines_(buf);
    Fl::copy(buf, (int)strlen(buf), 1);
    free(buf);
    return 1;
  }
  return 0;
}


void Fl_Cairo_Native_Input_Driver::paste() {
  if (widget->readonly()) fl_beep();
  else Fl::paste(*widget, 1);
}


int Fl_Cairo_Native_Input_Driver::handle_keyboard() {
  int del;
  if (Fl::compose(del)) {
    if (del || Fl::event_length()) {
      if (widget->readonly()) fl_beep();
      else {
        if (del) {
          GtkTextIter insert, mrk;
          gtk_text_buffer_get_selection_bounds(buffer_, &insert, &mrk);
          if (gtk_text_iter_compare(&insert, &mrk) > 0) mrk = insert;
          int mark2 = byte_pos_to_char_pos(char_pos_to_byte_pos_(gtk_text_iter_get_offset(&mrk)) - del);
          gtk_text_iter_set_offset(&insert, mark2);
          gtk_text_buffer_select_range(buffer_, &mrk, &insert);
        }
        replace_selection(Fl::event_text(), Fl::event_length());
      }
    }
    if (Fl::screen_driver()->has_marked_text() && Fl::compose_state) {
      widget->mark(insert_position() - Fl::compose_state);
    }
    return 1;
  }
  int mods = Fl::event_state() & (FL_META|FL_CTRL|FL_ALT);
  unsigned int shift = Fl::event_state() & FL_SHIFT;
  int retval = 0;
  if (Fl::event_key() == FL_Delete) {
    if (widget->readonly()) { fl_beep(); return 1; }
    int selected = gtk_text_buffer_get_has_selection(buffer_);
    if (mods == 0 && shift && selected) {
      GtkTextIter start, end; // copy_cut(): Shift-Delete with selection (WP,NP,WOW,GE,KE,OF)
      gtk_text_buffer_get_selection_bounds(buffer_, &start, &end);
      char *buf = gtk_text_buffer_get_text(buffer_, &start, &end, false);
      if (kind == SINGLE_LINE) put_back_newlines_(buf);
      Fl::copy(buf, (int)strlen(buf), 1);
      free(buf);
      replace_selection(NULL, 0);
      return 1;
    } else if (mods == 0 && ((shift && !selected) || !shift)) {
       // Delete or Shift-Delete no selection (WP,NP,WOW,GE,KE,!OF)
      GtkTextIter iter, next;
      gtk_text_buffer_get_selection_bounds(buffer_, &iter, NULL);
      next = iter;
      gtk_text_iter_forward_cursor_position(&next);
      gtk_text_buffer_select_range(buffer_, &iter, &next);
      replace_selection(NULL, 0);
      return 1;
    } else if (mods == FL_CTRL) {// kf_delete_word_right()  Ctrl-Delete    (WP,!NP,WOW,GE,KE,!OF)
      GtkTextIter iter, next;
      gtk_text_buffer_get_selection_bounds(buffer_, &iter, NULL);
      next = iter;
      gtk_text_iter_forward_word_end(&next);
      gtk_text_buffer_select_range(buffer_, &iter, &next);
      replace_selection(NULL, 0);
      return 1;
    }
  } else if (Fl::event_key() == FL_BackSpace) {
    if (widget->readonly()) { fl_beep(); return 1; }
    if (mods == 0) { // Backspace      (WP,NP,WOW,GE,KE,OF)
      if (gtk_text_buffer_get_has_selection(buffer_)) {
        replace_selection(NULL, 0);
      } else {
        GtkTextIter iter, previous;
        gtk_text_buffer_get_selection_bounds(buffer_, &iter, NULL);
        previous = iter;
        gtk_text_iter_backward_char(&previous);
        gtk_text_buffer_select_range(buffer_, &previous, &iter);
        replace_selection(NULL, 0);
      }
      retval = 1;
    }
    else if (mods == FL_CTRL) {//   kf_delete_word_left()  Ctrl-Backspace (WP,!NP,WOW,GE,KE,!OF)
      GtkTextIter iter, next;
      gtk_text_buffer_get_selection_bounds(buffer_, &iter, NULL);
      next = iter;
      gtk_text_iter_backward_word_start(&next);
      gtk_text_buffer_select_range(buffer_, &next, &iter);
      replace_selection(NULL, 0);
      return 1;
    }
  }
  if (retval >= 1) return retval;
  
  if (Fl::event_key() == FL_Right || Fl::event_key() == FL_Left) {
    GtkTextIter before, after;
    gtk_text_buffer_get_iter_at_mark(buffer_, &before, gtk_text_buffer_get_insert(buffer_));
    if (Fl::event_state() & FL_SHIFT) {
      gtk_text_buffer_get_iter_at_mark(buffer_, &after, gtk_text_buffer_get_selection_bound(buffer_));
      if (Fl::event_key() == (widget->right_to_left() ? FL_Right : FL_Left)) {
        gtk_text_iter_backward_cursor_position(&after);
      } else gtk_text_iter_forward_cursor_position(&after);
      gtk_text_buffer_select_range(buffer_, &before, &after);
    } else {
      after = before;
      insert_offset_ = -1;
      if (Fl::event_key() == (widget->right_to_left() ? FL_Right : FL_Left)) {
        gtk_text_iter_backward_cursor_position(&after);
      } else     {
        gtk_text_iter_forward_cursor_position(&after);
      }
      gtk_text_buffer_place_cursor(buffer_, &after);
      if (v_fl_scrollbar_) {
        text_view_scroll_mark_onscreen_();
      }
      if (h_fl_slider_) {
        text_view_scroll_mark_h_(&before);
      }
    }
    widget->redraw();
    return 1;
  } else if (Fl::event_key() == FL_Down || Fl::event_key() == FL_Up) {
    if (kind == SINGLE_LINE) return 1;
    GtkTextIter where, last, new_where;
    GdkRectangle strong;
    gtk_text_buffer_get_selection_bounds(buffer_, &where, &last);
    if ((Fl::event_state() & FL_SHIFT) && Fl::event_key() == FL_Down) new_where = last;
    else new_where = where;
    if (insert_offset_ < 0) {
      gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view_), &new_where, &strong, NULL);
      insert_offset_ = strong.x;
    }
    if (Fl::event_key() == FL_Down)
      gtk_text_view_forward_display_line(GTK_TEXT_VIEW(text_view_), &new_where);
    else
      gtk_text_view_backward_display_line(GTK_TEXT_VIEW(text_view_), &new_where);
    gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view_), &new_where, &strong, NULL);
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view_), &new_where, insert_offset_, strong.y+1);
    bool relative_to_mark = false;
    if (Fl::event_state() & FL_SHIFT) {
      if (Fl::event_key() == FL_Down) {
        gtk_text_buffer_select_range(buffer_, &where, &new_where);
        relative_to_mark = true;
      } else gtk_text_buffer_select_range(buffer_, &new_where, &last);
    } else gtk_text_buffer_place_cursor(buffer_, &new_where);
    text_view_scroll_mark_onscreen_(relative_to_mark);
    widget->redraw();
    return 1;
  } else if ((Fl::event_key() == FL_Enter || Fl::event_key() == FL_KP_Enter) &&
             !widget->readonly()) {
    if (kind == MULTIPLE_LINES) {
      replace_selection("\n", 1);
      text_view_scroll_mark_onscreen_();
      return 1;
    } else if (widget->when() & FL_WHEN_ENTER_KEY) {
      maybe_do_callback(FL_REASON_ENTER_KEY);
    }
  } else if (Fl::event_key() == FL_Home || Fl::event_key() == FL_End) {
    if (v_fl_scrollbar_) {
      if (Fl::event_state() & FL_SHIFT) {
        GtkTextIter cursor, mark, from, first, last;
        gtk_text_buffer_get_selection_bounds(buffer_, &cursor, &mark);
        gtk_text_buffer_get_start_iter(buffer_, &first);
        gtk_text_buffer_get_iter_at_offset (buffer_, &last, -1);
        if (Fl::event_key() == FL_Home) {
          from = (gtk_text_iter_compare(&cursor, &mark) > 0 ? cursor : mark);
          gtk_text_buffer_select_range(buffer_, &first, &from);
        } else {
          from = (gtk_text_iter_compare(&cursor, &mark) > 0 ? mark : cursor);
          gtk_text_buffer_select_range(buffer_, &from, &last);
        }
      }
      int changed = v_fl_scrollbar_->value( Fl::event_key() == FL_Home ? 0 :
                                           v_fl_scrollbar_->maximum() );
      if (changed) v_fl_scrollbar_->do_callback();
    }
    return 1;
  } else if (Fl::event_key() == FL_Tab && !Fl::event_state()) {
    replace_selection("\011", 1);
    return 1;
  }
  return 0;
}


// Scrolls the view vertically so the insertion cursor is visible
void Fl_Cairo_Native_Input_Driver::text_view_scroll_mark_onscreen_(bool relative_to_mark) {
  GtkTextIter cursor, mark;
  gtk_text_buffer_get_selection_bounds(buffer_, &cursor, &mark);
  GdkRectangle strong, strong2;
  gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view_),
                                     (relative_to_mark ? &mark : &cursor),
                                     &strong, NULL);
  gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(text_view_),
                                        GTK_TEXT_WINDOW_TEXT,
                                        strong.x, strong.y, &strong2.x, &strong2.y);
  if (strong2.y > allocation_.height - lineheight_ || strong2.y < 0) {
    if (strong2.y > allocation_.height - lineheight_ && strong.y+ lineheight_/2 > upper_) {
      upper_ = strong.y + lineheight_;
      gtk_adjustment_set_upper(v_adjust_, upper_);
    }
    int val = gtk_adjustment_get_value(v_adjust_);
    if (strong2.y < 0) val += strong2.y;
    else val += strong2.y + lineheight_ - allocation_.height;
    gtk_adjustment_set_value(v_adjust_, val);
  }
}


// Scrolls the view horizontally after left or right arrow key was pushed
// so the insertion cursor is visible
void Fl_Cairo_Native_Input_Driver::text_view_scroll_mark_h_(GtkTextIter *before) {
  GdkRectangle strong, before_rect;
  gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view_), NULL, &strong, NULL);
  gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(text_view_), before, &before_rect, NULL);
  int charwidth = strong.x - before_rect.x;
  gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(text_view_),
                                        GTK_TEXT_WINDOW_TEXT,
                                        strong.x, strong.y, &strong.x, &strong.y);
  if (strong.x > allocation_.width || strong.x < 0) {
    double d = gtk_adjustment_get_value(h_adjust_);
    gtk_adjustment_set_value(h_adjust_, d + charwidth);
  }
}


static GtkTextIter dnd_iter_position, dnd_iter_mark, newpos;
static Fl_Widget *dnd_save_focus = NULL;
static int drag_start = -1;
static Fl_Slider *click_in_scroller = NULL; // non-NULL means the FL_PUSH was done inside a slider

int Fl_Cairo_Native_Input_Driver::handle_mouse(int event) {
  if (v_fl_scrollbar_ && Fl::event_inside(v_fl_scrollbar_)) {
    int r = v_fl_scrollbar_->handle(event);
    if (event == FL_PUSH) {
      Fl::pushed(v_fl_scrollbar_);
      click_in_scroller = v_fl_scrollbar_;
      return r;
    }
  }
  if (h_fl_slider_ && Fl::event_inside(h_fl_slider_)) {
    int r = h_fl_slider_->handle(event);
    if (event == FL_PUSH) {
      Fl::pushed(h_fl_slider_);
      click_in_scroller = h_fl_slider_;
      return r;
    }
  }
  double dv = (v_adjust_ ? gtk_adjustment_get_value(v_adjust_) : 0);
  double dh = (h_adjust_ ? gtk_adjustment_get_value(h_adjust_) : 0);
  int v_scroll_w = (v_fl_scrollbar_ && widget->right_to_left() ? v_fl_scrollbar_->w() : 0);

  if (event == FL_PUSH) {
    if (Fl::dnd_text_ops() && (Fl::event_button() != FL_RIGHT_MOUSE) && widget->selectable()) {
      GtkTextIter oldpos, oldmark;
      gtk_text_buffer_get_selection_bounds(buffer_, &oldpos, &oldmark);
      // move insertion point to Fl::event_x(), Fl::event_y()
      GtkTextIter where;
      gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view_), &where,
        Fl::event_x() - (widget->x() + Fl::box_dx(widget->box()) + v_scroll_w) + dh,
        Fl::event_y() - (widget->y() + Fl::box_dy(widget->box())) + dv);
      gtk_text_buffer_place_cursor(buffer_, &where);

      newpos = where;
      gtk_text_buffer_select_range(buffer_, &oldpos, &oldmark);
      GtkTextIter current_insert, current_mark;
      gtk_text_buffer_get_selection_bounds(buffer_, &current_insert, &current_mark);
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
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view_), &where,
      Fl::event_x() - (widget->x() + Fl::box_dx(widget->box()) + v_scroll_w) + dh,
      Fl::event_y() - (widget->y() + Fl::box_dy(widget->box())) + dv);
    gtk_text_buffer_get_selection_bounds(buffer_, &insert, NULL);
    if (Fl::event_shift() && widget->selectable()) {
      GtkTextMark *mark = (gtk_text_iter_compare(&insert, &where) < 0 ?
                           gtk_text_buffer_get_selection_bound(buffer_) :
                           gtk_text_buffer_get_insert(buffer_));
      gtk_text_buffer_move_mark(buffer_, mark, &where);
    } else if (Fl::event_clicks() && widget->selectable()) {
      gtk_text_buffer_place_cursor(buffer_, &where);
      if (Fl::event_clicks() >= 2) {
        gtk_text_iter_forward_line(&where);
        GtkTextIter begin_line = where;
        gtk_text_iter_backward_line(&where);
        gtk_text_buffer_select_range(buffer_, &begin_line, &where);
      } else {
        gtk_text_iter_backward_word_start(&where);
        GtkTextIter word_end = where;
        gtk_text_iter_forward_word_end(&where);
        gtk_text_buffer_select_range(buffer_, &where, &word_end);
      }
    } else {
      gtk_text_buffer_place_cursor(buffer_, &where);
      insert_offset_ = -1;
    }
    widget->redraw();
    return 1;
  } else if (event == FL_DRAG) {
    if (click_in_scroller && (Fl::event_state() & FL_BUTTON1)) {
      return click_in_scroller->handle(event);
    }
    if (!widget->selectable()) return 0;
    if (drag_start >= 0) {
      if (Fl::event_is_click()) return 1; // debounce the mouse
      // save the position because sometimes we don't get DND_ENTER:
      gtk_text_buffer_get_selection_bounds(buffer_, &dnd_iter_position, &dnd_iter_mark);
      dnd_save_focus = widget;
      // drag the data:
      char *buf = gtk_text_buffer_get_text(buffer_, &dnd_iter_position, &dnd_iter_mark, false);
      Fl::copy(buf, (int)strlen(buf), 0);
      free(buf);
      Fl::screen_driver()->dnd(1);
      return 1;
    }
    // make selection follow the pointer
    GtkTextIter where, insert;
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view_), &where,
      Fl::event_x() - (widget->x() + Fl::box_dx(widget->box()) + v_scroll_w) + dh,
      Fl::event_y() - (widget->y() + Fl::box_dy(widget->box())) + dv);
    gtk_text_buffer_get_iter_at_mark(buffer_, &insert, gtk_text_buffer_get_insert(buffer_));
    if (!click_in_scroller) gtk_text_buffer_select_range(buffer_, &insert, &where);
    widget->redraw();
    return 1;
    
  } else if(event == FL_RELEASE) {
    click_in_scroller = NULL;
    if (Fl::event_is_click() && drag_start >= 0) {
      // user clicked in the field and wants to reset the cursor position...
      GtkTextIter current;
      gtk_text_buffer_get_iter_at_offset(buffer_, &current, drag_start);
      gtk_text_buffer_select_range(buffer_, &current, &current);
      drag_start = -1;
      widget->window()->cursor(FL_CURSOR_INSERT);
      return 1;
    }
  } else if((event == FL_ENTER || event == FL_MOVE) && !widget->readonly()) {
    bool b = Fl::event_inside(
            widget->x() + Fl::box_dx(widget->box()), widget->y() + Fl::box_dy(widget->box()),
            widget->w() - Fl::box_dw(widget->box()), widget->h() - Fl::box_dh(widget->box()));
    if (b && v_fl_scrollbar_ && Fl::event_inside(v_fl_scrollbar_)) b = false;
    if (b && h_fl_slider_ && Fl::event_inside(h_fl_slider_)) b = false;
    widget->window()->cursor(b ? FL_CURSOR_INSERT : FL_CURSOR_DEFAULT);
    return 1;
  } else if(event == FL_LEAVE) {
    widget->window()->cursor(FL_CURSOR_DEFAULT);
    return 1;
  } else if(event == FL_MOUSEWHEEL) {
    if (v_fl_scrollbar_) {
      int val = v_fl_scrollbar_->value() + lineheight_ * Fl::event_dy();
      int changed = v_fl_scrollbar_->value( v_fl_scrollbar_->clamp(val) );
      if (changed) v_fl_scrollbar_->do_callback();
    }
    if (h_fl_slider_) {
      int val = h_fl_slider_->value() + widget->textsize() * Fl::event_dx();
      int changed = h_fl_slider_->value( h_fl_slider_->clamp(val) );
      if (changed) h_fl_slider_->do_callback();
    }
    return 1;
  }
  return 0;
}


int Fl_Cairo_Native_Input_Driver::handle_paste() {
  if (widget->readonly()) fl_beep();
  else {
    const char *new_text = Fl::event_text();
    int len = Fl::event_length();
    if (kind == SINGLE_LINE) new_text = substitute_with_cr_ht(new_text, len);
    replace_selection(new_text, len);
    if (kind == SINGLE_LINE) free((char*)new_text);
  }
  return 1;
}


// Given a position in character units in the text, returns the corresponding
// position in byte units.
int Fl_Cairo_Native_Input_Driver::char_pos_to_byte_pos_(int char_count) {
  const char *text;
  if (!text_view_) {
    text = text_before_show_;
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
    gtk_text_buffer_get_start_iter(buffer_, &first);
    gtk_text_buffer_get_iter_at_offset(buffer_, &last, char_count);
    text = gtk_text_buffer_get_text(buffer_, &first, &last, true);
    int l = (int)strlen(text);
    free((char*)text);
    return l;
  }
}


int Fl_Cairo_Native_Input_Driver::insert_position() {
  GtkTextIter iter;
  gtk_text_buffer_get_selection_bounds(buffer_, &iter, NULL);
  int pos = gtk_text_iter_get_offset(&iter);
  return char_pos_to_byte_pos_(pos);
}


void Fl_Cairo_Native_Input_Driver::insert_position(int pos, int mark) {
  bool need_selection = (pos != mark);
  pos = byte_pos_to_char_pos(pos);
  GtkTextIter where, last;
  gtk_text_buffer_get_start_iter(buffer_, &where);
  gtk_text_iter_set_offset(&where, pos);
  last = where;
  if (need_selection) {
    mark = byte_pos_to_char_pos(mark);
    gtk_text_iter_set_offset(&last, mark);
  }
  gtk_text_buffer_select_range(buffer_, &where, &last);
}


int Fl_Cairo_Native_Input_Driver::mark() {
  GtkTextIter iter;
  gtk_text_buffer_get_selection_bounds(buffer_, NULL, &iter);
  int pos = gtk_text_iter_get_offset(&iter);
  return char_pos_to_byte_pos_(pos);
}


int Fl_Cairo_Native_Input_Driver::handle_dnd(int event) {
  switch (event) {
    case FL_DND_ENTER:
      Fl::belowmouse(widget); // send the leave events first
      if (dnd_save_focus != widget) {
        gtk_text_buffer_get_selection_bounds(buffer_, &dnd_iter_position, &dnd_iter_mark);
        dnd_save_focus = Fl::focus();
        Fl::focus(widget);
        widget->handle(FL_FOCUS);
      }
      // fall through:
    case FL_DND_DRAG:
      if (Fl::event_inside(widget)) {
        // move insertion point to Fl::event_x(), Fl::event_y()
        GtkTextIter where;
        double dv = (v_adjust_ ? gtk_adjustment_get_value(v_adjust_) : 0);
        double dh = (h_adjust_ ? gtk_adjustment_get_value(h_adjust_) : 0);
        int v_scroll_w = (v_fl_scrollbar_ && widget->right_to_left() ? v_fl_scrollbar_->w() : 0);
        bool b = gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view_), &where,
          Fl::event_x() - (widget->x() + Fl::box_dx(widget->box()) + v_scroll_w) + dh,
          Fl::event_y() - (widget->y() + Fl::box_dy(widget->box())) + dv);
        if (b) {
          gtk_text_buffer_place_cursor(buffer_, &where);
          widget->redraw();
        }
      }
      return 1;
      
    case FL_DND_LEAVE:
      GtkTextIter start, end;
      gtk_text_buffer_get_start_iter(buffer_, &start);
      gtk_text_buffer_get_end_iter(buffer_, &end);
      if (gtk_text_iter_in_range(&dnd_iter_position, &start, &end) &&
          gtk_text_iter_in_range(&dnd_iter_mark, &start, &end)) {
        gtk_text_buffer_select_range(buffer_, &dnd_iter_position, &dnd_iter_mark);
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
          gtk_text_buffer_get_selection_bounds(buffer_, &old_iter, NULL);
          GtkTextMark *old_mark = gtk_text_mark_new(NULL, true);
          gtk_text_buffer_add_mark(buffer_, old_mark, &old_iter);
          gtk_text_buffer_select_range(buffer_, &dnd_iter_position, &dnd_iter_mark);
          replace_selection(NULL, 0);
          gtk_text_buffer_get_iter_at_mark(buffer_, &old_iter, old_mark);
          gtk_text_buffer_place_cursor(buffer_, &old_iter);
          g_object_unref(old_mark);
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


// Replace ␍ (U+240d) characters in text by \n  and ␉ (U+2409) characters by \t.
// Done only when performing a copy operation from a single-line widget.
void Fl_Cairo_Native_Input_Driver::put_back_newlines_(char *text) {
  // ␍ or runs of ␍ are found bracketted by U+202c / U+202b
  // we also remove these
  const int lcr = 3; // number of bytes to encode U+240d, U+202c, or U+202b
  char RLE_utf8[4];
  fl_utf8encode(0x202b, RLE_utf8); // U+202b = right-to-left embedding (RLE)
  char PDF_utf8[4];
  fl_utf8encode(0x202c, PDF_utf8); // U+202c = pop directional formatting (PDF)
  char *p;
  int lskip;
  long l = strlen(text);
  char *save_text = text;
  while ( l > 0 && (p = strstr(text, "␍"))) { // search for ␍ = U+240d
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
  while ( l > 0 && (p = strstr(text, "␉"))) { // search for ␉ = U+2409
    *p = '\t';
    memmove(p + 1, p + lcr, l - (p + lcr - text) + 1);
    long offset = p + 1 - text;
    text += offset;
    l -= offset;
  }
}


//
// ***************************** Implementation of undo/redo *****************************
//


Fl_GTK3_Text_Undo_Action::Fl_GTK3_Text_Undo_Action() : undobuffer(NULL), undobufferlength(0),
  undocut(0), undoinsert(0) {
  undoat_chars = undocut_chars = undoinsert_chars = undoyankcut_chars = 0;
  }


Fl_GTK3_Text_Undo_Action::~Fl_GTK3_Text_Undo_Action() {
  if (undobuffer) free(undobuffer);
}


/*
 Resize the undo buffer_ to match at least the requested size.
 */
void Fl_GTK3_Text_Undo_Action::undobuffersize(int n)
{
  if (n > undobufferlength) {
    undobufferlength = n + 128;
    undobuffer = (char *)realloc(undobuffer, undobufferlength);
  }
}

void Fl_GTK3_Text_Undo_Action::clear() {
  undocut = undoinsert = 0;
  undocut_chars = undoinsert_chars = 0;
}


Fl_GTK3_Text_Undo_Action_List::Fl_GTK3_Text_Undo_Action_List() : list_(NULL), list_size_(0),
  list_capacity_(0) { }


Fl_GTK3_Text_Undo_Action_List::~Fl_GTK3_Text_Undo_Action_List() {
  clear();
}


int Fl_GTK3_Text_Undo_Action_List::size() const {
  return list_size_;
}


void Fl_GTK3_Text_Undo_Action_List::push(Fl_GTK3_Text_Undo_Action* action) {
  if (list_size_ == list_capacity_) {
    list_capacity_ += 25;
    list_ = (Fl_GTK3_Text_Undo_Action**)realloc(list_, list_capacity_ * sizeof(Fl_GTK3_Text_Undo_Action*));
  }
  list_[list_size_++] = action;
}


Fl_GTK3_Text_Undo_Action* Fl_GTK3_Text_Undo_Action_List::pop() {
  if (list_size_ > 0)
    return list_[--list_size_];
  else
    return NULL;
}


void Fl_GTK3_Text_Undo_Action_List::clear() {
  if (list_) {
    for (int i=0; i<list_size_; i++) {
      delete list_[i];
    }
    ::free(list_);
  }
  list_ = NULL;
  list_size_ = 0;
  list_capacity_ = 0;
}


int Fl_Cairo_Native_Input_Driver::apply_undo_() {
  if (!undo_->undocut_chars && !undo_->undoinsert_chars) return 0;

  int ilen = undo_->undocut;
  int ilen_chars = undo_->undocut_chars;
  int xlen = undo_->undoinsert;
  int xlen_chars = undo_->undoinsert_chars;
  int b_chars = undo_->undoat_chars - xlen_chars;

  if (ilen_chars) {
    GtkTextIter where;
    gtk_text_buffer_get_start_iter(buffer_, &where);
    gtk_text_iter_set_offset(&where, b_chars);
    gtk_text_buffer_select_range(buffer_, &where, &where);
    replace_selection(undo_->undobuffer, ilen);
    b_chars += ilen_chars;
  }

  if (xlen_chars) {
    undo_->undobuffersize(xlen);
    GtkTextIter where, last;
    gtk_text_buffer_get_start_iter(buffer_, &where);
    gtk_text_iter_set_offset(&where, b_chars);
    last = where;
    gtk_text_iter_set_offset(&last, b_chars + xlen_chars);
    char *rest = gtk_text_buffer_get_text(buffer_, &where, &last, true);
    memcpy(undo_->undobuffer, rest, xlen);
    free(rest);
    gtk_text_buffer_delete(buffer_, &where, &last);
    widget->redraw();
  }

  undo_->undocut = xlen;
  undo_->undocut_chars = xlen_chars;
  if (xlen_chars) {
    undo_->undoyankcut_chars = xlen_chars;
  }
  undo_->undoinsert = ilen;
  undo_->undoinsert_chars = ilen_chars;
  undo_->undoat_chars = b_chars;

  return 1;
}


int Fl_Cairo_Native_Input_Driver::undo() {
  if (apply_undo_() == 0)
    return 0;

  redo_list_->push(undo_);
  undo_ = undo_list_->pop();
  if (!undo_) undo_ = new Fl_GTK3_Text_Undo_Action();
  return 1;
}


int Fl_Cairo_Native_Input_Driver::redo() {
  Fl_GTK3_Text_Undo_Action *redo_action = redo_list_->pop();
  if (!redo_action)
    return 0;

  if (undo_->undocut || undo_->undoinsert)
    undo_list_->push(undo_);
  else
    delete undo_;
  undo_ = redo_action;

  return apply_undo_();
}


bool Fl_Cairo_Native_Input_Driver::can_undo() const {
  return (undo_->undocut || undo_->undoinsert);
}


bool Fl_Cairo_Native_Input_Driver::can_redo() const {
  return (redo_list_->size() > 0);
}


// ************************* End of implementation of undo/redo *************************


void Fl_Cairo_Native_Input_Driver::right_to_left() {
  if (text_view_) {
    widget->hide();
    allocation_.width = 0; // important
    resize();
    widget->show();
    widget->take_focus();
  }
}


void Fl_Cairo_Native_Input_Driver::hide_widget() {
  if (!widget->readonly() && (widget->when() & FL_WHEN_RELEASE))
    maybe_do_callback(FL_REASON_LOST_FOCUS);
  widget->clear_visible();
  widget->parent()->redraw();
}


void Fl_Cairo_Native_Input_Driver::select_all() {
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer_, &start);
  gtk_text_buffer_get_end_iter(buffer_, &end);
  gtk_text_buffer_select_range(buffer_, &start, &end);
  widget->damage(FL_DAMAGE_CHILD);
}


int Fl_Cairo_Native_Input_Driver::copy_cuts() {
  if (undo_->undoyankcut_chars && undo_->undoyankcut_chars == undo_->undocut_chars) {
    Fl::copy(undo_->undobuffer, undo_->undocut, 1);
    return 1;
  }
  return 0;
}
