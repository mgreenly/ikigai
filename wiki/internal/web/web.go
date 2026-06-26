// Package web serves the unauthenticated wiki read surface and embedded assets.
package web

import (
	"context"
	"embed"
	"errors"
	"html/template"
	"io/fs"
	"net/http"
	"strings"

	"wiki/internal/ask"
	"wiki/internal/markdown"
)

//go:embed layout.tmpl home.tmpl subject.tmpl static/tokens.css static/fonts/*.woff2
var assets embed.FS

var (
	homeTemplates    = template.Must(template.ParseFS(assets, "layout.tmpl", "home.tmpl"))
	subjectTemplates = template.Must(template.ParseFS(assets, "layout.tmpl", "subject.tmpl"))
)

var ErrNotFound = errors.New("web: subject not found")

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

// Mentioner lists wiki subjects mentioned by rendered answer text.
type Mentioner interface {
	MentionsIn(ctx context.Context, text string) ([]Ref, error)
}

// Ref is a mount-relative link target for the read surface.
type Ref struct {
	Href string
	Name string
}

// SubjectView is the rendered public page shape consumed by subject routes.
type SubjectView struct {
	Path     string
	Title    string
	Body     string
	Outbound []Ref
	Inbound  []Ref
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

// WithMentioner injects the answer mention lookup seam.
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

	Query       string
	Asked       bool
	Answer      ask.Answer
	AnswerHTML  template.HTML
	Cites       []Ref
	Mentions    []Ref
	Orphans     []Ref
	Subject     SubjectView
	SubjectHTML template.HTML
}

// NewHandler builds the read-surface mux.
func NewHandler(service, version, mount string, opts ...Option) http.Handler {
	h := &handler{service: service, version: version, mount: normalizeMount(mount)}
	for _, opt := range opts {
		opt(h)
	}

	mux := http.NewServeMux()
	mux.HandleFunc("GET /{$}", h.home)
	mux.HandleFunc("GET /subject/{type}/{slug}", h.subject)
	mux.Handle("GET /static/", StaticHandler())
	return mux
}

func (h *handler) home(w http.ResponseWriter, r *http.Request) {
	query := strings.TrimSpace(r.URL.Query().Get("q"))
	if query != "" {
		h.ask(w, r, query)
		return
	}

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
	if err := homeTemplates.ExecuteTemplate(w, "home", pageData{
		Service: h.service,
		Version: h.version,
		Mount:   h.mount,
		Orphans: orphans,
	}); err != nil {
		http.Error(w, "render home page", http.StatusInternalServerError)
	}
}

func (h *handler) ask(w http.ResponseWriter, r *http.Request, question string) {
	if h.asker == nil {
		http.Error(w, "ask wiki", http.StatusNotImplemented)
		return
	}
	answer, err := h.asker.Ask(r.Context(), r.Header.Get("X-Owner-Email"), question)
	if err != nil {
		http.Error(w, "ask wiki", http.StatusInternalServerError)
		return
	}

	cites := make([]Ref, 0, len(answer.Citations))
	for _, citation := range answer.Citations {
		if citation.Path == "" || citation.Title == "" {
			continue
		}
		cites = append(cites, Ref{
			Href: "subject/" + citation.Path,
			Name: citation.Title,
		})
	}
	var mentions []Ref
	if h.mentions != nil {
		refs, err := h.mentions.MentionsIn(r.Context(), answer.Text)
		if err != nil {
			http.Error(w, "link answer mentions", http.StatusInternalServerError)
			return
		}
		mentions = refs
	}

	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	if err := homeTemplates.ExecuteTemplate(w, "home", pageData{
		Service:    h.service,
		Version:    h.version,
		Mount:      h.mount,
		Query:      question,
		Asked:      true,
		Answer:     answer,
		AnswerHTML: markdown.Render(answer.Text),
		Cites:      cites,
		Mentions:   mentions,
	}); err != nil {
		http.Error(w, "render ask page", http.StatusInternalServerError)
	}
}

func (h *handler) subject(w http.ResponseWriter, r *http.Request) {
	if h.pages == nil {
		http.Error(w, "find subject page", http.StatusNotImplemented)
		return
	}
	path := r.PathValue("type") + "/" + r.PathValue("slug")
	subject, err := h.pages.PageByPath(r.Context(), path)
	if errors.Is(err, ErrNotFound) {
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.WriteHeader(http.StatusNotFound)
		if renderErr := subjectTemplates.ExecuteTemplate(w, "subject", pageData{
			Service: h.service,
			Version: h.version,
			Mount:   h.mount,
			Subject: SubjectView{
				Title: "Subject not found",
				Body:  "No page exists for this subject.",
			},
			SubjectHTML: markdown.Render("No page exists for this subject."),
		}); renderErr != nil {
			http.Error(w, "render subject page", http.StatusInternalServerError)
		}
		return
	}
	if err != nil {
		http.Error(w, "find subject page", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	if err := subjectTemplates.ExecuteTemplate(w, "subject", pageData{
		Service:     h.service,
		Version:     h.version,
		Mount:       h.mount,
		Subject:     subject,
		SubjectHTML: markdown.Render(subject.Body),
	}); err != nil {
		http.Error(w, "render subject page", http.StatusInternalServerError)
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
