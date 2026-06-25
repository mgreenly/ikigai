package web

import "embed"

//go:embed landing.html
var templateFiles embed.FS

//go:embed static
var staticFiles embed.FS
