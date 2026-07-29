#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a,b) ((a)<(b)?(a):(b))

static Atom atoms[11];
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

static int has_xdnd_aware(Display *dpy, Window win) {
    Atom actual_type;
    int actual_format;
    unsigned long n, left;
    unsigned char *data = NULL;
    int status = XGetWindowProperty(dpy, win, atoms[A_XdndAware], 0, 1,
                                     False, XA_ATOM, &actual_type, &actual_format,
                                     &n, &left, &data);
    if (status == Success && data) {
        XFree(data);
        return 1;
    }
    return 0;
}

static int xdnd_version(Display *dpy, Window win) {
    Atom actual_type;
    int actual_format;
    unsigned long n, left;
    unsigned char *data = NULL;
    int status = XGetWindowProperty(dpy, win, atoms[A_XdndAware], 0, 1,
                                     False, XA_ATOM, &actual_type, &actual_format,
                                     &n, &left, &data);
    if (status == Success && data) {
        int ver = (int)*(long*)data;
        XFree(data);
        return ver;
    }
    return 0;
}

static Window find_target(Display *dpy, Window src) {
    Window root, child;
    int wx, wy, wxr, wyr;
    unsigned int mask;
    if (!XQueryPointer(dpy, src, &root, &child, &wx, &wy, &wxr, &wyr, &mask))
        return 0;
    if (child == 0) return 0;

    Window win = child;
    while (win) {
        if (has_xdnd_aware(dpy, win))
            return win;
        Window parent = 0, *children = NULL;
        unsigned int n = 0;
        if (!XQueryTree(dpy, win, &root, &parent, &children, &n))
            return 0;
        if (children) XFree(children);
        if (parent == 0 || parent == root) break;
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

    if (req->target == atoms[A_Targets]) {
        Atom list[1] = { atoms[A_TextUriList] };
        XChangeProperty(dpy, req->requestor, req->property, XA_ATOM, 32,
                        PropModeReplace, (unsigned char*)list, 1);
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
        XChangeProperty(dpy, req->requestor, req->property, XA_STRING, 8,
                        PropModeReplace, (unsigned char*)buf, pos);
        free(buf);
    } else {
        notify.property = 0;
    }

send:
    XSendEvent(dpy, req->requestor, False, 0, (XEvent*)&notify);
    XFlush(dpy);
}

void xdnd_start_drag(Window src, char **uris, int n_uris) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) return;

    init_atoms(dpy);

    // Claim the selection
    XSetSelectionOwner(dpy, atoms[A_XdndSelection], src, CurrentTime);

    // Grab pointer
    int ret = XGrabPointer(dpy, src, False,
                            ButtonReleaseMask | PointerMotionMask,
                            GrabModeAsync, GrabModeAsync,
                            None, None, CurrentTime);
    if (ret != GrabSuccess) {
        XCloseDisplay(dpy);
        return;
    }

    Window target = 0;
    int version = 0;
    int accepted = 0;

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);

        switch (ev.type) {
        case MotionNotify: {
            Window new_target = find_target(dpy, src);
            if (new_target != target) {
                // Leave old target
                if (target)
                    send_client_msg(dpy, target, atoms[A_XdndLeave],
                                    (long)src, 0, 0, 0, 0);
                target = new_target;
                accepted = 0;
                if (target) {
                    version = MIN(5, xdnd_version(dpy, target));
                    if (version < 1) { target = 0; break; }
                    // XdndEnter
                    send_client_msg(dpy, target, atoms[A_XdndEnter],
                                    (long)src,
                                    ((long)version << 24) | 1, // bit0=1 => more than 3 types (we have 1 so inline fine)
                                    (long)atoms[A_TextUriList],
                                    0, 0);
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
                // Continue loop to handle SelectionRequest + XdndFinished
            } else {
                if (target)
                    send_client_msg(dpy, target, atoms[A_XdndLeave],
                                    (long)src, 0, 0, 0, 0);
                XUngrabPointer(dpy, CurrentTime);
                XCloseDisplay(dpy);
                return;
            }
            break;
        case SelectionRequest:
            provide_data(dpy, &ev.xselectionrequest, uris, n_uris);
            break;
        case ClientMessage:
            if (ev.xclient.message_type == atoms[A_XdndStatus]) {
                accepted = ev.xclient.data.l[1] & 1;
            } else if (ev.xclient.message_type == atoms[A_XdndFinished]) {
                XUngrabPointer(dpy, CurrentTime);
                XCloseDisplay(dpy);
                return;
            }
            break;
        }
    }
}
