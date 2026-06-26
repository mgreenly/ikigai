// Package web serves the unauthenticated wiki read surface and embedded assets.
package web

import (
	"context"
	"embed"
	"errors"
	"html/template"
	"io/fs"
	"net/http"

	"wiki/internal/ask"
)

//go:embed layout.tmpl home.tmpl static/tokens.css static/fonts/*.woff2
var assets embed.FS

var pageTemplates = template.Must(template.ParseFS(assets, "layout.tmpl", "home.tmpl"))

var ErrNotFound = errors.New("web: not found")

// Asker answers a question for the owner supplied by the front door.
type Asker interface {
	Ask(ctx context.Context, owner, question string) (ask.Answer, error)
}

// PageFinder resolves a public type/slug path to a rendered subject view.
type PageFinder interface {
	PageByPath(ctx context.Context, path string) (SubjectView, error)
}

// OrphanLister lists subjects with zero inbound mentions for the home index.
type OrphanLister interface {
	Orphans(ctx context.Context) ([]Ref, error)
}

// Mentioner is reserved for the subject-page mention seam added by later phases.
type Mentioner interface{}

// Ref is a mount-relative link target for the read surface.
type Ref struct {
	Href string
	Name string
}

// SubjectView is the rendered public page shape consumed by subject routes.
type SubjectView struct {
	Path  string
	Title string
	Body  string
}

// Option injects an external dependency seam.
type Option func(*handler)

// WithAsker injects the question-answering seam.
func WithAsker(a Asker) Option {
	return func(h *handler) {
		h.asker = a
	}
}

// WithPageFinder injects the subject-page lookup seam.
func WithPageFinder(p PageFinder) Option {
	return func(h *handler) {
		h.pages = p
	}
}

// WithOrphanLister injects the home-page orphan index seam.
func WithOrphanLister(o OrphanLister) Option {
	return func(h *handler) {
		h.orphans = o
	}
}

// WithMentioner injects the mention seam for later subject-page routes.
func WithMentioner(m Mentioner) Option {
	return func(h *handler) {
		h.mentions = m
	}
}

type handler struct {
	service string
	version string
	mount   string

	asker    Asker
	pages    PageFinder
	orphans  OrphanLister
	mentions Mentioner
}

type pageData struct {
	Service string
	Version string
	Mount   string
	Orphans []Ref
}

// NewHandler builds the read-surface mux.
func NewHandler(service, version, mount string, opts ...Option) http.Handler {
	h := &handler{service: service, version: version, mount: normalizeMount(mount)}
	for _, opt := range opts {
		opt(h)
	}

	mux := http.NewServeMux()
	mux.HandleFunc("GET /{$}", h.home)
	mux.Handle("GET /static/", StaticHandler())
	return mux
}

func (h *handler) home(w http.ResponseWriter, r *http.Request) {
	var orphans []Ref
	if h.orphans != nil {
		refs, err := h.orphans.Orphans(r.Context())
		if err != nil {
			http.Error(w, "list orphan pages", http.StatusInternalServerError)
			return
		}
		orphans = refs
	}

	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	if err := pageTemplates.ExecuteTemplate(w, "home", pageData{
		Service: h.service,
		Version: h.version,
		Mount:   h.mount,
		Orphans: orphans,
	}); err != nil {
		http.Error(w, "render home page", http.StatusInternalServerError)
	}
}

// LandingHandler returns the exact-root home page.
func LandingHandler(service, version string) http.HandlerFunc {
	h := NewHandler(service, version, "/")
	return func(w http.ResponseWriter, r *http.Request) {
		h.ServeHTTP(w, r)
	}
}

func normalizeMount(mount string) string {
	if mount == "" {
		return "/"
	}
	if mount[len(mount)-1] != '/' {
		return mount + "/"
	}
	return mount
}

// StaticHandler serves the embedded read-surface assets below /static/.
func StaticHandler() http.Handler {
	static, err := fs.Sub(assets, "static")
	if err != nil {
		panic(err)
	}
	return http.StripPrefix("/static/", http.FileServer(http.FS(static)))
}
