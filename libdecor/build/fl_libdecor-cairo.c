struct libdecor_frame;
extern void fl_libdecor_frame_clamp_min_content_size(struct libdecor_frame *frame,
                                                   int content_width, int content_height);
#define libdecor_frame_set_min_content_size fl_libdecor_frame_clamp_min_content_size
#include "../src/plugins/cairo/libdecor-cairo.c"
#undef libdecor_frame_clamp_min_content_size

/*
 FLTK-added utility function to give access to the pixel array representing
 the titlebar of a window decorated by the cairo plugin of libdecor.
   frame: a libdecor-defined pointer given by fl_xid(win)->frame (with Fl_Window *win);
   *width, *height: returned assigned to the width and height in pixels of the titlebar;
   *stride: returned assigned to the number of bytes per line of the pixel array;
   return value: start of the pixel array, which is in BGRA order.
 */
unsigned char *fl_libdecor_cairo_titlebar_buffer(struct libdecor_frame *frame,
                                                 int *width, int *height, int *stride)
{
  struct libdecor_frame_cairo *lfc = (struct libdecor_frame_cairo *)frame;
  struct border_component *bc = &lfc->title_bar.title;
  struct buffer *buffer = bc->server.buffer;
  *width = buffer->buffer_width;
  *height = buffer->buffer_height;
  *stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, buffer->buffer_width);
  return (unsigned char*)buffer->data;
}
