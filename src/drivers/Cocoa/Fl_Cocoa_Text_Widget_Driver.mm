//
//  Fl_Cocoa_Text_Widget_Driver.mm
//

#include "../../Fl_Text_Widget_Driver.H"
#include <FL/platform.H> // for fl_mac_xid
#include <FL/Fl_Native_Text_Widget.H>
#include <FL/Fl_Window.H>
#include "../Quartz/Fl_Quartz_Graphics_Driver.H"
#include "Fl_Cocoa_Window_Driver.H"
#include "../../Fl_Screen_Driver.H"

#import <Cocoa/Cocoa.h>

@class FLNativeTextView;

class Fl_Cocoa_Text_Widget_Driver : public Fl_Text_Widget_Driver {
public:
  FLNativeTextView *text_view;
  NSScrollView *scroll_view;
  NSString *text_before_show;
  Fl_Cocoa_Text_Widget_Driver();
  ~Fl_Cocoa_Text_Widget_Driver();
  void show_widget() FL_OVERRIDE;
  void hide_widget() FL_OVERRIDE;
  void resize(int x, int y, int w, int h) FL_OVERRIDE;
  void textfontandsize() FL_OVERRIDE;
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
  void focus() FL_OVERRIDE;
  void unfocus() FL_OVERRIDE;
  void select_all() FL_OVERRIDE;
  void copy() FL_OVERRIDE;
  void paste() FL_OVERRIDE;
  void right_to_left() FL_OVERRIDE;
  void draw() FL_OVERRIDE;
  void full_text_size();
};


@interface FLNativeTextView : NSTextView {
  @public
  Fl_Cocoa_Text_Widget_Driver *driver;
}
- (void)doCommandBySelector:(SEL)aSelector;
@end

@implementation FLNativeTextView

- (void)doCommandBySelector:(SEL)aSelector {
  NSString *s = [[NSApp currentEvent] characters];
  if (aSelector == @selector(insertNewline:) && driver->kind == Fl_Text_Widget_Driver::SINGLE_LINE) {
    if (driver->widget->when() & FL_WHEN_ENTER_KEY) {
      driver->maybe_do_callback(FL_REASON_ENTER_KEY);
    }
    Fl::e_keysym = ([s isEqualToString:@"\r"] ? FL_Enter : FL_KP_Enter);
    driver->widget->parent()->handle(FL_SHORTCUT);
    return; // skip Enter key in single-line widgets
  }
  else if (aSelector == @selector(cancelOperation:)) { // escape
    Fl::e_keysym = FL_Escape;
    Fl::handle(FL_SHORTCUT, driver->widget->top_window());
    return;
  }
  else if (aSelector == @selector(insertTab:)) { // tab
    Fl::e_keysym = FL_Tab;
    Fl::handle(FL_KEYBOARD, driver->widget->window());
    return;
  }
  else if (aSelector == @selector(insertBacktab:)) { // shift+tab
    Fl::e_keysym = FL_Tab;
    Fl::e_state = FL_SHIFT;
    Fl::handle(FL_KEYBOARD, driver->widget->window());
    return;
  }
  [super doCommandBySelector:aSelector];
}

@end

@interface FLTextDelegate : NSObject <NSTextDelegate, NSTextViewDelegate> {
}
- (void)textDidChange:(NSNotification *)notification;
- (void)textViewDidChangeSelection:(NSNotification *)notification;
@end


@implementation FLTextDelegate
- (void)textDidChange:(NSNotification *)notification {
  FLNativeTextView *text_view = (FLNativeTextView*)[notification object];
  if (text_view->driver->kind == Fl_Text_Widget_Driver::SINGLE_LINE) {
    NSLayoutManager *lom = [text_view layoutManager];
    NSTextContainer *container = [text_view textContainer];
    CGRect big = [text_view frame];
    big.size.width = 20000;
    [text_view setFrame:big];
    NSRange gr = [lom glyphRangeForCharacterRange:NSMakeRange(0, 10000000) actualCharacterRange:NULL];
    NSRect rect = [lom boundingRectForGlyphRange:gr inTextContainer:container];
    NSView *scroll_view = [[text_view superview] superview];
    CGRect fr = [scroll_view frame];
//printf("rect.size.width=%.0f  fr.size.width=%.0f\n",rect.size.width, fr.size.width);
    if (rect.size.width >= fr.size.width) { // long text
      fr.size.width = rect.size.width + fr.origin.x;
    }
    [text_view setFrame:fr];
  } else if (!text_view->driver->widget->wrap()) text_view->driver->full_text_size();
  
  if (!text_view->driver->text_before_show) {
    text_view->driver->widget->set_changed();
    if (text_view->driver->widget->when() & FL_WHEN_CHANGED) {
      text_view->driver->widget->do_callback(FL_REASON_CHANGED);
    }
  }
}

- (void)textViewDidChangeSelection:(NSNotification *)notification {
  FLNativeTextView *text_view = (FLNativeTextView*)[notification object];
  text_view->driver->widget->take_focus();
}
@end


void Fl_Cocoa_Text_Widget_Driver::full_text_size() {
  NSString *content = [text_view string];
  if ([content length] == 0) return;
  if (widget->right_to_left()) {
    CGRect big = [text_view frame];
    big.size.width = 20000;
    [text_view setFrame:big];
  }
  NSLayoutManager *lom = [text_view layoutManager];
  NSTextContainer *container = [text_view textContainer];
  NSUInteger line_end, line_start, next = 0, right = 0, bottom = 0;
  do {
    NSRange r = NSMakeRange(next, 1);
    [content getLineStart:&line_start end:&next contentsEnd:&line_end forRange:r];
    r = NSMakeRange(line_start, line_end - line_start);
    NSRange gr = [lom glyphRangeForCharacterRange:r actualCharacterRange:NULL];
    NSRect rect = [lom boundingRectForGlyphRange:gr inTextContainer:container];
    CGFloat r2 = (widget->right_to_left() ? 0 : rect.origin.x) + rect.size.width;
    /*NSUInteger pos = (line_end - 2 < line_start ? line_start : line_end - 2);
    if (pos == line_end-2) {
      NSUInteger c2 = [content characterAtIndex:line_end-2];
      NSUInteger c1 = [content characterAtIndex:line_end-1];
      printf("%c%c r2=%.0f \n", c2, c1, r2);
    }*/
    if (ceil(r2) > right) right = ceil(r2);
    bottom = rect.origin.y + rect.size.height;
  } while (next < [content length]);
  CGRect fr = [scroll_view frame];
  fr.size.width = right + 5 +1;
  fr.size.height = bottom + widget->textsize();
  [text_view setFrame:fr];
}


Fl_Text_Widget_Driver *Fl_Text_Widget_Driver::newTextWidgetDriver(Fl_Native_Text_Widget *n) {
  Fl_Text_Widget_Driver *retval = (Fl_Text_Widget_Driver*)new Fl_Cocoa_Text_Widget_Driver();
  retval->widget = n;
  return retval;
}


Fl_Cocoa_Text_Widget_Driver::Fl_Cocoa_Text_Widget_Driver() : Fl_Text_Widget_Driver() {
  scroll_view = nil;
  text_view = nil;
  text_before_show = nil;
}


Fl_Cocoa_Text_Widget_Driver::~Fl_Cocoa_Text_Widget_Driver() {
  [text_before_show release];
  if (text_view) {
    FLTextDelegate *delegate = [text_view delegate];
    if (delegate) {
      [text_view setDelegate:nil];
      [delegate release];
    }
    [scroll_view setDocumentView:nil];
    [scroll_view removeFromSuperview];
  }
}


static void delayed_scroll_to_visible(id text_view) {
  Fl::remove_check((Fl_Timeout_Handler)delayed_scroll_to_visible, text_view);
  [text_view scrollRangeToVisible:[text_view selectedRange]];
}


void Fl_Cocoa_Text_Widget_Driver::resize(int x, int y, int w, int h) {
  CGRect fr = CGRectMake(x, widget->window()->h()-(y+h), w, h);
  fr = NSInsetRect(fr, Fl::box_dx(widget->box()), Fl::box_dy(widget->box()));
  int ns = widget->top_window()->screen_num();
  float s = Fl::screen_scale(ns);
  fr = CGRectMake(fr.origin.x * s, fr.origin.y * s, fr.size.width * s, fr.size.height * s);
  [scroll_view setFrame:fr];
  textfontandsize();
  if (kind == SINGLE_LINE) {
    [text_view didChangeText];
  } else {
    if (!widget->wrap()) full_text_size();
    else {
      CGRect scroll_fr = [scroll_view frame];
      CGRect text_fr = [text_view frame];
      if (text_fr.size.width != scroll_fr.size.width) {
        text_fr.size.width = scroll_fr.size.width;
        [text_view setFrame:fr]; // necessary to resize width, bad to resize height
      }
    }
    Fl::add_check((Fl_Timeout_Handler)delayed_scroll_to_visible, text_view);
  }
}


void Fl_Cocoa_Text_Widget_Driver::show_widget() {
  NSWindow *flwin = (NSWindow*)fl_mac_xid(widget->window());
  if (!flwin) return;
  if (!scroll_view) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    CGRect fr = CGRectMake(widget->x(), widget->window()->h()-(widget->y()+widget->h()), widget->w(), widget->h());
    fr = NSInsetRect(fr, Fl::box_dx(widget->box()), Fl::box_dy(widget->box()));
    int ns = widget->top_window()->screen_num();
    float s = Fl::screen_scale(ns);
    fr = CGRectMake(fr.origin.x * s, fr.origin.y * s, fr.size.width * s, fr.size.height * s);
    scroll_view = [[NSScrollView alloc] initWithFrame:fr];
    [[flwin contentView] addSubview:scroll_view];
    if ([flwin parentWindow]) [flwin setIgnoresMouseEvents:NO];
    [scroll_view release];
    text_view = [[FLNativeTextView alloc] initWithFrame:fr];
    [scroll_view setDocumentView:text_view];
    if ([[text_view superview] superview] != scroll_view)
      Fl::fatal("Problem in class Fl_Cocoa_Text_Widget_Driver");
    [text_view release];
    [(NSText*)text_view setDelegate:[[FLTextDelegate alloc] init]];
    text_view->driver = this;
    if (widget->right_to_left()) [text_view makeBaseWritingDirectionRightToLeft:nil];
    [text_view setAllowsDocumentBackgroundColorChange:YES];
    uchar r, g, b;
    Fl::get_color(widget->color(), r, g, b);
    [text_view setBackgroundColor:[NSColor colorWithRed:r/255. green:g/255. blue:b/255. alpha:1.]];
    Fl::get_color(widget->textcolor(), r, g, b);
    [text_view setTextColor:[NSColor colorWithRed:r/255. green:g/255. blue:b/255. alpha:1.]];
    Fl::get_color(widget->cursor_color(), r, g, b);
    [text_view setInsertionPointColor:[NSColor colorWithRed:r/255. green:g/255. blue:b/255. alpha:1.]];
    widget->textfont(widget->textfont());
    [text_view setRichText:NO];
    if (!widget->selectable()) [text_view setSelectable:NO];
    if (widget->readonly()) [text_view setEditable:NO];
    else {
      [flwin makeFirstResponder:text_view];
      [text_view setAllowsUndo:YES];
    }
    [scroll_view setHasVerticalScroller:(kind == Fl_Text_Widget_Driver::MULTIPLE_LINES)];
    [scroll_view setHasHorizontalScroller:
     (kind == Fl_Text_Widget_Driver::MULTIPLE_LINES && !widget->wrap())];
    if ([scroll_view hasVerticalScroller] || [scroll_view hasHorizontalScroller]) {
      [scroll_view setScrollerStyle:NSScrollerStyleOverlay];
    }
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    NSParagraphStyle *start = [NSParagraphStyle defaultParagraphStyle];
    [style setParagraphStyle:start];
    [style setLineBreakMode:([scroll_view hasHorizontalScroller] ?
                             NSLineBreakByClipping : NSLineBreakByWordWrapping)];
    [text_view setDefaultParagraphStyle:style];
    if (text_before_show) {
      [text_view setString:text_before_show];
      [text_view didChangeText];
      [text_view setSelectedRange:NSMakeRange(0, 0)];
      [text_before_show release];
      text_before_show = nil;
    }
    [pool release];
  } else if ([scroll_view isHidden]) {
    if (kind == Fl_Text_Widget_Driver::MULTIPLE_LINES) {
      NSRange save_range = [text_view selectedRange];
      [text_view setSelectedRange:NSMakeRange(0, 100000000)];
      if (widget->right_to_left()) {
        [text_view makeBaseWritingDirectionRightToLeft:nil];
      } else {
        [text_view makeBaseWritingDirectionLeftToRight:nil];
      }
      [text_view setSelectedRange:save_range];
    } else {
      if (widget->right_to_left()) {
        [text_view makeBaseWritingDirectionRightToLeft:nil];
      } else {
        [text_view makeBaseWritingDirectionLeftToRight:nil];
      }
      [text_view didChangeText];
    }
    [scroll_view setHidden:NO];
  }
}


void Fl_Cocoa_Text_Widget_Driver::hide_widget() {
  [scroll_view setHidden:YES];
  if (!widget->readonly() && (widget->when() & FL_WHEN_RELEASE))
    maybe_do_callback(FL_REASON_LOST_FOCUS);
}


void Fl_Cocoa_Text_Widget_Driver::textfontandsize() {
  if (!text_view) return;
  extern Fl_Fontdesc* fl_fonts;
  if (!fl_fonts) fl_fonts = Fl_Graphics_Driver::default_driver().calc_fl_fonts();
  NSString *name = [NSString stringWithUTF8String:(fl_fonts + widget->textfont())->name];
  int ns = widget->top_window()->screen_num();
  float s = Fl::screen_scale(ns);
  NSFont *nsfont = [NSFont fontWithName:name size:widget->textsize() * s];
  [text_view setFont:nsfont];
  if ([[text_view string] length]) [text_view didChangeText];
}


const char *Fl_Cocoa_Text_Widget_Driver::value() {
  return text_view ? [[text_view string] UTF8String] : [text_before_show UTF8String];
}


void Fl_Cocoa_Text_Widget_Driver::value(const char *t, int len) {
  if (!t) t = "";
  [text_before_show release];
  text_before_show = [[NSString alloc] initWithBytes:t length:len encoding:NSUTF8StringEncoding];
  if (text_view) {
    [text_view setString:text_before_show];
    [text_view didChangeText];
    [text_before_show release];
    text_before_show = nil;
    [text_view setSelectedRange:NSMakeRange(0, 0)];
  }
}


int Fl_Cocoa_Text_Widget_Driver::insert_position() {
  NSRange r = [text_view selectedRange];
  return [[[text_view string] substringToIndex:r.location] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
}


static NSRange position_to_range(NSTextView *text_view, int pos, int mark) {
  const char *t = [[text_view string] UTF8String];
  int maxi = strlen(t);
  if (pos > maxi) pos = maxi;
  if (mark > maxi) mark = maxi;
  int last  = pos > mark ? pos : mark;
  int first = pos < mark ? pos : mark;
  char *p = (char *)fl_utf8back(t+last, t, t+maxi);
  NSString *s = [[NSString alloc] initWithBytes:t length:p-t encoding:NSUTF8StringEncoding];
  int last_c = [s length];
  [s release];
  int first_c = last_c;
  if (first < last) {
    p = (char *)fl_utf8back(t+first, t, t+maxi);
    s = [[NSString alloc] initWithBytes:t length:p-t encoding:NSUTF8StringEncoding];
    first_c = [s length];
    [s release];
  }
  return NSMakeRange(first_c, last_c - first_c);
}


void Fl_Cocoa_Text_Widget_Driver::insert_position(int pos, int mark) {
  NSRange r = position_to_range(text_view, pos, mark);
  [text_view setSelectedRange:r];
  [text_view scrollRangeToVisible:r];
}


void Fl_Cocoa_Text_Widget_Driver::readonly(bool on_off) {
  [text_view setEditable:!on_off];
}


void Fl_Cocoa_Text_Widget_Driver::selectable(bool on_off) {
  [text_view setSelectable:on_off];
}


void Fl_Cocoa_Text_Widget_Driver::replace(int from, int to, const char *text, int len) {
  NSRange r = position_to_range(text_view, from, to);
  if (len == 0 && text) len = strlen(text);
  NSString *s;
  if (len) s = [[[NSString alloc] initWithBytes:text length:len encoding:NSUTF8StringEncoding] autorelease];
  else s = [NSString string];
  [text_view insertText:s replacementRange:r];
  r = NSMakeRange(r.location + [s length], 0);
  [text_view setSelectedRange:r];
  [text_view scrollRangeToVisible:r];
}


void Fl_Cocoa_Text_Widget_Driver::replace_selection(const char *text, int len) {
  NSString *s;
  if (len) s = [[[NSString alloc] initWithBytes:text length:len encoding:NSUTF8StringEncoding] autorelease];
  else s = [NSString string];
  [text_view insertText:s replacementRange:[text_view selectedRange]];
}


int Fl_Cocoa_Text_Widget_Driver::mark() {
  NSRange r = [text_view selectedRange];
  return [[[text_view string] substringToIndex:r.location + r.length] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
}


unsigned Fl_Cocoa_Text_Widget_Driver::index(int i) const {
  const char *s = [[text_view string] UTF8String];
  int len = strlen(s);
  return (i >= 0 && i < len ? fl_utf8decode(s + i, s + len, &len) : 0);
}


int Fl_Cocoa_Text_Widget_Driver::undo() {
  if (![[text_view undoManager] canUndo]) return 0;
  [[text_view undoManager] undo];
  return 1;
}


int Fl_Cocoa_Text_Widget_Driver::redo() {
  if (![[text_view undoManager] canRedo]) return 0;
  [[text_view undoManager] redo];
  return 1;
}


bool Fl_Cocoa_Text_Widget_Driver::can_undo() const {
  return [[text_view undoManager] canUndo];
}


bool Fl_Cocoa_Text_Widget_Driver::can_redo() const {
  return [[text_view undoManager] canRedo];
}


void Fl_Cocoa_Text_Widget_Driver::focus() {
  if (!widget->readonly()) {
    if ([[text_view window] firstResponder] != text_view) {
      [[text_view window] makeFirstResponder:text_view];
    }
  }
}


void Fl_Cocoa_Text_Widget_Driver::unfocus() {
  NSWindow *xid = [text_view window];
  if ([xid firstResponder] == text_view) {
    NSView *v = [xid parentWindow] ? nil: [xid contentView];
    [xid makeFirstResponder:v];
  }
  if (!widget->readonly() && (widget->when() & FL_WHEN_RELEASE))
    maybe_do_callback(FL_REASON_LOST_FOCUS);
}


void Fl_Cocoa_Text_Widget_Driver::select_all() {
  [text_view setSelectedRange:NSMakeRange(0, 10000000)];
}


void Fl_Cocoa_Text_Widget_Driver::copy() {
  if (text_view->driver->kind == Fl_Text_Widget_Driver::MULTIPLE_LINES) {
    [text_view writeSelectionToPasteboard:[NSPasteboard generalPasteboard]
                              types:[text_view writablePasteboardTypes]];
  } else {
    NSRange r = [text_view selectedRange];
    if (!r.length) return;
    NSString *sub = [[text_view string] substringWithRange:r];
    // When single-line, replace "␍" by newline and "␉" by tab.
    // Also remove U+202b and U+202c characters
    unichar uni = 0x202c;
    NSString *pdf = [NSString stringWithCharacters:&uni length:1];
    uni = 0x202b;
    NSString *rle = [NSString stringWithCharacters:&uni length:1];
    sub = [sub stringByReplacingOccurrencesOfString:pdf withString:@""];
    sub = [sub stringByReplacingOccurrencesOfString:rle withString:@""];
    sub = [sub stringByReplacingOccurrencesOfString:@"␍" withString:@"\n"];
    sub = [sub stringByReplacingOccurrencesOfString:@"␉" withString:@"\t"];
    NSPasteboard *clip = [NSPasteboard generalPasteboard];
    [clip declareTypes:[NSArray arrayWithObject:@"public.utf8-plain-text"] owner:nil];
    [clip setString:sub forType:@"public.utf8-plain-text"];
  }
}


void Fl_Cocoa_Text_Widget_Driver::paste() {
  if (text_view->driver->kind == Fl_Text_Widget_Driver::SINGLE_LINE) {
    // When single-line, replace newlines by "␍" and tabs by "␉" in pasted text
    NSPasteboard *clip = [NSPasteboard generalPasteboard];
    NSString *found = [clip availableTypeFromArray:[NSArray arrayWithObjects:@"public.utf8-plain-text", @"public.utf16-plain-text", @"com.apple.traditional-mac-plain-text", nil]];
    if (found) {
      NSString *s = [clip stringForType:found];
      s = [s stringByReplacingOccurrencesOfString:@"\n" withString:@"␍"];
      s = [s stringByReplacingOccurrencesOfString:@"\t" withString:@"␉"];
      [text_view insertText:s replacementRange:[text_view selectedRange]];
    }
  } else [text_view pasteAsPlainText:nil];
}


void Fl_Cocoa_Text_Widget_Driver::right_to_left() {
  if (scroll_view) {
    widget->hide();
    widget->show();
    widget->take_focus();
  }
}


void Fl_Cocoa_Text_Widget_Driver::draw() {
  if (Fl_Surface_Device::surface() != Fl_Display_Device::display_device()) {
    Fl_Window *win = widget->window();
    NSWindow *nswin = (NSWindow*)fl_mac_xid(win);
    CGImageRef img = Fl_Cocoa_Window_Driver::capture_decorated_window_10_6(nswin);
    int bt = [nswin frame].size.height - [[nswin contentView] frame].size.height;
    float s = Fl::screen_driver()->scale(0);
    if (Fl_Cocoa_Window_Driver::driver(win)->mapped_to_retina()) { s *= 2; bt *= 2; }
    CGRect cgr = CGRectMake(s*widget->x(), s*widget->y() + bt,
                            s*widget->w(), s*widget->h());
    CGImageRef img2 = CGImageCreateWithImageInRect(img, cgr);
    CGImageRelease(img);
    Fl_Quartz_Graphics_Driver *gr = (Fl_Quartz_Graphics_Driver*)fl_graphics_driver;
    gr->draw_CGImage(img2, widget->x(),widget->y(), widget->w(), widget->h(),
                     0, 0, widget->w(), widget->h());
    CGImageRelease(img2);
  }
}
