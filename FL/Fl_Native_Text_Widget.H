//
//  Fl_Native_Text_Widget.H
//
//

#ifndef FL_NATIVE_TEXT_WIDGET_H
#define FL_NATIVE_TEXT_WIDGET_H

#include <FL/Fl_Group.H>

class Fl_Text_Widget_Driver;

class FL_EXPORT Fl_Native_Text_Widget : public Fl_Native_Widget {
  friend class Fl_Native_Multiline_Text_Widget;
private:
  Fl_Color cursor_color_;
  Fl_Font font_;
  Fl_Fontsize font_size_;
  Fl_Color text_color_;
  bool is_readonly_;
  bool is_selectable_;
  bool rtl_;
  bool wrap_;
  Fl_Text_Widget_Driver *driver_;
public:
  Fl_Native_Text_Widget(int x, int y, int w, int h, const char *l = 0);
  ~Fl_Native_Text_Widget();
  void append(const char *t, int length = -1);
  void right_to_left(bool rtl);
  bool right_to_left() { return rtl_; }
  int handle(int event) FL_OVERRIDE;
  void draw() FL_OVERRIDE;
  void resize(int x, int y, int w, int h) FL_OVERRIDE;
  void textfont(Fl_Font);
  void textsize(Fl_Fontsize);
  void textcolor(Fl_Color);
  Fl_Font textfont() { return font_; }
  Fl_Fontsize textsize() { return font_size_; }
  Fl_Color textcolor() { return text_color_; }
  Fl_Color cursor_color() const {return cursor_color_;}
  void cursor_color(Fl_Color c) { cursor_color_ = c; }
  const char *value();
  int value(const char *);
  int value(const char *, int len);
  int insert(const char* text, int len = 0);
  int insert_position() const;
  void insert_position(int pos);
  void insert_position(int pos, int m);
  void readonly(bool on_off);
  bool readonly();
  void selectable(bool on_off);
  bool selectable();
  int replace(int from, int to, const char *text, int len = 0);
  void select_all();
  void copy();
  void paste();
  int cut(int a, int b);
  int cut();
  int mark() const;
  void mark(int n);
  unsigned index(int i) const;
  int undo();
  int redo();
  bool can_undo() const;
  bool can_redo() const;
  bool wrap() { return wrap_; }
  void got_focus() FL_OVERRIDE;
  void lost_focus() FL_OVERRIDE;
};


class FL_EXPORT Fl_Native_Multiline_Text_Widget : public Fl_Native_Text_Widget {
public:
  Fl_Native_Multiline_Text_Widget(int x, int y, int w, int h, const char *l = 0);
  void wrap(bool value) { wrap_ = value; }
};

#endif /* FL_NATIVE_TEXT_WIDGET_H */
