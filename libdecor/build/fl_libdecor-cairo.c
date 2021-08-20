#include "../src/plugins/cairo/libdecor-cairo.c"

unsigned char *fl_libdecor_titlebar_buffer(struct libdecor_frame *frame, int *w, int *h, int *stride)
{
  struct libdecor_frame_cairo *lfc = (struct libdecor_frame_cairo *)frame;
  struct border_component *bd = &lfc->title_bar.title;
  struct buffer *buffer = bd->server.buffer;
  *w = buffer->buffer_width;
  *h = buffer->buffer_height;
  *stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, buffer->buffer_width);
  return (unsigned char*)buffer->data;
}
