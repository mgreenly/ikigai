package webhooks

import (
	"crypto/hmac"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/hex"
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
// it trusts nothing: it never echoes caller identity and returns a byte-identical
// 404 for every authentication failure. Bearer authenticates before reading;
// github-hmac reads under the cap first because its signature covers the body.
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

		// 4. Resolve the hook, then dispatch authentication by its stored scheme.
		wh, secretHash, secret, found, err := svc.store.GetByName(ctx, name)
		if err != nil {
			log.ErrorContext(ctx, "ingress: GetByName failed", "error", err)
			notFound(w)
			return
		}
		if !found {
			notFound(w)
			return
		}

		// GitHub HMAC covers the raw body, so that scheme reads under the cap first.
		// Bearer authentication remains before any body read.
		var body []byte
		if wh.Verification == "bearer" {
			presented, ok := bearerToken(r.Header.Get("Authorization"))
			if !ok || !verifySecret(presented, secretHash) {
				notFound(w)
				return
			}
		} else if wh.Verification != "github-hmac" {
			notFound(w)
			return
		}

		r.Body = http.MaxBytesReader(w, r.Body, maxBodyBytes)
		body, err = io.ReadAll(r.Body)
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
		if wh.Verification == "github-hmac" && !verifyGitHubHMAC(r.Header.Get("X-Hub-Signature-256"), body, secret) {
			notFound(w)
			return
		}

		var headers map[string]string
		if wh.Verification == "github-hmac" {
			headers = make(map[string]string)
			for _, key := range []string{"x-github-event", "x-github-delivery"} {
				if value := r.Header.Get(key); value != "" {
					headers[key] = value
				}
			}
		}
		if err := svc.Record(ctx, wh, r.Header.Get("Content-Type"), body, headers); err != nil {
			log.ErrorContext(ctx, "ingress: Record failed", "error", err)
			http.Error(w, "internal error\n", http.StatusInternalServerError)
			return
		}

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusAccepted)
		io.WriteString(w, `{"status":"accepted"}`)
	})
}

func verifyGitHubHMAC(header string, body []byte, secret string) bool {
	const prefix = "sha256="
	if !strings.HasPrefix(header, prefix) {
		return false
	}
	presented, err := hex.DecodeString(strings.TrimPrefix(header, prefix))
	if err != nil || len(presented) != sha256.Size {
		return false
	}
	mac := hmac.New(sha256.New, []byte(secret))
	_, _ = mac.Write(body)
	return subtle.ConstantTimeCompare(presented, mac.Sum(nil)) == 1
}

// notFound writes the single byte-identical 404 shared by every authentication
// failure so the public ingress leaks nothing about which check rejected the call.
func notFound(w http.ResponseWriter) {
	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.WriteHeader(http.StatusNotFound)
	_, _ = io.WriteString(w, notFoundBody)
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
