//
// Classes Fl_Native_Input and Fl_Native_Multiline_Input for the Fast Light Tool Kit (FLTK).
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

#ifndef FL_NATIVE_INPUT_H
#define FL_NATIVE_INPUT_H

#include <FL/Fl_Input.H>


class Fl_Native_Input_Driver;

/** This is the FLTK platform-native text input widget.
 
 It displays a single line of text and lets the user edit it.
 The text may contain any Unicode text.
 In contrast with Fl_Input, an Fl_Native_Input widget supports any kind of script, right-to-left included,
 on platforms (macOS, Windows, Wayland and X11-with-cairo)
 that natively implement it. On other platforms, this widget is the same as an Fl_Input widget.
 Characters can be input using the keyboard
 or a text input method. Character composition is done using dead keys and/or a compose key as defined by the operating system.
 Text selection, copy/paste to/from the clipboard, undo, redo, and text drag-and-drop are supported.
 By default, it is drawn with an inset box and a white background.
 */
class FL_EXPORT Fl_Native_Input : public Fl_Input {
private:
  bool is_readonly_;
  bool is_selectable_;
  bool rtl_;
protected:
  Fl_Native_Input_Driver *driver;
public:
  Fl_Native_Input(int x, int y, int w, int h, const char *l = 0);
  virtual ~Fl_Native_Input();
  void right_to_left(bool rtl);
  bool right_to_left();
  int handle(int event) FL_OVERRIDE;
  void draw() FL_OVERRIDE;
  void resize(int x, int y, int w, int h) FL_OVERRIDE;
  void paste();
  void select_all();
  int input_type() const;
  void input_type(int t);
  void selectable(bool on_off);
  bool selectable();
#ifndef FL_DOXYGEN // don't repeat docu of inherited members of Fl_Input_
  void append(const char *t, int length = 0, char keep_selection = 0);
  void textfont(Fl_Font);
  void textsize(Fl_Fontsize);
  void textcolor(Fl_Color);
  Fl_Font textfont();
  Fl_Fontsize textsize();
  Fl_Color textcolor();
  const char *value();
  int value(const char *);
  int value(const char *, int len);
  int insert(const char* text, int len = 0);
  int insert_position() const;
  void insert_position(int pos);
  void insert_position(int pos, int m);
  void readonly(bool on_off);
  bool readonly();
  int replace(int from, int to, const char *text, int len = 0);
  int copy(int clipboard);
  int copy_cuts();
  int cut(int a, int b);
  int cut();
  int cut(int n);
  int mark() const;
  void mark(int n);
  unsigned index(int i) const;
  int size() const;
  void size(int W, int H);
  int static_value(const char *str, int len);
  int static_value(const char *str);
  int undo();
  int redo();
  bool can_undo() const;
  bool can_redo() const;
#endif // !FL_DOXYGEN
};


/** This is the FLTK platform-native multiple-line text input widget.
 Beside features of the Fl_Native_Input widget, Fl_Native_Multiline_Input contains a vertical scroller
 and can optionally wrap long lines at word boundaries. When word wrapping is off, a horizontal scroller
 is available.
*/
class FL_EXPORT Fl_Native_Multiline_Input : public Fl_Native_Input {
public:
  Fl_Native_Multiline_Input(int x, int y, int w, int h, const char *l = 0);
  virtual ~Fl_Native_Multiline_Input();
};

#endif /* FL_NATIVE_INPUT_H */
