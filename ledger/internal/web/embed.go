package web

import "embed"

//go:embed landing.html
var templateFS embed.FS

//go:embed static
var staticFS embed.FS
