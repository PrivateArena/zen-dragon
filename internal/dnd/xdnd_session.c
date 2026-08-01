#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MIN(a,b) ((a)<(b)?(a):(b))

static Atom atoms[12];
enum {
    A_XdndAware,
    A_XdndSelection,
    A_XdndEnter,
    A_XdndPosition,
    A_XdndStatus,
    A_XdndLeave,
    A_XdndDrop,
    A_XdndFinished,
    A_XdndActionCopy,
    A_XdndProxy,
    A_TextUriList,
    A_Targets,
};

static void init_atoms(Display *dpy) {
    atoms[A_XdndAware]      = XInternAtom(dpy, "XdndAware", False);
    atoms[A_XdndSelection]  = XInternAtom(dpy, "XdndSelection", False);
    atoms[A_XdndEnter]      = XInternAtom(dpy, "XdndEnter", False);
    atoms[A_XdndPosition]   = XInternAtom(dpy, "XdndPosition", False);
    atoms[A_XdndStatus]     = XInternAtom(dpy, "XdndStatus", False);
    atoms[A_XdndLeave]      = XInternAtom(dpy, "XdndLeave", False);
    atoms[A_XdndDrop]       = XInternAtom(dpy, "XdndDrop", False);
    atoms[A_XdndFinished]   = XInternAtom(dpy, "XdndFinished", False);
    atoms[A_XdndActionCopy] = XInternAtom(dpy, "XdndActionCopy", False);
    atoms[A_XdndProxy]      = XInternAtom(dpy, "XdndProxy", False);
    atoms[A_TextUriList]    = XInternAtom(dpy, "text/uri-list", False);
    atoms[A_Targets]        = XInternAtom(dpy, "TARGETS", False);
}

static void send_client_msg(Display *dpy, Window target, Atom msg_type, long a, long b, long c, long d, long e) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = target;
    ev.xclient.message_type = msg_type;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = a;
    ev.xclient.data.l[1] = b;
    ev.xclient.data.l[2] = c;
    ev.xclient.data.l[3] = d;
    ev.xclient.data.l[4] = e;
    XSendEvent(dpy, target, False, NoEventMask, &ev);
    XFlush(dpy);
}

// Recurse from `win` down the window tree, following the point (x, y)
// relative to `win`, and return the deepest viewable InputOutput window
// that contains the point. GDK-style: XQueryPointer is unreliable during
// an active grab, so we hit-test against the tree instead.
static Window deepest_window_at(Display *dpy, Window win, int x, int y) {
    Window root, parent;
    Window *children = NULL;
    unsigned int n = 0;
    if (!XQueryTree(dpy, win, &root, &parent, &children, &n))
        return win;
    if (n == 0) {
        if (children) XFree(children);
        return win;
    }
    // children are listed bottom-to-top; the topmost is last.
    for (int i = (int)n - 1; i >= 0; i--) {
        XWindowAttributes attrs;
        if (!XGetWindowAttributes(dpy, children[i], &attrs))
            continue;
        if (attrs.map_state != IsViewable)
            continue;
        if (attrs.class != InputOutput)
            continue;
        if (x >= attrs.x && x < attrs.x + attrs.width &&
            y >= attrs.y && y < attrs.y + attrs.height) {
            Window result = deepest_window_at(dpy, children[i],
                                              x - attrs.x, y - attrs.y);
            if (children) XFree(children);
            return result;
        }
    }
    if (children) XFree(children);
    return win;
}

// Resolve the XDnD target window for `win`, following XdndProxy and
// requiring XdndAware version >= 3. Returns 0 if not a usable target.
static Window xdnd_check_dest(Display *dpy, Window win, int *version) {
    Atom type;
    int fmt;
    unsigned long n, left;
    unsigned char *data = NULL;
    Window proxy = 0;

    if (XGetWindowProperty(dpy, win, atoms[A_XdndProxy], 0, 1, False,
                           XA_WINDOW, &type, &fmt, &n, &left, &data) == Success && data) {
        if (type != None && fmt == 32 && n == 1)
            proxy = *(Window*)data;
        XFree(data);
    }

    Window target = proxy ? proxy : win;
    if (XGetWindowProperty(dpy, target, atoms[A_XdndAware], 0, 1, False,
                           XA_ATOM, &type, &fmt, &n, &left, &data) == Success && data) {
        int ver = 0;
        if (type != None && fmt == 32 && n == 1)
            ver = (int)*(Atom*)data;
        XFree(data);
        if (ver >= 3) {
            if (version) *version = ver;
            return target;
        }
        fprintf(stderr, "zen-dragon: xdnd_check_dest: %lu has XdndAware=%d (<3)\n",
                (unsigned long)target, ver);
    }
    return 0;
}

// Read the raw XdndAware version of a window (used after xdnd_check_dest
// already confirmed it is a valid target).
static int xdnd_version_of(Display *dpy, Window win) {
    Atom type;
    int fmt;
    unsigned long n, left;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, win, atoms[A_XdndAware], 0, 1, False,
                           XA_ATOM, &type, &fmt, &n, &left, &data) == Success && data) {
        int ver = 0;
        if (type != None && fmt == 32 && n == 1)
            ver = (int)*(Atom*)data;
        XFree(data);
        return ver;
    }
    return 0;
}

// Find the XdndAware target under the screen point (x_root, y_root).
// Falls back to walking up from the deepest window under the point.
static Window find_target(Display *dpy, int x_root, int y_root) {
    Window root = DefaultRootWindow(dpy);
    Window deepest = deepest_window_at(dpy, root, x_root, y_root);

    Window win = deepest;
    while (win) {
        int version = 0;
        Window t = xdnd_check_dest(dpy, win, &version);
        if (t) {
            return t;
        }
        Window r, parent;
        Window *children = NULL;
        unsigned int n = 0;
        if (!XQueryTree(dpy, win, &r, &parent, &children, &n))
            break;
        if (children) XFree(children);
        if (parent == 0 || parent == root)
            break;
        win = parent;
    }
    return 0;
}

static void provide_data(Display *dpy, XSelectionRequestEvent *req, char **uris, int n_uris) {
    XSelectionEvent notify;
    memset(&notify, 0, sizeof(notify));
    notify.type = SelectionNotify;
    notify.display = req->display;
    notify.requestor = req->requestor;
    notify.selection = req->selection;
    notify.target = req->target;
    notify.time = req->time;
    notify.property = req->property;

    fprintf(stderr, "zen-dragon: SelectionRequest target=%lu property=%lu requestor=%lu\n",
            (unsigned long)req->target, (unsigned long)req->property, (unsigned long)req->requestor);

    if (req->target == atoms[A_Targets]) {
        Atom list[1] = { atoms[A_TextUriList] };
        XChangeProperty(dpy, req->requestor, req->property, XA_ATOM, 32,
                        PropModeReplace, (unsigned char*)list, 1);
        fprintf(stderr, "zen-dragon: responded TARGETS -> text/uri-list\n");
    } else if (req->target == atoms[A_TextUriList] || req->target == XA_STRING) {
        // Build URI list
        size_t total = 0;
        for (int i = 0; i < n_uris; i++)
            total += strlen(uris[i]) + 2; // uri + \r\n
        char *buf = malloc(total + 1);
        if (!buf) { notify.property = 0; goto send; }
        size_t pos = 0;
        for (int i = 0; i < n_uris; i++) {
            size_t len = strlen(uris[i]);
            memcpy(buf + pos, uris[i], len);
            pos += len;
            buf[pos++] = '\r';
            buf[pos++] = '\n';
        }
        buf[pos] = 0;
        // Property type must match the requested target, not XA_STRING:
        // picky targets (GTK, Qt, browsers) validate it.
        XChangeProperty(dpy, req->requestor, req->property, req->target, 8,
                        PropModeReplace, (unsigned char*)buf, pos);
        free(buf);
        fprintf(stderr, "zen-dragon: provided %d uri(s), %d bytes\n", n_uris, (int)pos);
    } else {
        notify.property = 0;
        fprintf(stderr, "zen-dragon: unsupported target, refusing\n");
    }

send:
    XSendEvent(dpy, req->requestor, False, 0, (XEvent*)&notify);
    XFlush(dpy);
}

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*1000LL + ts.tv_nsec/1000000LL;
}

// dpy is Gio's own X11 Display (from app.X11ViewEvent.Display). It must be
// used, not a new connection, because (1) the implicit pointer grab held by
// Gio's connection while the button is down prevents any other connection's
// XGrabPointer from succeeding (AlreadyGrabbed), and (2) XdndStatus/
// SelectionRequest ClientMessages are delivered only to the connection that
// owns the src window — which is Gio's. The caller must invoke this
// synchronously (blocking), never racing Gio's w.Event() loop.
void xdnd_start_drag(Display *dpy, Window src, char **uris, int n_uris) {
    if (!dpy) return;

    init_atoms(dpy);

    // Claim the selection
    XSetSelectionOwner(dpy, atoms[A_XdndSelection], src, CurrentTime);
    XSync(dpy, False);

    // Grab pointer with a drag cursor for OS feedback.
    // owner_events=False so Gio's own window does NOT receive duplicate
    // pointer events on the same Display; the grab owns them exclusively.
    // Events not part of the Xdnd protocol are stashed below and replayed
    // with XPutBackEvent before returning, so Gio's event pump never loses
    // them and the every-other-drag state corruption is eliminated.
    Cursor drag_cursor = XCreateFontCursor(dpy, XC_fleur);
    int ret = XGrabPointer(dpy, src, False,
                            ButtonReleaseMask | PointerMotionMask,
                            GrabModeAsync, GrabModeAsync,
                            None, drag_cursor, CurrentTime);
    if (ret != GrabSuccess) {
        fprintf(stderr, "zen-dragon: XGrabPointer failed, code=%d "
                        "(0=Success 1=AlreadyGrabbed 2=InvalidTime 3=NotViewable 4=Frozen)\n", ret);
        XFreeCursor(dpy, drag_cursor);
        return;
    }

    Window target = 0;
    int version = 0;
    int accepted = 0;
    int drop_sent = 0;
    long long drop_deadline = 0;

    // Stash non-Xdnd events and replay them before returning so Gio's
    // own event pump never sees its events silently consumed.
    XEvent stash[64];
    int n_stash = 0;

    XEvent ev;
    while (1) {
        if (!XPending(dpy)) {
            if (drop_sent && now_ms() > drop_deadline) {
                // Target never sent XdndFinished (pre-v5 target); give up.
                fprintf(stderr, "zen-dragon: timeout waiting for XdndFinished\n");
                break;
            }
            struct timespec ts = { 0, 2000000 }; // 2ms, avoid busy loop
            nanosleep(&ts, NULL);
            continue;
        }
        XNextEvent(dpy, &ev);

        switch (ev.type) {
        case MotionNotify: {
            Window new_target = find_target(dpy, ev.xmotion.x_root, ev.xmotion.y_root);
            if (new_target != target) {
                // Leave old target
                if (target)
                    send_client_msg(dpy, target, atoms[A_XdndLeave],
                                    (long)src, 0, 0, 0, 0);
                target = new_target;
                accepted = 0;
                if (target) {
                    version = MIN(5, xdnd_version_of(dpy, target));
                    fprintf(stderr, "zen-dragon: target found win=%lu version=%d\n",
                            (unsigned long)target, version);
                    // bit0=0: only one data type, inline in l[2].
                    // bit0=1 would mean "more than 3 types, read my
                    // XdndTypeList property" which we never set.
                    send_client_msg(dpy, target, atoms[A_XdndEnter],
                                    (long)src,
                                    ((long)version << 24) | 0,
                                    (long)atoms[A_TextUriList],
                                    0, 0);
                    fprintf(stderr, "zen-dragon: sent XdndEnter\n");
                } else {
                    fprintf(stderr, "zen-dragon: no XdndAware target under pointer (%d,%d)\n",
                            ev.xmotion.x_root, ev.xmotion.y_root);
                }
            }
            if (target) {
                int rx = ev.xmotion.x_root;
                int ry = ev.xmotion.y_root;
                send_client_msg(dpy, target, atoms[A_XdndPosition],
                                (long)src, 0,
                                (long)((rx << 16) | (ry & 0xffff)),
                                (long)CurrentTime,
                                (long)atoms[A_XdndActionCopy]);
            }
            break;
        }
        case ButtonRelease:
            if (target && accepted) {
                send_client_msg(dpy, target, atoms[A_XdndDrop],
                                (long)src, 0, (long)CurrentTime, 0, 0);
                XFlush(dpy);
                drop_sent = 1;
                drop_deadline = now_ms() + 10000;
                fprintf(stderr, "zen-dragon: sent XdndDrop\n");
                // Continue loop to handle SelectionRequest + XdndFinished
            } else {
                fprintf(stderr, "zen-dragon: release, %s\n",
                        target ? "target rejected (not accepted)" : "no target");
                if (target)
                    send_client_msg(dpy, target, atoms[A_XdndLeave],
                                    (long)src, 0, 0, 0, 0);
                goto done;
            }
            break;
        case SelectionRequest:
            provide_data(dpy, &ev.xselectionrequest, uris, n_uris);
            break;
        case ClientMessage:
            if (ev.xclient.message_type == atoms[A_XdndStatus]) {
                accepted = ev.xclient.data.l[1] & 1;
                fprintf(stderr, "zen-dragon: XdndStatus accepted=%d action=%ld\n",
                        accepted, ev.xclient.data.l[4]);
            } else if (ev.xclient.message_type == atoms[A_XdndFinished]) {
                fprintf(stderr, "zen-dragon: XdndFinished received\n");
                goto done;
            }
            break;
        default:
            if (n_stash < 64)
                stash[n_stash++] = ev;
            break;
        }
    }

done:
    XUngrabPointer(dpy, CurrentTime);
    XFreeCursor(dpy, drag_cursor);
    for (int i = n_stash - 1; i >= 0; i--)
        XPutBackEvent(dpy, &stash[i]);
}
