<!-- codegraph-file-count: 12 -->

# zen-dragon

## Purpose
A native desktop file browser/manager built with Go and the Gio toolkit. It provides a GUI for file navigation with integrated search and X11 drag-and-drop support. CGO is used for low-level directory walking and Xdnd protocol handling.

## Architecture
main.go -> gui/app.go (RunUI) -> gui/{state,list,actions}.go (rendering, selection, input)
internal/search/{scanner,bridge,walker} -> streaming SearchResult -> GUI drain
internal/dnd/{xdnd,xdnd_session} <- list.go drag gestures -> X11 DnD

## File Tree
```
main.go
internal/
  gui/
    app.go
    list.go
    state.go
    actions.go
    theme.go
  search/
    bridge.go
    scanner.go
    walker.c
    walker.h
  dnd/
    xdnd.go
    xdnd_session.c
```

## Component Roles
| File / Module | Role | LOC | Key Exports (with signatures) |
|---|---|---|---|
| main.go | Application entry point | ~88 | `func main()` |
| internal/gui/app.go | Gio UI initialization and event loop | ~109 | `func RunUI(cfg *Config, results <-chan search.SearchResult) error`, `func (s *UIState) drainResults(results <-chan search.SearchResult)`, `func (s *UIState) layout(gtx C, th *material.Theme) D` |
| internal/gui/state.go | UI state management and selection | ~82 | `func NewUIState() *UIState`, `func (s *UIState) AddRow(r search.SearchResult)`, `func (s *UIState) CheckedURIs() []string`, `func (s *UIState) AllURIs() []string` |
| internal/gui/list.go | Result list rendering and row drag handling | ~226 | `func (s *UIState) LayoutResults(gtx C, th *material.Theme) D`, `func (s *UIState) LayoutRow(gtx C, th *material.Theme, i int) D`, `func (s *UIState) processRowDrag(gtx C, row *FileRow)` |
| internal/gui/actions.go | Keyboard shortcuts and copy actions | ~37 | `func (s *UIState) HandleKeyPress(e key.Event)`, `func (s *UIState) copyPath(path string)` |
| internal/gui/theme.go | Material theme initialization | ~28 | `func NewTheme() *material.Theme` |
| internal/search/scanner.go | Search/scan orchestration and result streaming | ~64 | `func (s *Scanner) Scan(ctx context.Context, out chan<- SearchResult) error` |
| internal/search/bridge.go | CGO bridge for C walker to Go callback | ~37 | `func walkWithCallback(root string, recursive, fullPath bool, cb func(string))` |
| internal/search/walker.c | POSIX directory walking (C) | ~90 | `void walk_directory_cgo(const char *root, int recursive, int full_path, void *user_data)` |
| internal/search/walker.h | Walker C header | ~7 | no exports |
| internal/dnd/xdnd.go | X11 drag-and-drop initiation (Go/CGO) | ~51 | `func PathsToURIList(paths []string) string`, `func StartDrag(display unsafe.Pointer, x11Win uintptr, paths []string)` |
| internal/dnd/xdnd_session.c | X11 Xdnd protocol implementation | ~360 | `void xdnd_start_drag(Display *dpy, Window src, char **uris, int n_uris)` |

## Cross-References
| File | Called by / calls | Why it's central |
|---|---|---|
| main.go | calls RunUI, Scan | Application bootstrap wiring |
| internal/gui/app.go | calls layout, drainResults | UI event loop and rendering core |
| internal/gui/list.go | calls LayoutResults, LayoutRow | Primary result list presentation |
| internal/gui/state.go | calls NewUIState, AddRow | Shared selection and URI state |
| internal/search/scanner.go | calls Scan | Streaming search results to UI |
| internal/dnd/xdnd_session.c | called by StartDrag | X11 DnD protocol state machine |

## Key Architectural Patterns
1. Gio UI Loop: GUI is driven by Gio's `gtx C` layout and event model, with `RunUI` orchestrating the main frame.
2. CGO Layering: C code is isolated to `internal/search/walker.c` and `internal/dnd/xdnd_session.c`, each exposed through thin Go wrappers (`bridge.go`, `xdnd.go`).
3. Channel-Based Streaming: Search results flow via `chan<- search.SearchResult` from `Scanner.Scan` into `RunUI`'s result drain loop.
4. Selection State as Source of Truth: `UIState` holds checked/all URIs; copy actions and drag initiation both read from it.
5. Row-Level Drag Detection: `processRowDrag` in `list.go` handles drag gesture detection per row, bridging GUI hit-testing to Xdnd initiation.

## Read Triggers
| If you need to... | Open these files |
|---|---|
| Change the main window layout | internal/gui/app.go, internal/gui/list.go |
| Add a keyboard shortcut | internal/gui/actions.go |
| Modify search result streaming | internal/search/scanner.go, internal/gui/app.go |
| Add a new copy action | internal/gui/actions.go, internal/gui/state.go |
| Change drag-and-drop behavior | internal/dnd/xdnd.go, internal/dnd/xdnd_session.c |
| Modify directory walking logic | internal/search/walker.c, internal/search/bridge.go |
| Update UI theme | internal/gui/theme.go |
| Change row rendering | internal/gui/list.go, internal/gui/state.go |

## Key Commits
b65acbd8938a88b099f64155a680f266bdd4a5db - Introduce link hover tooltips and accelerated scrolling

## Dependencies
### Go Runtime / GUI
| Package / Module | Role | Version |
|---|---|---|
| gioui.org | Gio GUI toolkit | v0.10.1 |
| golang.org/x/image | Image processing | v0.26.0 |
| golang.org/x/net | Networking | v0.48.0 |

### System / CGO
| Package / Module | Role | Version |
|---|---|---|
| X11 (libX11) | Drag-and-drop protocol via CGO | system |

## Build & Run
| Command | Purpose |
|---|---|
| `make build` | Build zen-dragon binary |
| `make run` | Build and run |
| `make clean` | Remove binary and clean Go cache |
