package dnd

/*
#cgo LDFLAGS: -lX11
#include <X11/Xlib.h>
#include <stdlib.h>

void xdnd_start_drag(Display *dpy, Window src, char **uris, int n_uris);
*/
import "C"
import (
	"net/url"
	"strings"
	"unsafe"
)

func PathsToURIList(paths []string) string {
	var b strings.Builder
	for _, p := range paths {
		u := url.URL{Scheme: "file", Path: p}
		b.WriteString(u.String())
		b.WriteString("\r\n")
	}
	return b.String()
}

// StartDrag blocks until the drag finishes (drop, or cancel). Call it
// synchronously from the goroutine that owns Gio's event loop — NOT as
// `go dnd.StartDrag(...)`. It reuses Gio's own X connection (display),
// which is required for XGrabPointer to succeed and for XDnD
// ClientMessages/SelectionRequests to be delivered back to this process.
func StartDrag(display unsafe.Pointer, x11Win uintptr, paths []string) {
	n := len(paths)
	if n == 0 || display == nil {
		return
	}

	cUris := make([]*C.char, n)
	for i, p := range paths {
		u := url.URL{Scheme: "file", Path: p}
		cUris[i] = C.CString(u.String())
	}
	defer func() {
		for _, cu := range cUris {
			C.free(unsafe.Pointer(cu))
		}
	}()

	C.xdnd_start_drag((*C.Display)(display), C.Window(x11Win), &cUris[0], C.int(n))
}
