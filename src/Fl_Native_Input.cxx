//
//  Fl_Native_Input.cxx
//

#include <FL/Fl_Native_Input.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Window.H>
#include "../src/Fl_Native_Input_Driver.H"

//
// Section to support platforms that don't implement Fl_Native_Input with a native input widget
//

// Eliminate platforms implementing Fl_Native_Input with a native input widget
#if !(defined(__APPLE__) && (!defined(FLTK_USE_X11) || !FLTK_USE_X11)) && \
    !defined(FLTK_USE_CAIRO) && !defined(_WIN32)

#include <FL/Fl_Input.H>
#include <FL/Fl_Multiline_Input.H>

// Class Fl_Backup_Native_Input_Driver gives a default implementation of
// Fl_Native_Input_Driver for platforms that don't provide a platform-native text widget.
// In this implementation, Fl_Native_Input_Driver is an Fl_Group
// containing an Fl_Input or Fl_Multiline_Input, and focus is managed normally by FLTK.
class Fl_Backup_Native_Input_Driver : public Fl_Native_Input_Driver {
private:
  Fl_Input *input_;
public:
  Fl_Backup_Native_Input_Driver() { input_ = NULL; }
  ~Fl_Backup_Native_Input_Driver() { if (input_) delete input_; }
  void show_widget() FL_OVERRIDE {
    if (!input_) {
      widget->begin();
      if (kind == SINGLE_LINE) input_ = new Fl_Input(widget->x()+Fl::box_dx(widget->box()),
                                                    widget->y()+Fl::box_dy(widget->box()),
                                                    widget->w()-Fl::box_dw(widget->box()),
                                                    widget->h()-Fl::box_dh(widget->box()),
                                                    NULL);
      else input_ = new Fl_Multiline_Input(widget->x()+Fl::box_dx(widget->box()),
                                          widget->y()+Fl::box_dy(widget->box()),
                                          widget->w()-Fl::box_dw(widget->box()),
                                          widget->h()-Fl::box_dh(widget->box()),
                                          NULL);
      input_->textfont(widget->textfont());
      input_->textsize(widget->textsize());
      input_->textcolor(widget->textcolor());
      input_->color(widget->color());
      input_->wrap(widget->wrap());
      input_->box(FL_FLAT_BOX);
      widget->end();
    }
  }
  Fl_Native_Group *as_native_group() FL_OVERRIDE { return NULL; }
  void value(const char *t, int len) FL_OVERRIDE {
    if (!input_) show_widget();
    input_->value(t, len);
  }
  const char *value() FL_OVERRIDE {
    return input_ ? input_->value() : "";
  }
  unsigned index(int i) const FL_OVERRIDE {
    return input_ ? input_->index(i) : 0;
  }
  void replace(int from, int to, const char *text, int len) FL_OVERRIDE {
    if (!input_) show_widget();
    input_->replace(from, to, text, len);
  }
  void replace_selection(const char *text, int len) FL_OVERRIDE {
    if (!input_) show_widget();
    input_->replace(insert_position(), mark(), text, len);
  }
  int insert_position() FL_OVERRIDE {
    return input_ ? input_->insert_position() : 0;
  }
  void insert_position(int pos, int mark) FL_OVERRIDE {
    if (input_) input_->insert_position(pos, mark);
  }
  int mark() FL_OVERRIDE {
    return input_ ? input_->mark() : 0;
  }
  int handle_focus() FL_OVERRIDE {
    return widget->Fl_Native_Group::handle(FL_FOCUS);
  }
  int handle_dnd(int event) FL_OVERRIDE {
    return input_->handle(event);
  }
  void select_all() FL_OVERRIDE {
    if (input_) input_->insert_position(0, input_->size());
  }
  void paste() FL_OVERRIDE {
    if (!input_) show_widget();
    Fl::paste(*input_, 1);
  }
  void copy() FL_OVERRIDE {
    if (!input_) return;
    int i = input_->insert_position();
    int m = input_->mark();
    if (i > m) { int tmp = i; i = m; m = tmp; }
    if (i < m) {
      const char *val = input_->value();
      Fl::copy(val + i, m-i, 1);
    }
  }
  int undo() FL_OVERRIDE {
    return input_ ? input_->undo() : 0;
  }
  int redo() FL_OVERRIDE {
    return input_ ? input_->redo() : 0;
  }
  bool can_undo() const FL_OVERRIDE {
    return input_ ? input_->can_undo() : false;
  }
  bool can_redo() const FL_OVERRIDE {
    return input_ ? input_->can_redo() : false;
  }
  void textcolor() FL_OVERRIDE {
    if (input_) {
      input_->textcolor(widget->textcolor());
      input_->redraw();
    }
  }
  void textfontandsize() FL_OVERRIDE {
    if (input_) {
      input_->textfont(widget->textfont());
      input_->textsize(widget->textsize());
      input_->redraw();
    }
  }
};


Fl_Native_Input_Driver *Fl_Native_Input_Driver::newTextWidgetDriver(Fl_Native_Input *n) {
  Fl_Backup_Native_Input_Driver *retval = new Fl_Backup_Native_Input_Driver();
  retval->widget = n;
  return retval;
}
#endif
//
// End of section to support platforms that don't implement Fl_Native_Input with a native input widget
//


Fl_Native_Input::Fl_Native_Input(int x, int y, int w, int h, const char *l) :
      Fl_Native_Group(x,y,w,h,l) {
  end();
  driver_ = Fl_Native_Input_Driver::newTextWidgetDriver(this);
  font_size_ = FL_NORMAL_SIZE;
  font_ = FL_HELVETICA;
  text_color_ = FL_FOREGROUND_COLOR;
  cursor_color_ = FL_FOREGROUND_COLOR;
  color(FL_BACKGROUND2_COLOR, FL_SELECTION_COLOR);
  is_readonly_ = false;
  is_selectable_ = true;
  wrap_ = false;
  rtl_ = false;
  driver_->kind = Fl_Native_Input_Driver::SINGLE_LINE;
}


Fl_Native_Input::~Fl_Native_Input() {
  delete driver_;
};


void Fl_Native_Input::get_focus() {
  driver_->focus();
}


Fl_Native_Group *Fl_Native_Input::as_native_group() {
  return driver_->as_native_group();
}


void Fl_Native_Input::lost_focus() {
  driver_->unfocus();
  if (!readonly() && (when() & FL_WHEN_RELEASE))
    driver_->maybe_do_callback(FL_REASON_LOST_FOCUS);
}


void Fl_Native_Input::append(const char *t, int length) {
  driver_->append(t, length);
}


int Fl_Native_Input::handle(int event) {
  switch (event) {
    case FL_SHOW:
      driver_->show_widget();
      break;
    case FL_HIDE:
      driver_->hide_widget();
      return 1;
    case FL_PUSH:
    case FL_DRAG:
    case FL_ENTER:
    case FL_MOVE:
    case FL_RELEASE:
    case FL_MOUSEWHEEL:
    case FL_LEAVE:
      if (event == FL_PUSH && active()) {
        if (Fl::focus() != this) {
          Fl::focus(this);
          handle(FL_FOCUS);
        }
      }
      if (driver_->handle_mouse(event)) return 1;
      break;
    case FL_FOCUS:
      if (active() && !readonly()) return driver_->handle_focus();
      break;
    case FL_UNFOCUS:
      return 1;
    case FL_KEYBOARD:
      if (Fl::e_keysym == FL_Tab) return 0;
      return driver_->handle_keyboard();
    case FL_PASTE:
      return driver_->handle_paste();
    case FL_DND_ENTER:
    case FL_DND_DRAG:
    case FL_DND_RELEASE:
    case FL_DND_LEAVE:
      return driver_->handle_dnd(event);
    case FL_DEACTIVATE:
      return (driver_->deactivate(), 1);
    case FL_ACTIVATE:
      return (driver_->activate(), 1);
   default:
      break;
  }
  return Fl_Native_Group::handle(event);
}


void Fl_Native_Input::draw() {
  Fl_Group::draw();
  driver_->draw();
}


void Fl_Native_Input::resize(int x, int y, int w, int h) {
  Fl_Group::resize(x, y, w, h);
  driver_->resize(this->x(), this->y(), this->w(), this->h());
}


void Fl_Native_Input::textfont(Fl_Font f) {
  font_ = f;
  driver_->textfontandsize();
}


void Fl_Native_Input::textsize(Fl_Fontsize s) {
  font_size_ = s;
  driver_->textfontandsize();
}


void Fl_Native_Input::textcolor(Fl_Color c) {
  text_color_ = c;
  driver_->textcolor();
}


const char *Fl_Native_Input::value() {
  return driver_->value();
}


int Fl_Native_Input::value(const char *t, int l) {
  driver_->value(t, l);
  return 1;
}


int Fl_Native_Input::value(const char *t) {
  return t ? value(t, (int)strlen(t)) : value("", 0);
}


int Fl_Native_Input::insert_position() const {
  return driver_->insert_position();
}


void Fl_Native_Input::insert_position(int pos) {
  driver_->insert_position(pos);
}


void Fl_Native_Input::insert_position(int pos, int m) {
  driver_->insert_position(pos, m);
}


void Fl_Native_Input::readonly(bool on_off) {
  is_readonly_ = on_off;
  driver_->readonly(on_off);
}


bool Fl_Native_Input::readonly() {
  return is_readonly_;
}


void Fl_Native_Input::selectable(bool on_off) {
  is_selectable_ = on_off;
  driver_->selectable(on_off);
}


bool Fl_Native_Input::selectable() {
  return is_selectable_;
}


int Fl_Native_Input::replace(int from, int to, const char *text, int len) {;
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


int Fl_Native_Input::insert(const char *text, int len) {
  driver_->replace_selection(text, len);
  return 1;
}


int Fl_Native_Input::cut(int a, int b) {
  return replace(a, b, NULL, 0);
}


int Fl_Native_Input::cut() {
  driver_->copy();
  driver_->replace_selection(NULL, 0);
  return 1;
}


int Fl_Native_Input::mark() const {
  return driver_->mark();
}


void Fl_Native_Input::mark(int n) {
  insert_position(insert_position(), n);
}


unsigned Fl_Native_Input::index(int i) const {
  return driver_->index(i);
}


int Fl_Native_Input::undo() {
  return driver_->undo();
}


int Fl_Native_Input::redo() {
  return driver_->redo();
}


bool Fl_Native_Input::can_undo() const {
  return driver_->can_undo();
}


bool Fl_Native_Input::can_redo() const {
  return driver_->can_redo();
}


void Fl_Native_Input::select_all() {
  driver_->select_all();
}


void Fl_Native_Input::copy() {
  driver_->copy();
}


void Fl_Native_Input::paste() {
  driver_->paste();
}


void Fl_Native_Input::right_to_left(bool rtl) {
  rtl_ = rtl;
  driver_->right_to_left();
}


Fl_Native_Multiline_Input::Fl_Native_Multiline_Input(int x, int y, int w, int h, const char *l) : Fl_Native_Input(x,y,w,h,l) {
  driver_->kind = Fl_Native_Input_Driver::MULTIPLE_LINES;
  wrap(true);
}


void Fl_Native_Input_Driver::deactivate() {
  if (Fl::focus() == widget) unfocus();
  textcolor();
}


void Fl_Native_Input_Driver::activate() {
  textcolor();
}


void Fl_Native_Input_Driver::maybe_do_callback(Fl_Callback_Reason reason) {
  if (widget->changed() || (widget->when()&FL_WHEN_NOT_CHANGED)) {
    widget->do_callback(reason);
  }
}


// Given a position in byte units in the text, returns the corresponding
// position in character units of the character containing said byte.
int Fl_Native_Input_Driver::byte_pos_to_char_pos(int pos) {
  const char *text = value(), *p = text, *end = text + strlen(text);
  int len, char_count = 0;
  while (true) {
    fl_utf8decode(p , end, &len);
    p += len;
    if (p - text > pos) break;
    char_count++;
  }
  return char_count;
}
