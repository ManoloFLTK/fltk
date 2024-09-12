//
// Fl_WinAPI_Native_Input_Driver for the Fast Light Tool Kit (FLTK).
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
#include <FL/platform.H> // for fl_win32_xid
#include "../WinAPI/Fl_WinAPI_Window_Driver.H"
#include "../WinAPI/Fl_WinAPI_Screen_Driver.H"
#include "../GDI/Fl_Font.H"
#include <FL/math.h>
#include <FL/fl_ask.H>          // fl_beep()

#include <windows.h>


class Fl_WinAPI_Native_Input_Driver : public Fl_Native_Input_Driver {
private:
  char *text_before_show_;
  int char_pos_to_byte_pos_(int char_count);
public:
  Fl_WinAPI_Native_Input_Driver();
  ~Fl_WinAPI_Native_Input_Driver();
  HWND edit_win;
  HBRUSH brush;
  static WNDPROC fltk_wnd_proc;
  static WNDPROC win32_edit_wnd_proc;
  void show_widget() FL_OVERRIDE;
  void hide_widget() FL_OVERRIDE;
  void resize() FL_OVERRIDE;
  void textfontandsize() FL_OVERRIDE;
  void textcolor() FL_OVERRIDE;
  const char *value() FL_OVERRIDE;
  void value(const char *t, int len) FL_OVERRIDE;
  int insert_position() FL_OVERRIDE;
  void insert_position(int pos, int mark) FL_OVERRIDE;
  void readonly(bool on_off) FL_OVERRIDE;
  void selectable(bool on_off) FL_OVERRIDE;
  void replace(int from, int to, const char *text, int len) FL_OVERRIDE;
  void replace_selection(const char *text, int len) FL_OVERRIDE;
  int mark() FL_OVERRIDE;
  unsigned index(int i) const FL_OVERRIDE;
  int undo() FL_OVERRIDE;
  int redo() FL_OVERRIDE;
  bool can_undo() const FL_OVERRIDE;
  bool can_redo() const FL_OVERRIDE;
  int handle_focus(int) FL_OVERRIDE;
  void select_all() FL_OVERRIDE;
  int copy() FL_OVERRIDE;
  void paste() FL_OVERRIDE;
  int handle_paste() FL_OVERRIDE;
  void right_to_left() FL_OVERRIDE;
  void draw() FL_OVERRIDE;
  const char *text_before_show() { return text_before_show_;}
  int handle_dnd(int event) FL_OVERRIDE;
  void activate() FL_OVERRIDE;
  void deactivate() FL_OVERRIDE;
};


Fl_Native_Input_Driver *Fl_Native_Input_Driver::newNativeInputDriver(Fl_Native_Input *n) {
  Fl_Native_Input_Driver *retval = (Fl_Native_Input_Driver*)new Fl_WinAPI_Native_Input_Driver();
  retval->widget = n;
  retval->widget->maximum_size(INT_MAX);
  return retval;
}


WNDPROC Fl_WinAPI_Native_Input_Driver::fltk_wnd_proc = NULL;
WNDPROC Fl_WinAPI_Native_Input_Driver::win32_edit_wnd_proc = NULL;


Fl_WinAPI_Native_Input_Driver::Fl_WinAPI_Native_Input_Driver() : Fl_Native_Input_Driver() {
  text_before_show_ = NULL;
  edit_win = NULL;
  brush = NULL;
}


Fl_WinAPI_Native_Input_Driver::~Fl_WinAPI_Native_Input_Driver() {
  delete[] text_before_show_;
  if (edit_win) DestroyWindow(edit_win);
  if (brush) DeleteObject(brush);
}


void Fl_WinAPI_Native_Input_Driver::resize() {
  int nscreen = Fl_Window_Driver::driver(widget->top_window())->screen_num();
  float s = Fl::screen_driver()->scale(nscreen);
  int xp = widget->x() + Fl::box_dx(widget->box());
  xp = int(round(xp * s));
  int yp = widget->y() + Fl::box_dy(widget->box());
  yp = int(round(yp * s));
  int wp = widget->w()-Fl::box_dw(widget->box());
  wp = int(wp * s);
  int hp = widget->h()-Fl::box_dh(widget->box());
  hp = int(hp * s);
  UINT flags = SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
  SetWindowPos(edit_win, 0, xp, yp, wp, hp, flags);
  textfontandsize();
}


// the current native widget having focus or NULL if something else has focus
static Fl_Widget *focus_to_native_widget = 0;


static LRESULT CALLBACK fltk_wnd_proc_plus_focus(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (uMsg == WM_CTLCOLOREDIT) {
    Fl_WinAPI_Native_Input_Driver *dr =
      (Fl_WinAPI_Native_Input_Driver*)GetWindowLongPtrW((HWND)lParam, GWLP_USERDATA);
    if (dr) {
      uchar r,g,b;
      Fl::get_color(dr->widget->active() ? dr->widget->textcolor() : fl_inactive(dr->widget->textcolor()), r, g, b);
      SetTextColor((HDC)wParam, RGB(r, g, b)); // set the text color
      Fl::get_color(dr->widget->color(), r, g, b);
      COLORREF bgcol = RGB(r, g, b);
      SetBkColor((HDC)wParam, bgcol); // set the text background color
      if (dr->brush) DeleteObject(dr->brush);
      dr->brush = CreateSolidBrush(bgcol); // set the widget background color (same as text bg color)
      return (LRESULT)dr->brush;
    }
  }
  if (uMsg == WM_COMMAND && (HIWORD(wParam) == EN_SETFOCUS || HIWORD(wParam) == EN_UPDATE)) {
    Fl_WinAPI_Native_Input_Driver *dr = (Fl_WinAPI_Native_Input_Driver*)GetWindowLongPtrW((HWND)lParam, GWLP_USERDATA);
    if (HIWORD(wParam) == EN_SETFOCUS) {
      if (dr->widget->active()) {
        dr->widget->take_focus();
      }
      return 0;
    } else if (HIWORD(wParam) == EN_UPDATE) { // runs after each change to text
      if (!dr->text_before_show()) {
        dr->widget->set_changed();
        if (dr->widget->when() & FL_WHEN_CHANGED) {
          dr->widget->do_callback(FL_REASON_CHANGED);
        }
      }
    }
  }
  return CallWindowProcW(Fl_WinAPI_Native_Input_Driver::fltk_wnd_proc, hWnd, uMsg, wParam, lParam);
}


static char *wchar_to_utf8(const wchar_t *wstr, char *&utf8);


static LRESULT CALLBACK fltk_edit_wnd_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  static int drag_start = -1;
  bool use_edit_proc = true;
  Fl_WinAPI_Native_Input_Driver *dr =
    (Fl_WinAPI_Native_Input_Driver*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
  if (uMsg == WM_KEYDOWN) {
    if ((wParam == VK_TAB && (dr->kind == Fl_Native_Input_Driver::SINGLE_LINE || dr->widget->tab_nav())) || wParam == VK_ESCAPE) { // handle Tab or Esc keystrokes by FLTK
      use_edit_proc = false;
    } else if (wParam == VK_RETURN && dr->kind == Fl_Native_Input_Driver::SINGLE_LINE) {
      if (dr->widget->when() & FL_WHEN_ENTER_KEY) {
        dr->maybe_do_callback(FL_REASON_ENTER_KEY);
      }
      use_edit_proc = false;
    }
    // detect Ctrl but not AltGr and not Ctrl-C or Ctrl-V
    if ( (GetKeyState(VK_CONTROL) >> 15) && !(GetAsyncKeyState(VK_RMENU) >> 15) &&
        !(GetKeyState('C') >> 15) && !(GetKeyState('V') >> 15) ) {
      use_edit_proc = false;
    }
  } else if (uMsg == WM_LBUTTONDOWN) {
    if (Fl::dnd_text_ops()) {
      int oldpos, oldmark;
      SendMessageW(dr->edit_win, EM_GETSEL, (WPARAM)&oldpos, (LPARAM)&oldmark);
      int newpos = LOWORD(SendMessageW(dr->edit_win, EM_CHARFROMPOS, (WPARAM)0, lParam));
      if (Fl::focus() == dr->widget && !Fl::event_state(FL_SHIFT) &&
          ( (newpos >= oldmark && newpos < oldpos) || (newpos >= oldpos && newpos < oldmark) ) ) {
        // user clicked in the selection, may be trying to drag
        drag_start = newpos;
        return 0;
      }
      drag_start = -1;
    }
  } else if (uMsg == WM_MOUSEMOVE) {
    if (Fl::dnd_text_ops() && drag_start >= 0 && !Fl::event_is_click()) {
      // drag the data:
      int from, to;
      SendMessageW(dr->edit_win, EM_GETSEL, (WPARAM)&from, (LPARAM)&to);
      if (from != to) {
        if (from > to) { int temp = from; from = to; to = temp; }
        WCHAR *out2 = new WCHAR[(to-from)+1];
        if (dr->kind == Fl_Native_Input_Driver::MULTIPLE_LINES) {
          HLOCAL h = (HLOCAL)SendMessageW(dr->edit_win, EM_GETHANDLE, (WPARAM)0, (LPARAM)0);
          WCHAR *out = (WCHAR*)LocalLock(h);
          memcpy(out2, out + from, (to-from) * sizeof(WCHAR));
          LocalUnlock(h);
        } else {
          int l = GetWindowTextLengthW(dr->edit_win);
          WCHAR *out = new WCHAR[l+1];
          GetWindowTextW(dr->edit_win, out, l+1);
          memcpy(out2, out + from, (to-from) * sizeof(WCHAR));
          delete[] out;
        }
        out2[to - from] = 0;
        char *utf8 = NULL;
        wchar_to_utf8(out2, utf8);
        delete[] out2;
        Fl::copy(utf8, strlen(utf8), 0);
        free(utf8);
        drag_start = -1;
        Fl::screen_driver()->dnd();
        return 0;
      }
    }
  } else if (uMsg == WM_LBUTTONUP) {
    drag_start = -1;
  }
  if ( use_edit_proc ) { // standard processing of most messages by an EDIT window
    return CallWindowProcW(Fl_WinAPI_Native_Input_Driver::win32_edit_wnd_proc,
                           hWnd, uMsg, wParam, lParam);
  }
  // apply FLTK's handling to remaining messages to catch shortcuts
  HWND xid = fl_win32_xid(dr->widget->window());
  return CallWindowProcW(Fl_WinAPI_Native_Input_Driver::fltk_wnd_proc, xid, uMsg, wParam, lParam);
}


void Fl_WinAPI_Native_Input_Driver::show_widget() {
  HWND flwin = (HWND)fl_win32_xid(widget->window());
  if (!flwin) return;
  if (!edit_win) {
    DWORD style, style_ex;
    style = WS_CHILD | WS_VISIBLE ;
    style_ex = (widget->right_to_left() ? WS_EX_LAYOUTRTL : 0);
    if (kind == MULTIPLE_LINES) {
      style |= WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
    }
    if (kind == SINGLE_LINE || !widget->wrap()) style |= ES_AUTOHSCROLL;
    if (widget->readonly()) style |= ES_READONLY;
    if (!widget->active()) style |= WS_DISABLED;
    int nscreen = Fl_Window_Driver::driver(widget->top_window())->screen_num();
    float s = Fl::screen_driver()->scale(nscreen);
    int xp = widget->x() + Fl::box_dx(widget->box());
    xp = int(round(xp * s));
    int yp = widget->y() + Fl::box_dy(widget->box());
    yp = int(round(yp * s));
    int wp = widget->w()-Fl::box_dw(widget->box());
    wp = int(wp * s);
    int hp = widget->h()-Fl::box_dh(widget->box());
    hp = int(hp * s);
    edit_win = CreateWindowExW(style_ex,
                               L"EDIT", NULL, style,
                               xp, yp, wp, hp,
                               flwin,
                               NULL, // menu
                               fl_display,
                               NULL); // creation parameters
    // Replace the parent window's WndProc by fltk_wnd_proc_plus_focus(),
    // adequate for an EDIT-containing window. Memorize the previous WndProc as fltk_wnd_proc().
    WNDPROC current_wnd_proc = (WNDPROC)GetWindowLongPtrW(flwin, GWLP_WNDPROC);
    if (current_wnd_proc != fltk_wnd_proc_plus_focus) {
      if (!fltk_wnd_proc) fltk_wnd_proc = current_wnd_proc;
      SetWindowLongPtrW(flwin, GWLP_WNDPROC, (LONG_PTR)fltk_wnd_proc_plus_focus);
    }
    // Replace the EDIT's Wndproc by fltk_edit_wnd_proc.
    // Memorize the previous WndProc as win32_edit_wnd_proc().
    current_wnd_proc = (WNDPROC)GetWindowLongPtrW(edit_win, GWLP_WNDPROC);
    if (current_wnd_proc != fltk_edit_wnd_proc) {
      if (!win32_edit_wnd_proc) win32_edit_wnd_proc = current_wnd_proc;
      SetWindowLongPtrW(edit_win, GWLP_WNDPROC, (LONG_PTR)fltk_edit_wnd_proc);
    }
    // Attach the Fl_WinAPI_Native_Input_Driver pter to the EDIT window as user-data.
    SetWindowLongPtrW(edit_win, GWLP_USERDATA, (LONG_PTR)this);
//fprintf(stderr,"CreateWindowExW=%p WinAPIdriver=%p\n",edit_win,this); fflush(stderr);

    fl_font(widget->textfont(), widget->textsize());
    Fl_GDI_Font_Descriptor *desc = (Fl_GDI_Font_Descriptor*)fl_graphics_driver->font_descriptor();
    SendMessageW(edit_win, WM_SETFONT, (WPARAM)desc->fid, (LPARAM)1);
    if (text_before_show_) {
      value(text_before_show_, -1);
    }
  } else {
    ShowWindow(edit_win, SW_SHOW);
  }
}


void Fl_WinAPI_Native_Input_Driver::hide_widget() {
  ShowWindow(edit_win, SW_HIDE);
  if (!widget->readonly() && (widget->when() & FL_WHEN_RELEASE))
    maybe_do_callback(FL_REASON_LOST_FOCUS);
}


void Fl_WinAPI_Native_Input_Driver::textfontandsize() {
  if (!edit_win) return;
  fl_font(widget->textfont(), widget->textsize());
  Fl_GDI_Font_Descriptor *desc = (Fl_GDI_Font_Descriptor*)fl_graphics_driver->font_descriptor();
  SendMessageW(edit_win, WM_SETFONT, (WPARAM)desc->fid, (LPARAM)1);
}


void Fl_WinAPI_Native_Input_Driver::textcolor()  {
  if (edit_win) {
    ShowWindow(edit_win, SW_HIDE);
    ShowWindow(edit_win, SW_SHOW);
    widget->redraw();
  }
}


static char *utf8_ = NULL;


static char *wchar_to_utf8(const wchar_t *wstr, char *&utf8) {
  unsigned len = (unsigned)wcslen(wstr);
  unsigned wn = fl_utf8fromwc(NULL, 0, wstr, len) + 1; // query length
  utf8 = (char *)realloc(utf8, wn);
  wn = fl_utf8fromwc(utf8, wn, wstr, len); // convert string
  utf8[wn] = 0;
  return utf8;
}


const char *Fl_WinAPI_Native_Input_Driver::value() {
  if (!edit_win) return text_before_show_;
  if (kind == SINGLE_LINE) {
    int l = GetWindowTextLengthW(edit_win);
    wchar_t *out = new wchar_t[l+1];
    GetWindowTextW(edit_win, out, l+1);
    out[l] = 0;
    wchar_to_utf8(out, utf8_);
    delete[] out;
  } else {
    HLOCAL h = (HLOCAL)SendMessageW(edit_win, EM_GETHANDLE, (WPARAM)0, (LPARAM)0);
    WCHAR *out = (WCHAR*)LocalLock(h);
    wchar_to_utf8(out, utf8_);
    LocalUnlock(h);
    char *p = utf8_;
    while ((p = strstr(p, "\r\n"))) { // remove Windows-style line ends
      memmove(p, p+1, strlen(p+1)+1);
    }
  }
  return utf8_;
}


static wchar_t *wbuf_ = NULL;

static wchar_t *utf8_to_wchar(const char *utf8, wchar_t *&wbuf, int lg = -1) {
  unsigned len = (lg >= 0) ? (unsigned)lg : (unsigned)strlen(utf8);
  unsigned wn = fl_utf8toUtf16(utf8, len, NULL, 0) + 1; // Query length
  wbuf = (wchar_t *)realloc(wbuf, sizeof(wchar_t) * wn);
  wn = fl_utf8toUtf16(utf8, len, (unsigned short *)wbuf, wn); // Convert string
  wbuf[wn] = 0;
  return wbuf;
}


void Fl_WinAPI_Native_Input_Driver::value(const char *t, int len) {
  if (!t) t = "";
  if (len < 0) len = strlen(t);
  if (text_before_show_ != t)  {
    delete[] text_before_show_;
    text_before_show_ = new char[len + 1];
    memcpy(text_before_show_, t, len);
    text_before_show_[len] = 0;
  }
  if (edit_win ) {
    if (kind == MULTIPLE_LINES) { // make sure we have Windows-style lines
      char *p = text_before_show_;
      while ((p = strchr(p, '\n')) && p > text_before_show_ && *(p-1) != '\r') {
        int offset = p - text_before_show_;
        text_before_show_ = (char*)realloc(text_before_show_, ++len + 1);
        p = text_before_show_ + offset;
        memmove(p+1, p, strlen(p)+1);
        *p = '\r';
        p += 2;
      }
    } else { // remove line ends
      char *p = text_before_show_;
      while ((p = strstr(p, "\r\n"))) {
        memmove(p, p+2, strlen(p+2)+1);
      }
      p = text_before_show_;
      while ((p = strchr(p, '\n'))) {
        memmove(p, p+1, strlen(p+1)+1);
      }
    }
    wchar_t *new_text = utf8_to_wchar(text_before_show_, wbuf_, len);
    SetWindowTextW(edit_win, new_text);
    delete[] text_before_show_;
    text_before_show_ = NULL;
  }
}


// Given a position in character units in the text, returns the corresponding
// position in byte units.
int Fl_WinAPI_Native_Input_Driver::char_pos_to_byte_pos_(int char_count) {
  const char *text;
  if (!edit_win) {
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
    wchar_t *out = new wchar_t[char_count+1];
    GetWindowTextW(edit_win, out, char_count+1);
    out[char_count] = 0;
    wchar_to_utf8(out, utf8_);
    delete[] out;
    return (int)strlen(utf8_);
  }
}


int Fl_WinAPI_Native_Input_Driver::insert_position() {
  DWORD from;
  SendMessageW(edit_win, EM_GETSEL, (WPARAM)&from, (LPARAM)NULL);
  return char_pos_to_byte_pos_(from);
}


void Fl_WinAPI_Native_Input_Driver::insert_position(int pos, int mark) {
  bool need_selection = (pos != mark && widget->selectable());
  pos = byte_pos_to_char_pos(pos);
  if (need_selection) mark = byte_pos_to_char_pos(mark);
  else mark = pos;
  SendMessageW(edit_win, EM_SETSEL, (WPARAM)pos, (LPARAM)mark);
}


void Fl_WinAPI_Native_Input_Driver::readonly(bool on_off) {
  SendMessageW(edit_win, EM_SETREADONLY, (WPARAM)on_off, (LPARAM)0);
}


void Fl_WinAPI_Native_Input_Driver::selectable(bool on_off) {
}


void Fl_WinAPI_Native_Input_Driver::replace(int from, int to, const char *text, int len) {
  insert_position(from, to);
  replace_selection(text, len);
}


void Fl_WinAPI_Native_Input_Driver::replace_selection(const char *text, int len) {
  utf8_to_wchar(text, wbuf_, len);
  SendMessageW(edit_win, EM_REPLACESEL, (WPARAM)TRUE, (LPARAM)wbuf_);
}


int Fl_WinAPI_Native_Input_Driver::mark() {
  DWORD to;
  SendMessageW(edit_win, EM_GETSEL, (WPARAM)NULL, (LPARAM)&to);
  return char_pos_to_byte_pos_(to);
}


unsigned Fl_WinAPI_Native_Input_Driver::index(int i) const {
  const char *s = widget->value();
  int len = (int)strlen(s);
  unsigned r = (i >= 0 && i < len ? fl_utf8decode(s + i, s + len, &len) : 0);
  return r;
}


int Fl_WinAPI_Native_Input_Driver::undo() {
  if (!can_undo()) return 0;
  SendMessageW(edit_win, EM_UNDO, (WPARAM)0, (LPARAM)0);
  return 1;
}


int Fl_WinAPI_Native_Input_Driver::redo() {
  return 0;
}


bool Fl_WinAPI_Native_Input_Driver::can_undo() const {
  return (SendMessageW(edit_win, EM_CANUNDO, (WPARAM)0, (LPARAM)0) != 0);
}


bool Fl_WinAPI_Native_Input_Driver::can_redo() const {
  return 0;
}


int Fl_WinAPI_Native_Input_Driver::handle_focus(int event) {
  if (event == FL_FOCUS) {
    //fprintf(stderr,"focus to %s\n",widget->label());fflush(stderr);
    SetFocus(edit_win);
    focus_to_native_widget = widget;
  } else if (event == FL_UNFOCUS) {
    //fprintf(stderr,"unfocus to %s\n",widget->label());fflush(stderr);
    Fl_Widget *f = Fl::focus();
    if (f && f != focus_to_native_widget) {
      SetFocus(fl_win32_xid(f->top_window()));
      focus_to_native_widget = 0;
    }
  }
  return 1;
}


void Fl_WinAPI_Native_Input_Driver::select_all() {
  if (!widget->selectable()) fl_beep();
  else SendMessageW(edit_win, EM_SETSEL, (WPARAM)0, (LPARAM)100000000);
}


int Fl_WinAPI_Native_Input_Driver::copy() {
  SendMessageW(edit_win, WM_COPY, (WPARAM)0, (LPARAM)0);
  return 1;
}


void Fl_WinAPI_Native_Input_Driver::paste() {
    if (widget->readonly()) fl_beep();
    else SendMessageW(edit_win, WM_PASTE, (WPARAM)0, (LPARAM)0);
}


void Fl_WinAPI_Native_Input_Driver::right_to_left() {
  if (edit_win) {
    char *content = strdup(value());
    DestroyWindow(edit_win);
    edit_win = 0;
    show_widget();
    value(content, -1);
    free(content);
  }
}


void Fl_WinAPI_Native_Input_Driver::draw() {
  if (Fl_Surface_Device::surface() != Fl_Display_Device::display_device()) {
    Fl_Surface_Device::push_current(Fl_Display_Device::display_device());
    Fl_WinAPI_Screen_Driver *dr = (Fl_WinAPI_Screen_Driver *)Fl::screen_driver();
    float scaling = dr->scale(widget->top_window()->screen_num());
    RECT r;
    GetWindowRect(edit_win, &r);
    HDC save_gc = (HDC)fl_graphics_driver->gc();
    fl_graphics_driver->gc(GetDC(NULL));
    // capture from screen
    Fl_RGB_Image *top = dr->read_win_rectangle_unscaled(r.left, r.top, r.right-r.left,
                                                        r.bottom-r.top, 0);
//fprintf(stderr,"top=%dx%d\n",top->data_w(),top->data_h()); fflush(stderr);
    ReleaseDC(NULL, (HDC)fl_graphics_driver->gc());
    fl_graphics_driver->gc(save_gc);
    Fl_Surface_Device::pop_current();
    if (top) { // draw captured image
      top->scale(top->data_w() / scaling, top->data_h() / scaling);
      top->draw(widget->x() + Fl::box_dx(widget->box()),
                widget->y() + Fl::box_dy(widget->box()));
      delete top;
    }
  }
}

static Fl_Widget *dnd_save_focus;

int Fl_WinAPI_Native_Input_Driver::handle_dnd(int event) {
  switch (event) {
    case FL_DND_ENTER:
      Fl::belowmouse(widget); // send the leave events first
      if (dnd_save_focus != widget) {
        dnd_save_focus = Fl::focus();
        handle_focus(FL_FOCUS);
      }
      // fall through:
    case FL_DND_DRAG:
      if (Fl::event_inside(widget)) {
        // move insertion point to Fl::event_x(), Fl::event_y()
        Fl_WinAPI_Screen_Driver *dr = (Fl_WinAPI_Screen_Driver *)Fl::screen_driver();
        float scaling = dr->scale(widget->top_window()->screen_num());
        DWORD where;
        int offset = 0;
        if (kind == MULTIPLE_LINES && widget->right_to_left()) {
          float f = dr->dpi[widget->top_window()->screen_num()][0] / 96.;
          offset = 15 * f;
        }
        int X = Fl::event_x() - (widget->x() + Fl::box_dx(widget->box()));
        int Y = Fl::event_y() - (widget->y() + Fl::box_dy(widget->box()));
        where = MAKELPARAM(DWORD(X * scaling - offset), DWORD(Y*scaling));
        where = SendMessageW(edit_win, EM_CHARFROMPOS, (WPARAM)0, (LPARAM)where);
        where = LOWORD(where);
        SendMessageW(edit_win, EM_SETSEL, (WPARAM)where, (LPARAM)where);
        widget->redraw();
      }
      return 1;

    case FL_DND_LEAVE:
      if (dnd_save_focus && dnd_save_focus != widget) {
        Fl::focus(dnd_save_focus);
      }
      Fl::first_window()->cursor(FL_CURSOR_MOVE);
      dnd_save_focus = NULL;
      return 1;

    case FL_DND_RELEASE:
      if (dnd_save_focus == widget) {
          ;
      } else if (dnd_save_focus) {
        dnd_save_focus->handle(FL_UNFOCUS);
      }
      dnd_save_focus = NULL;
      widget->take_focus();
      widget->window()->cursor(FL_CURSOR_INSERT);
      return 1;

    default:
      return 0;
  }
}


int Fl_WinAPI_Native_Input_Driver::handle_paste() {
  if (widget->readonly()) fl_beep();
  else replace_selection(Fl::event_text(), Fl::event_length());
  return 1;
}


void Fl_WinAPI_Native_Input_Driver::activate() {
  EnableWindow(edit_win, true);
  Fl_Native_Input_Driver::activate();
}


void Fl_WinAPI_Native_Input_Driver::deactivate() {
  EnableWindow(edit_win, false);
  Fl_Native_Input_Driver::deactivate();
}
