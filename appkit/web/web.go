package web

import (
	"errors"
	"html/template"
	"io/fs"
	"mime"
	"net/http"
	"os"
	"path/filepath"
)

func init() {
	_ = mime.AddExtensionType(".woff2", "font/woff2")
}

// Site is a service's loaded web surface rooted at one on-disk directory.
type Site struct {
	root string
	tmpl *template.Template
}

// Load parses every top-level *.html and *.tmpl file under root into one
// template set. A root with no template files is valid for static-only sites.
func Load(root string) (*Site, error) {
	info, err := os.Stat(root)
	if err != nil {
		return nil, err
	}
	if !info.IsDir() {
		return nil, &fs.PathError{Op: "stat", Path: root, Err: errors.New("not a directory")}
	}

	tmpl := template.New("")
	for _, pattern := range []string{"*.html", "*.tmpl"} {
		matches, err := filepath.Glob(filepath.Join(root, pattern))
		if err != nil {
			return nil, err
		}
		if len(matches) == 0 {
			continue
		}
		if _, err := tmpl.ParseFiles(matches...); err != nil {
			return nil, err
		}
	}

	return &Site{root: root, tmpl: tmpl}, nil
}

// Render executes the named template with an HTML content type.
func (s *Site) Render(w http.ResponseWriter, name string, data any) error {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	return s.tmpl.ExecuteTemplate(w, name, data)
}

// Static returns the handler for the site's static/ subtree.
func (s *Site) Static() http.Handler {
	staticRoot := filepath.Join(s.root, "static")
	return http.StripPrefix("/static/", http.FileServer(http.FS(noDirFS{fsys: osDirFS{root: staticRoot}})))
}

type noDirFS struct {
	fsys fs.FS
}

func (f noDirFS) Open(name string) (fs.File, error) {
	file, err := f.fsys.Open(name)
	if err != nil {
		return nil, err
	}
	info, err := file.Stat()
	if err != nil {
		_ = file.Close()
		return nil, err
	}
	if info.IsDir() {
		_ = file.Close()
		return nil, fs.ErrNotExist
	}
	return file, nil
}

type osDirFS struct {
	root string
}

func (f osDirFS) Open(name string) (fs.File, error) {
	root := f.root
	if root == "" {
		root = "."
	}
	return http.Dir(root).Open(name)
}
