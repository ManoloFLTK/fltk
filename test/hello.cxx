#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Native_Text_Widget.H>

int main(int argc, char **argv) {
  Fl_Window *window = new Fl_Window(440, 280);
  bool readonly = false;
  bool selectable = true;
  const char *label;
  if (readonly && !selectable) label = "Read only text box";
  else if (readonly && selectable) label = "Selectable text box";
  else label = "Editable text box";
  Fl_Native_Text_Widget *box = new Fl_Native_Text_Widget(20, 40, 400, 200, label);
  box->align(FL_ALIGN_TOP);
  box->textfont(FL_COURIER_BOLD);
  box->textsize(20);
  box->textcolor(FL_DARK3);
  box->cursor_color(FL_RED);
  box->color(FL_LIGHT2);
  box->box(FL_DOWN_FRAME);
  box->right_to_left(true);
  box->value(
    "بداية القرن التاسع عشر أول مشروع لرسم خرائط الموقع، تبعه العديد من مواطنيه لا سيما«أوستن هنري لايارد» في عام 1850 و«هنري روليِنسون»"
             );
  //box->wrap(0);
  window->end();
  window->resizable(box);
  box->readonly(readonly);
  box->selectable(selectable);
  window->show();
  return Fl::run();
}
