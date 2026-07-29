package gui

import (
	"image/color"

	"gioui.org/widget/material"
)

var (
	nightBg          = color.NRGBA{R: 30, G: 30, B: 30, A: 255}
	nightRowBg       = color.NRGBA{R: 38, G: 38, B: 38, A: 255}
	nightRowBgAlt    = color.NRGBA{R: 34, G: 34, B: 34, A: 255}
	nightRowSelected = color.NRGBA{R: 48, G: 60, B: 80, A: 255}
	nightText        = color.NRGBA{R: 220, G: 220, B: 220, A: 255}
	nightStatusBg    = color.NRGBA{R: 20, G: 20, B: 20, A: 255}
	nightStatusText  = color.NRGBA{R: 150, G: 150, B: 150, A: 255}
	nightAccent      = color.NRGBA{R: 100, G: 150, B: 255, A: 255}
)

func NewTheme() *material.Theme {
	th := material.NewTheme()
	th.Bg = nightBg
	th.Fg = nightText
	th.ContrastBg = nightAccent
	th.ContrastFg = nightText
	return th
}
