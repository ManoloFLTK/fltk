#ifndef PDFIO_GRAPHICS_DRIVER_H
#define PDFIO_GRAPHICS_DRIVER_H

#include <FL/Fl_Paged_Device.H>
#include <FL/Fl_Graphics_Driver.H>
typedef struct _pdfio_file_s pdfio_file_t;
typedef struct _pdfio_dict_s pdfio_dict_t;
typedef struct _pdfio_stream_s pdfio_stream_t;
typedef struct _pdfio_obj_s pdfio_obj_t;


class PDFio_Graphics_Driver : public Fl_Graphics_Driver {
  friend class PDFio_File_Surface;
public:
  pdfio_file_t *pdf;
  pdfio_dict_t *dict;
  pdfio_stream_t *stream;
  static const int system_font_count;
  static const char *system_font_name[];
  pdfio_obj_t **system_font_object;
  PDFio_Graphics_Driver();
  ~PDFio_Graphics_Driver();
  void rect(int x, int y, int w, int h) FL_OVERRIDE;
  void rectf(int x, int y, int w, int h) FL_OVERRIDE;
  void line_style(int style, int width, char *dashes=0) FL_OVERRIDE;
  void line(int x1, int y1, int x2, int y2) FL_OVERRIDE;
  void xyline(int x, int y, int x1) FL_OVERRIDE;
  void xyline(int x, int y, int x1, int y2) FL_OVERRIDE;
  void xyline(int x, int y, int x1, int y2, int x3) FL_OVERRIDE;
  void yxline(int x, int y, int y1) FL_OVERRIDE;
  void yxline(int x, int y, int y1, int x2) FL_OVERRIDE;
  void yxline(int x, int y, int y1, int x2, int y3) FL_OVERRIDE;
  void font(int f, int s) FL_OVERRIDE;
  void draw(const char *str, int n, int x, int y) FL_OVERRIDE;
  void draw(const char*, int, float, float) FL_OVERRIDE;
  void draw(int, const char*, int, int, int) FL_OVERRIDE;
  void rtl_draw(const char*, int, int, int) FL_OVERRIDE;
  void color(uchar r, uchar g, uchar b) FL_OVERRIDE;
  void color(Fl_Color c) FL_OVERRIDE;
  void push_clip(int x, int y, int w, int h) FL_OVERRIDE;
  void pop_clip() FL_OVERRIDE;
  void draw_image(const uchar*, int, int, int, int, int, int) FL_OVERRIDE;
  void draw_image_mono(const uchar*, int, int, int, int, int, int) FL_OVERRIDE;
  void draw_image(void (*)(void*, int, int, int, uchar*), void*, int, int, int, int, int) FL_OVERRIDE;
  void draw_image_mono(void (*)(void*, int, int, int, uchar*), void*, int, int, int, int, int) FL_OVERRIDE;
  double width(const char*, int) FL_OVERRIDE;
  int height() FL_OVERRIDE;
  int descent() FL_OVERRIDE;
  void point(int x, int y) FL_OVERRIDE { rectf(x,y,1,1);}
  void line(int x, int y, int x1, int y1, int x2, int y2) FL_OVERRIDE;
  void loop(int x0, int y0, int x1, int y1, int x2, int y2) FL_OVERRIDE;
  void loop(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) FL_OVERRIDE;
  void polygon(int x0, int y0, int x1, int y1, int x2, int y2) FL_OVERRIDE;
  void polygon(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) FL_OVERRIDE;
  void arc(double x, double y, double r, double start, double a) FL_OVERRIDE;
  void arc(int x, int y, int w, int h, double a1, double a2) FL_OVERRIDE;
  void pie(int x, int y, int w, int h, double a1, double a2) FL_OVERRIDE;
  void circle(double x, double y, double r) FL_OVERRIDE;
  void end_line() FL_OVERRIDE;
  void end_points() FL_OVERRIDE;
  void end_polygon() FL_OVERRIDE;
  void end_complex_polygon() FL_OVERRIDE;
  void draw_rgb(Fl_RGB_Image*, int, int, int, int, int, int) FL_OVERRIDE;
  void draw_pixmap(Fl_Pixmap *pxm, int XP, int YP, int WP, int HP, int cx, int cy) FL_OVERRIDE;
  void draw_bitmap(Fl_Bitmap *pxm, int XP, int YP, int WP, int HP, int cx, int cy) FL_OVERRIDE;

  virtual int clip_box(int x, int y, int w, int h, int &X, int &Y, int &W, int &H) FL_OVERRIDE {
    return 0;
  }
  virtual int not_clipped(int x, int y, int w, int h) FL_OVERRIDE {return 1;}
  virtual void push_no_clip() FL_OVERRIDE {}
};


class PDFio_File_Surface : public Fl_Paged_Device
{
  int width, height;
  Fl_Surface_Device *previous_surface;
  int left_margin, top_margin;
  char *aux_fname;
  char *doc_fname;
  pdfio_file_t *aux_pdf;
  FILE *aux_FILE;
  bool first_page;
  float scale_x, scale_y;
  double angle;
public:
  PDFio_File_Surface();
  int begin_job(const char *defaultname,
                enum Fl_Paged_Device::Page_Format format = Fl_Paged_Device::A4,
    enum Fl_Paged_Device::Page_Layout layout = Fl_Paged_Device::PORTRAIT);
  int begin_job(int, int*, int *, char **) FL_OVERRIDE {return 1;} // don't use
  int begin_document(const char* fixedfilename,
                     enum Fl_Paged_Device::Page_Format format = Fl_Paged_Device::A4,
		     enum Fl_Paged_Device::Page_Layout layout = Fl_Paged_Device::PORTRAIT);
  int begin_custom(const char* fixedfilename, int pwidth, int pheight);
  int printable_rect(int *w, int *h) FL_OVERRIDE;
  void margins(int *left, int *top, int *right, int *bottom) FL_OVERRIDE;
  void origin(int x, int y) FL_OVERRIDE;
  void origin (int *x, int *y) FL_OVERRIDE;
  void translate(int x, int y) FL_OVERRIDE;
  void untranslate() FL_OVERRIDE;
  int begin_page (void) FL_OVERRIDE;
  int end_page(void) FL_OVERRIDE;
  void end_job(void) FL_OVERRIDE;
  void *pdf();
  void scale(float s_x, float s_y) FL_OVERRIDE;
  void rotate(float rot_angle) FL_OVERRIDE;
  const char *outfname() {return doc_fname; }
  ~PDFio_File_Surface();
};


#endif // PDFIO_GRAPHICS_DRIVER_H

