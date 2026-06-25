package web

import (
	"embed"
	"io/fs"
	"mime"
)

//go:embed landing.html
var templateFS embed.FS

//go:embed static
var embeddedFS embed.FS

var staticFS = mustSub(embeddedFS, "static")

func init() {
	_ = mime.AddExtensionType(".woff2", "font/woff2")
}

func mustSub(fsys fs.FS, dir string) fs.FS {
	sub, err := fs.Sub(fsys, dir)
	if err != nil {
		panic(err)
	}
	return sub
}
