#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include "platform/platform_backend.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

class MacOSWindowImpl;

lotui::KeyModifiers keyModifiers(NSEvent* event) {
    const NSEventModifierFlags flags = event.modifierFlags;
    return {
        (flags & NSEventModifierFlagShift) != 0,
        (flags & NSEventModifierFlagControl) != 0,
        (flags & NSEventModifierFlagOption) != 0,
        (flags & NSEventModifierFlagCommand) != 0,
    };
}

lotui::KeyCode keyCode(NSEvent* event) {
    NSString* characters = event.charactersIgnoringModifiers;
    if (characters.length == 0) {
        return lotui::KeyCode::Unknown;
    }
    switch ([characters characterAtIndex:0]) {
    case '\t': return lotui::KeyCode::Tab;
    case '\r':
    case '\n': return lotui::KeyCode::Enter;
    case ' ': return lotui::KeyCode::Space;
    case 0x1B: return lotui::KeyCode::Escape;
    case 0x7F: return lotui::KeyCode::Backspace;
    case NSDeleteFunctionKey: return lotui::KeyCode::Delete;
    case NSLeftArrowFunctionKey: return lotui::KeyCode::Left;
    case NSRightArrowFunctionKey: return lotui::KeyCode::Right;
    case NSUpArrowFunctionKey: return lotui::KeyCode::Up;
    case NSDownArrowFunctionKey: return lotui::KeyCode::Down;
    case NSHomeFunctionKey: return lotui::KeyCode::Home;
    case NSEndFunctionKey: return lotui::KeyCode::End;
    case NSPageUpFunctionKey: return lotui::KeyCode::PageUp;
    case NSPageDownFunctionKey: return lotui::KeyCode::PageDown;
    default: return lotui::KeyCode::Unknown;
    }
}

@interface LotUIProbeView : NSView <NSTextInputClient> {
@public
    MacOSWindowImpl* owner;
    NSUInteger markedTextLength;
    NSRange markedSelection;
}
- (void)pushMousePosition:(NSEvent*)event;
- (void)pushMouseButton:(NSEvent*)event
                  button:(lotui::PointerButton)button
                 pressed:(BOOL)pressed;
- (void)scrollWheel:(NSEvent*)event;
@end

@interface LotUIWindowDelegate : NSObject <NSWindowDelegate> {
@public
    MacOSWindowImpl* owner;
}
@end

class MacOSWindowImpl final : public lotui::PlatformWindow {
public:
    explicit MacOSWindowImpl(const lotui::WindowOptions& options);
    ~MacOSWindowImpl() override;

    void show() override;
    bool pollEvent(lotui::PlatformEvent& event) override;
    bool setPointerCapture(bool enabled) override;
    void setTextInputState(
        const lotui::TextInputState& state) override;
    lotui::WindowMetrics metrics() const override;
    lotui::NativeWindowHandle nativeHandle() const override;

    void pushCloseRequested();
    void pushResize();
    void pushMouse(float x, float y);
    void pushMouseButton(
        float x,
        float y,
        lotui::PointerButton button,
        bool pressed);
    void pushScroll(
        float x,
        float y,
        float deltaX,
        float deltaY,
        lotui::ScrollDeltaMode mode);
    void pushKey(
        lotui::KeyCode key,
        lotui::KeyModifiers modifiers,
        bool pressed,
        bool repeat);
    void pushFocus(bool focused);
    void pushDpiChanged();
    void pushTextInput(NSString* text);
    void pushComposition(NSString* text, NSRange selection);
    void pushCompositionEnd();
    bool textInputEnabled() const noexcept;
    NSRect textInputScreenRect() const;

private:
    void updateMetalLayer();

    NSWindow* window_{nil};
    LotUIProbeView* view_{nil};
    CAMetalLayer* metalLayer_{nil};
    LotUIWindowDelegate* delegate_{nil};
    lotui::WindowMetrics metrics_{};
    std::deque<lotui::PlatformEvent> events_;
    bool closeRequested_{false};
    bool pointerCaptured_{false};
    lotui::TextInputState textInputState_{};
};

@implementation LotUIProbeView

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self != nil) {
        markedTextLength = 0;
        markedSelection = NSMakeRange(0, 0);
    }
    return self;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    (void)event;
    return YES;
}

- (void)pushMousePosition:(NSEvent*)event {
    if (owner == nullptr) {
        return;
    }
    const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    owner->pushMouse(
        static_cast<float>(point.x),
        static_cast<float>(self.bounds.size.height - point.y));
}

- (void)pushMouseButton:(NSEvent*)event
                  button:(lotui::PointerButton)button
                 pressed:(BOOL)pressed {
    if (owner == nullptr) {
        return;
    }
    const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    owner->pushMouseButton(
        static_cast<float>(point.x),
        static_cast<float>(self.bounds.size.height - point.y),
        button,
        pressed == YES);
}

- (void)mouseMoved:(NSEvent*)event {
    [self pushMousePosition:event];
}

- (void)mouseDragged:(NSEvent*)event {
    [self pushMousePosition:event];
}

- (void)rightMouseDragged:(NSEvent*)event {
    [self pushMousePosition:event];
}

- (void)otherMouseDragged:(NSEvent*)event {
    [self pushMousePosition:event];
}

- (void)mouseDown:(NSEvent*)event {
    [self pushMouseButton:event
                   button:lotui::PointerButton::Primary
                  pressed:YES];
}

- (void)mouseUp:(NSEvent*)event {
    [self pushMouseButton:event
                   button:lotui::PointerButton::Primary
                  pressed:NO];
}

- (void)rightMouseDown:(NSEvent*)event {
    [self pushMouseButton:event
                   button:lotui::PointerButton::Secondary
                  pressed:YES];
}

- (void)rightMouseUp:(NSEvent*)event {
    [self pushMouseButton:event
                   button:lotui::PointerButton::Secondary
                  pressed:NO];
}

- (void)otherMouseDown:(NSEvent*)event {
    const auto button = event.buttonNumber == 2
        ? lotui::PointerButton::Middle
        : lotui::PointerButton::Auxiliary1;
    [self pushMouseButton:event button:button pressed:YES];
}

- (void)otherMouseUp:(NSEvent*)event {
    const auto button = event.buttonNumber == 2
        ? lotui::PointerButton::Middle
        : lotui::PointerButton::Auxiliary1;
    [self pushMouseButton:event button:button pressed:NO];
}

- (void)scrollWheel:(NSEvent*)event {
    if (owner == nullptr) {
        return;
    }
    const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    owner->pushScroll(
        static_cast<float>(point.x),
        static_cast<float>(self.bounds.size.height - point.y),
        -static_cast<float>(event.scrollingDeltaX),
        -static_cast<float>(event.scrollingDeltaY),
        event.hasPreciseScrollingDeltas
            ? lotui::ScrollDeltaMode::Pixel
            : lotui::ScrollDeltaMode::Line);
}

- (void)keyDown:(NSEvent*)event {
    if (owner != nullptr) {
        owner->pushKey(
            keyCode(event),
            keyModifiers(event),
            true,
            event.isARepeat == YES);
        if (owner->textInputEnabled()) {
            [self interpretKeyEvents:@[event]];
        }
    }
}

- (BOOL)hasMarkedText {
    return markedTextLength > 0;
}

- (NSRange)markedRange {
    return markedTextLength > 0
        ? NSMakeRange(0, markedTextLength)
        : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange {
    return markedSelection;
}

- (void)setMarkedText:(id)value
         selectedRange:(NSRange)selection
       replacementRange:(NSRange)replacement {
    (void)replacement;
    NSString* text = [value isKindOfClass:[NSAttributedString class]]
        ? [(NSAttributedString*)value string]
        : (NSString*)value;
    markedTextLength = text.length;
    markedSelection = selection;
    if (owner != nullptr) {
        owner->pushComposition(text, selection);
    }
}

- (void)unmarkText {
    const BOOL hadMarkedText = markedTextLength > 0;
    markedTextLength = 0;
    markedSelection = NSMakeRange(0, 0);
    if (hadMarkedText && owner != nullptr) {
        owner->pushCompositionEnd();
    }
}

- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText {
    return @[];
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)actual {
    (void)range;
    if (actual != nullptr) {
        *actual = NSMakeRange(NSNotFound, 0);
    }
    return nil;
}

- (void)insertText:(id)value replacementRange:(NSRange)replacement {
    (void)replacement;
    NSString* text = [value isKindOfClass:[NSAttributedString class]]
        ? [(NSAttributedString*)value string]
        : (NSString*)value;
    markedTextLength = 0;
    markedSelection = NSMakeRange(0, 0);
    if (owner != nullptr) {
        owner->pushTextInput(text);
        owner->pushCompositionEnd();
    }
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    (void)point;
    return NSNotFound;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
                          actualRange:(NSRangePointer)actual {
    if (actual != nullptr) {
        *actual = range;
    }
    return owner != nullptr ? owner->textInputScreenRect() : NSZeroRect;
}

- (void)doCommandBySelector:(SEL)selector {
    (void)selector;
}

- (void)keyUp:(NSEvent*)event {
    if (owner != nullptr) {
        owner->pushKey(
            keyCode(event),
            keyModifiers(event),
            false,
            false);
    }
}

@end

@implementation LotUIWindowDelegate

- (BOOL)windowShouldClose:(NSWindow*)sender {
    if (owner != nullptr) {
        owner->pushCloseRequested();
    }
    [sender orderOut:nil];
    return NO;
}

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (owner != nullptr) {
        owner->pushResize();
    }
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    (void)notification;
    if (owner != nullptr) {
        owner->pushFocus(true);
    }
}

- (void)windowDidResignKey:(NSNotification*)notification {
    (void)notification;
    if (owner != nullptr) {
        owner->pushFocus(false);
    }
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
    (void)notification;
    if (owner != nullptr) {
        owner->pushDpiChanged();
    }
}

@end

MacOSWindowImpl::MacOSWindowImpl(const lotui::WindowOptions& options) {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp finishLaunching];

    NSWindowStyleMask style =
        NSWindowStyleMaskTitled |
        NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable;
    if (options.resizable) {
        style |= NSWindowStyleMaskResizable;
    }

    window_ = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, options.width, options.height)
                  styleMask:style
                    backing:NSBackingStoreBuffered
                      defer:NO];
    if (window_ == nil) {
        throw std::runtime_error("failed to create the Cocoa window");
    }

    delegate_ = [[LotUIWindowDelegate alloc] init];
    delegate_->owner = this;
    window_.delegate = delegate_;
    window_.releasedWhenClosed = NO;
    window_.title = [NSString stringWithUTF8String:options.title.c_str()];
    [window_ center];
    [window_ setAcceptsMouseMovedEvents:YES];

    view_ = [[LotUIProbeView alloc] initWithFrame:window_.contentView.bounds];
    view_->owner = this;
    view_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    view_.wantsLayer = YES;
    metalLayer_ = [CAMetalLayer layer];
    view_.layer = metalLayer_;
    window_.contentView = view_;
    [window_ makeFirstResponder:view_];

    const NSRect content = window_.contentView.bounds;
    metrics_.width = static_cast<int>(content.size.width);
    metrics_.height = static_cast<int>(content.size.height);
    metrics_.dpiScale = static_cast<float>(window_.backingScaleFactor);
    updateMetalLayer();
}

MacOSWindowImpl::~MacOSWindowImpl() {
    if (view_ != nil) {
        view_->owner = nullptr;
    }
    if (delegate_ != nil) {
        delegate_->owner = nullptr;
    }
    if (window_ != nil) {
        window_.delegate = nil;
        [window_ orderOut:nil];
        [window_ close];
    }
    view_ = nil;
    metalLayer_ = nil;
    delegate_ = nil;
    window_ = nil;
}

void MacOSWindowImpl::show() {
    [window_ makeKeyAndOrderFront:nil];
    [window_ makeFirstResponder:view_];
    [NSApp activateIgnoringOtherApps:YES];
}

bool MacOSWindowImpl::pollEvent(lotui::PlatformEvent& event) {
    @autoreleasepool {
        while (events_.empty()) {
            NSEvent* nativeEvent = [NSApp
                nextEventMatchingMask:NSEventMaskAny
                            untilDate:[NSDate distantPast]
                               inMode:NSDefaultRunLoopMode
                              dequeue:YES];
            if (nativeEvent == nil) {
                break;
            }
            [NSApp sendEvent:nativeEvent];
        }
        [NSApp updateWindows];
    }

    if (events_.empty()) {
        return false;
    }
    event = events_.front();
    events_.pop_front();
    return true;
}

bool MacOSWindowImpl::setPointerCapture(bool enabled) {
    pointerCaptured_ = enabled;
    return true;
}

void MacOSWindowImpl::setTextInputState(
    const lotui::TextInputState& state) {
    const bool wasEnabled = textInputState_.enabled;
    textInputState_ = state;
    if (wasEnabled && !state.enabled && view_ != nil) {
        [view_ unmarkText];
    }
    if (state.enabled && window_ != nil && view_ != nil) {
        [window_ makeFirstResponder:view_];
        [[view_ inputContext] invalidateCharacterCoordinates];
    }
}

lotui::WindowMetrics MacOSWindowImpl::metrics() const {
    return metrics_;
}

lotui::NativeWindowHandle MacOSWindowImpl::nativeHandle() const {
    return {
        lotui::NativeWindowSystem::MetalLayer,
        nullptr,
        reinterpret_cast<std::uintptr_t>((__bridge void*)metalLayer_),
    };
}

void MacOSWindowImpl::pushCloseRequested() {
    if (!closeRequested_) {
        closeRequested_ = true;
        events_.push_back({lotui::PlatformEventType::CloseRequested});
    }
}

void MacOSWindowImpl::pushResize() {
    const NSRect content = window_.contentView.bounds;
    metrics_.width = static_cast<int>(content.size.width);
    metrics_.height = static_cast<int>(content.size.height);
    updateMetalLayer();
    lotui::PlatformEvent event{lotui::PlatformEventType::Resized};
    event.width = metrics_.width;
    event.height = metrics_.height;
    events_.push_back(event);
}

void MacOSWindowImpl::pushMouse(float x, float y) {
    lotui::PlatformEvent event{lotui::PlatformEventType::MouseMoved};
    event.x = x;
    event.y = y;
    events_.push_back(event);
}

void MacOSWindowImpl::pushMouseButton(
    float x,
    float y,
    lotui::PointerButton button,
    bool pressed) {
    lotui::PlatformEvent event{
        pressed
            ? lotui::PlatformEventType::MouseButtonPressed
            : lotui::PlatformEventType::MouseButtonReleased};
    event.x = x;
    event.y = y;
    event.button = button;
    events_.push_back(event);
}

void MacOSWindowImpl::pushScroll(
    float x,
    float y,
    float deltaX,
    float deltaY,
    lotui::ScrollDeltaMode mode) {
    lotui::PlatformEvent event{lotui::PlatformEventType::MouseWheel};
    event.x = x;
    event.y = y;
    event.scrollX = deltaX;
    event.scrollY = deltaY;
    event.scrollMode = mode;
    events_.push_back(event);
}

void MacOSWindowImpl::pushKey(
    lotui::KeyCode key,
    lotui::KeyModifiers modifiers,
    bool pressed,
    bool repeat) {
    lotui::PlatformEvent event{
        pressed
            ? lotui::PlatformEventType::KeyPressed
            : lotui::PlatformEventType::KeyReleased};
    event.key = key;
    event.modifiers = modifiers;
    event.repeat = repeat;
    events_.push_back(event);
}

void MacOSWindowImpl::pushFocus(bool focused) {
    metrics_.focused = focused;
    events_.push_back({
        focused
            ? lotui::PlatformEventType::FocusGained
            : lotui::PlatformEventType::FocusLost});
}

void MacOSWindowImpl::pushDpiChanged() {
    metrics_.dpiScale = static_cast<float>(window_.backingScaleFactor);
    updateMetalLayer();
    lotui::PlatformEvent event{lotui::PlatformEventType::DpiChanged};
    event.dpiScale = metrics_.dpiScale;
    events_.push_back(event);
}

void MacOSWindowImpl::pushTextInput(NSString* text) {
    if (!textInputState_.enabled || text == nil || text.length == 0) {
        return;
    }
    lotui::PlatformEvent event{lotui::PlatformEventType::TextInput};
    event.text = text.UTF8String == nullptr ? "" : text.UTF8String;
    if (!event.text.empty()) {
        events_.push_back(std::move(event));
    }
}

void MacOSWindowImpl::pushComposition(
    NSString* text,
    NSRange selection) {
    if (!textInputState_.enabled || text == nil) {
        return;
    }
    const NSUInteger selectionLocation = std::min(
        selection.location == NSNotFound ? text.length : selection.location,
        text.length);
    const NSUInteger selectionEnd = std::min(
        selectionLocation + selection.length, text.length);
    NSString* prefix = [text substringToIndex:selectionLocation];
    NSString* selected = [text substringWithRange:NSMakeRange(
        selectionLocation, selectionEnd - selectionLocation)];

    lotui::PlatformEvent event{lotui::PlatformEventType::TextComposition};
    event.text = text.UTF8String == nullptr ? "" : text.UTF8String;
    event.selectionStart = prefix.UTF8String == nullptr
        ? 0
        : std::char_traits<char>::length(prefix.UTF8String);
    event.selectionLength = selected.UTF8String == nullptr
        ? 0
        : std::char_traits<char>::length(selected.UTF8String);
    events_.push_back(std::move(event));
}

void MacOSWindowImpl::pushCompositionEnd() {
    if (textInputState_.enabled) {
        events_.push_back({lotui::PlatformEventType::TextCompositionEnd});
    }
}

bool MacOSWindowImpl::textInputEnabled() const noexcept {
    return textInputState_.enabled;
}

NSRect MacOSWindowImpl::textInputScreenRect() const {
    if (window_ == nil || view_ == nil) {
        return NSZeroRect;
    }
    const lotui::Rect& logical = textInputState_.inputRect;
    const NSRect viewRect = NSMakeRect(
        logical.x,
        view_.bounds.size.height - logical.y - logical.height,
        std::max(1.0F, logical.width),
        std::max(1.0F, logical.height));
    const NSRect windowRect = [view_ convertRect:viewRect toView:nil];
    return [window_ convertRectToScreen:windowRect];
}

void MacOSWindowImpl::updateMetalLayer() {
    if (metalLayer_ == nil || view_ == nil) {
        return;
    }

    const CGFloat scale = window_ != nil
        ? window_.backingScaleFactor
        : NSScreen.mainScreen.backingScaleFactor;
    metalLayer_.contentsScale = scale;
    metalLayer_.frame = view_.bounds;
    metalLayer_.drawableSize = CGSizeMake(
        view_.bounds.size.width * scale,
        view_.bounds.size.height * scale);

    metrics_.dpiScale = static_cast<float>(scale);
    metrics_.framebufferWidth =
        static_cast<int>(metalLayer_.drawableSize.width);
    metrics_.framebufferHeight =
        static_cast<int>(metalLayer_.drawableSize.height);
}

namespace lotui {
namespace {

class MacOSPlatformBackend final : public PlatformBackend {
public:
    std::unique_ptr<PlatformWindow> createWindow(
        const WindowOptions& options) override {
        return std::make_unique<MacOSWindowImpl>(options);
    }
};

} // namespace

std::unique_ptr<PlatformBackend> createPlatformBackend() {
    return std::make_unique<MacOSPlatformBackend>();
}

} // namespace lotui
