package search

/*
#include <stdlib.h>
#include "walker.h"
*/
import "C"
import "unsafe"

var entryCallback func(string)

//export goEntryCallback
func goEntryCallback(path *C.char, _ unsafe.Pointer) {
	if entryCallback != nil {
		entryCallback(C.GoString(path))
	}
}

func walkWithCallback(root string, recursive, fullPath bool, cb func(string)) {
	entryCallback = cb
	defer func() { entryCallback = nil }()

	cRoot := C.CString(root)
	defer C.free(unsafe.Pointer(cRoot))

	cRec := C.int(0)
	if recursive {
		cRec = 1
	}
	cFull := C.int(0)
	if fullPath {
		cFull = 1
	}

	C.walk_directory_cgo(cRoot, cRec, cFull, nil)
}
