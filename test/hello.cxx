#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Native_Text_Widget.H>
#include <FL/Fl_Input.H>

void delete_win(Fl_Widget *w) {
  delete w;
}

void cb(Fl_Widget *, void *d) {
  printf("%s runs callback\n",(char*)d);
}

int main(int argc, char **argv) {
  Fl_Window *window = new Fl_Window(490, 280, "top");
  bool readonly = false;
  bool selectable = true;
  const char *label;
  if (readonly && !selectable) label = "Read only native text boxes";
  else if (readonly && selectable) label = "Selectable native text boxes";
  else label = "Editable native text boxes";
  Fl_Native_Multiline_Text_Widget *box = new Fl_Native_Multiline_Text_Widget(20, 40, 450, 100, label);
  box->align(FL_ALIGN_TOP);
  box->textfont(FL_COURIER);
  box->textsize(20);
  box->textcolor(FL_DARK3);
  box->cursor_color(FL_RED);
  box->color(FL_LIGHT2);
  box->box(FL_DOWN_FRAME);
  box->right_to_left(true);
  box->value(
    "أنو عَبد الله مُحَمَّد بن مُوسَى الخَوارِزمي عالم رياضيات وفلك وجغرافيا مسلم. يكنى بأبي جعفر. قيل أنه ولد حوالي 164هـ 781م وقيل أنه توفيَ بعد 232 هـ أي (بعد 847م)."
    //"Request a notification when it is a good time to start drawing a new frame, by creating a frame callback."
            );
#define SUBWIN 1
#if SUBWIN
  Fl_Window *subwin = new Fl_Window(20, box->y()+box->h()+10, 450, 50+40,"subwin");
  subwin->color(FL_YELLOW);
#endif
  Fl_Native_Text_Widget *box2 = new Fl_Native_Text_Widget(
#if SUBWIN
      0, 0,
#else
      20, box->y()+box->h()+10,
#endif
      450, 50, label);
  box2->textfont(box->textfont());
  box2->textsize(box->textsize());
  box2->textcolor(box->textcolor());
  box2->cursor_color(FL_RED);
  box2->color(box->color());
  box2->box(box->box());
  box2->right_to_left(box->right_to_left());
  box2->value("كان لإسهاماته تأثير كبير في اللغة.");
  box2->callback(cb, (void*)"Fl_Native_Text_Widget");
  //box2->value(box->value());
#if SUBWIN
  subwin->resizable(subwin);
  //Fl_Input *input =new Fl_Input(subwin->x(),subwin->y()+subwin->h()+10, subwin->w(),30, NULL);
  Fl_Input *input = new Fl_Input(box2->x(),box2->y()+box2->h()+10, subwin->w(),30, NULL);
  subwin->end();
#else
  Fl_Input *input =new Fl_Input(box2->x(),box2->y()+box2->h()+10, box2->w(),30, NULL);
#endif
  input->value("Fl_Input");
  input->callback(cb, (void*)"Fl_Input");
  input->textfont(box->textfont());
  input->textsize(box->textsize());
  window->end();
  window->resizable(box);
  box->readonly(readonly);
  box->selectable(selectable);
  window->callback(delete_win);
  window->show();
//printf("Native-single:%p  Native-multiple:%p\n", box2, box);
  return Fl::run();
}
