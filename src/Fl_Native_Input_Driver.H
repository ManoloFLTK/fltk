//
// Fl_Native_Input_Driver and Fl_Backup_Native_Input_Driver for the Fast Light Tool Kit (FLTK).
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

#ifndef FL_NATIVE_INPUT_DRIVER_H
#define FL_NATIVE_INPUT_DRIVER_H

#ifndef FL_DOXYGEN

#include <FL/Fl_Native_Input.H>
#include <limits.h>
#include <FL/Fl_Export.H>

class Fl_Native_Input_Driver {
public:
  enum kind_enum {SINGLE_LINE, MULTIPLE_LINES} kind;
  static Fl_Native_Input_Driver *newNativeInputDriver(Fl_Native_Input*);
  Fl_Native_Input *widget;
  Fl_Native_Input_Driver() {}
  virtual ~Fl_Native_Input_Driver() {}
  virtual void show_widget() {}
  virtual void hide_widget() {}
  virtual void resize() {}
  virtual void textfontandsize() {}
  virtual void textcolor() {}
  virtual const char *value() { return NULL; }
  virtual void value(const char *t, int len) {}
  virtual void static_value(const char *t, int len) { value(t, len); }
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
  virtual int size() { return (int)strlen(value()); }
  virtual int copy_cuts() { return 0; }
  virtual void deactivate();
  virtual void activate();
  virtual void set_kind(kind_enum k) { this->kind = k; };
  virtual bool is_backup_driver() { return false; }
  void maybe_do_callback(Fl_Callback_Reason reason);
  int byte_pos_to_char_pos(int pos);
};


// Class Fl_Backup_Native_Input_Driver gives a default implementation of
// Fl_Native_Input_Driver for platforms that don't provide a platform-native text widget.
class Fl_Backup_Native_Input_Driver : public Fl_Native_Input_Driver {
public:
  Fl_Backup_Native_Input_Driver();
  ~Fl_Backup_Native_Input_Driver();
  void show_widget() FL_OVERRIDE;
  void hide_widget() FL_OVERRIDE;
  void value(const char *t, int len) FL_OVERRIDE;
  const char *value() FL_OVERRIDE;
  unsigned index(int i) const FL_OVERRIDE;
  void replace(int from, int to, const char *text, int len) FL_OVERRIDE;
  void replace_selection(const char *text, int len) FL_OVERRIDE;
  int insert_position() FL_OVERRIDE;
  void insert_position(int pos, int mark) FL_OVERRIDE;
  int mark() FL_OVERRIDE;
  int handle_keyboard() FL_OVERRIDE;
  int handle_mouse(int event) FL_OVERRIDE;
  int handle_focus(int event) FL_OVERRIDE;
  int handle_dnd(int event) FL_OVERRIDE;
  int handle_paste() FL_OVERRIDE;
  void select_all() FL_OVERRIDE;
  void paste() FL_OVERRIDE;
  int copy() FL_OVERRIDE;
  int undo() FL_OVERRIDE;
  int redo() FL_OVERRIDE;
  bool can_undo() const FL_OVERRIDE;
  bool can_redo() const FL_OVERRIDE;
  void textcolor() FL_OVERRIDE;
  void textfontandsize() FL_OVERRIDE;
  int size() FL_OVERRIDE;
  int copy_cuts() FL_OVERRIDE;
  void set_kind(kind_enum) FL_OVERRIDE;
  bool is_backup_driver() FL_OVERRIDE { return true; }
};


#endif // ndef FL_DOXYGEN

#endif /* FL_NATIVE_INPUT_DRIVER_H */
