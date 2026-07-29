package gui

import (
	"gioui.org/io/key"
)

func (s *UIState) copyPath(path string) {
	s.ClipText = path
	s.Message = "Path copied"
}

func (s *UIState) copyName(name string) {
	s.ClipText = name
	s.Message = "Name copied"
}

func (s *UIState) copyURI(uri string) {
	s.ClipText = uri
	s.Message = "URI copied"
}

func (s *UIState) HandleKeyPress(e key.Event) {
	if e.State != key.Press {
		return
	}
	switch e.Name {
	case "A", "a":
		if e.Modifiers.Contain(key.ModCtrl) {
			s.SelectAll()
			s.Message = "All selected"
		}
	case key.NameEscape:
		s.DeselectAll()
		s.Message = "Deselected all"
	}
}
