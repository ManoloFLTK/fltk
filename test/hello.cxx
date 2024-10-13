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
  Fl_Native_Multiline_Text_Widget *box = new Fl_Native_Multiline_Text_Widget(20, 30, 450, 100, "Fl_Native_Multiline_Text_Widget");
  box->align(FL_ALIGN_TOP);
  box->textfont(FL_HELVETICA);
  box->textsize(25);
  box->textcolor(FL_DARK3);
  box->cursor_color(FL_RED);
  box->color(FL_LIGHT2);
  box->box(FL_DOWN_FRAME);
  box->right_to_left(true);
  box->value(
    "أنو عَبد الله مُحَمَّد بن مُوسَى الخَوارِزمي عالم رياضيات وفلك وجغرافيا مسلم. يكنى بأبي جعفر. قيل أنه ولد حوالي 164هـ 781م وقيل أنه توفيَ بعد 232 هـ أي (بعد 847م)."
 //"Le comité Nobel de physique a surpris son monde. En célébrant, mardi 8 octobre, deux pionniers des « réseaux de neurones artificiels », l’Américain John Hopfield (91 ans) et le Britannique Geoffrey Hinton (76 ans), il surfe sur la tendance actuelle de l’intelligence artificielle, qu’on associerait plus volontiers à l’informatique."
 //" C’est la reconnaissance qu’un courant de la physique, la physique statistique, a fait l’effort d’aller vers d’autres domaines. C’est une bonne nouvelle , constate Rémi Monasson, chercheur du CNRS au Laboratoire de physique de l’Ecole normale supérieure de Paris. Stéphane Mallat, professeur au Collège de France, salue un prix « surprenant » et constate qu’en retour, l’intelligence artificielle aide beaucoup les physiciens de nos jours, pour l’imagerie, la modélisation, les simulations…"
            );
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
      450, 50, "Fl_Native_Text_Widget");
  box2->textfont(box->textfont());
  box2->textsize(box->textsize());
  box2->textcolor(box->textcolor());
  box2->cursor_color(FL_RED);
  box2->color(box->color());
  box2->box(box->box());
  box2->right_to_left(true);
  box2->value("كان لإسهاماته تأثير كبير فيالخَوارِزمي الخَوارِزمياللغة الخَوارِزمي.");
  //box2->value("Fl_Native_Text_Widget");
  box2->callback(cb, (void*)"Fl_Native_Text_Widget");
  //box2->value(box->value());
#if SUBWIN
  subwin->resizable(subwin);
  Fl_Input *input = new Fl_Input(box2->x(),box2->y()+box2->h()+20, subwin->w(),30, NULL);
  subwin->end();
#else
  Fl_Input *input =new Fl_Input(box2->x(),box2->y()+box2->h()+20, box2->w(),30, NULL);
#endif
  input->label("Fl_Input");
  input->align(FL_ALIGN_TOP);
  input->value("Lorem ipsum dolor sit amet.");
  input->callback(cb, (void*)"Fl_Input");
  //input->textfont(box->textfont());
  //input->textsize(box->textsize());
  window->end();
  window->resizable(box);
  box->readonly(readonly);
  box->selectable(selectable);
  window->callback(delete_win);
//box->hide();
//box2->hide();
  window->show();
//printf("Native-single:%p  Native-multiple:%p\n", box2, box);
  return Fl::run();
}
