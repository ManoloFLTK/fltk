//
//  Fl_Native_Text_Widget.cxx
//

#include <FL/Fl_Native_Text_Widget.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Window.H>
#include "../src/Fl_Text_Widget_Driver.H"

#if !(defined(__APPLE__) && (!defined(FLTK_USE_X11) || !FLTK_USE_X11)) && \
    !defined(FLTK_USE_CAIRO)
Fl_Text_Widget_Driver *Fl_Text_Widget_Driver::newTextWidgetDriver(Fl_Native_Text_Widget *n) {
  Fl_Text_Widget_Driver *retval = new Fl_Text_Widget_Driver();
  retval->widget = n;
  return retval;
}
#endif

Fl_Native_Text_Widget::Fl_Native_Text_Widget(int x, int y, int w, int h, const char *l) : Fl_Native_Widget(x,y,w,h,l) {
  driver_ = Fl_Text_Widget_Driver::newTextWidgetDriver(this);
  font_size_ = FL_NORMAL_SIZE;
  font_ = FL_HELVETICA;
  text_color_ = FL_FOREGROUND_COLOR;
  cursor_color_ = FL_FOREGROUND_COLOR;
  color(FL_BACKGROUND2_COLOR, FL_SELECTION_COLOR);
  is_readonly_ = false;
  is_selectable_ = true;
  wrap_ = false;
  rtl_ = false;
  driver_->kind = Fl_Text_Widget_Driver::SINGLE_LINE;
}


Fl_Native_Text_Widget::~Fl_Native_Text_Widget() {
  delete driver_;
};


void Fl_Native_Text_Widget::append(const char *t, int length) {
  driver_->append(t, length);
}


int Fl_Native_Text_Widget::handle(int event) {
  if (event == FL_SHOW) {
    driver_->show_widget();
    return 1;
  } else if (event == FL_HIDE) {
    driver_->hide_widget();
    return 1;
  } else if (event == FL_PUSH || event == FL_DRAG || event == FL_ENTER || event == FL_MOVE ||
             event == FL_RELEASE || event == FL_MOUSEWHEEL || event == FL_LEAVE) {
    int r = 0;
    if (event == FL_PUSH && active()){
      if (Fl::focus() != this) {
        Fl::focus(this);
        handle(FL_FOCUS);
      }
    }
    r = driver_->handle_mouse(event);
    return (r ? r : Fl_Native_Widget::handle(event));
  } else if (event == FL_FOCUS && active() && !readonly()) {
    return 1;
  } else if (event == FL_UNFOCUS) {
    driver_->unfocus();
    return Fl_Native_Widget::handle(event);
  } else if (event == FL_KEYBOARD) {
    if (Fl::e_keysym == FL_Tab) return 0;
    return driver_->handle_keyboard();
  } else if (event == FL_PASTE) {
    return driver_->handle_paste();
  } else if (event == FL_DND_ENTER || event == FL_DND_DRAG || event == FL_DND_RELEASE ||
             event == FL_DND_LEAVE) {
    return driver_->handle_dnd(event);
  }
  return Fl_Native_Widget::handle(event);
}


void Fl_Native_Text_Widget::got_focus() {
  driver_->focus();
}


void Fl_Native_Text_Widget::lost_focus() {
  driver_->unfocus();
}


void Fl_Native_Text_Widget::draw() {
  Fl_Native_Widget::draw();
  driver_->draw();
}


void Fl_Native_Text_Widget::resize(int x, int y, int w, int h) {
  Fl_Native_Widget::resize(x, y, w, h);
  driver_->resize(this->x(), this->y(), this->w(), this->h());
}


void Fl_Native_Text_Widget::textfont(Fl_Font f) {
  font_ = f;
  driver_->textfontandsize();
}


void Fl_Native_Text_Widget::textsize(Fl_Fontsize s) {
  font_size_ = s;
  driver_->textfontandsize();
}


void Fl_Native_Text_Widget::textcolor(Fl_Color c) {
  text_color_ = c;
}


const char *Fl_Native_Text_Widget::value() {
  return driver_->value();
}


int Fl_Native_Text_Widget::value(const char *t, int l) {
  driver_->value(t, l);
  return 1;
}


int Fl_Native_Text_Widget::value(const char *t) {
  return t ? value(t, (int)strlen(t)) : value("", 0);
}


int Fl_Native_Text_Widget::insert_position() const {
  return driver_->insert_position();
}


void Fl_Native_Text_Widget::insert_position(int pos) {
  driver_->insert_position(pos);
}


void Fl_Native_Text_Widget::insert_position(int pos, int m) {
  driver_->insert_position(pos, m);
}


void Fl_Native_Text_Widget::readonly(bool on_off) {
  is_readonly_ = on_off;
  driver_->readonly(on_off);
}


bool Fl_Native_Text_Widget::readonly() {
  return is_readonly_;
}


void Fl_Native_Text_Widget::selectable(bool on_off) {
  is_selectable_ = on_off;
  driver_->selectable(on_off);
}


bool Fl_Native_Text_Widget::selectable() {
  return is_selectable_;
}


int Fl_Native_Text_Widget::replace(int from, int to, const char *text, int len) {;
  if (from < 0) from = 0;
  if (to < 0) to = 0;
  if (from > to) {
    int tmp = to;
    to = from;
    from = tmp;
  }
  if (from == to && len == 0 && *text == 0) return 0;
  driver_->replace(from, to, text, len);
  return 1;
}


int Fl_Native_Text_Widget::insert(const char *text, int len) {
  driver_->replace_selection(text, len);
  return 1;
}


int Fl_Native_Text_Widget::cut(int a, int b) {
  return replace(a, b, NULL, 0);
}


int Fl_Native_Text_Widget::cut() {
  driver_->copy();
  driver_->replace_selection(NULL, 0);
  return 1;
}


int Fl_Native_Text_Widget::mark() const {
  return driver_->mark();
}


void Fl_Native_Text_Widget::mark(int n) {
  insert_position(insert_position(), n);
}


unsigned Fl_Native_Text_Widget::index(int i) const {
  return driver_->index(i);
}


int Fl_Native_Text_Widget::undo() {
  return driver_->undo();
}


int Fl_Native_Text_Widget::redo() {
  return driver_->redo();
}


bool Fl_Native_Text_Widget::can_undo() const {
  return driver_->can_undo();
}


bool Fl_Native_Text_Widget::can_redo() const {
  return driver_->can_redo();
}


void Fl_Native_Text_Widget::select_all() {
  driver_->select_all();
}


void Fl_Native_Text_Widget::copy() {
  driver_->copy();
}


void Fl_Native_Text_Widget::paste() {
  driver_->paste();
}


void Fl_Native_Text_Widget::right_to_left(bool rtl) {
  rtl_ = rtl;
  driver_->right_to_left();
}


Fl_Native_Multiline_Text_Widget::Fl_Native_Multiline_Text_Widget(int x, int y, int w, int h, const char *l) : Fl_Native_Text_Widget(x,y,w,h,l) {
  driver_->kind = Fl_Text_Widget_Driver::MULTIPLE_LINES;
  wrap(true);
}


void Fl_Text_Widget_Driver::maybe_do_callback(Fl_Callback_Reason reason) {
  if (widget->changed() || (widget->when()&FL_WHEN_NOT_CHANGED)) {
    widget->do_callback(reason);
  }
}
