package gui

import (
	"io"
	"strings"

	"zen-dragon/internal/search"

	"gioui.org/app"
	"gioui.org/io/clipboard"
	"gioui.org/io/key"
	"gioui.org/layout"
	"gioui.org/op"
	"gioui.org/op/paint"
	"gioui.org/unit"
	"gioui.org/widget/material"
)

type C = layout.Context
type D = layout.Dimensions

type Config struct {
	Keyword     string
	Root        string
	Recursive   bool
	Regex       bool
	ScrollSpeed float64
}

func RunUI(cfg *Config, results <-chan search.SearchResult) error {
	th := NewTheme()
	state := NewUIState()
	state.Searching = true
	state.ScrollSpeed = cfg.ScrollSpeed

	w := new(app.Window)
	w.Option(app.Title("zen-dragon: "+cfg.Keyword), app.Size(unit.Dp(900), unit.Dp(600)))

	var ops op.Ops

	for {
		e := w.Event()
		switch e := e.(type) {
		case app.X11ViewEvent:
			if e.Valid() {
				state.X11Window = e.Window
				state.X11Display = e.Display
			}
		case app.FrameEvent:
			state.drainResults(results)

			gtx := app.NewContext(&ops, e)
			state.layout(gtx, th)
			e.Frame(&ops)
		case key.Event:
			state.HandleKeyPress(e)
		case app.DestroyEvent:
			state.Searching = false
			return e.Err
		}
	}
}

func (s *UIState) drainResults(results <-chan search.SearchResult) {
	for {
		select {
		case r, ok := <-results:
			if !ok {
				s.Searching = false
				return
			}
			s.AddRow(r)
		default:
			return
		}
	}
}

func (s *UIState) flushClipboard(gtx C) {
	if s.ClipText == "" {
		return
	}
	gtx.Execute(clipboard.WriteCmd{
		Type: "text/plain;charset=utf-8",
		Data: readCloser{strings.NewReader(s.ClipText)},
	})
	s.ClipText = ""
}

type readCloser struct {
	*strings.Reader
}

func (readCloser) Close() error { return nil }

var _ io.ReadCloser = readCloser{}

func (s *UIState) layout(gtx C, th *material.Theme) {
	s.flushClipboard(gtx)

	paint.Fill(gtx.Ops, nightBg)

	layout.Flex{Axis: layout.Vertical}.Layout(gtx,
		layout.Rigid(func(gtx C) D {
			return s.LayoutStatus(gtx, th)
		}),
		layout.Flexed(1, func(gtx C) D {
			return s.LayoutResults(gtx, th)
		}),
	)
}
