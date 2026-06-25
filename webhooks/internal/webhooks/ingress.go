package webhooks

import (
	"errors"
	"io"
	"log/slog"
	"net/http"
	"strings"
)

// maxBodyBytes caps the inbound webhook payload at 1 MiB. A request whose body
// exceeds this (after a correct secret) is rejected with 413; a body of exactly
// maxBodyBytes is accepted.
const maxBodyBytes = 1 << 20 // 1 MiB

// notFoundBody is the single response body shared by every authentication outcome
// — wrong secret, unknown name, missing/malformed header — so the public ingress
// is byte-identical across all failures and leaks nothing about which check failed.
const notFoundBody = "not found\n"

// NewIngressHandler builds the public ingress endpoint for POST /in/<name>. It is
// the only surface a third party reaches directly (no front-door auth chain), so
// it trusts nothing: it never echoes caller identity, returns a byte-identical 404
// for every authentication failure, and authenticates before reading the body.
func NewIngressHandler(svc *Service, log *slog.Logger) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		ctx := r.Context()

		// 1. Method gate — name-agnostic, no store lookup.
		if r.Method != http.MethodPost {
			http.Error(w, "method not allowed\n", http.StatusMethodNotAllowed)
			return
		}

		// 2. Front-door identity headers must never appear on the public ingress;
		//    their presence means a misrouted internal request. X-Forwarded-Proto
		//    is a legitimate proxy header and is NOT rejected.
		if r.Header.Get("X-Owner-Email") != "" || r.Header.Get("X-Client-Id") != "" {
			notFound(w)
			return
		}

		// 3. Extract the name from the path.
		name := strings.TrimPrefix(r.URL.Path, "/in/")
		if name == "" {
			notFound(w)
			return
		}

		// 4. Authenticate BEFORE reading the body. Any failure — missing/empty/
		//    malformed header, unknown name, wrong secret — yields the identical 404.
		presented, ok := bearerToken(r.Header.Get("Authorization"))
		if !ok {
			notFound(w)
			return
		}
		wh, secretHash, found, err := svc.store.GetByName(ctx, name)
		if err != nil {
			log.ErrorContext(ctx, "ingress: GetByName failed", "error", err)
			notFound(w)
			return
		}
		if !found || !verifySecret(presented, secretHash) {
			notFound(w)
			return
		}

		// 5. Read the body under a hard cap; over the cap → 413.
		r.Body = http.MaxBytesReader(w, r.Body, maxBodyBytes)
		body, err := io.ReadAll(r.Body)
		if err != nil {
			var mbe *http.MaxBytesError
			if errors.As(err, &mbe) {
				http.Error(w, "payload too large\n", http.StatusRequestEntityTooLarge)
				return
			}
			log.ErrorContext(ctx, "ingress: read body failed", "error", err)
			http.Error(w, "bad request\n", http.StatusBadRequest)
			return
		}

		// 6. Durably record the event (owner is the STORED owner, never caller input).
		if err := svc.Record(ctx, wh, r.Header.Get("Content-Type"), body); err != nil {
			log.ErrorContext(ctx, "ingress: Record failed", "error", err)
			http.Error(w, "internal error\n", http.StatusInternalServerError)
			return
		}

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusAccepted)
		io.WriteString(w, `{"status":"accepted"}`)
	})
}

// notFound writes the single byte-identical 404 shared by every authentication
// failure so the public ingress leaks nothing about which check rejected the call.
func notFound(w http.ResponseWriter) {
	http.Error(w, notFoundBody, http.StatusNotFound)
}

// bearerToken extracts the secret from an "Authorization: Bearer <secret>" header.
// ok is false for a missing, malformed, or empty-secret header.
func bearerToken(header string) (secret string, ok bool) {
	const prefix = "Bearer "
	if len(header) <= len(prefix) || !strings.EqualFold(header[:len(prefix)], prefix) {
		return "", false
	}
	secret = header[len(prefix):]
	if secret == "" {
		return "", false
	}
	return secret, true
}
