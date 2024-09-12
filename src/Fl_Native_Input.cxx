//
//  Fl_Native_Input.cxx
//

#include <FL/Fl_Native_Input.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Input_.H>
#include "../src/Fl_Native_Input_Driver.H"

//
// Section to support platforms that don't implement Fl_Native_Input with a native input widget
//

// Eliminate platforms implementing Fl_Native_Input with a native input widget
#if !(defined(__APPLE__) && (!defined(FLTK_USE_X11) || !FLTK_USE_X11)) && \
    !defined(FLTK_USE_CAIRO) && !defined(_WIN32) && !defined(FL_DOXYGEN)

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
      Fl_Group *g = Fl_Group::current();
      Fl_Group::current(widget);
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
      input_->tab_nav(this->Fl_Native_Input_Driver::tab_nav());
      Fl_Group::current(g);
    }
  }
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
  int handle_focus(int event) FL_OVERRIDE {
    return widget->Fl_Group::handle(event);
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
  int copy() FL_OVERRIDE {
    if (!input_) return 0;
    int i = input_->insert_position();
    int m = input_->mark();
    if (i > m) { int tmp = i; i = m; m = tmp; }
    if (i < m) {
      const char *val = input_->value();
      Fl::copy(val + i, m-i, 1);
      return 1;
    }
    return 0;
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
  int size() FL_OVERRIDE {
    return input_ ? input_->size() : 0;
  }
  void tab_nav(int val) FL_OVERRIDE {
    if (input_) input_->tab_nav(val);
    Fl_Native_Input_Driver::tab_nav(val);
  }
  int copy_cuts() FL_OVERRIDE {
    return (input_ ? input_->copy_cuts() : 0);
  }
};


Fl_Native_Input_Driver *Fl_Native_Input_Driver::newNativeInputDriver(Fl_Native_Input *n) {
  Fl_Backup_Native_Input_Driver *retval = new Fl_Backup_Native_Input_Driver();
  retval->widget = n;
  retval->widget->type(0);
  return retval;
}
#endif
//
// End of section to support platforms that don't implement Fl_Native_Input with a native input widget
//


/** Create a new Fl_Native_Input widget.
 This constructor creates a single-line, text input widget and adds it to the current Fl_Group.
 The value() is set to NULL.
 The default boxtype is FL_DOWN_BOX.
 \param    x,y,w,h  the dimensions of the new widget
 \param    l  an optional label text
 */
Fl_Native_Input::Fl_Native_Input(int x, int y, int w, int h, const char *l) : Fl_Group(x,y,w,h,l) {
  end();
  box(FL_DOWN_BOX);
  type(FL_NATIVE_INPUT);
  driver = Fl_Native_Input_Driver::newNativeInputDriver(this);
  font_size_ = FL_NORMAL_SIZE;
  font_ = FL_HELVETICA;
  text_color_ = FL_FOREGROUND_COLOR;
  cursor_color_ = FL_FOREGROUND_COLOR;
  color(FL_BACKGROUND2_COLOR, FL_SELECTION_COLOR);
  is_readonly_ = false;
  is_selectable_ = true;
  wrap_ = false;
  rtl_ = false;
  shortcut_ = 0;
  driver->kind = Fl_Native_Input_Driver::SINGLE_LINE;
}


/** Destructor */
Fl_Native_Input::~Fl_Native_Input() {
  delete driver;
};


/** Returns whether the Fl_Native_Input widget uses a platform native widget.
 The result is \e true for  platforms macOS, Windows, Wayland and X11-with-cairo and \e false otherwise.*/
bool Fl_Native_Input::uses_native_widget() {
  return type() == FL_NATIVE_INPUT;
}

/**
 Append text at the end.
This function appends the string in \e t to the end of the text.
\param [in]  t  text that will be appended
\param [in]  length  length of text, or 0 if the string is terminated by nul.
\param [in]  keep_selection  if this is 1, the current text selection will remain, if 0, the cursor will move to the end of the inserted text.
 */
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
  return Fl_Group::handle(event);
}


void Fl_Native_Input::draw() {
  if (damage() & ~FL_DAMAGE_CHILD) {
    draw_box();
  }
  driver->draw();
  if (children()) draw_children();
}


void Fl_Native_Input::resize(int x, int y, int w, int h) {
  Fl_Group::resize(x, y, w, h);
  driver->resize();
}


/** Return the text font */
Fl_Font Fl_Native_Input::textfont() { return font_; }


/** Return the text size */
Fl_Fontsize Fl_Native_Input::textsize() { return font_size_; }


/** Return the text color */
Fl_Color Fl_Native_Input::textcolor() { return text_color_; }


/** Return the cursor color */
Fl_Color Fl_Native_Input::cursor_color() const {return cursor_color_;}

    
/** Set the cursor color */
void Fl_Native_Input::cursor_color(Fl_Color c) { cursor_color_ = c; }


/**
 Sets the font of the text in the input field.
 The text font defaults to FL_HELVETICA.
  \param [in]  f  the new text font
 */
void Fl_Native_Input::textfont(Fl_Font f) {
  font_ = f;
  driver->textfontandsize();
}

/**
 Sets the size of the text in the input field.
 The text height defaults to FL_NORMAL_SIZE.
\param [in]  s  the new font height in pixel units
 */
void Fl_Native_Input::textsize(Fl_Fontsize s) {
  font_size_ = s;
  driver->textfontandsize();
}


/**
 Sets the color of the text in the input field.
 The text color defaults to FL_FOREGROUND_COLOR.
\param     [in]  c  new text color
 */
void Fl_Native_Input::textcolor(Fl_Color c) {
  text_color_ = c;
  driver->textcolor();
}


/**
 Returns the UTF8-encoded text displayed in the widget.
 This function returns the current text content of the widget, as a pointer to an internal buffer valid only until the next event is handled.
 \return pointer to an internal buffer - do not free() this
 */
const char *Fl_Native_Input::value() {
  return driver->value();
}


/** Changes the widget text.
 This function changes the text and sets the mark and the point to the end of it for Fl_Native_Input and
 to the start of it for Fl_Native_Multiline_Input .
 The string is copied to the internal buffer. Passing NULL is the same as "".
 \param [in]  t  the new text
 \param [in]  l  the byte length of the new text
 \return 1
*/
int Fl_Native_Input::value(const char *t, int l) {
  driver->value(t, l);
  return 1;
}


/** Changes the widget text.
 This function changes the text and sets the mark and the point to the end of it for Fl_Native_Input and
 to the start of it for Fl_Native_Multiline_Input .
 The string is copied to the internal buffer. Passing NULL is the same as "".
\param [in]  t  the new text
\return 1
 */
int Fl_Native_Input::value(const char *t) {
  return t ? value(t, (int)strlen(t)) : value("", 0);
}


/**
 Changes the widget text.
 This function changes the text. Function undo is not active on text set with this function.
 You can use the len parameter to directly set the length if you know it already.
 \param [in]  str  the new text
 \param   [in]  len  the length of the new text
 \return 1
 */
int Fl_Native_Input::static_value(const char *str, int len) {
  driver->static_value(str, len);
  return 1;
}


/** Same as static_value(str, strlen(str)) */
int Fl_Native_Input::static_value(const char *str) {
  return static_value(str, (int)strlen(str));
}


/**
 Gets the position of the text cursor.
 \return the cursor position as an index in the range 0..size()
 */
int Fl_Native_Input::insert_position() const {
  return driver->insert_position();
}


/**
 Sets the cursor position and mark.
 position(n) is the same as position(n, n).
 \param pos  new index for cursor and mark
*/
void Fl_Native_Input::insert_position(int pos) {
  driver->insert_position(pos);
}


/** Sets the index for the cursor and mark.
 The input widget maintains two pointers into the string. The position (pos) is where the cursor is.
 The mark (m) is the other end of the selected text. If they are equal then there is no selection.
 Changing this does not affect the clipboard (use copy() to do that).
 Changing these values causes a redraw(). The new values are bounds checked.
\param pos  index for the cursor position
\param m  index for the mark
 */
void Fl_Native_Input::insert_position(int pos, int m) {
  driver->insert_position(pos, m);
}


/**Sets the read-only state of the input field.
\param [in] on_off  if \e false, the text in this widget can be edited by the user */
void Fl_Native_Input::readonly(bool on_off) {
  is_readonly_ = on_off;
  driver->readonly(on_off);
}


/** Gets the read-only state of the input field.
 \return \e true if this widget is read-only
*/
bool Fl_Native_Input::readonly() {
  return is_readonly_;
}


/** Sets the selectable state of the input field.
 \param [in] on_off  if \e true, a range of text in this widget can be selected by the user */
void Fl_Native_Input::selectable(bool on_off) {
  is_selectable_ = on_off;
  driver->selectable(on_off);
}


/** Gets the selectable state of the input field.
\return \e true if this widget is selectable
*/
bool Fl_Native_Input::selectable() {
  return is_selectable_;
}


/**Deletes text from \e from to \e to and inserts the new string \e text.
 All changes to the text buffer go through this function. It deletes the region between \e from and \e to
 (either one may be less or equal to the other), and then inserts the string \e text at that point and moves the mark() and position()
 to the end of the insertion. Does the callback if when() & FL_WHEN_CHANGED and there is a change.

 Set \e from and \e to equal to not delete anything. Set \e text to NULL to not insert anything.

 \e len can be zero or \e strlen(text), which saves a tiny bit of time if you happen to already know the length of the insertion, or can be used to insert a portion of a string. If \e len is zero, \e strlen(text) is used instead.

 \e from and \e to are clamped to the 0..size() range, so it is safe to pass any values. \e from, \e to, and \e len are
 numbers of bytes (not characters), where \e from and \e to count from 0 to size() (end of buffer).

 Parameters
 \param [in]  from  beginning index of text to be deleted
 \param [in]  to  ending index of text to be deleted and insertion position
 \param [in]  text  string that will be inserted
 \param [in]  len  length of text or 0 for nul terminated strings

 \return 0 if nothing changed, 1 otherwise
 */
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


/**
  Inserts text at the cursor position.
 This function inserts the string in \e text at the cursor position() and moves the new position and mark to the end of the inserted text.
 \param [in]  text  text that will be inserted
 \param [in]  len  length of text, or 0 if the string is terminated by nul.
 \return 1
*/
int Fl_Native_Input::insert(const char *text, int len) {
  driver->replace_selection(text, len);
  return 1;
}


/**
 Deletes all characters between index a and b.
 This function deletes the currently selected text without storing it in the clipboard. To use the clipboard, you may call copy() first.
 \param     a,b  range of bytes rounded to full characters and clamped to the buffer
 \return 0 if no data was copied
*/
int Fl_Native_Input::cut(int a, int b) {
  return replace(a, b, NULL, 0);
}

    
/**
 Deletes the next \e n bytes rounded to characters before or after the cursor.
 This function deletes the currently selected text without storing it in the clipboard.
 To use the clipboard, you may call copy() first.
 \param    n  number of bytes rounded to full characters and clamped to the buffer. A negative number will cut characters to the left of the cursor.
 \return 0 if no data was copied
*/
int Fl_Native_Input::cut(int n) {
  int a = insert_position();
  return replace(a, a + n, NULL, 0);
}


/**
 Deletes the current selection.
 This function deletes the currently selected text without storing it in the clipboard. To use the clipboard, you may call copy() first.
 \return 1
*/
int Fl_Native_Input::cut() {
  driver->copy();
  driver->replace_selection(NULL, 0);
  return 1;
}


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
  driver->kind = (t == FL_MULTILINE_INPUT ? Fl_Native_Input_Driver::MULTIPLE_LINES : Fl_Native_Input_Driver::SINGLE_LINE);
}


/** Gets the maximum length of the input field in characters. **/
int Fl_Native_Input::maximum_size() const {
  return driver->maximum_size();
}


/** Sets the maximum length of the input field in characters.
 This limits the number of characters that can be inserted in the widget.*/
void Fl_Native_Input::maximum_size(int m) {
  driver->maximum_size(m);
}


/** Return the shortcut key associated with this input widget.
\see Fl_Button::shortcut()
*/
int Fl_Native_Input::shortcut() const {return shortcut_;}


/** Sets the shortcut key associated with this widget.
Pressing the shortcut key gives text editing focus to this widget.
\param [in]  s  new shortcut keystroke
\see Fl_Button::shortcut()
*/
void Fl_Native_Input::shortcut(int s) { shortcut_ = s; }


/** Returns the number of bytes in value(). **/
int Fl_Native_Input::size() const {
  return driver->size();
}


/** Same as Fl_Widget::size(int W, int H) */
void Fl_Native_Input::size(int W, int H) {
  Fl_Widget::size(W, H);
}

/**Gets the current position of the selection mark in bytes. **/
int Fl_Native_Input::mark() const {
  return driver->mark();
}


/** Sets the current selection mark.
 mark(n) is the same as insert_position(insert_position(),n).
 \param n  new index of the mark
 */
void Fl_Native_Input::mark(int n) {
  insert_position(insert_position(), n);
}


/**
 Returns the character at index i.
 This function returns the UTF-8 character at i as a ucs4 character code.
 \param [in]  i  index into the value field
 \return the character at index i
*/
unsigned Fl_Native_Input::index(int i) const {
  return driver->index(i);
}


/**
 Undoes previous changes to the text buffer.
 This call undoes a number of previous calls to replace().
 \return non-zero if any change was made.
*/
int Fl_Native_Input::undo() {
  return driver->undo();
}


/**
 Redo previous undo operation.
 This call reapplies previously executed undo operations.
 \return non-zero if any change was made.
*/
int Fl_Native_Input::redo() {
  return driver->redo();
}


/** Return \e true if the widget can undo the last change */
bool Fl_Native_Input::can_undo() const {
  return driver->can_undo();
}


/** Return \e true if the widget can redo the last undo action */
bool Fl_Native_Input::can_redo() const {
  return driver->can_redo();
}


/** Select all text in the input widget */
void Fl_Native_Input::select_all() {
  driver->select_all();
}


/** Put the current selection into the clipboard.
 This function copies the current selection between mark() and position() to the cut/paste clipboard.
 This does not replace the old clipboard contents if position() and mark() are equal.
 \param clipboard  1
 \return 0 if no text is selected, 1 if the selection was copied
 */
int Fl_Native_Input::copy(int clipboard) {
  return driver->copy();
}


/** Replace the current selection by  the clipboard content */
void Fl_Native_Input::paste() {
  driver->paste();
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


/** Sets whether the Tab key does focus navigation, or inserts tab characters into Fl_Native_Multiline_Input.
 By default this flag is enabled to provide the 'normal' behavior most users expect; Tab navigates focus to the next widget.
 Disabling this flag gives the old FLTK behavior where Tab inserts a tab character into the text field, in which case only the mouse
 can be used to navigate to the next field.
 \param     [in]  val  If val is 1, Tab advances focus (default). If val is 0, Tab inserts a tab character.
 */
void Fl_Native_Input::tab_nav(int val) {
  driver->tab_nav(val);
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
 \see uses_native_widget()
 */
Fl_Native_Multiline_Input::Fl_Native_Multiline_Input(int x, int y, int w, int h, const char *l) : Fl_Native_Input(x,y,w,h,l) {
  driver->kind = Fl_Native_Input_Driver::MULTIPLE_LINES;
  wrap(true);
}


/** Destructor */
Fl_Native_Multiline_Input::~Fl_Native_Multiline_Input() {}


/**
  Copies the \e yank buffer to the clipboard.
  This method copies all the previous contiguous cuts from the undo information to the clipboard.
  \return 0 if the operation did not change the clipboard, 1 otherwise.
  \note This method is presently implemented for the Linux/Unix platform only.
*/
int Fl_Native_Input::copy_cuts() {
  return driver->copy_cuts();
}

    
/** Gets whether the Tab key causes focus navigation in multiline input fields or not.
  If enabled (default), hitting Tab causes focus navigation to the next widget.
  If disabled, hitting Tab inserts a tab character into the text field.
  \return 1 if Tab advances focus (default), 0 if Tab inserts tab characters.
 */
int Fl_Native_Input::tab_nav() {
  return driver->tab_nav();
}


/** Return whether lines are wrapped at word boundaries. */
bool Fl_Native_Input::wrap() { return wrap_; }


/** Set whether lines are wrapped at word boundaries.
Meaningful for class Fl_Native_Multiline_Input only. */
void Fl_Native_Input::wrap(bool value) { wrap_ = value; }


#ifndef FL_DOXYGEN

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
