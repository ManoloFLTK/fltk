//
// Fl_Cocoa_Native_Input_Driver for the Fast Light Tool Kit (FLTK).
//
// Copyright 2025 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//

#include <FL/platform.H> // for fl_mac_xid

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7

#include "../../Fl_Native_Input_Driver.H"
#include "../Quartz/Fl_Quartz_Graphics_Driver.H"
#include "../Darwin/Fl_Darwin_System_Driver.H"  // for calc_mac_os_version()
#include "Fl_Cocoa_Window_Driver.H"
#include "../../Fl_Screen_Driver.H"
#include "../Quartz/Fl_Font.H"

#import <Cocoa/Cocoa.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_13
#  define NSPasteboardTypeFileURL  NSFilenamesPboardType
#endif

@class FLNativeTextView;

class Fl_Cocoa_Native_Input_Driver : public Fl_Native_Input_Driver {
public:
  FLNativeTextView *text_view;
  NSScrollView *scroll_view;
  NSString *text_before_show;
  bool initial_fill;
  Fl_Cocoa_Native_Input_Driver();
  ~Fl_Cocoa_Native_Input_Driver();
  void show_widget() FL_OVERRIDE;
  void hide_widget() FL_OVERRIDE;
  void resize() FL_OVERRIDE;
  void textfontandsize() FL_OVERRIDE;
  void textcolor() FL_OVERRIDE;
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
  int handle_focus(int event) FL_OVERRIDE;
  void select_all() FL_OVERRIDE;
  int copy() FL_OVERRIDE;
  void paste() FL_OVERRIDE;
  void right_to_left() FL_OVERRIDE;
  void draw() FL_OVERRIDE;
  void full_text_size();
};


@interface FLNativeTextView : NSTextView {
  @public
  Fl_Cocoa_Native_Input_Driver *driver;
}
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender;
- (void)doCommandBySelector:(SEL)aSelector;
- (BOOL)acceptsFirstResponder;
@end

@implementation FLNativeTextView

- (BOOL)acceptsFirstResponder
{
  return driver->widget->active() ? YES : NO;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender {
  if (driver->kind == Fl_Native_Input_Driver::SINGLE_LINE) {
    NSPasteboard *pboard = [sender draggingPasteboard];
    NSString *s = nil;
    if ([[pboard types] containsObject:NSPasteboardTypeFileURL]) {
      NSArray *a = [pboard readObjectsForClasses:[NSArray arrayWithObject:[NSURL class]]
                                         options:nil]; // 10.6
      NSURL *url = (NSURL*)[a objectAtIndex:0];
      s = [url path];
    } else {
      s = [pboard stringForType:NSPasteboardTypeString];
      s = [s stringByReplacingOccurrencesOfString:@"\n" withString:@"␍"];
      s = [s stringByReplacingOccurrencesOfString:@"\t" withString:@"␉"];
    }
    if (s) { // insert dragged text after having replaced \n's and \t's
      NSPoint point = [sender draggingLocation];
      NSRect frame = [driver->scroll_view frame];
      NSRect visible = [driver->scroll_view documentVisibleRect];
      point.x += (visible.origin.x - frame.origin.x);
      point.y = driver->widget->textsize() / 2;
      NSRange r = NSMakeRange(
        [driver->text_view characterIndexForInsertionAtPoint:point], 0);
      [driver->text_view setSelectedRange:r];
      [driver->text_view insertText:s replacementRange:r];
      return YES;
    }
    return NO;
  } else {
    return [super performDragOperation:sender];
  }
}

- (void)doCommandBySelector:(SEL)aSelector {
  NSString *s = [[NSApp currentEvent] characters];
  if (aSelector == @selector(insertNewline:) && driver->kind == Fl_Native_Input_Driver::SINGLE_LINE) {
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
    if (driver->kind == Fl_Native_Input_Driver::SINGLE_LINE || driver->widget->tab_nav()) {
      Fl::e_keysym = FL_Tab;
      Fl::handle(FL_KEYBOARD, driver->widget->window());
      return;
    }
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
  if (text_view->driver->kind == Fl_Native_Input_Driver::SINGLE_LINE) {
    NSLayoutManager *lom = [text_view layoutManager];
    NSTextContainer *container = [text_view textContainer];
    CGRect big = [text_view frame];
    big.size.width = 20000;
    [text_view setFrame:big];
    NSRange gr = [lom glyphRangeForCharacterRange:NSMakeRange(0, 10000000) // 10.7
                             actualCharacterRange:NULL];
    NSRect rect = [lom boundingRectForGlyphRange:gr inTextContainer:container]; // 10.7
    CGRect fr = [text_view->driver->scroll_view frame];
//printf("rect.size.width=%.0f  fr.size.width=%.0f\n",rect.size.width, fr.size.width);
    if (rect.size.width >= fr.size.width) { // long text
      fr.size.width = rect.size.width + fr.origin.x;
    }
    [text_view setFrame:fr];
  } else if (!text_view->driver->widget->wrap()) text_view->driver->full_text_size();
  
  if (!text_view->driver->text_before_show && !text_view->driver->initial_fill) {
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


void Fl_Cocoa_Native_Input_Driver::full_text_size() {
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
    NSRange gr = [lom glyphRangeForCharacterRange:r actualCharacterRange:NULL]; // 10.7
    NSRect rect = [lom boundingRectForGlyphRange:gr inTextContainer:container]; // 10.7
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


Fl_Native_Input_Driver *Fl_Native_Input_Driver::newNativeInputDriver(Fl_Native_Input *n) {
  Fl_Native_Input_Driver *retval = NULL;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
  retval = (Fl_Darwin_System_Driver::calc_mac_os_version() >= 100700 ?
            new Fl_Cocoa_Native_Input_Driver() : NULL);
#endif
  if (!retval) retval = new Fl_Backup_Native_Input_Driver();
  retval->widget = n;
  retval->widget->maximum_size(INT_MAX);
  return retval;
}


Fl_Cocoa_Native_Input_Driver::Fl_Cocoa_Native_Input_Driver() : Fl_Native_Input_Driver() {
  scroll_view = nil;
  text_view = nil;
  text_before_show = nil;
  initial_fill = false;
}


Fl_Cocoa_Native_Input_Driver::~Fl_Cocoa_Native_Input_Driver() {
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
  [text_view scrollRangeToVisible:[text_view selectedRange]];
}


void Fl_Cocoa_Native_Input_Driver::resize() {
  Fl_Window *top = widget->top_window();
  int X, Y;
  widget->top_window_offset(X, Y);
  CGRect fr = CGRectMake(X, top->h() - (Y + widget->h()), widget->w(), widget->h());
  fr = NSInsetRect(fr, Fl::box_dx(widget->box()), Fl::box_dy(widget->box()));
  float s = Fl::screen_scale(top->screen_num());
  fr = CGRectMake(fr.origin.x * s, fr.origin.y * s, fr.size.width * s, fr.size.height * s);
  [scroll_view setFrame:fr];
  textfontandsize();
  if (kind == MULTIPLE_LINES) {
    if (!widget->wrap()) full_text_size();
    else {
      CGRect scroll_fr = [scroll_view frame];
      CGRect text_fr = [text_view frame];
      if (text_fr.size.width != scroll_fr.size.width) {
        text_fr.size.width = scroll_fr.size.width;
        [text_view setFrame:fr]; // necessary to resize width, bad to resize height
      }
    }
    Fl::add_timeout(0, (Fl_Timeout_Handler)delayed_scroll_to_visible, text_view);
  }
}


void Fl_Cocoa_Native_Input_Driver::show_widget() {
  if (!widget->window()->shown()) return;
  if (!scroll_view) {
    // The native input object is an NSScrollView containing an FLNativeTextView.
    // Together, these 2 cocoa objects produce a scrollable text input widget.
    // The NSScrollView is made a subview of the top window's contentView,
    // even if the native object belongs to a subwindow.
    // Focus remains always to the top window, but that window's firstResponder
    // goes to the FLNativeTextView (instead of the contentView elsewhere in FLTK)
    // when the native widget takes focus. This way, the native object receives
    // and processes events and draws the NSScrollView without FLTK doing anything
    // except running custom methods of FLNativeTextView.
    // A key detail when the native widget is in a subwindow: the area of this
    // widget in the subwindow's contentView needs to be made fully transparent
    // in order for the underlying NSScrollView to be visible. That's done in
    // member Fl_Cocoa_Native_Input_Driver::draw().
    Fl_Window *top = widget->top_window();
    int X, Y;
    widget->top_window_offset(X, Y);
    CGRect fr = CGRectMake(X, top->h() - (Y + widget->h()), widget->w(), widget->h());
    fr = NSInsetRect(fr, Fl::box_dx(widget->box()), Fl::box_dy(widget->box()));
    float s = Fl::screen_scale(top->screen_num());
    fr = CGRectMake(fr.origin.x * s, fr.origin.y * s, fr.size.width * s, fr.size.height * s);
    scroll_view = [[NSScrollView alloc] initWithFrame:fr];
    NSWindow *flwin = (NSWindow*)fl_mac_xid(top);
    [[flwin contentView] addSubview:scroll_view];
    [scroll_view release];
    text_view = [[FLNativeTextView alloc] initWithFrame:fr];
    [scroll_view setDocumentView:text_view];
    [text_view release];
    [(NSText*)text_view setDelegate:[[FLTextDelegate alloc] init]];
    text_view->driver = this;
    if (widget->right_to_left()) [text_view makeBaseWritingDirectionRightToLeft:nil]; // 10.6
    [text_view setAllowsDocumentBackgroundColorChange:YES];
    uchar r, g, b;
    Fl::get_color(widget->color(), r, g, b);
    [text_view setBackgroundColor:[NSColor colorWithRed:r/255. green:g/255. blue:b/255. alpha:1.]];
    textcolor();
    Fl::get_color(widget->cursor_color(), r, g, b);
    [text_view setInsertionPointColor:[NSColor colorWithRed:r/255. green:g/255. blue:b/255. alpha:1.]];
    textfontandsize();
    [text_view setRichText:NO];
    if (!widget->selectable()) [text_view setSelectable:NO];
    if (widget->readonly()) [text_view setEditable:NO];
    else if (widget->active()) [text_view setAllowsUndo:YES];
    [scroll_view setHasVerticalScroller:(kind == Fl_Native_Input_Driver::MULTIPLE_LINES)];
    [scroll_view setHasHorizontalScroller:
     (kind == Fl_Native_Input_Driver::MULTIPLE_LINES && !widget->wrap())];
    if ([scroll_view hasVerticalScroller] || [scroll_view hasHorizontalScroller]) {
      [scroll_view setScrollerStyle:NSScrollerStyleOverlay]; // 10.7
    }
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    NSParagraphStyle *start = [NSParagraphStyle defaultParagraphStyle];
    [style setParagraphStyle:start];
    [style setLineBreakMode:([scroll_view hasHorizontalScroller] ?
                             NSLineBreakByClipping : NSLineBreakByWordWrapping)];
    [text_view setDefaultParagraphStyle:style];
    if (text_before_show) {
      [text_view setString:text_before_show];
      [text_view setSelectedRange:NSMakeRange(0, 0)];
      [text_before_show release];
      text_before_show = nil;
      initial_fill = true;
      [text_view didChangeText];
      initial_fill = false;
    }
  } else if ([scroll_view isHidden]) {
    initial_fill = true;
    if (kind == Fl_Native_Input_Driver::MULTIPLE_LINES) {
      NSRange save_range = [text_view selectedRange];
      [text_view setSelectedRange:NSMakeRange(0, 100000000)];
      if (widget->right_to_left()) {
        [text_view makeBaseWritingDirectionRightToLeft:nil]; // 10.6
      } else {
        [text_view makeBaseWritingDirectionLeftToRight:nil]; // 10.6
      }
      [text_view setSelectedRange:save_range];
    } else {
      if (widget->right_to_left()) {
        [text_view makeBaseWritingDirectionRightToLeft:nil]; // 10.6
      } else {
        [text_view makeBaseWritingDirectionLeftToRight:nil]; // 10.6
      }
      [text_view didChangeText];
    }
    initial_fill = false;
    [scroll_view setHidden:NO];
  }
}


void Fl_Cocoa_Native_Input_Driver::hide_widget() {
  [scroll_view setHidden:YES];
  if (!widget->readonly() && (widget->when() & FL_WHEN_RELEASE))
    maybe_do_callback(FL_REASON_LOST_FOCUS);
}


void Fl_Cocoa_Native_Input_Driver::textfontandsize() {
  if (!text_view) return;
  if (!fl_fonts) fl_fonts = Fl_Graphics_Driver::default_driver().calc_fl_fonts();
  NSString *name = [NSString stringWithUTF8String:(fl_fonts + widget->textfont())->name];
  int ns = widget->top_window()->screen_num();
  float s = Fl::screen_scale(ns);
  NSFont *nsfont = [NSFont fontWithName:name size:widget->textsize() * s];
  [text_view setFont:nsfont];
  if ([[text_view string] length]) {
    initial_fill = true;
    [text_view didChangeText];
    initial_fill = false;
  }
}


void Fl_Cocoa_Native_Input_Driver::textcolor() {
  if (!text_view) return;
  uchar r, g, b;
  Fl::get_color(widget->active() ? widget->textcolor() : fl_inactive(widget->textcolor()), r, g, b);
  [text_view setTextColor:[NSColor colorWithRed:r/255. green:g/255. blue:b/255. alpha:1.]];
}


const char *Fl_Cocoa_Native_Input_Driver::value() {
  return text_view ? [[text_view string] UTF8String] : [text_before_show UTF8String];
}


void Fl_Cocoa_Native_Input_Driver::value(const char *t, int len) {
  if (!t) t = "";
  [text_before_show release];
  text_before_show = [[NSString alloc] initWithBytes:t length:len encoding:NSUTF8StringEncoding];
  if (text_view) {
    [text_view setString:text_before_show];
    [text_view didChangeText];
    [text_before_show release];
    text_before_show = nil;
    [text_view setSelectedRange:NSMakeRange(0, 0)];
    if (kind == MULTIPLE_LINES)
      [text_view scrollRangeToVisible:[text_view selectedRange]];
  }
}


int Fl_Cocoa_Native_Input_Driver::insert_position() {
  NSRange r = [text_view selectedRange];
  return [[[text_view string] substringToIndex:r.location] lengthOfBytesUsingEncoding:NSUTF8StringEncoding]; // 10.4
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


void Fl_Cocoa_Native_Input_Driver::insert_position(int pos, int mark) {
  NSRange r = position_to_range(text_view, pos, mark);
  [text_view setSelectedRange:r];
  [text_view scrollRangeToVisible:r];
}


void Fl_Cocoa_Native_Input_Driver::readonly(bool on_off) {
  [text_view setEditable:!on_off];
}


void Fl_Cocoa_Native_Input_Driver::selectable(bool on_off) {
  [text_view setSelectable:on_off];
}


void Fl_Cocoa_Native_Input_Driver::replace(int from, int to, const char *text, int len) {
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


void Fl_Cocoa_Native_Input_Driver::replace_selection(const char *text, int len) {
  NSString *s;
  if (len) s = [[[NSString alloc] initWithBytes:text length:len encoding:NSUTF8StringEncoding] autorelease];
  else s = [NSString string];
  [text_view insertText:s replacementRange:[text_view selectedRange]];
}


int Fl_Cocoa_Native_Input_Driver::mark() {
  NSRange r = [text_view selectedRange];
  return [[[text_view string] substringToIndex:r.location + r.length] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
}


unsigned Fl_Cocoa_Native_Input_Driver::index(int i) const {
  const char *s = [[text_view string] UTF8String];
  int len = strlen(s);
  return (i >= 0 && i < len ? fl_utf8decode(s + i, s + len, &len) : 0);
}


int Fl_Cocoa_Native_Input_Driver::undo() {
  if (![[text_view undoManager] canUndo]) return 0;
  [[text_view undoManager] undo];
  return 1;
}


int Fl_Cocoa_Native_Input_Driver::redo() {
  if (![[text_view undoManager] canRedo]) return 0;
  [[text_view undoManager] redo];
  return 1;
}


bool Fl_Cocoa_Native_Input_Driver::can_undo() const {
  return [[text_view undoManager] canUndo];
}


bool Fl_Cocoa_Native_Input_Driver::can_redo() const {
  return [[text_view undoManager] canRedo];
}


int Fl_Cocoa_Native_Input_Driver::handle_focus(int event) {
  NSWindow *xid = (NSWindow*)fl_mac_xid(widget->top_window());
  if (event == FL_FOCUS) {
    //printf("focus() %s\n",widget->label());
    if ([xid firstResponder] != text_view) {
      [xid makeFirstResponder:text_view];
      [text_view scrollRangeToVisible:[text_view selectedRange]];
    }
  } else if (event == FL_UNFOCUS) {
    if ([xid firstResponder] == text_view) {
      //printf("unfocus() %s\n",widget->label());
      [xid makeFirstResponder:[xid contentView]];
    }
  }
  return 1;
}


void Fl_Cocoa_Native_Input_Driver::select_all() {
  [text_view setSelectedRange:NSMakeRange(0, 10000000)];
}


int Fl_Cocoa_Native_Input_Driver::copy() {
  if (text_view->driver->kind == Fl_Native_Input_Driver::MULTIPLE_LINES) {
    [text_view writeSelectionToPasteboard:[NSPasteboard generalPasteboard]
                              types:[text_view writablePasteboardTypes]];
  } else {
    NSRange r = [text_view selectedRange];
    if (!r.length) return 0;
    NSString *sub = [[text_view string] substringWithRange:r];
    // When single-line, replace "␍" by newline and "␉" by tab.
    // Also remove U+202b and U+202c characters
    unichar uni = 0x202c;
    NSString *pdf = [NSString stringWithCharacters:&uni length:1];
    uni = 0x202b;
    NSString *rle = [NSString stringWithCharacters:&uni length:1];
    sub = [sub stringByReplacingOccurrencesOfString:pdf withString:@""]; // 10.5
    sub = [sub stringByReplacingOccurrencesOfString:rle withString:@""];
    sub = [sub stringByReplacingOccurrencesOfString:@"␍" withString:@"\n"];
    sub = [sub stringByReplacingOccurrencesOfString:@"␉" withString:@"\t"];
    NSPasteboard *clip = [NSPasteboard generalPasteboard];
    [clip declareTypes:[NSArray arrayWithObject:@"public.utf8-plain-text"] owner:nil];
    [clip setString:sub forType:@"public.utf8-plain-text"];
  }
  return 1;
}


void Fl_Cocoa_Native_Input_Driver::paste() {
  if (text_view->driver->kind == Fl_Native_Input_Driver::SINGLE_LINE) {
    // When single-line, replace newlines by "␍" and tabs by "␉" in pasted text
    NSPasteboard *clip = [NSPasteboard generalPasteboard];
    NSString *found = [clip availableTypeFromArray:[NSArray arrayWithObjects:@"public.utf8-plain-text", @"public.utf16-plain-text", @"com.apple.traditional-mac-plain-text", nil]];
    if (found) {
      NSString *s = [clip stringForType:found];
      s = [s stringByReplacingOccurrencesOfString:@"\n" withString:@"␍"]; // 10.5
      s = [s stringByReplacingOccurrencesOfString:@"\t" withString:@"␉"];
      [text_view insertText:s replacementRange:[text_view selectedRange]];
    }
  } else [text_view pasteAsPlainText:nil];
}


void Fl_Cocoa_Native_Input_Driver::right_to_left() {
  if (scroll_view) {
    widget->hide();
    widget->show();
    widget->take_focus();
  }
}


static Fl_RGB_Image *cgimage_to_rgb(CGImageRef img) {
  int w = CGImageGetWidth(img);
  int h = CGImageGetHeight(img);
  CGColorSpaceRef cspace = CGColorSpaceCreateDeviceRGB();
  uchar *rgba = new uchar[4 * w * h];
  CGContextRef auxgc = CGBitmapContextCreate(rgba, w, h, 8, 4 * w, cspace,
                                             kCGImageAlphaPremultipliedLast);
  CGColorSpaceRelease(cspace);
  CGContextDrawImage(auxgc, CGRectMake(0, 0, w, h), img);
  CGContextRelease(auxgc);
  Fl_RGB_Image *rgb = new Fl_RGB_Image(rgba, w, h, 4);
  rgb->alloc_array = 1;
  return rgb;
}


void Fl_Cocoa_Native_Input_Driver::draw() {
  if (Fl_Surface_Device::surface() != Fl_Display_Device::display_device()) {
    NSWindow *nswin = (NSWindow*)fl_mac_xid(widget->top_window());
    CGImageRef img = Fl_Cocoa_Window_Driver::capture_decorated_window_10_5(nswin); // 10.6
    int bt = [nswin frame].size.height - [[nswin contentView] frame].size.height;
    float s = Fl::screen_driver()->scale(0);
    int X, Y;
    widget->top_window_offset(X, Y);
    if (Fl_Cocoa_Window_Driver::driver(widget->top_window())->mapped_to_retina()) { s *= 2; bt *= 2; }
    CGRect cgr = CGRectMake(s*X, s*Y + bt, s*widget->w(), s*widget->h());
    cgr = NSInsetRect(cgr, s*Fl::box_dx(widget->box()), s*Fl::box_dy(widget->box()));
    CGImageRef img2 = CGImageCreateWithImageInRect(img, cgr); // 10.4
    CGImageRelease(img);
    Fl_RGB_Image *rgb = cgimage_to_rgb(img2);
    CGImageRelease(img2);
    rgb->scale(widget->w() - Fl::box_dw(widget->box()), widget->h() - Fl::box_dh(widget->box()), 1, 1);
    rgb->draw(widget->x() + Fl::box_dx(widget->box()), widget->y() + Fl::box_dy(widget->box()));
    delete rgb;
  } else if (widget->window()->parent()) {
    // Here, the widget is in a subwindow. We make that widget's area fully transparent
    // so the NSScrollView below it is visible.
    // Compute the widget's area in the subwindow, and clear it in the subwindow's bitmap context.
    CGRect fr = CGRectMake(widget->x(), widget->y(), widget->w(), widget->h());
    fr = NSInsetRect(fr, Fl::box_dx(widget->box()), Fl::box_dy(widget->box()));
    Fl_Window *win = widget->window();
    do {
      Fl_Cocoa_Window_Driver *dr = (Fl_Cocoa_Window_Driver*)Fl_Window_Driver::driver(win);
      CGContextClearRect(dr->gc, fr);
      fr.origin.x += win->x();
      fr.origin.y += win->y();
      win = win->window();
    } while (win->parent());
  }
}

#endif // MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
