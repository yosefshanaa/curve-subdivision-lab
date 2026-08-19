// x11window.h — a minimal X11 window with no build-time X dependency.
//
// Xlib is loaded at runtime with dlopen and the handful of entry points and
// public structs the program needs are declared here. That keeps the build to
// `g++ *.cpp -ldl` on any Linux, with no -dev packages to install, and the
// program still runs headless (screenshot mode) when there is no display.
#pragma once

#include <string>

#include "canvas.h"

namespace sl {

struct WindowEvent {
    enum Type { None, Key, MouseDown, MouseUp, MouseMove, Wheel, Resize, Close };
    Type type = None;
    int key = 0;        // ASCII for Key events
    int x = 0, y = 0;   // pointer position
    int button = 0;     // 1 = left, 2 = middle, 3 = right
    int dir = 0;        // wheel: +1 up, -1 down
    int w = 0, h = 0;   // resize
};

class X11Window {
public:
    ~X11Window();

    bool open(int w, int h, const std::string& title);
    void close();
    bool isOpen() const { return win_ != 0; }
    const char* error() const { return error_.c_str(); }

    void setTitle(const std::string& t);
    void present(const Canvas& cv);
    bool poll(WindowEvent& ev);   // non-blocking; false when the queue is empty
    // Block until an event arrives or the timeout expires. Keeps an idle window
    // at 0% CPU instead of spinning on poll().
    void waitEvent(int timeoutMs);

    int width() const { return w_; }
    int height() const { return h_; }

private:
    bool loadXlib();
    void ensureImage(int w, int h);

    void* lib_ = nullptr;
    void* dpy_ = nullptr;
    unsigned long win_ = 0;
    void* gc_ = nullptr;
    unsigned long wmDelete_ = 0;
    int w_ = 0, h_ = 0;
    std::string error_;

    struct Image;
    Image* img_ = nullptr;
};

}  // namespace sl
