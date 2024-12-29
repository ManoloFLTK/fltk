//
//  Fl_Native_Input_Driver.H
//
//

#ifndef FL_NATIVE_INPUT_DRIVER_H
#define FL_NATIVE_INPUT_DRIVER_H

#include <FL/Fl_Native_Input.H>
#include <limits.h>
#include <FL/Fl_Export.H>

class Fl_Native_Input_Driver {
protected:
  int maximum_size_;
public:
  enum kind_enum {SINGLE_LINE, MULTIPLE_LINES} kind;
  static Fl_Native_Input_Driver *newTextWidgetDriver(Fl_Native_Input*);
  Fl_Native_Input *widget;
  Fl_Native_Input_Driver() : maximum_size_(32767) {}
  virtual ~Fl_Native_Input_Driver() {}
  virtual bool is_native_group() { return true; }
  virtual void show_widget() {}
  virtual void hide_widget() {}
  virtual void resize(int x, int y, int w, int h) {}
  virtual void textfontandsize() {}
  virtual void textcolor() {}
  virtual const char *value() { return NULL; }
  virtual void value(const char *t, int len) {}
  virtual int insert_position() { return 0; }
  void insert_position(int pos) { insert_position(pos, pos); }
  virtual void insert_position(int pos, int mark) {}
  virtual void readonly(bool on_off) {}
  virtual void selectable(bool on_off) {}
  virtual void replace(int from, int to, const char *text, int len) {}
  virtual void replace_selection(const char *text, int len) {}
  void append(const char *t, int length) { replace(INT_MAX, INT_MAX, t, (length == 0 ? (int)strlen(t) : length)); }
  virtual int mark() { return 0; }
  virtual unsigned index(int i) const { return 0; }
  virtual int undo() { return 0; }
  virtual int redo() { return 0; }
  virtual bool can_undo() const { return false; }
  virtual bool can_redo() const { return false; }
  virtual void select_all() {}
  virtual int copy() { return 0;}
  virtual void paste() {}
  virtual void draw() {}
  virtual int handle_focus(int) { return 1; }
  virtual int handle_keyboard() { return 0; }
  virtual int handle_mouse(int event) { return 0; }
  virtual int handle_paste() { return 0; }
  virtual int handle_dnd(int event) { return 0; }
  virtual void right_to_left() {}
  virtual int maximum_size() const { return maximum_size_; };
  virtual void maximum_size(int m) {}
  virtual int size() { return (int)strlen(value()); }
  virtual void tab_nav(int val) { widget->tab_nav_ = val; }
  void deactivate();
  void activate();
  void maybe_do_callback(Fl_Callback_Reason reason);
  int byte_pos_to_char_pos(int pos);
};

#endif /* FL_NATIVE_INPUT_DRIVER_H */
