#include "PDFio_Graphics_Driver.H"
#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Paged_Device.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/filename.H>
#include <FL/fl_string_functions.h>
#include "pdfio.h"
#include "pdfio-content.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

static char *run_and_close_native_file_chooser(Fl_Native_File_Chooser *chooser);


PDFio_File_Surface::PDFio_File_Surface()
{
  driver(new PDFio_Graphics_Driver);
  aux_fname = NULL;
  doc_fname = NULL;
  aux_pdf = NULL;
  aux_FILE = NULL;
  first_page = true;
}


PDFio_File_Surface::~PDFio_File_Surface()
{
  delete driver();
}


int PDFio_File_Surface::begin_job(const char *defaultname, enum Fl_Paged_Device::Page_Format format,
				  enum Fl_Paged_Device::Page_Layout layout)
{
  Fl_Native_File_Chooser *chooser = new Fl_Native_File_Chooser();
  chooser->type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
  chooser->title("Set PDF filename");       
  chooser->filter("PDF Files\t*.pdf");
  int l = defaultname ? strlen(defaultname) : 0;
  char *preset = new char[l + 5];
  strcpy(preset, defaultname ? defaultname : "" );
  char *p = strchr(preset, '.');
  if (p) *p = 0;
  strcat(preset, ".pdf");
  chooser->preset_file(preset);
  delete[] preset;
  chooser->options(Fl_Native_File_Chooser::SAVEAS_CONFIRM | chooser->options());
  char *plotfilename = run_and_close_native_file_chooser(chooser);
  if(plotfilename == NULL) return 1;
  return begin_document(plotfilename, format, layout);
}
  

int PDFio_File_Surface::begin_document(const char* fixedfilename, enum Fl_Paged_Device::Page_Format format,
				       enum Fl_Paged_Device::Page_Layout layout)
{
  if (layout == LANDSCAPE) {
    return begin_custom(fixedfilename, page_formats[format].height, page_formats[format].width);
    }
  return begin_custom(fixedfilename, page_formats[format].width, page_formats[format].height);
}


bool error_cb(pdfio_file_t *pdf, const char *message, void *data) {
  return false;
}

static ssize_t output_cb(FILE *in, const char *buffer, size_t datalen) {
  return fwrite(buffer, 1, datalen, in);
}


int PDFio_File_Surface::begin_custom(const char* plotfilename, int pwidth, int pheight)
  {
  PDFio_Graphics_Driver *d = (PDFio_Graphics_Driver*)driver();
  width = pwidth;
  height = pheight;
  pdfio_rect_t media_box = { 0.0, 0.0, (double)width, (double)height };
  d->pdf = pdfioFileCreate(plotfilename, "2.0", &media_box, &media_box, error_cb, NULL);
  if (d->pdf == NULL) {
    fl_alert("Error opening %s for writing\n", plotfilename);
    return 1;
  }
  doc_fname = fl_strdup(plotfilename);
  d->dict = pdfioDictCreate(d->pdf);
  pdfioFileSetTitle(d->pdf, fl_filename_name(plotfilename));
  pdfioFileSetCreator(d->pdf, "FLTK library");
  pdfioFileSetCreationDate(d->pdf, time(NULL));
  
#ifdef _WIN32 // needs checking
  aux_fname = new char[FL_PATH_MAX];
  errno_t err = tmpnam_s(aux_fname, FL_PATH_MAX);
  if (!err) aux_FILE = fl_fopen(aux_fname, "w+b");
  else { delete[] aux_fname; aux_fname = NULL; }
#else
  aux_FILE = tmpfile();
#endif
  if (!aux_FILE) {
    pdfioFileClose(d->pdf);
    d->pdf = NULL;
    fl_alert("Error creating temporary file\n");
    return 1;
  }
  aux_pdf = pdfioFileCreateOutput((pdfio_output_cb_t)output_cb, aux_FILE, "2.0", &media_box, &media_box, error_cb, NULL);
  pdfio_obj_t *obj = pdfioFileCreateObj(aux_pdf, d->dict);
  d->stream = pdfioObjCreateStream(obj, PDFIO_FILTER_NONE);

  previous_surface = Fl_Surface_Device::surface();
  if (width == page_formats[Fl_Paged_Device::A4].width) {
    left_margin = 18;
    top_margin = 18;
  }
  else {
    left_margin = 12;
    top_margin = 12;
  }
  set_current();
  return 0;
}


int PDFio_File_Surface::begin_page()
{
  PDFio_Graphics_Driver *d = (PDFio_Graphics_Driver*)driver();
  pdfioContentMatrixScale(d->stream, 1, -1);
  pdfioContentMatrixTranslate(d->stream, left_margin, - height + top_margin);
  pdfioContentSetLineCap(d->stream, PDFIO_LINECAP_SQUARE);
  pdfioContentSetLineWidth(d->stream, 1);
  pdfioContentSave(d->stream);
  x_offset = y_offset = 0;
  angle = 0;
  scale_x = scale_y = 1;
  return 0;
}


int PDFio_File_Surface::printable_rect(int *w, int *h)
{
  if(w) *w = (int)((width - 2 * left_margin) / scale_x + .5);
  if(h) *h = (int)((height - 2 * top_margin) / scale_y + .5);
  return 0;
}


void PDFio_File_Surface::margins(int *left, int *top, int *right, int *bottom)
{
  *left = left_margin;
  *top = top_margin;
  *right = left_margin;
  *bottom = top_margin;
}


void PDFio_File_Surface::origin(int x, int y)
{
  PDFio_Graphics_Driver *d = (PDFio_Graphics_Driver*)driver();
  pdfioContentRestore(d->stream);
  pdfioContentSave(d->stream);
  x_offset = x;
  y_offset = y;
  pdfioContentMatrixScale(d->stream, scale_x, scale_y);
  pdfioContentMatrixTranslate(d->stream, x, y);
  pdfioContentMatrixRotate(d->stream, angle);
}


void PDFio_File_Surface::origin(int *px, int *py)
{
  Fl_Paged_Device::origin(px, py);
}


void PDFio_File_Surface::translate(int x, int y)
{
  PDFio_Graphics_Driver *d = (PDFio_Graphics_Driver*)driver();
  pdfioContentSave(d->stream);
  pdfioContentMatrixTranslate(d->stream, x, y);
}


void PDFio_File_Surface::untranslate()
{
  PDFio_Graphics_Driver *d = (PDFio_Graphics_Driver*)driver();
  pdfioContentRestore(d->stream);
}

extern "C" {
  void _pdfioFileFlush(pdfio_file_t *);
}

int PDFio_File_Surface::end_page()
{
  PDFio_Graphics_Driver *d = (PDFio_Graphics_Driver*)driver();
  pdfioContentRestore(d->stream);
  char line[9000];
  // alternative method without private function _pdfioFileFlush() :
  //memset(line, ' ', sizeof(line));
  // able to get all output written to file
  //pdfioStreamWrite(d->stream, line, sizeof(line));
  _pdfioFileFlush(aux_pdf);
  fflush(aux_FILE);
  long eof = ftell(aux_FILE);
  pdfio_stream_t *page_stream = pdfioFileCreatePage(d->pdf, d->dict);
  rewind(aux_FILE);
  if (first_page) {
    // necessary for acrobat reader: at first page, skip until past line "stream\n"
    first_page = false;
    char *p;
    do p = fgets(line, sizeof(line), aux_FILE);
    while (p && strcmp(line, "stream\n"));
  }
  while (true) {
    size_t r = sizeof(line);
    if (r > eof) r = eof;
    r = fread(line, 1, r, aux_FILE);
    if (r <= 0) break;
    pdfioStreamWrite(page_stream, line, r);
    eof -= r;
  }
  rewind(aux_FILE);
  pdfioStreamClose(page_stream);
  return 0;
}


void PDFio_File_Surface::end_job()
{
  PDFio_Graphics_Driver *d = (PDFio_Graphics_Driver*)driver();
  bool b = pdfioStreamClose(d->stream);
  if (aux_pdf) {
    b = pdfioFileClose(aux_pdf);
    aux_pdf = NULL;
    fclose(aux_FILE);
  }
  if (aux_fname) {
    fl_unlink(aux_fname);
    delete[] aux_fname;
    aux_fname = NULL;
  }
  if (d->pdf) pdfioFileClose(d->pdf);
  if (doc_fname) free(doc_fname);
  doc_fname = NULL;
  previous_surface->set_current();
}


void PDFio_File_Surface::scale(float s_x, float s_y)
{
  if (s_y == 0.) s_y = s_x;
  scale_x = s_x;
  scale_y = s_y;
  PDFio_Graphics_Driver *d = (PDFio_Graphics_Driver*)driver();
  pdfioContentRestore(d->stream);
  pdfioContentSave(d->stream);
  pdfioContentMatrixScale(d->stream, scale_x, scale_y);
  pdfioContentMatrixRotate(d->stream, angle);
  x_offset = y_offset = 0;
}


void PDFio_File_Surface::rotate(float degrees)
{
  angle = degrees < 0 ? degrees + 360. : degrees;
  if (angle == 0 || angle == 360.) return;
  // Necessary. Suggests use of landscape is better.
  if (angle == 90. || angle == 180. || angle == 270.) angle += 0.05;
  PDFio_Graphics_Driver *d = (PDFio_Graphics_Driver*)driver();
  pdfioContentRestore(d->stream);
  pdfioContentSave(d->stream);
  pdfioContentMatrixScale(d->stream, scale_x, scale_y);
  pdfioContentMatrixTranslate(d->stream, x_offset, y_offset);
  pdfioContentMatrixRotate(d->stream, angle);
}


void *PDFio_File_Surface::pdf()
{
  return ((PDFio_Graphics_Driver*)driver())->pdf;
}


// ==================================================

const char *PDFio_Graphics_Driver::system_font_name[] = {
  "Helvetica",
  "Helvetica-Bold",
  "Helvetica-Oblique",
  "Helvetica-BoldOblique",
  "Courier",
  "Courier-Bold",
  "Courier-Oblique",
  "Courier-BoldOblique",
  "Times-Roman",
  "Times-Bold",
  "Times-Italic",
  "Times-BoldItalic",
};
const int PDFio_Graphics_Driver::system_font_count = sizeof(system_font_name) / sizeof(char*);

PDFio_Graphics_Driver::PDFio_Graphics_Driver()
{
  pdf = NULL;
  stream = NULL;
  dict = NULL;
  system_font_object = new pdfio_obj_t*[system_font_count];
  for (Fl_Font f = 0; f < system_font_count; f++) system_font_object[f] = NULL;
}

PDFio_Graphics_Driver::~PDFio_Graphics_Driver()
{
  delete[] system_font_object;
}

void PDFio_Graphics_Driver::line(int x1, int y1, int x2, int y2)
{
  pdfioContentPathMoveTo(stream, x1, y1);
  pdfioContentPathLineTo(stream, x2, y2);
  pdfioContentStroke(stream);
}


void PDFio_Graphics_Driver::xyline(int x, int y, int x1)
{
  line(x, y, x1, y);
}


void PDFio_Graphics_Driver::xyline(int x, int y, int x1, int y2)
{
  pdfioContentPathMoveTo(stream, x, y);
  pdfioContentPathLineTo(stream, x1, y);
  pdfioContentPathLineTo(stream, x1, y2);
  pdfioContentStroke(stream);
}


void PDFio_Graphics_Driver::xyline(int x, int y, int x1, int y2, int x3) {
  pdfioContentPathMoveTo(stream, x, y);
  pdfioContentPathLineTo(stream, x1, y);
  pdfioContentPathLineTo(stream, x1, y2);
  pdfioContentPathLineTo(stream, x3, y2);
  pdfioContentStroke(stream);
}


void PDFio_Graphics_Driver::yxline(int x, int y, int y1)
{
  line(x, y, x, y1);
}


void PDFio_Graphics_Driver::yxline(int x, int y, int y1, int x2)
{
  pdfioContentPathMoveTo(stream, x, y);
  pdfioContentPathLineTo(stream, x, y1);
  pdfioContentPathLineTo(stream, x2, y1);
  pdfioContentStroke(stream);
}


void PDFio_Graphics_Driver::yxline(int x, int y, int y1, int x2, int y3){
  pdfioContentPathMoveTo(stream, x, y);
  pdfioContentPathLineTo(stream, x, y1);
  pdfioContentPathLineTo(stream, x2, y1);
  pdfioContentPathLineTo(stream, x2, y3);
  pdfioContentStroke(stream);
}


void PDFio_Graphics_Driver::rect(int x, int y, int w, int h)
{
  pdfioContentPathRect(stream,  x,  y,  w,  h);
  pdfioContentStroke(stream);
}


void PDFio_Graphics_Driver::rectf(int x, int y, int w, int h)
{
  pdfioContentPathRect(stream,  x,  y,  w,  h);
  pdfioContentFill(stream, false);
}


void PDFio_Graphics_Driver::draw(const char *str, int n, int x, int y)
{
  draw(0, str, n, x, y);
}


void PDFio_Graphics_Driver::draw(int angle, const char* str, int n, int x, int y)
{
  pdfioContentSave(stream);
  pdfioContentMatrixTranslate(stream, x, y);
  pdfioContentMatrixScale(stream, 1,-1);
  if (angle) pdfioContentMatrixRotate(stream, -angle);
  pdfioContentTextBegin(stream);
  pdfioContentTextMoveTo(stream, 0,0);
  char *s = new char[n+1];
  memcpy(s, str, n);
  s[n] = 0;
  pdfioContentTextShow(stream, false, s);
  delete[] s;
  pdfioContentTextEnd(stream);
  pdfioContentRestore(stream);
}


void PDFio_Graphics_Driver::rtl_draw(const char *str, int n, int x, int y) {
  draw(0, str, n, x - width(str, n), y);
}


void PDFio_Graphics_Driver::font(int f, int s)
{
  if (f >= system_font_count) f = 0;
  Fl_Graphics_Driver::font(f, s);
  Fl_Graphics_Driver *dr = Fl_Display_Device::display_device()->driver();
  dr->font(f, s);
  char local_font_name[10];
  snprintf(local_font_name, sizeof(local_font_name), "Font%d", f);
  bool b;
  if (!system_font_object[f]) {
    system_font_object[f] = pdfioFileCreateFontObjFromBase(pdf, system_font_name[f]);
    b = pdfioPageDictAddFont(dict, pdfioStringCreate(pdf, local_font_name), 
                             system_font_object[f]);
  }
  if (stream) pdfioContentSetTextFont(stream, local_font_name, s);
}


static void do_color(pdfio_stream_t *stream, uchar red, uchar green, uchar blue) {
  double r, g, b;
  r = red/255.; g = green/255.; b = blue/255.;
  pdfioContentSetStrokeColorDeviceRGB(stream, r, g, b);
  pdfioContentSetFillColorDeviceRGB(stream, r, g, b);
}


void PDFio_Graphics_Driver::color(Fl_Color c)
{
  Fl_Graphics_Driver::color(c);
  uchar red, green, blue;
  Fl::get_color(c, red, green, blue);
  do_color(stream, red, green, blue);
}


void PDFio_Graphics_Driver::color(uchar red, uchar green, uchar blue)
{
  Fl_Graphics_Driver::color( fl_rgb_color(red, green, blue) );
  do_color(stream, red, green, blue);
}


double PDFio_Graphics_Driver::width(const char* str, int l)
{
  Fl_Graphics_Driver *dr = Fl_Display_Device::display_device()->driver();
  return dr->width(str, l);
}


int PDFio_Graphics_Driver::height() {
  return size();
}


int PDFio_Graphics_Driver::descent() {
  return Fl_Display_Device::display_device()->driver()->descent();
}


void PDFio_Graphics_Driver::draw(const char* str, int n, float fx, float fy) {
  draw(str, n, (int)fx, (int)fy);
}


void PDFio_Graphics_Driver::push_clip(int x, int y, int w, int h)
{
  pdfioContentSave(stream);
  pdfioContentPathRect(stream, x, y, w, h);
  pdfioContentClip(stream, false);
  pdfioContentPathEnd(stream);
}


void PDFio_Graphics_Driver::pop_clip()
{
  pdfioContentRestore(stream);
}


void PDFio_Graphics_Driver::line_style(int style, int width, char *dashes) {
  if (width == 0) width = 1;
  pdfioContentSetLineWidth(stream, width);
  
  int s_style = (style & 0xff);
  if (s_style == FL_DASH || s_style == FL_DASHDOT) 
    pdfioContentSetDashPattern(stream, 0, 2 * width, 2 * width);
  else if (s_style == FL_DOT || s_style == FL_DASHDOTDOT) {
    if (!(style & 0x200)) pdfioContentSetDashPattern(stream, 0,  width,  width);
    else pdfioContentSetDashPattern(stream, 0, 0.01 * width, 1.99 * width);
  } else pdfioContentSetDashPattern(stream, 0, 0, 0);
  
// pdfioStreamPrintf(stream, "[%g %g %g %g] 0 d\n", 3 * width, width, width, width);
  
  int cap = (style &0xf00) >> 8;
  if (cap) cap--;
  pdfioContentSetLineCap(stream, (pdfio_linecap_t)cap);

  int join = (style & 0xf000) >> 12;
  if (join) join--;
  pdfioContentSetLineJoin(stream, (pdfio_linejoin_t)join);
}


void PDFio_Graphics_Driver::line(int x, int y, int x1, int y1, int x2, int y2) {
  pdfioContentPathMoveTo(stream, x, y);
  pdfioContentPathLineTo(stream, x1, y1);
  pdfioContentPathLineTo(stream, x2, y2);
  pdfioContentStroke(stream);
}


void PDFio_Graphics_Driver::loop(int x0, int y0, int x1, int y1, int x2, int y2) {
  pdfioContentPathMoveTo(stream, x0, y0);
  pdfioContentPathLineTo(stream, x1, y1);
  pdfioContentPathLineTo(stream, x2, y2);
  pdfioContentPathClose(stream);
  pdfioContentStroke(stream);
}


void PDFio_Graphics_Driver::loop(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
  pdfioContentPathMoveTo(stream, x0, y0);
  pdfioContentPathLineTo(stream, x1, y1);
  pdfioContentPathLineTo(stream, x2, y2);
  pdfioContentPathLineTo(stream, x3, y3);
  pdfioContentPathClose(stream);
  pdfioContentStroke(stream);
}


void PDFio_Graphics_Driver::polygon(int x0, int y0, int x1, int y1, int x2, int y2) {
  pdfioContentPathMoveTo(stream, x0, y0);
  pdfioContentPathLineTo(stream, x1, y1);
  pdfioContentPathLineTo(stream, x2, y2);
  pdfioContentPathClose(stream);
  pdfioContentFill(stream, false);
}


void PDFio_Graphics_Driver::polygon(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
  pdfioContentPathMoveTo(stream, x0, y0);
  pdfioContentPathLineTo(stream, x1, y1);
  pdfioContentPathLineTo(stream, x2, y2);
  pdfioContentPathLineTo(stream, x3, y3);
  pdfioContentPathClose(stream);
  pdfioContentFill(stream, false);
}


void PDFio_Graphics_Driver::arc(double x, double y, double r, double start, double a) {
  Fl_Graphics_Driver::arc(x, y, r, start, a);
}


void PDFio_Graphics_Driver::circle(double x, double y, double r) {
  arc(x, y, r, 0, 360);
}


void PDFio_Graphics_Driver::arc(int x, int y, int w, int h, double a1, double a2) {
  if (w <= 1 || h <= 1) return;
  float cx = x + 0.5f*w - 0.5f, cy = y + 0.5f*h - 0.5f;
  if (w != h) {
    fl_push_matrix();
    fl_translate(cx, cy);
    fl_scale(w-1, h-1);
    begin_line();
    Fl_Graphics_Driver::arc(0,0,0.5,a1,a2);
    end_line();
    fl_pop_matrix();
  } else {
    float r = (w+h)*0.25f-0.5f;
    begin_line();
    Fl_Graphics_Driver::arc(cx, cy, r, a1, a2);
    end_line();
  }
}


void PDFio_Graphics_Driver::pie(int x, int y, int w, int h, double a1, double a2) {
  if (w <= 1 || h <= 1) return;
  float cx = x + 0.5f*w - 0.5f, cy = y + 0.5f*h - 0.5f;
  if (w != h) {
    fl_push_matrix();
    fl_translate(cx, cy);
    fl_scale(w-1, h-1);
    begin_complex_polygon();
    vertex(0, 0);
    Fl_Graphics_Driver::arc(0,0,0.5,a1,a2);
    end_complex_polygon();
    fl_pop_matrix();
  } else {
    float r = (w + h) * 0.25f - 0.5f;
    begin_complex_polygon();
    vertex(cx, cy);
    Fl_Graphics_Driver::arc(cx, cy, r, a1, a2);
    end_complex_polygon();
  }
}


void PDFio_Graphics_Driver::end_points() {
  for (int i = 0; i < n; i++) {
    point(xpoint[i].x, xpoint[i].y);
  }
}


void PDFio_Graphics_Driver::end_line() {
  if (n < 2) {
    end_points();
    return;
  }
  if (n<=1) return;
  pdfioContentPathMoveTo(stream, xpoint[0].x, xpoint[0].y);
  for (int i=1; i<n; i++)
    pdfioContentPathLineTo(stream, xpoint[i].x, xpoint[i].y);
  pdfioContentStroke(stream);
}


void PDFio_Graphics_Driver::end_polygon() {
  fixloop();
  if (n < 3) {
    end_line();
    return;
  }
  if (n<=1) return;
  pdfioContentPathMoveTo(stream, xpoint[0].x, xpoint[0].y);
  for (int i=1; i<n; i++)
    pdfioContentPathLineTo(stream, xpoint[i].x, xpoint[i].y);
  pdfioContentPathClose(stream);
  pdfioContentFill(stream, false);
}


void PDFio_Graphics_Driver::end_complex_polygon() {
  gap();
  if (n < 3) {
    end_line();
    return;
  }
  if (n<=1) return;
  pdfioContentPathMoveTo(stream, xpoint[0].x, xpoint[0].y);
  for (int i=1; i<n; i++)
    pdfioContentPathLineTo(stream, xpoint[i].x, xpoint[i].y);
  pdfioContentPathClose(stream);
  pdfioContentFill(stream, false);
  pdfioContentPathEnd(stream);
}


static void draw_known_image(PDFio_Graphics_Driver *dr, Fl_Image *img, const char *name, int XP, int YP, int WP, int HP, int cx, int cy) {
  dr->push_clip(XP ,YP ,WP ,HP);
  XP -= cx; YP -= cy;
  pdfioContentMatrixTranslate(dr->stream, XP, YP + img->h()/2.);
  pdfioContentMatrixScale(dr->stream, 1, -1);
  pdfioContentDrawImage(dr->stream, name, 0, -img->h()/2., img->w(), img->h());
  dr->pop_clip();
}


static void prepare_new_rgb(PDFio_Graphics_Driver *dr, Fl_RGB_Image *rgb, const char *name) {
  int depth = (rgb->d() >= 3 ? 3 : 1);
  bool has_alpha = rgb->d() % 2 == 0;
  pdfio_obj_t *pdf_image = pdfioFileCreateImageObjFromData(dr->pdf, rgb->array,
                             rgb->data_w(), rgb->data_h(), depth, NULL, has_alpha, false);
  char *persistent = pdfioStringCreate(dr->pdf, name);
  pdfioPageDictAddImage(dr->dict, persistent, pdf_image);
  pdfioDictSetBoolean(dr->dict, persistent, true);
}


void PDFio_Graphics_Driver::draw_rgb(Fl_RGB_Image *rgb, int XP, int YP, int WP, int HP, int cx, int cy) {
  char name[100];
  snprintf(name, sizeof(name), "RGB%p_%p_%d_%d", rgb, rgb->array, rgb->data_w(), rgb->data_h());
  if (pdfioDictGetType(dict, name) == PDFIO_VALTYPE_NONE) {
    prepare_new_rgb(this, rgb, name);
  }
  draw_known_image(this, rgb, name, XP, YP, WP, HP, cx, cy);
}


void PDFio_Graphics_Driver::draw_pixmap(Fl_Pixmap *pxm, int XP, int YP, int WP, int HP, int cx, int cy) {
  char name[100];
  snprintf(name, sizeof(name), "PXM%p_%p_%d_%d", pxm, pxm->data(), pxm->data_w(), pxm->data_h());
  if (pdfioDictGetType(dict, name) == PDFIO_VALTYPE_NONE) {
    Fl_RGB_Image rgb(pxm);
    prepare_new_rgb(this, &rgb, name);
  }
  draw_known_image(this, pxm, name, XP, YP, WP, HP, cx, cy);
}


void PDFio_Graphics_Driver::draw_bitmap(Fl_Bitmap *bm, int XP, int YP, int WP, int HP, int cx, int cy) {
  char name[100];
  snprintf(name, sizeof(name), "BMP%p_%p_%d_%d", bm, bm->data(), bm->data_w(), bm->data_h());
  if (pdfioDictGetType(dict, name) == PDFIO_VALTYPE_NONE) {
    Fl_Image_Surface surf(bm->data_w(), bm->data_h());
    Fl_Surface_Device::push_current(&surf);
    fl_color(FL_WHITE);
    fl_rectf(0, 0, bm->data_w(), bm->data_h());
    fl_color(FL_BLACK);
    bm->draw(0, 0);
    Fl_Surface_Device::pop_current();
    Fl_RGB_Image *rgb3 = surf.image();
    uchar *data = new uchar[bm->data_w() * bm->data_h() * 4];
    memset(data, 0, bm->data_w() * bm->data_h() * 4);
    Fl_RGB_Image *rgb4 = new Fl_RGB_Image(data, bm->data_w(), bm->data_h(), 4);
    rgb4->alloc_array = 1;
    uchar red, green, blue;
    Fl_Color c = Fl_Graphics_Driver::color();
    Fl::get_color(c, red, green, blue);
    const uchar *p = rgb3->array;
    uchar *q = data;
    for (int r = 0; r < bm->data_h(); r++) {
      for (int c = 0; c < bm->data_w(); c++) {
        if (*p == 0 && *(p+1) == 0 && *(p+2) == 0) {
          *q = red; *(q+1) = green; *(q+2) = blue; *(q+3) = 0xff;
        }
        p += 3; q += 4;
      }
    }
    delete rgb3;
    rgb4->scale(bm->w(), bm->h(), 0, 1);
    prepare_new_rgb(this, rgb4, name);
    delete rgb4;
  }
  draw_known_image(this, bm, name, XP, YP, WP, HP, cx, cy);
}


void PDFio_Graphics_Driver::draw_image(const uchar *buf, int x, int y, int w, int h, int d, int l) {
  if (d < 0) {
    pdfioContentSave(stream);
    pdfioContentMatrixTranslate(stream, x, y);
    pdfioContentMatrixScale(stream, -1, 1);
    x = -w; y = 0; buf -= (w-1)*abs(d);
  }
  if (l < 0) {
    pdfioContentSave(stream);
    pdfioContentMatrixTranslate(stream, x, y);
    pdfioContentMatrixScale(stream, 1, -1);
    x = 0; y = -h; buf -= (h-1)*abs(l);
  }
  Fl_RGB_Image *rgb = new Fl_RGB_Image(buf, w, h, abs(d), abs(l));
  static int count = 0;
  char name[20];
  snprintf(name, sizeof(name), "RTRGB%d", ++count);
  prepare_new_rgb(this, rgb, name);
  draw_known_image(this, rgb, name, x, y, w, h, 0, 0);
  delete rgb;
  if (d < 0) pdfioContentRestore(stream);
  if (l < 0) pdfioContentRestore(stream);
}


void PDFio_Graphics_Driver::draw_image(void (*cb)(void*, int, int, int, uchar*), void *data,
                                       int x, int y, int w, int h, int d) {
  uchar *buf = new uchar[w*h*d];
  for (int j = 0; j < h; j++) {
    cb(data, 0, j, w, buf + j*w*d);
  }
  draw_image(buf, x, y, w, h, d, 0);
  delete [] buf;
}


struct mono_image_data {
  const uchar *buf;
  int d;
  int l;
};

static void mono_image_cb(mono_image_data* data, int x, int y, int w, uchar* buf) {
  for (int i = 0; i < w; i++)
    *buf++ = *(data->buf + y*data->l + (x++)*data->d);
}


void PDFio_Graphics_Driver::draw_image_mono(const uchar *buf, int x, int y, int w, int h, 
                                            int d, int l) {
  mono_image_data data;
  data.buf = buf; data.d = d; data.l = (l?l:w*d);
  draw_image((Fl_Draw_Image_Cb)mono_image_cb, (void*)&data, x, y, w, h, 1);
}


void PDFio_Graphics_Driver::draw_image_mono(void (*cb)(void*, int, int, int, uchar*), 
                                            void *data, int x, int y, int w, int h, int d) {
  uchar *buf = new uchar[w*h*d];
  for (int j = 0; j < h; j++) {
    cb(data, 0, j, w, buf + j*w*d);
  }
  draw_image_mono(buf, x, y, w, h, d, 0);
  delete[] buf;
}


// ===========================================

static char *run_and_close_native_file_chooser(Fl_Native_File_Chooser *chooser)
//returns chosen file in static memory or NULL if user cancelled
{
  static char filename[FL_PATH_MAX];
  char *retval = NULL;
  static char last_visited_directory[FL_PATH_MAX] = "";
  char *p;
  if ( chooser->directory() == NULL &&
    (chooser->preset_file() == NULL || *chooser->preset_file() != '/') ) {
    if( *last_visited_directory )fl_chdir(last_visited_directory);
    chooser->directory(last_visited_directory);
    p = (char *)chooser->preset_file();
    if(p != NULL && *p != 0) {
      p = fl_strdup(fl_filename_name(p));
      if(p != NULL) {
        chooser->preset_file(p);
        free(p);
        }
      }
  }
  if ( chooser->show() == 0 ) {
    strcpy(filename, chooser->filename());
    strcpy(last_visited_directory, filename);
    p = strrchr(last_visited_directory, '/');
    if(p != NULL) *p = 0;
    retval = filename;
    }
  delete chooser;
  return retval;
}
