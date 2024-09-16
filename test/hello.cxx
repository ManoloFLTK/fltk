#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Native_Text_Widget.H>

void delete_win(Fl_Widget *w) {
  delete w;
}

int main(int argc, char **argv) {
  Fl_Window *window = new Fl_Window(490, 280);
  bool readonly = false;
  bool selectable = true;
  const char *label;
  if (readonly && !selectable) label = "Read only text boxes";
  else if (readonly && selectable) label = "Selectable text boxes";
  else label = "Editable text boxes";
  Fl_Native_Text_Widget *box = new Fl_Native_Text_Widget(20, 40, 450, 100, label);
  box->align(FL_ALIGN_TOP);
  box->textfont(FL_COURIER_BOLD);
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
  Fl_Native_Text_Widget *box2 = new Fl_Native_Text_Widget(20, box->y()+box->h()+10, 450, 50, label);
  box2->kind(Fl_Native_Text_Widget::SINGLE_LINE);
  box2->textfont(box->textfont());
  box2->textsize(box->textsize());
  box2->textcolor(box->textcolor());
  box2->cursor_color(FL_RED);
  box2->color(box->color());
  box2->box(box->box());
  box2->right_to_left(box->right_to_left());
  box2->value("كان لإسهاماته تأثير كبير في اللغة.");
  //box2->value(box->value());
  window->end();
  window->resizable(box);
  box->readonly(readonly);
  box->selectable(selectable);
  window->callback(delete_win);
  window->show();
  return Fl::run();
}
