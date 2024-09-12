#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Native_Text_Widget.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_PDF_File_Surface.H>

#include <stdio.h>

Fl_Native_Multiline_Text_Widget *multiple;
Fl_Native_Text_Widget *single;
Fl_Input *input;

void open_cb(Fl_Widget *, Fl_Native_Multiline_Text_Widget *box) {
  Fl_Native_File_Chooser fnfc;
  fnfc.title("Select text file");
  fnfc.type(Fl_Native_File_Chooser::BROWSE_FILE);
  if ( fnfc.show() ) return;
  FILE *in = fopen(fnfc.filename(), "r");
  fseek(in, 0, SEEK_END);
  long pos = ftell(in);
  rewind(in);
  char *text = new char[pos + 1];
  fread(text, 1, pos, in);
  fclose(in);
  box->value(text, (int)pos);
}


void quit_cb(Fl_Widget *w) {
  delete w->top_window();
}


void select_all_cb(Fl_Widget *) {
  if (Fl::focus() == multiple) multiple->select_all();
  else if (Fl::focus() == single) single->select_all();
}


void copy_cb(Fl_Widget *) {
  if (Fl::focus() == multiple) multiple->copy();
  else if (Fl::focus() == single) single->copy();
  else if (Fl::focus() == input) input->copy(1);
}


void paste_cb(Fl_Widget *) {
  if (Fl::focus() == multiple) multiple->paste();
  else if (Fl::focus() == single) single->paste();
  else if (Fl::focus() == input) Fl::paste(*input, 1);
}


void cut_cb(Fl_Widget *, Fl_Native_Multiline_Text_Widget *box) {
  if (Fl::focus() == multiple) multiple->cut();
  else if (Fl::focus() == single) single->cut();
  else if (Fl::focus() == input) {
    input->copy(1);
    input->cut();
  }
}


void undo_cb(Fl_Widget *) {
  if (Fl::focus() == multiple) multiple->undo();
  else if (Fl::focus() == single) single->undo();
  else if (Fl::focus() == input) input->undo();
}


void redo_cb(Fl_Widget *) {
  if (Fl::focus() == multiple) multiple->redo();
  else if (Fl::focus() == single) single->redo();
  else if (Fl::focus() == input) input->redo();
}


void toggle_rtl_cb(Fl_Widget *) {
  if (Fl::focus() == multiple) {
    multiple->right_to_left(!multiple->right_to_left());
  } else if (Fl::focus() == single) {
    single->right_to_left(!single->right_to_left());
  }
}


void print_cb(Fl_Widget *) {
  Fl_Window *win = Fl::first_window();
  Fl_PDF_File_Surface *p = new Fl_PDF_File_Surface();
  if (!p->begin_job("FLTK.pdf")) {
    win->take_focus();
    p->begin_page();
    p->draw_decorated_window(win);
    p->end_page();
    p->end_job();
  }
  delete p;
}


void font_cb(Fl_Widget *w, fl_intptr_t font) {
  if (multiple->visible()) multiple->textfont((Fl_Font)font);
  if (single->visible()) single->textfont((Fl_Font)font);
}


void fontsize_cb(Fl_Widget *w, fl_intptr_t size) {
  if (multiple->visible()) multiple->textsize((Fl_Fontsize)size);
  if (single->visible()) single->textsize((Fl_Fontsize)size);
}


Fl_Menu_Item items[] = {
  {"File",0,0,0,FL_SUBMENU},
  {"Open", FL_COMMAND+'o', (Fl_Callback*)open_cb, 0, 0},
  {"Print to PDF", FL_COMMAND+'p', (Fl_Callback*)print_cb, 0, 0},
#if !__APPLE__ || FLTK_USE_X11
  {"Quit", FL_COMMAND+'q', (Fl_Callback*)quit_cb, 0, 0},
#endif
  {0},
  {"Edit",0,0,0,FL_SUBMENU},
  {"Cut", FL_COMMAND+'x', (Fl_Callback*)cut_cb, 0, 0},
  {"Copy", FL_COMMAND+'c', (Fl_Callback*)copy_cb, 0, 0},
  {"Paste", FL_COMMAND+'v', (Fl_Callback*)paste_cb, 0, FL_MENU_DIVIDER},
  {"Undo", FL_COMMAND+'z', (Fl_Callback*)undo_cb, 0, 0},
  {"Redo", FL_COMMAND+'Z', (Fl_Callback*)redo_cb, 0, FL_MENU_DIVIDER},
  {"Select All", FL_COMMAND+'a', (Fl_Callback*)select_all_cb, 0, FL_MENU_DIVIDER},
  {"RtoL ⇆ LtoR", FL_COMMAND+'R', (Fl_Callback*)toggle_rtl_cb, 0, 0},
  {0},
  {"Format",0,0,0,FL_SUBMENU},
  {"Courier", 0, (Fl_Callback*)font_cb, (void*)FL_COURIER, FL_MENU_RADIO},
  {"Courier Bold", 0, (Fl_Callback*)font_cb, (void*)FL_COURIER_BOLD, FL_MENU_RADIO},
  {"Times", 0, (Fl_Callback*)font_cb, (void*)FL_TIMES, FL_MENU_RADIO},
  {"Times Bold", 0, (Fl_Callback*)font_cb, (void*)FL_TIMES_BOLD, FL_MENU_RADIO},
  {"Helvetica", 0, (Fl_Callback*)font_cb, (void*)FL_HELVETICA, FL_MENU_RADIO|FL_MENU_VALUE},
  {"Helvetica Bold", 0, (Fl_Callback*)font_cb, (void*)FL_HELVETICA_BOLD,
    FL_MENU_RADIO|FL_MENU_DIVIDER},
  {"14", 0, (Fl_Callback*)fontsize_cb, (void*)14, FL_MENU_RADIO},
  {"18", 0, (Fl_Callback*)fontsize_cb, (void*)18, FL_MENU_RADIO|FL_MENU_VALUE},
  {"22", 0, (Fl_Callback*)fontsize_cb, (void*)22, FL_MENU_RADIO},
  {0},
  {0}
};

void delete_win(Fl_Widget *w) {
  delete w;
}

void cb(Fl_Widget *wid) {
  printf("%s runs callback\n", wid->label());
}

void load_file(Fl_Native_Multiline_Text_Widget *widget, const char *fname)
{
  FILE *in = fopen(fname, "r");
  if (in) {
    fseek(in, 0, SEEK_END);
    long pos = ftell(in);
    rewind(in);
    char *text = new char[pos];
    fread(text, 1, pos, in);
    fclose(in);
    widget->value(text, (int)pos);
    delete[] text;
  }
}

int main(int argc, char **argv) {
  int offset_for_menubar = 0;
#if !__APPLE__ || FLTK_USE_X11
  offset_for_menubar = 30;
#endif
  Fl_Window *window = new Fl_Window(490, 265+offset_for_menubar, "top");
  Fl_Sys_Menu_Bar *menubar = new Fl_Sys_Menu_Bar(0,0,window->w(),30);
  menubar->menu(items);

  bool readonly = false;
  bool selectable = true;
  Fl_Native_Multiline_Text_Widget *box = new Fl_Native_Multiline_Text_Widget(20, 30+offset_for_menubar, 450, 100, "Fl_Native_Multiline_Text_Widget");
  multiple = box;
  Fl_Menu_Item *table = (Fl_Menu_Item*)menubar->menu();
  (table+1)->user_data(box);
  box->align(FL_ALIGN_TOP);
  box->textfont(FL_HELVETICA);
  box->textsize(18);
  box->textcolor(FL_DARK3);
  box->cursor_color(FL_RED);
  box->color(FL_LIGHT2);
  box->box(FL_DOWN_FRAME);
  box->right_to_left(true);
  box->wrap(true);
#define SUBWIN 0//1
#if SUBWIN
  Fl_Window *subwin = new Fl_Window(20, box->y()+box->h()+20, 450, 50+40,"subwin");
  subwin->color(FL_YELLOW);
#endif
  Fl_Native_Text_Widget *box2 = new Fl_Native_Text_Widget(
#if SUBWIN
      0, 0,
#else
      20, box->y()+box->h()+20,
#endif
      450, 35, "Fl_Native_Text_Widget");
  single = box2;
  box2->textfont(box->textfont());
  box2->textsize(box->textsize());
  box2->textcolor(box->textcolor());
  box2->cursor_color(FL_RED);
  box2->color(box->color());
  box2->box(box->box());
  box2->right_to_left(true);
  box2->value(
              "أبو عَبد الله مُحَمَّد بن مُوسَى الخَوارِزمي عالم رياضيات وفلك وجغرافيا مسلم."
              );
  box2->callback(cb);
#if SUBWIN
  subwin->resizable(subwin);
  input = new Fl_Input(box2->x(),box2->y()+box2->h()+20, subwin->w(),30, NULL);
  subwin->end();
#else
  input =new Fl_Input(box2->x(),box2->y()+box2->h()+20, box2->w(),30, NULL);
#endif
  input->label("Fl_Input");
  input->align(FL_ALIGN_TOP);
  input->value("Lorem ipsum dolor sit amet.");
  input->callback(cb);
  window->end();
  window->resizable(box);
  box->readonly(readonly);
  box->selectable(selectable);
  window->callback(delete_win);
//box->hide();
//box2->hide();
  window->show();
  int n = 1;
  while (n < argc) {
    if (!Fl::arg(argc, argv, n) ) {
      load_file(box, argv[n++]);
    }
  }
  return Fl::run();
}
