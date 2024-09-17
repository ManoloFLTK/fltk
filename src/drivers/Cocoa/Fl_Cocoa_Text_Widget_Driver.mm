//
//  Fl_Cocoa_Text_Widget_Driver.mm
//

#include <config.h> // for BORDER_WIDTH
#include "../../Fl_Text_Widget_Driver.H"
#include <FL/platform.H>
#include <FL/Fl_Native_Text_Widget.H>
#include <FL/Fl_Graphics_Driver.H>
#import <Cocoa/Cocoa.h>

@class FLTextView2;

class Fl_Cocoa_Text_Widget_Driver : public Fl_Text_Widget_Driver {
public:
  FLTextView2 *text_view;
  NSScrollView *scroll_view;
  NSString *text_before_show;
  Fl_Cocoa_Text_Widget_Driver();
  ~Fl_Cocoa_Text_Widget_Driver();
  void show_widget() FL_OVERRIDE;
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
};


@interface FLTextView2 : NSTextView {
  @public
  Fl_Cocoa_Text_Widget_Driver *driver;
}
- (void)doCommandBySelector:(SEL)aSelector;
- (void)insertText:(id)aString replacementRange:(NSRange)r;
@end

@implementation FLTextView2
- (void)doCommandBySelector:(SEL)aSelector {
  NSString *s = [[NSApp currentEvent] characters];
  if ([s isEqualTo:@"c"]) { // cmd-C to copy selection
    [self writeSelectionToPasteboard:[NSPasteboard generalPasteboard]
                                types:[self writablePasteboardTypes]];
    return;
  }
  if ([s isEqualTo:@"x"] && !driver->widget->readonly()) { // cmd-X to cut selection
    [self writeSelectionToPasteboard:[NSPasteboard generalPasteboard]
                                types:[self writablePasteboardTypes]];
    driver->widget->cut();
    return;
  }
  if ([s isEqualTo:@"v"]) { // cmd-V to paste
    [self pasteAsPlainText:self];
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
    Fl::handle(FL_SHORTCUT, driver->widget->window());
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

- (void)insertText:(id)aString replacementRange:(NSRange)r {
  if ([aString length] && [aString isKindOfClass:[NSString class]]) {
    unichar u = [aString characterAtIndex:0];
    NSRange s_r = [self selectedRange];
    s_r.length = 0;
    if (u == 0x1c || u == 0x1d) {
      NSParagraphStyle *style = [[self typingAttributes] valueForKey:@"NSParagraphStyle"];
      NSString *s = [self string];
      if (s_r.location >= [s length]) s_r.location--;
      NSRange composed = [s rangeOfComposedCharacterSequenceAtIndex:s_r.location];
      if (u == 0x1c) { // move char before
        s_r.location += (driver->rtl ? +composed.length : -1);
      } else { // move char after
        s_r.location += (driver->rtl? (s_r.location>0?-1:0) : +composed.length);
      }
    } else if (u == 0x1) { // Home
      s_r.location = 0;
    } else if (u == 0x4) { // End
      s_r.location = [[self string] length];
    } else if (u == 0x1f || u == 0x1e) { // down or up arrow
      NSLayoutManager *lom = [self layoutManager];
      NSUInteger gi = [lom glyphIndexForCharacterAtIndex:s_r.location];
      NSPoint pt = [lom locationForGlyphAtIndex:gi];
      float s = Fl::screen_scale( driver->widget->window()->screen_num() );
      pt.y += driver->widget->textsize() * (u == 0x1f /* down */ ? s : -s);
      NSRect rect = [lom lineFragmentRectForGlyphAtIndex:gi effectiveRange:NULL];
      pt.x += rect.origin.x;
      pt.y += rect.origin.y;
      if (pt.y <= 0) return; // stop at 1st line
      gi = [lom glyphIndexForPoint:pt inTextContainer:[self textContainer]
            fractionOfDistanceThroughGlyph:NULL];
      s_r = [lom characterRangeForGlyphRange:NSMakeRange(gi, 1) actualGlyphRange:NULL];
      s_r.length = 0;
    } else if (u == 0x7f) { // delete forward
      NSString *s = [self string];
      if (s_r.location < [s length]) {
        NSRange composed = [s rangeOfComposedCharacterSequenceAtIndex:s_r.location];
        [super insertText:@"" replacementRange:composed];
      }
      return;
    } else goto way_out;
    [self setSelectedRange:s_r];
    [self scrollRangeToVisible:s_r];
    return;
  }
way_out:
  [super insertText:aString replacementRange:r];
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
  FLTextView2 *text_view = (FLTextView2*)[notification object];
  if (text_view->driver->widget->kind() != Fl_Native_Text_Widget::SINGLE_LINE) return;
  NSLayoutManager *lom = [text_view layoutManager];
  NSUInteger gi = [lom glyphIndexForCharacterAtIndex:0];
  NSPoint pt = [lom locationForGlyphAtIndex:gi];
//NSLog(@"scroll_view width=%.0f text_view width=%.0f new=%.0f", [scroll_view frame].size.width , [text_view frame].size.width, pt.x + 20);
  if (pt.x + 20 > [text_view frame].size.width) { // long text
    CGRect fr = [scroll_view frame];
    fr.size.width = pt.x + 20;
    [text_view setFrame:fr];
  } else if (text_view->driver->rtl) { // short text
    busy = YES;
    [text_view makeBaseWritingDirectionRightToLeft:nil];
    busy = NO;
  }
}

- (void)textViewDidChangeSelection:(NSNotification *)notification {
  FLTextView2 *text_view = (FLTextView2*)[notification object];
  text_view->driver->widget->take_focus();
}
@end


Fl_Text_Widget_Driver *Fl_Text_Widget_Driver::newTextWidgetDriver() {
  return (Fl_Text_Widget_Driver*)new Fl_Cocoa_Text_Widget_Driver();
}


Fl_Cocoa_Text_Widget_Driver::Fl_Cocoa_Text_Widget_Driver() : Fl_Text_Widget_Driver() {
  text_view = NULL;
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
}


void Fl_Cocoa_Text_Widget_Driver::show_widget() {
  NSView *view = [fl_mac_xid(widget->window()) contentView];
  if (view && !text_view) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    CGRect fr = CGRectMake(widget->x(), widget->window()->h()-(widget->y()+widget->h()), widget->w(), widget->h());
    fr = NSInsetRect(fr, BORDER_WIDTH, BORDER_WIDTH);
    int ns = widget->top_window()->screen_num();
    float s = Fl::screen_scale(ns);
    fr = CGRectMake(fr.origin.x * s, fr.origin.y * s, fr.size.width * s, fr.size.height * s);
    scroll_view = [[NSScrollView alloc] initWithFrame:fr];
    [view addSubview:scroll_view];
    [scroll_view release];
    text_view = [[FLTextView2 alloc] initWithFrame:fr];
    [scroll_view setDocumentView:text_view];
    [text_view release];
    [(NSText*)text_view setDelegate:[[FLTextDelegate alloc] initWithScroll:scroll_view]];
    text_view->driver = this;
    if (widget->kind() != Fl_Native_Text_Widget::SINGLE_LINE && rtl)
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
    if (widget->kind() == Fl_Native_Text_Widget::MULTIPLE_LINES) [scroll_view setHasVerticalScroller:YES];
    [scroll_view setHasHorizontalScroller:YES];
    [scroll_view setScrollerStyle:NSScrollerStyleOverlay];
    if (!widget->readonly()) {
      [[view window] makeFirstResponder:text_view];
      [text_view setAllowsUndo:YES];
    }
    if (widget->kind() == Fl_Native_Text_Widget::SINGLE_LINE || !rtl) {
      NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
      NSParagraphStyle *start = [NSParagraphStyle defaultParagraphStyle];
      [style setParagraphStyle:start];
      [style setLineBreakMode:NSLineBreakByClipping];
      [text_view setDefaultParagraphStyle:style];
    }
    if (text_before_show) {
      [text_view setString:text_before_show];
      [text_view didChangeText];
      [text_view scrollRangeToVisible:NSMakeRange([text_before_show length], 0)];
      [text_before_show release];
      text_before_show = nil;
    }
    [pool release];
  }
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
      [text_view setAllowsUndo:YES];
    }
  }
}


void Fl_Cocoa_Text_Widget_Driver::unfocus() {
  if ([[text_view window] firstResponder] == text_view) {
    [[text_view window] makeFirstResponder:nil];
  }
}
