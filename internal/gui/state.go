package gui

import (
	"unsafe"

	"zen-dragon/internal/search"

	"gioui.org/gesture"
	"gioui.org/layout"
	"gioui.org/widget"
)

type FileRow struct {
	Result      search.SearchResult
	Checkbox    widget.Bool
	CopyPath    widget.Clickable
	CopyName    widget.Clickable
	CopyContent widget.Clickable
	Drag        gesture.Drag
	Hover       gesture.Hover
	Hovered     bool
}

type UIState struct {
	Rows           []FileRow
	ResultList     layout.List
	Searching      bool
	MatchCount     int
	Message        string
	ClipText       string
	X11Window      uintptr
	X11Display     unsafe.Pointer
	ScrollSettling bool
	ScrollSpeed    float64
	HoveredRow     int
}

func NewUIState() *UIState {
	return &UIState{
		ResultList: layout.List{Axis: layout.Vertical},
		HoveredRow: -1,
	}
}

func (s *UIState) AddRow(r search.SearchResult) {
	s.Rows = append(s.Rows, FileRow{Result: r})
	s.MatchCount++
}

func (s *UIState) SelectAll() {
	for i := range s.Rows {
		s.Rows[i].Checkbox.Value = true
	}
}

func (s *UIState) DeselectAll() {
	for i := range s.Rows {
		s.Rows[i].Checkbox.Value = false
	}
}

func (s *UIState) CheckedCount() int {
	n := 0
	for i := range s.Rows {
		if s.Rows[i].Checkbox.Value {
			n++
		}
	}
	return n
}

func (s *UIState) CheckedURIs() []string {
	var uris []string
	for i := range s.Rows {
		if s.Rows[i].Checkbox.Value {
			uris = append(uris, s.Rows[i].Result.Path)
		}
	}
	return uris
}

func (s *UIState) AllURIs() []string {
	uris := make([]string, len(s.Rows))
	for i := range s.Rows {
		uris[i] = s.Rows[i].Result.Path
	}
	return uris
}
