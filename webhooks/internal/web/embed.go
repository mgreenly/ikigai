package web

import (
	"embed"
	"mime"
)

//go:embed landing.html static
var content embed.FS

func init() {
	if err := mime.AddExtensionType(".woff2", "font/woff2"); err != nil {
		panic(err)
	}
}
