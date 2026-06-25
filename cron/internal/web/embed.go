package web

import "embed"

//go:embed landing.html static
var content embed.FS
