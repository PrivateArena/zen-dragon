package gui

import (
	"image"
	"os"

	"gioui.org/gesture"
	"gioui.org/io/pointer"
	"gioui.org/layout"
	"gioui.org/op"
	"gioui.org/op/clip"
	"gioui.org/op/paint"
	"gioui.org/unit"
	"gioui.org/widget"
	"gioui.org/widget/material"
	"zen-dragon/internal/dnd"
)

const dragThreshold float32 = 15

type dragState struct {
	Tag     *FileRow
	StartY  float32
	Pressed bool
	Dragged bool
}

var dragCtx dragState
var dragActive bool

func (s *UIState) LayoutResults(gtx C, th *material.Theme) D {
	if s.Searching && len(s.Rows) == 0 {
		return layout.Center.Layout(gtx, func(gtx C) D {
			return material.Body1(th, "Searching...").Layout(gtx)
		})
	}
	if !s.Searching && len(s.Rows) == 0 {
		return layout.Center.Layout(gtx, func(gtx C) D {
			return material.Body1(th, "No files found.").Layout(gtx)
		})
	}

	return s.ResultList.Layout(gtx, len(s.Rows), func(gtx C, i int) D {
		return s.LayoutRow(gtx, th, i)
	})
}

func (s *UIState) LayoutRow(gtx C, th *material.Theme, i int) D {
	if i >= len(s.Rows) {
		return D{}
	}
	row := &s.Rows[i]

	// 1. Process drag events first (handler was registered in the previous frame)
	s.processRowDrag(gtx, row)

	// 2. Lay out the content, recording its ops
	m := op.Record(gtx.Ops)
	dims := s.layoutRowContent(gtx, th, row, i)
	contentOps := m.Stop()

	// 3. Register the drag source over the full row, then replay content on top
	//    so buttons/checkbox (children) take pointer priority.
	defer clip.Rect(image.Rectangle{Max: dims.Size}).Push(gtx.Ops).Pop()
	row.Drag.Add(gtx.Ops)
	contentOps.Add(gtx.Ops)

	return dims
}

func (s *UIState) processRowDrag(gtx C, row *FileRow) {
	for {
		ev, ok := row.Drag.Update(gtx.Metric, gtx.Source, gesture.Both)
		if !ok {
			break
		}
		switch ev.Kind {
		case pointer.Press:
			dragCtx.Tag = row
			dragCtx.StartY = ev.Position.Y
			dragCtx.Pressed = true
			dragCtx.Dragged = false
		case pointer.Drag:
			if dragCtx.Pressed && dragCtx.Tag == row {
				dy := ev.Position.Y - dragCtx.StartY
				if dy < 0 {
					dy = -dy
				}
				if dy >= dragThreshold && s.X11Window != 0 && s.X11Display != nil && !dragActive {
					dragCtx.Pressed = false
					dragCtx.Dragged = true
					dragActive = true
					uris := s.CheckedURIs()
					if len(uris) == 0 {
						uris = []string{"file://" + row.Result.Path}
					}
					os.Stderr.WriteString("zen-dragon: starting XDnD drag with " + itoa(len(uris)) + " files\n")
					// Synchronous on Gio's own connection: blocks the frame
					// loop for the duration of the drag (modal, like dragon).
					dnd.StartDrag(s.X11Display, s.X11Window, uris)
					dragActive = false
				}
			}
		case pointer.Release:
			if dragCtx.Pressed && dragCtx.Tag == row && !dragCtx.Dragged {
				row.Checkbox.Value = !row.Checkbox.Value
			}
			dragCtx.Pressed = false
			dragCtx.Dragged = false
			dragCtx.Tag = nil
		case pointer.Cancel:
			dragCtx.Pressed = false
			dragCtx.Dragged = false
			dragCtx.Tag = nil
		}
	}
}

func (s *UIState) layoutRowContent(gtx C, th *material.Theme, row *FileRow, i int) D {
	return layout.Stack{}.Layout(gtx,
		layout.Expanded(func(gtx C) D {
			bg := nightRowBg
			if i%2 == 1 {
				bg = nightRowBgAlt
			}
			if row.Checkbox.Value {
				bg = nightRowSelected
			}
			paint.FillShape(gtx.Ops, bg, clip.Rect{Max: gtx.Constraints.Min}.Op())
			return D{Size: gtx.Constraints.Min}
		}),
		layout.Stacked(func(gtx C) D {
			return layout.Inset{Top: 4, Bottom: 4, Left: 8, Right: 8}.Layout(gtx, func(gtx C) D {
				return layout.Flex{
					Axis:      layout.Horizontal,
					Alignment: layout.Middle,
				}.Layout(gtx,
					layout.Rigid(func(gtx C) D {
						ch := material.CheckBox(th, &row.Checkbox, "")
						ch.Size = unit.Dp(20)
						return ch.Layout(gtx)
					}),
					layout.Flexed(1, func(gtx C) D {
						path := row.Result.Path
						if len(path) > 70 {
							path = path[:33] + "..." + path[len(path)-34:]
						}
						lb := material.Body2(th, path)
						lb.TextSize = unit.Sp(12)
						lb.MaxLines = 1
						lb.Color = nightText
						return layout.Inset{Left: 4, Right: 4}.Layout(gtx, lb.Layout)
					}),
					layout.Rigid(func(gtx C) D {
						if row.CopyPath.Clicked(gtx) {
							s.copyPath(row.Result.Path)
							gtx.Execute(op.InvalidateCmd{})
						}
						return actionButton(th, &row.CopyPath, "Path", gtx)
					}),
					layout.Rigid(func(gtx C) D {
						if row.CopyName.Clicked(gtx) {
							s.copyName(row.Result.Name)
							gtx.Execute(op.InvalidateCmd{})
						}
						return actionButton(th, &row.CopyName, "Name", gtx)
					}),
					layout.Rigid(func(gtx C) D {
						if row.CopyContent.Clicked(gtx) {
							s.copyURI("file://" + row.Result.Path)
							gtx.Execute(op.InvalidateCmd{})
						}
						return actionButton(th, &row.CopyContent, "Copy", gtx)
					}),
				)
			})
		}),
	)
}

func actionButton(th *material.Theme, clickable *widget.Clickable, label string, gtx C) D {
	btn := material.ButtonLayout(th, clickable)
	btn.CornerRadius = unit.Dp(3)
	return btn.Layout(gtx, func(gtx C) D {
		return layout.UniformInset(unit.Dp(2)).Layout(gtx, func(gtx C) D {
			lb := material.Label(th, unit.Sp(10), label)
			lb.Color = nightAccent
			return lb.Layout(gtx)
		})
	})
}

func (s *UIState) LayoutStatus(gtx C, th *material.Theme) D {
	var label string
	if s.Searching {
		label = "Searching...  |  "
	}
	label += "Found: " + itoa(s.MatchCount)
	if n := s.CheckedCount(); n > 0 {
		label += "  |  Selected: " + itoa(n)
	}
	if s.Message != "" {
		label += "  |  " + s.Message
	}

	paint.FillShape(gtx.Ops, nightStatusBg, clip.Rect{Max: gtx.Constraints.Min}.Op())
	lb := material.Body2(th, label)
	lb.TextSize = unit.Sp(11)
	lb.Color = nightStatusText
	return layout.Inset{Top: 2, Bottom: 2, Left: 8, Right: 8}.Layout(gtx, lb.Layout)
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	var buf [12]byte
	i := len(buf)
	for n > 0 {
		i--
		buf[i] = byte('0' + n%10)
		n /= 10
	}
	return string(buf[i:])
}
