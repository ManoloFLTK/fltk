//
//  Fl_Text_Widget_Driver.H
//
//

#ifndef Fl_Text_Widget_Driver_h
#define Fl_Text_Widget_Driver_h

#include <FL/Fl_Native_Text_Widget.H>
#include <limits.h>

class Fl_Text_Widget_Driver {
public:
  enum kind_enum {SINGLE_LINE, MULTIPLE_LINES} kind;
  static Fl_Text_Widget_Driver *newTextWidgetDriver(Fl_Native_Text_Widget*);
  Fl_Native_Text_Widget *widget;
  virtual ~Fl_Text_Widget_Driver() {}
  virtual void show_widget() {}
  virtual void hide_widget() {}
  virtual void resize(int x, int y, int w, int h) {}
  virtual void textfontandsize() {}
  virtual const char *value() { return NULL; }
  virtual void value(const char *t, int len) {}
  virtual int insert_position() { return 0; }
  void insert_position(int pos) { insert_position(pos, pos); }
  virtual void insert_position(int pos, int mark) {}
  virtual void readonly(bool on_off) {}
  virtual void selectable(bool on_off) {}
  virtual void replace(int from, int to, const char *text, int len) {}
  virtual void replace_selection(const char *text, int len) {}
  void append(const char *t, int length) { replace(INT_MAX, INT_MAX, t, (length == -1 ? (int)strlen(t) : length)); }
  virtual int mark() { return 0; }
  virtual unsigned index(int i) const { return 0; }
  virtual int undo() { return 0; }
  virtual int redo() { return 0; }
  virtual bool can_undo() const { return false; }
  virtual bool can_redo() const { return false; }
  virtual void focus() {}
  virtual void unfocus() {}
  virtual void select_all() {}
  virtual void copy() {}
  virtual void paste() {}
  virtual void draw() {}
  virtual int handle_keyboard() { return 0; }
  virtual int handle_mouse(int event) { return 0; }
  virtual int handle_paste() { return 1; }
  virtual int handle_dnd(int event) { return 1; }
  virtual void right_to_left() {}
  void maybe_do_callback(Fl_Callback_Reason reason);
};

#endif /* Fl_Text_Widget_Driver_h */
