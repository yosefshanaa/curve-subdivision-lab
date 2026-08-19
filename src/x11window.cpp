#include "x11window.h"

#include <dlfcn.h>
#include <sys/select.h>

#include <cstdlib>
#include <cstring>
#include <vector>

namespace sl {
namespace {

// ---- Xlib constants (from X.h / Xlib.h; part of the frozen X11 ABI)
constexpr long kKeyPressMask        = 1L << 0;
constexpr long kButtonPressMask     = 1L << 2;
constexpr long kButtonReleaseMask   = 1L << 3;
constexpr long kPointerMotionMask   = 1L << 6;
constexpr long kExposureMask        = 1L << 15;
constexpr long kStructureNotifyMask = 1L << 17;

constexpr int kKeyPress        = 2;
constexpr int kButtonPress     = 4;
constexpr int kButtonRelease   = 5;
constexpr int kMotionNotify    = 6;
constexpr int kExpose          = 12;
constexpr int kConfigureNotify = 22;
constexpr int kClientMessage   = 33;

constexpr int kZPixmap  = 2;
constexpr int kLSBFirst = 0;

// ---- Public Xlib structs, declared here rather than included.
struct XKeyEventX {
    int type;
    unsigned long serial;
    int send_event;
    void* display;
    unsigned long window, root, subwindow, time;
    int x, y, x_root, y_root;
    unsigned int state, keycode;
    int same_screen;
};

struct XButtonEventX {
    int type;
    unsigned long serial;
    int send_event;
    void* display;
    unsigned long window, root, subwindow, time;
    int x, y, x_root, y_root;
    unsigned int state, button;
    int same_screen;
};

struct XMotionEventX {
    int type;
    unsigned long serial;
    int send_event;
    void* display;
    unsigned long window, root, subwindow, time;
    int x, y, x_root, y_root;
    unsigned int state;
    char is_hint;
    int same_screen;
};

struct XConfigureEventX {
    int type;
    unsigned long serial;
    int send_event;
    void* display;
    unsigned long event, window;
    int x, y, width, height, border_width;
    unsigned long above;
    int override_redirect;
};

struct XClientMessageEventX {
    int type;
    unsigned long serial;
    int send_event;
    void* display;
    unsigned long window, message_type;
    int format;
    union {
        char b[20];
        short s[10];
        long l[5];
    } data;
};

// sizeof(XEvent) is 24 longs on LP64; use a raw buffer and reinterpret.
struct XEventBuf {
    alignas(8) unsigned char raw[192];
    int type() const { return *reinterpret_cast<const int*>(raw); }
};

// XImage as declared in Xlib.h. Filling it in by hand and calling XInitImage
// avoids XCreateImage/XDestroyImage entirely, so the pixel buffer stays ours.
struct XImageX {
    int width, height;
    int xoffset;
    int format;
    char* data;
    int byte_order;
    int bitmap_unit;
    int bitmap_bit_order;
    int bitmap_pad;
    int depth;
    int bytes_per_line;
    int bits_per_pixel;
    unsigned long red_mask, green_mask, blue_mask;
    void* obdata;
    struct {
        void* create_image;
        void* destroy_image;
        void* get_pixel;
        void* put_pixel;
        void* sub_image;
        void* add_pixel;
    } f;
};

struct Xlib {
    void* (*XOpenDisplay)(const char*) = nullptr;
    int (*XCloseDisplay)(void*) = nullptr;
    int (*XDefaultScreen)(void*) = nullptr;
    void* (*XDefaultVisual)(void*, int) = nullptr;
    int (*XDefaultDepth)(void*, int) = nullptr;
    unsigned long (*XRootWindow)(void*, int) = nullptr;
    unsigned long (*XBlackPixel)(void*, int) = nullptr;
    unsigned long (*XCreateSimpleWindow)(void*, unsigned long, int, int, unsigned, unsigned,
                                         unsigned, unsigned long, unsigned long) = nullptr;
    int (*XDestroyWindow)(void*, unsigned long) = nullptr;
    int (*XStoreName)(void*, unsigned long, const char*) = nullptr;
    int (*XSelectInput)(void*, unsigned long, long) = nullptr;
    int (*XMapWindow)(void*, unsigned long) = nullptr;
    void* (*XCreateGC)(void*, unsigned long, unsigned long, void*) = nullptr;
    int (*XFreeGC)(void*, void*) = nullptr;
    int (*XPutImage)(void*, unsigned long, void*, void*, int, int, int, int, unsigned,
                     unsigned) = nullptr;
    int (*XInitImage)(void*) = nullptr;
    int (*XNextEvent)(void*, void*) = nullptr;
    int (*XPending)(void*) = nullptr;
    int (*XFlush)(void*) = nullptr;
    int (*XSync)(void*, int) = nullptr;
    unsigned long (*XInternAtom)(void*, const char*, int) = nullptr;
    int (*XSetWMProtocols)(void*, unsigned long, unsigned long*, int) = nullptr;
    int (*XLookupString)(void*, char*, int, unsigned long*, void*) = nullptr;
    int (*XConnectionNumber)(void*) = nullptr;
    unsigned long (*XkbKeycodeToKeysym)(void*, unsigned int, int, int) = nullptr;
};

Xlib g;

}  // namespace

struct X11Window::Image {
    XImageX x{};
    std::vector<uint32_t> pixels;
};

X11Window::~X11Window() { close(); }

bool X11Window::loadXlib() {
    if (lib_) return true;
    lib_ = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
    if (!lib_) lib_ = dlopen("libX11.so", RTLD_LAZY | RTLD_LOCAL);
    if (!lib_) {
        error_ = "libX11.so.6 not found — run with --shot to render offscreen";
        return false;
    }
#define SL_SYM(name)                                                     \
    g.name = reinterpret_cast<decltype(g.name)>(dlsym(lib_, #name));     \
    if (!g.name) { error_ = std::string("missing X symbol: ") + #name; return false; }
    SL_SYM(XOpenDisplay) SL_SYM(XCloseDisplay) SL_SYM(XDefaultScreen)
    SL_SYM(XDefaultVisual) SL_SYM(XDefaultDepth) SL_SYM(XRootWindow) SL_SYM(XBlackPixel)
    SL_SYM(XCreateSimpleWindow) SL_SYM(XDestroyWindow) SL_SYM(XStoreName)
    SL_SYM(XSelectInput) SL_SYM(XMapWindow) SL_SYM(XCreateGC) SL_SYM(XFreeGC)
    SL_SYM(XPutImage) SL_SYM(XInitImage) SL_SYM(XNextEvent) SL_SYM(XPending)
    SL_SYM(XFlush) SL_SYM(XSync) SL_SYM(XInternAtom) SL_SYM(XSetWMProtocols)
    SL_SYM(XLookupString) SL_SYM(XConnectionNumber)
#undef SL_SYM
    // Optional: only used to read keysyms when XLookupString yields nothing.
    g.XkbKeycodeToKeysym =
        reinterpret_cast<decltype(g.XkbKeycodeToKeysym)>(dlsym(lib_, "XkbKeycodeToKeysym"));
    return true;
}

bool X11Window::open(int w, int h, const std::string& title) {
    if (!loadXlib()) return false;

    dpy_ = g.XOpenDisplay(nullptr);
    if (!dpy_) {
        error_ = "cannot open display (is DISPLAY set?) — use --shot to render offscreen";
        return false;
    }
    int screen = g.XDefaultScreen(dpy_);
    if (g.XDefaultDepth(dpy_, screen) < 24) {
        error_ = "need a 24-bit or deeper TrueColor visual";
        g.XCloseDisplay(dpy_);
        dpy_ = nullptr;
        return false;
    }

    w_ = w;
    h_ = h;
    win_ = g.XCreateSimpleWindow(dpy_, g.XRootWindow(dpy_, screen), 0, 0, unsigned(w),
                                 unsigned(h), 0, 0, 0x0D0D13);
    g.XStoreName(dpy_, win_, title.c_str());
    g.XSelectInput(dpy_, win_,
                   kKeyPressMask | kButtonPressMask | kButtonReleaseMask | kPointerMotionMask |
                       kExposureMask | kStructureNotifyMask);

    wmDelete_ = g.XInternAtom(dpy_, "WM_DELETE_WINDOW", 0);
    g.XSetWMProtocols(dpy_, win_, &wmDelete_, 1);

    g.XMapWindow(dpy_, win_);
    gc_ = g.XCreateGC(dpy_, win_, 0, nullptr);
    g.XFlush(dpy_);
    return true;
}

void X11Window::close() {
    if (dpy_) {
        if (gc_) g.XFreeGC(dpy_, gc_);
        if (win_) g.XDestroyWindow(dpy_, win_);
        g.XCloseDisplay(dpy_);
    }
    delete img_;
    img_ = nullptr;
    dpy_ = nullptr;
    win_ = 0;
    gc_ = nullptr;
}

void X11Window::setTitle(const std::string& t) {
    if (dpy_ && win_) g.XStoreName(dpy_, win_, t.c_str());
}

void X11Window::ensureImage(int w, int h) {
    if (img_ && img_->x.width == w && img_->x.height == h) return;
    if (!img_) img_ = new Image();
    img_->pixels.assign(size_t(w) * h, 0);
    XImageX& x = img_->x;
    std::memset(&x, 0, sizeof(x));
    x.width = w;
    x.height = h;
    x.format = kZPixmap;
    x.data = reinterpret_cast<char*>(img_->pixels.data());
    x.byte_order = kLSBFirst;
    x.bitmap_unit = 32;
    x.bitmap_bit_order = kLSBFirst;
    x.bitmap_pad = 32;
    x.depth = 24;
    x.bits_per_pixel = 32;
    x.bytes_per_line = w * 4;
    x.red_mask = 0x00FF0000;
    x.green_mask = 0x0000FF00;
    x.blue_mask = 0x000000FF;
    g.XInitImage(&x);
}

void X11Window::present(const Canvas& cv) {
    if (!dpy_ || !win_) return;
    ensureImage(cv.w, cv.h);
    std::memcpy(img_->pixels.data(), cv.px.data(), cv.px.size() * sizeof(uint32_t));
    g.XPutImage(dpy_, win_, gc_, &img_->x, 0, 0, 0, 0, unsigned(cv.w), unsigned(cv.h));
    g.XFlush(dpy_);
}

void X11Window::waitEvent(int timeoutMs) {
    if (!dpy_) return;
    if (g.XPending(dpy_) > 0) return;
    int fd = g.XConnectionNumber(dpy_);
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    select(fd + 1, &set, nullptr, nullptr, &tv);
}

bool X11Window::poll(WindowEvent& out) {
    out = WindowEvent{};
    if (!dpy_) return false;
    if (g.XPending(dpy_) <= 0) return false;

    XEventBuf ev;
    g.XNextEvent(dpy_, &ev);
    switch (ev.type()) {
        case kKeyPress: {
            auto* k = reinterpret_cast<XKeyEventX*>(ev.raw);
            char buf[8] = {0};
            unsigned long keysym = 0;
            int n = g.XLookupString(k, buf, sizeof(buf) - 1, &keysym, nullptr);
            out.type = WindowEvent::Key;
            if (n > 0) {
                out.key = static_cast<unsigned char>(buf[0]);
            } else {
                // Arrow keys and friends report no string; map the useful ones.
                switch (keysym) {
                    case 0xFF51: out.key = 'j'; break;   // Left
                    case 0xFF53: out.key = 'l'; break;   // Right
                    case 0xFF52: out.key = 'i'; break;   // Up
                    case 0xFF54: out.key = 'k'; break;   // Down
                    case 0xFF1B: out.key = 27; break;    // Escape
                    default: out.key = 0; break;
                }
            }
            out.x = k->x;
            out.y = k->y;
            return true;
        }
        case kButtonPress:
        case kButtonRelease: {
            auto* b = reinterpret_cast<XButtonEventX*>(ev.raw);
            out.x = b->x;
            out.y = b->y;
            if (b->button == 4 || b->button == 5) {
                if (ev.type() == kButtonRelease) return poll(out);   // wheel: press only
                out.type = WindowEvent::Wheel;
                out.dir = (b->button == 4) ? 1 : -1;
            } else {
                out.type = (ev.type() == kButtonPress) ? WindowEvent::MouseDown
                                                       : WindowEvent::MouseUp;
                out.button = int(b->button);
            }
            return true;
        }
        case kMotionNotify: {
            auto* m = reinterpret_cast<XMotionEventX*>(ev.raw);
            out.type = WindowEvent::MouseMove;
            out.x = m->x;
            out.y = m->y;
            out.button = (m->state & (1u << 8)) ? 1 : ((m->state & (1u << 10)) ? 3 : 0);
            return true;
        }
        case kConfigureNotify: {
            auto* c = reinterpret_cast<XConfigureEventX*>(ev.raw);
            if (c->width != w_ || c->height != h_) {
                w_ = c->width;
                h_ = c->height;
                out.type = WindowEvent::Resize;
                out.w = w_;
                out.h = h_;
                return true;
            }
            return poll(out);
        }
        case kExpose: {
            out.type = WindowEvent::Resize;   // treat as "redraw needed"
            out.w = w_;
            out.h = h_;
            return true;
        }
        case kClientMessage: {
            auto* c = reinterpret_cast<XClientMessageEventX*>(ev.raw);
            if (static_cast<unsigned long>(c->data.l[0]) == wmDelete_) {
                out.type = WindowEvent::Close;
                return true;
            }
            return poll(out);
        }
        default:
            return poll(out);
    }
}

}  // namespace sl
