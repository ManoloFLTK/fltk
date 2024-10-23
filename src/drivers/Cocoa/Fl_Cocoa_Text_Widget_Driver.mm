//
//  Fl_Cocoa_Text_Widget_Driver.mm
//

#include <config.h> // for BORDER_WIDTH
#include "../../Fl_Text_Widget_Driver.H"
#include <FL/platform.H>
#include <FL/Fl_Native_Text_Widget.H>
#include <FL/Fl_Graphics_Driver.H>
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
  void textfont(Fl_Font f) FL_OVERRIDE;
  const char *value() FL_OVERRIDE;
  void value(const char *t, int len) FL_OVERRIDE;
  int insert_position() FL_OVERRIDE;
  void insert_position(int pos, int mark) FL_OVERRIDE;
  void readonly(bool on_off) FL_OVERRIDE;
  void selectable(bool on_off) FL_OVERRIDE;
  void replace(int from, int to, const char *text, int len) FL_OVERRIDE;
  int mark() FL_OVERRIDE;
  unsigned index(int i) const FL_OVERRIDE;
  int undo() FL_OVERRIDE;
  int redo() FL_OVERRIDE;
  bool can_undo() const FL_OVERRIDE;
  bool can_redo() const FL_OVERRIDE;
  void focus() FL_OVERRIDE;
  void unfocus() FL_OVERRIDE;
  void copy() FL_OVERRIDE;
  void paste() FL_OVERRIDE;
};


@interface FLNativeTextView : NSTextView {
  @public
  Fl_Cocoa_Text_Widget_Driver *driver;
}
- (void)insertText:(id)string replacementRange:(NSRange)r;
- (void)doCommandBySelector:(SEL)aSelector;
@end

@implementation FLNativeTextView

- (void)insertText:(id)string replacementRange:(NSRange)r {
  if (driver->kind == Fl_Text_Widget_Driver::SINGLE_LINE &&
      [string isKindOfClass:[NSString class]] && [string isEqualToString:@"\n"]) {
    if (driver->widget->changed() || (driver->widget->when() & FL_WHEN_NOT_CHANGED))
        driver->widget->do_callback(FL_REASON_ENTER_KEY);
    [self selectAll:self];
    if (driver->widget->when() & FL_WHEN_RELEASE)
      Fl::focus(0);
    return;
  }
  [super insertText:string replacementRange:r];
}

- (void)doCommandBySelector:(SEL)aSelector {
  NSString *s = [[NSApp currentEvent] characters];
  if ([s isEqualTo:@"c"]) { // cmd-C to copy selection
    driver->copy();
    return;
  }
  if ([s isEqualTo:@"x"] && !driver->widget->readonly()) { // cmd-X to cut selection
    driver->copy();
    driver->widget->cut();
    return;
  }
  if ([s isEqualTo:@"v"]) { // cmd-V to paste
    driver->paste();
    return;
  }
  if ([s isEqualTo:@"a"]) { // cmd-A to select all
    [self selectAll:self];
    return;
  }
  if ([s isEqualTo:@"z"]) { // cmd-Z to undo
    [[self undoManager] undo];
    return;
  }
  if ([s isEqualTo:@"Z"]) { // cmd-^Z to redo
    [[self undoManager] redo];
    return;
  }
  if (aSelector == @selector(cancelOperation:)) { // escape
    Fl::e_keysym = FL_Escape;
    Fl::handle(FL_SHORTCUT, driver->widget->top_window());
    return;
  }
  if (aSelector == @selector(insertTab:)) { // tab
    Fl::e_keysym = FL_Tab;
    Fl::handle(FL_KEYBOARD, driver->widget->window());
    return;
  }
  if (aSelector == @selector(insertBacktab:)) { // shift+tab
    Fl::e_keysym = FL_Tab;
    Fl::e_state = FL_SHIFT;
    Fl::handle(FL_KEYBOARD, driver->widget->window());
    return;
  }
  [super doCommandBySelector:aSelector];
}

@end

@interface FLTextDelegate : NSObject <NSTextDelegate, NSTextViewDelegate> {
  NSScrollView *scroll_view;
}
- (FLTextDelegate*)initWithScroll:(NSScrollView*)s;
- (void)textDidChange:(NSNotification *)notification;
- (void)textViewDidChangeSelection:(NSNotification *)notification;
@end


@implementation FLTextDelegate
- (FLTextDelegate*)initWithScroll:(NSScrollView*)s {
  self = [super init];
  if (self) {
    scroll_view = s;
  }
  return self;
}

- (void)textDidChange:(NSNotification *)notification {
  static BOOL busy = NO;
  if (busy) return;
  FLNativeTextView *text_view = (FLNativeTextView*)[notification object];
  if (text_view->driver->kind == Fl_Text_Widget_Driver::SINGLE_LINE) {
    NSLayoutManager *lom = [text_view layoutManager];
    NSUInteger gi = [lom glyphIndexForCharacterAtIndex:0];
    NSPoint pt = [lom locationForGlyphAtIndex:gi];
    CGRect fr = [scroll_view frame];
    if (pt.x + 20 > fr.size.width) { // long text
      fr.size.width = pt.x + 20;
    } else if (text_view->driver->widget->right_to_left()) { // short text
      busy = YES;
      [text_view makeBaseWritingDirectionRightToLeft:nil];
      busy = NO;
    } else return;
    [text_view setFrame:fr];
  }
  text_view->driver->widget->set_changed();
  if (text_view->driver->widget->when() & FL_WHEN_CHANGED)
    text_view->driver->widget->do_callback(FL_REASON_CHANGED);
}

- (void)textViewDidChangeSelection:(NSNotification *)notification {
  FLNativeTextView *text_view = (FLNativeTextView*)[notification object];
  text_view->driver->widget->take_focus();
}
@end


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


void Fl_Cocoa_Text_Widget_Driver::resize(int x, int y, int w, int h) {
  CGRect fr = CGRectMake(x, widget->window()->h()-(y+h), w, h);
  fr = NSInsetRect(fr, BORDER_WIDTH, BORDER_WIDTH);
  int ns = widget->top_window()->screen_num();
  float s = Fl::screen_scale(ns);
  fr = CGRectMake(fr.origin.x * s, fr.origin.y * s, fr.size.width * s, fr.size.height * s);
  [scroll_view setFrame:fr];
  textfont(widget->textfont());
  if (kind == SINGLE_LINE) {
    [text_view didChangeText];
  } else {
    [text_view setFrame:fr];
    [text_view scrollRangeToVisible:[text_view selectedRange]];
  }
}


void Fl_Cocoa_Text_Widget_Driver::show_widget() {
  NSWindow *flwin = (NSWindow*)fl_mac_xid(widget->window());
  if (!flwin) return;
  if (!scroll_view) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    CGRect fr = CGRectMake(widget->x(), widget->window()->h()-(widget->y()+widget->h()), widget->w(), widget->h());
    fr = NSInsetRect(fr, BORDER_WIDTH, BORDER_WIDTH);
    int ns = widget->top_window()->screen_num();
    float s = Fl::screen_scale(ns);
    fr = CGRectMake(fr.origin.x * s, fr.origin.y * s, fr.size.width * s, fr.size.height * s);
    scroll_view = [[NSScrollView alloc] initWithFrame:fr];
    [[flwin contentView] addSubview:scroll_view];
    if ([flwin parentWindow]) [flwin setIgnoresMouseEvents:NO];
    [scroll_view release];
    text_view = [[FLNativeTextView alloc] initWithFrame:fr];
    [scroll_view setDocumentView:text_view];
    [text_view release];
    [(NSText*)text_view setDelegate:[[FLTextDelegate alloc] initWithScroll:scroll_view]];
    text_view->driver = this;
    if (kind != Fl_Text_Widget_Driver::SINGLE_LINE && widget->right_to_left())
      [text_view makeBaseWritingDirectionRightToLeft:nil];
    [text_view setAllowsDocumentBackgroundColorChange:YES];
    uchar r, g, b;
    Fl::get_color(widget->color(), r, g, b);
    [text_view setBackgroundColor:[NSColor colorWithRed:r/255. green:g/255. blue:b/255. alpha:1.]];
    Fl::get_color(widget->textcolor(), r, g, b);
    [text_view setTextColor:[NSColor colorWithRed:r/255. green:g/255. blue:b/255. alpha:1.]];
    Fl::get_color(widget->cursor_color(), r, g, b);
    [text_view setInsertionPointColor:[NSColor colorWithRed:r/255. green:g/255. blue:b/255. alpha:1.]];
    widget->textsize(widget->textsize());
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
      (kind == Fl_Text_Widget_Driver::SINGLE_LINE || !widget->wrap())];
    [scroll_view setScrollerStyle:NSScrollerStyleOverlay];
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    NSParagraphStyle *start = [NSParagraphStyle defaultParagraphStyle];
    [style setParagraphStyle:start];
    [style setLineBreakMode:
       (kind == Fl_Text_Widget_Driver::SINGLE_LINE || !widget->wrap() ? NSLineBreakByClipping
        : NSLineBreakByWordWrapping)];
    [text_view setDefaultParagraphStyle:style];
    if (text_before_show) {
      [text_view setString:text_before_show];
      [text_view didChangeText];
      [text_view scrollRangeToVisible:NSMakeRange([text_before_show length], 0)];
      [text_before_show release];
      text_before_show = nil;
    }
    [pool release];
  } else if ([scroll_view isHidden]) {
    [scroll_view setHidden:NO];
  }
}


void Fl_Cocoa_Text_Widget_Driver::hide_widget() {
  [scroll_view setHidden:YES];
}


void Fl_Cocoa_Text_Widget_Driver::textfont(Fl_Font f) {
  if (!text_view) return;
  extern Fl_Fontdesc* fl_fonts;
  if (!fl_fonts) fl_fonts = Fl_Graphics_Driver::default_driver().calc_fl_fonts();
  NSString *name = [NSString stringWithUTF8String:(fl_fonts + f)->name];
  int ns = widget->top_window()->screen_num();
  float s = Fl::screen_scale(ns);
  NSFont *nsfont = [NSFont fontWithName:name size:widget->textsize() * s];
  [text_view setFont:nsfont];
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
    [text_view scrollRangeToVisible:NSMakeRange([text_before_show length], 0)];
    [text_before_show release];
    text_before_show = nil;
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
}


void Fl_Cocoa_Text_Widget_Driver::copy() {
  if (text_view->driver->kind == Fl_Text_Widget_Driver::MULTIPLE_LINES) {
    [text_view writeSelectionToPasteboard:[NSPasteboard generalPasteboard]
                              types:[text_view writablePasteboardTypes]];
  } else {
    NSRange r = [text_view selectedRange];
    if (!r.length) return;
    NSString *sub = [[text_view string] substringWithRange:r];
    // When single-line, replace "␍" by newline and "^I" by tab.
    // Also remove U+202b and U+202c characters
    char utf8[4];
    fl_utf8encode(0x202c, utf8); utf8[3] = 0;
    NSString *pdf = [NSString stringWithUTF8String:utf8];
    fl_utf8encode(0x202b, utf8); utf8[3] = 0;
    NSString *rle = [NSString stringWithUTF8String:utf8];
    sub = [sub stringByReplacingOccurrencesOfString:pdf withString:@""];
    sub = [sub stringByReplacingOccurrencesOfString:rle withString:@""];
    sub = [sub stringByReplacingOccurrencesOfString:@"␍" withString:@"\n"];
    sub = [sub stringByReplacingOccurrencesOfString:@"^I" withString:@"\t"];
    NSPasteboard *clip = [NSPasteboard generalPasteboard];
    [clip declareTypes:[NSArray arrayWithObject:@"public.utf8-plain-text"] owner:nil];
    [clip setString:sub forType:@"public.utf8-plain-text"];
  }
}


void Fl_Cocoa_Text_Widget_Driver::paste() {
  if (text_view->driver->kind == Fl_Text_Widget_Driver::MULTIPLE_LINES) {
    [text_view pasteAsPlainText:nil];
  } else {
    // When single-line, replace newlines by "␍" and tabs by "^I" in pasted text
    NSPasteboard *clip = [NSPasteboard generalPasteboard];
    NSString *found = [clip availableTypeFromArray:[NSArray arrayWithObjects:@"public.utf8-plain-text", @"public.utf16-plain-text", @"com.apple.traditional-mac-plain-text", nil]];
    if (found) {
      NSString *s = [clip stringForType:found];
      s = [s stringByReplacingOccurrencesOfString:@"\n" withString:@"␍"];
      s = [s stringByReplacingOccurrencesOfString:@"\t" withString:@"^I"];
      NSRange r = [text_view selectedRange];
      [text_view insertText:s replacementRange:r];
    }
  }
}
