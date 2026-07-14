package dropbox

import (
	"errors"
	"net/http"
	"time"
)

// ContentHandler returns the loopback GET /content handler (PLAN.md §4): the
// byte-delivery route consumers use to fetch the current mirror bytes for a file
// referenced by a `file.*` event's content_url.
//
// Trust model: the route is UNAUTHENTICATED and loopback-only — one box is one
// owner, so there is no second principal and the perimeter is "it is on
// 127.0.0.1". The composition root applies the chassis loopback guard.
//
// Resolution: the `path` query value is URL-encoded (it is also URL-encoded in
// the event content_url); Go's net/http decodes it into r.URL.Query(). We then
// resolve it THROUGH the files index (case-insensitive, §2 case-folding) via
// Service.Content to the canonical stored display path, and serve that on-disk
// file with http.ServeContent (Range/HEAD/Content-Type/conditional requests for
// free). The optional `rev` query param enforces the §4 exact-bytes contract: a
// non-matching rev → 409 rather than silently serving newer bytes.
//
// Error mapping never leaks a path or a 500 for a confinement attempt:
//   - path not in the index (or after a delete)      → 404
//   - confinement/escape error from the mirror layer → 404
//   - stale rev (ErrRevMismatch)                      → 409
func (s *Service) ContentHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// The path query value is percent-decoded by net/http's query parser.
		path := r.URL.Query().Get("path")
		if path == "" {
			http.Error(w, "not found", http.StatusNotFound)
			return
		}

		// Optional exact-bytes contract: an empty/absent rev means "current bytes".
		var revPtr *string
		if rev := r.URL.Query().Get("rev"); rev != "" {
			revPtr = &rev
		}

		row, err := s.Content(path, revPtr)
		if err != nil {
			switch {
			case errors.Is(err, ErrRevMismatch):
				http.Error(w, "conflict", http.StatusConflict)
			default:
				// ErrNotFound, ErrPathEscape, a disk/index skew, anything else:
				// never 500-leak a path. The endpoint's only public answer for a
				// missing/unservable resource is 404.
				http.Error(w, "not found", http.StatusNotFound)
			}
			return
		}
		f, _, err := s.Mirror.Open(row.Path)
		if err != nil {
			http.Error(w, "not found", http.StatusNotFound)
			return
		}
		defer f.Close()

		// Feed http.ServeContent the canonical display path (for the Content-Type
		// sniff by extension) and the indexed modtime (for Last-Modified +
		// conditional requests). A modtime parse failure falls back to the zero
		// time, which ServeContent treats as "unknown" (omits Last-Modified).
		modTime, perr := time.Parse(eventTimeFormat, row.UpdatedAt)
		if perr != nil {
			modTime = time.Time{}
		}
		http.ServeContent(w, r, row.Path, modTime, f)
	})
}
