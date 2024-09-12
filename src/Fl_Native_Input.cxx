//
// Classes Fl_Native_Input and Fl_Backup_Native_Input_Driver for the Fast Light Tool Kit (FLTK).
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

#include <FL/Fl_Group.H>
#include "../src/Fl_Native_Input_Driver.H"
#include <FL/Fl_Input.H>
#include <FL/Fl_Multiline_Input.H>

#ifndef FL_DOXYGEN

//
// Section to support platforms that don't implement Fl_Native_Input with a native input widget
//

Fl_Backup_Native_Input_Driver::Fl_Backup_Native_Input_Driver() { }


Fl_Backup_Native_Input_Driver::~Fl_Backup_Native_Input_Driver() { }


void Fl_Backup_Native_Input_Driver::show_widget() {
  widget->Fl_Input::handle(FL_SHOW);
}


void Fl_Backup_Native_Input_Driver::hide_widget() {
  widget->Fl_Input::handle(FL_HIDE);
}


void Fl_Backup_Native_Input_Driver::value(const char *t, int len)  {
  widget->Fl_Input::value(t, len);
}


const char *Fl_Backup_Native_Input_Driver::value() {
  return widget->Fl_Input::value();
}


unsigned Fl_Backup_Native_Input_Driver::index(int i) const  {
  return widget->Fl_Input::index(i);
}


void Fl_Backup_Native_Input_Driver::replace(int from, int to, const char *text, int len)  {
  widget->Fl_Input::replace(from, to, text, len);
}


void Fl_Backup_Native_Input_Driver::replace_selection(const char *text, int len)  {
  widget->Fl_Input::replace(insert_position(), mark(), text, len);
}


int Fl_Backup_Native_Input_Driver::insert_position()  {
  return widget->Fl_Input::insert_position();
}


void Fl_Backup_Native_Input_Driver::insert_position(int pos, int mark)  {
  widget->Fl_Input::insert_position(pos, mark);
}


int Fl_Backup_Native_Input_Driver::mark()  {
  return widget->Fl_Input::mark();
}


int Fl_Backup_Native_Input_Driver::handle_keyboard()  {
  return widget->Fl_Input::handle(FL_KEYBOARD);
}

int Fl_Backup_Native_Input_Driver::handle_mouse(int event)  {
  return widget->Fl_Input::handle(event);
}


int Fl_Backup_Native_Input_Driver::handle_focus(int event)  {
  return widget->Fl_Input::handle(event);
}


int Fl_Backup_Native_Input_Driver::handle_dnd(int event)  {
  return widget->Fl_Input::handle(event);
}


int Fl_Backup_Native_Input_Driver::handle_paste() {
  return widget->Fl_Input::handle(FL_PASTE);
}


void Fl_Backup_Native_Input_Driver::select_all()  {
  widget->Fl_Input::insert_position(0, size());
}


void Fl_Backup_Native_Input_Driver::paste()  {
  Fl::paste(*widget, 1);
}


int Fl_Backup_Native_Input_Driver::copy()  {
  int i = widget->insert_position();
  int m = widget->mark();
  if (i > m) { int tmp = i; i = m; m = tmp; }
  if (i < m) {
    const char *val = widget->value();
    Fl::copy(val + i, m-i, 1);
    return 1;
  }
  return 0;
}


int Fl_Backup_Native_Input_Driver::undo()  {
  return widget->Fl_Input::undo();
}


int Fl_Backup_Native_Input_Driver::redo()  {
  return widget->Fl_Input::redo();
}


bool Fl_Backup_Native_Input_Driver::can_undo() const  {
  return widget->Fl_Input::can_undo();
}


bool Fl_Backup_Native_Input_Driver::can_redo() const  {
  return widget->Fl_Input::can_redo();
}


void Fl_Backup_Native_Input_Driver::textcolor() {
  widget->redraw();
}


void Fl_Backup_Native_Input_Driver::textfontandsize() {
  widget->redraw();
}


int Fl_Backup_Native_Input_Driver::size()  {
  return widget->Fl_Input::size();
}


int Fl_Backup_Native_Input_Driver::copy_cuts()  {
  return widget->copy_cuts();
}


void Fl_Backup_Native_Input_Driver::set_kind(kind_enum k) {
  Fl_Native_Input_Driver::set_kind(k);
  if (k == MULTIPLE_LINES) widget->type(FL_MULTILINE_INPUT);
}


//
// End of section to support platforms that don't implement Fl_Native_Input with a native input widget
//

#endif // ndef FL_DOXYGEN


/** Create a new Fl_Native_Input widget.
 This constructor creates a single-line, text input widget and adds it to the current Fl_Group.
 The value() is set to NULL.
 The default boxtype is FL_DOWN_BOX.
 \param    x,y,w,h  the dimensions of the new widget
 \param    l  an optional label text
 */
Fl_Native_Input::Fl_Native_Input(int x, int y, int w, int h, const char *l) : Fl_Input(x,y,w,h,l) {
  box(FL_DOWN_BOX);
  align(FL_ALIGN_LEFT);
  set_flag(SHORTCUT_LABEL);
  set_flag(NEEDS_KEYBOARD);
  driver = Fl_Native_Input_Driver::newNativeInputDriver(this);
  textsize(FL_NORMAL_SIZE);
  textfont(FL_HELVETICA);
  textcolor(FL_FOREGROUND_COLOR);
  cursor_color(FL_FOREGROUND_COLOR);
  color(FL_BACKGROUND2_COLOR, FL_SELECTION_COLOR);
  is_readonly_ = false;
  is_selectable_ = true;
  wrap(false);
  rtl_ = false;
  shortcut(0);
  driver->set_kind(Fl_Native_Input_Driver::SINGLE_LINE);
}


/** Destructor */
Fl_Native_Input::~Fl_Native_Input() {
  delete driver;
};


int Fl_Native_Input::handle(int event) {
  switch (event) {
    case FL_SHOW:
      driver->show_widget();
      break;
    case FL_HIDE:
      driver->hide_widget();
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
      if (driver->handle_mouse(event)) return 1;
      break;
    case FL_FOCUS:
      if (active() && !readonly()) return driver->handle_focus(FL_FOCUS);
      break;
    case FL_UNFOCUS: {
      int r = driver->handle_focus(FL_UNFOCUS);
      if (!readonly() && (when() & FL_WHEN_RELEASE))
        driver->maybe_do_callback(FL_REASON_LOST_FOCUS);
      return r;
      }
    case FL_KEYBOARD:
      if (Fl::e_keysym == FL_Tab &&
          (driver->kind == Fl_Native_Input_Driver::SINGLE_LINE || tab_nav())) return 0;
      return driver->handle_keyboard();
    case FL_PASTE:
      return driver->handle_paste();
    case FL_DND_ENTER:
    case FL_DND_DRAG:
    case FL_DND_RELEASE:
    case FL_DND_LEAVE:
      return driver->handle_dnd(event);
    case FL_DEACTIVATE:
      return (driver->deactivate(), 1);
    case FL_ACTIVATE:
      return (driver->activate(), 1);
   default:
      break;
  }
  return Fl_Input::handle(event);
}


void Fl_Native_Input::draw() {
  if (driver->is_backup_driver()) {
    Fl_Input::draw();
  } else {
    if (damage() & ~FL_DAMAGE_CHILD) {
      draw_box();
    }
    driver->draw();
  }
}


void Fl_Native_Input::resize(int x, int y, int w, int h) {
  Fl_Input::resize(x, y, w, h);
  driver->resize();
}


/** Set whether text selection in widget is possible */
void Fl_Native_Input::selectable(bool on_off) {
  is_selectable_ = on_off;
  driver->selectable(on_off);
}


/** Return whether text selection in widget is possible */
bool Fl_Native_Input::selectable() {
  return is_selectable_;
}


#ifndef FL_DOXYGEN


void Fl_Native_Input::append(const char *t, int length, char keep_selection) {
  int insert, v_mark;
  if (keep_selection) {
    insert = insert_position();
    v_mark = mark();
  }
  driver->append(t, length);
  if (keep_selection) {
    insert_position(insert, v_mark);
  }
}


Fl_Font Fl_Native_Input::textfont() { return Fl_Input_::textfont(); }


Fl_Fontsize Fl_Native_Input::textsize() { return Fl_Input_::textsize(); }


Fl_Color Fl_Native_Input::textcolor() { return Fl_Input_::textcolor(); }


void Fl_Native_Input::textfont(Fl_Font f) {
  Fl_Input_::textfont(f);
  driver->textfontandsize();
}


void Fl_Native_Input::textsize(Fl_Fontsize s) {
  Fl_Input_::textsize(s);
  driver->textfontandsize();
}


void Fl_Native_Input::textcolor(Fl_Color c) {
  Fl_Input_::textcolor(c);
  driver->textcolor();
}


const char *Fl_Native_Input::value() {
  return driver->value();
}


int Fl_Native_Input::value(const char *t, int l) {
  driver->value(t, l);
  return 1;
}


int Fl_Native_Input::value(const char *t) {
  return t ? value(t, (int)strlen(t)) : value("", 0);
}


int Fl_Native_Input::static_value(const char *str, int len) {
  driver->static_value(str, len);
  return 1;
}


int Fl_Native_Input::static_value(const char *str) {
  return static_value(str, (int)strlen(str));
}


int Fl_Native_Input::insert_position() const {
  return driver->insert_position();
}


void Fl_Native_Input::insert_position(int pos) {
  driver->insert_position(pos);
}


void Fl_Native_Input::insert_position(int pos, int m) {
  driver->insert_position(pos, m);
}


void Fl_Native_Input::readonly(bool on_off) {
  is_readonly_ = on_off;
  if (is_readonly_) clear_flag(NEEDS_KEYBOARD);
  driver->readonly(on_off);
}


bool Fl_Native_Input::readonly() {
  return is_readonly_;
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
  driver->replace(from, to, text, len);
  return 1;
}


int Fl_Native_Input::insert(const char *text, int len) {
  driver->replace_selection(text, len);
  return 1;
}


int Fl_Native_Input::cut(int a, int b) {
  return replace(a, b, NULL, 0);
}

    
int Fl_Native_Input::cut(int n) {
  int a = insert_position();
  return replace(a, a + n, NULL, 0);
}


int Fl_Native_Input::cut() {
  driver->copy();
  driver->replace_selection(NULL, 0);
  return 1;
}


int Fl_Native_Input::size() const {
  return driver->size();
}


void Fl_Native_Input::size(int W, int H) {
  Fl_Widget::size(W, H);
}

int Fl_Native_Input::mark() const {
  return driver->mark();
}


void Fl_Native_Input::mark(int n) {
  insert_position(insert_position(), n);
}


unsigned Fl_Native_Input::index(int i) const {
  return driver->index(i);
}


int Fl_Native_Input::undo() {
  return driver->undo();
}


int Fl_Native_Input::redo() {
  return driver->redo();
}


bool Fl_Native_Input::can_undo() const {
  return driver->can_undo();
}


bool Fl_Native_Input::can_redo() const {
  return driver->can_redo();
}


int Fl_Native_Input::copy(int clipboard) {
  return driver->copy();
}


#endif // !FL_DOXYGEN


/**
 Gets the input field type.
 \return FL_MULTILINE_INPUT  or  FL_NORMAL_INPUT
*/
int Fl_Native_Input::input_type() const {
  return (driver->kind == Fl_Native_Input_Driver::MULTIPLE_LINES ? FL_MULTILINE_INPUT : FL_NORMAL_INPUT);
}


/** Sets the input field type.
 This member function is effective only if called before the widget gets show()'n.
 \param [in]  t  FL_MULTILINE_INPUT  or  FL_NORMAL_INPUT
 */
void Fl_Native_Input::input_type(int t) {
  driver->set_kind(t == FL_MULTILINE_INPUT ? Fl_Native_Input_Driver::MULTIPLE_LINES : Fl_Native_Input_Driver::SINGLE_LINE);
}


/** Replace the current selection by  the clipboard content */
void Fl_Native_Input::paste() {
  driver->paste();
}


/** Select all text in the input widget */
void Fl_Native_Input::select_all() {
  driver->select_all();
}

    
/** Return whether the input widget preferentially contains right-to-left text */
bool Fl_Native_Input::right_to_left() {
  return rtl_;
}

/** Set whether the native input widget is expected to contain right-to-left or left-to-right text
 \param rtl \e true for right-to-left , \e false for left-to-right text
 */
void Fl_Native_Input::right_to_left(bool rtl) {
  rtl_ = rtl;
  driver->right_to_left();
}


/** Create a new Fl_Native_Multiline_Input widget.
 This constructor creates a multiple-line, text input widget and adds it to the current Fl_Group.
 In contrast with Fl_Multiline_Input, an Fl_Native_Multiline_Input widget supports any kind of script, right-to-left included,
 on platforms that natively implement it.
 On other platforms, this widget is the same as Fl_Multiline_Input.
 A vertical scrollbar is also added. If \e Fl_Native_Input::wrap(false) is called later, a horizontal scroll is added.
 The value() is set to NULL.
 The default boxtype is FL_DOWN_BOX.
 \param    x,y,w,h  the dimensions of the new widget
 \param    l  an optional label text
 */
Fl_Native_Multiline_Input::Fl_Native_Multiline_Input(int x, int y, int w, int h, const char *l) : Fl_Native_Input(x,y,w,h,l) {
  driver->set_kind(Fl_Native_Input_Driver::MULTIPLE_LINES);
  wrap(true);
}


/** Destructor */
Fl_Native_Multiline_Input::~Fl_Native_Multiline_Input() {}


#ifndef FL_DOXYGEN

/**
  Copies the \e yank buffer to the clipboard.
  This method copies all the previous contiguous cuts from the undo information to the clipboard.
  \return 0 if the operation did not change the clipboard, 1 otherwise.
  \note This method is presently implemented for the Linux/Unix platform only.
*/
int Fl_Native_Input::copy_cuts() {
  return driver->copy_cuts();
}


void Fl_Native_Input_Driver::deactivate() {
  if (Fl::focus() == widget) handle_focus(FL_UNFOCUS);
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

#endif // ndef FL_DOXYGEN
