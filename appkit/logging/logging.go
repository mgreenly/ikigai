// Package logging configures an ikigai app's structured logger: log/slog
// emitting JSON records to a writer at a configured level, plus a request-id
// middleware that tags each request and emits begin/end debug records. It also
// folds in ULID id generation (formerly each service's internal/ids).
//
// This is the uniform half lifted verbatim from every service's
// internal/logging + internal/ids (appkit extraction, PLAN §B).
package logging

import (
	"context"
	"crypto/rand"
	"encoding/base32"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"strings"
	"time"
)

// ParseLevel maps a level name (debug|info|warn|error) to a slog.Level,
// erroring on anything else. Surrounding whitespace and case are ignored.
func ParseLevel(name string) (slog.Level, error) {
	switch strings.ToLower(strings.TrimSpace(name)) {
	case "debug":
		return slog.LevelDebug, nil
	case "info":
		return slog.LevelInfo, nil
	case "warn":
		return slog.LevelWarn, nil
	case "error":
		return slog.LevelError, nil
	default:
		return 0, fmt.Errorf("invalid log level %q (want debug|info|warn|error)", name)
	}
}

// New returns a slog.Logger that writes JSON records at or above level to w.
// It does not mutate the package default; callers inject the returned logger
// explicitly.
func New(level slog.Level, w io.Writer) *slog.Logger {
	h := slog.NewJSONHandler(w, &slog.HandlerOptions{Level: level})
	return slog.New(h)
}

var enc = base32.StdEncoding.WithPadding(base32.NoPadding)

// NewULID returns a ULID-shaped (RFC 4648 base32, 26-char) opaque id: 48 bits of
// millisecond time followed by 80 bits of cryptographic randomness, time-ordered
// and unguessable. No external dependency is needed for that property. It is the
// fold-in of each service's internal/ids.NewULID.
func NewULID() string {
	var b [16]byte
	now := uint64(time.Now().UnixMilli())
	b[0] = byte(now >> 40)
	b[1] = byte(now >> 32)
	b[2] = byte(now >> 24)
	b[3] = byte(now >> 16)
	b[4] = byte(now >> 8)
	b[5] = byte(now)
	if _, err := rand.Read(b[6:]); err != nil {
		// crypto/rand failure is non-recoverable.
		panic("crypto/rand failed: " + err.Error())
	}
	return enc.EncodeToString(b[:])
}

type ctxKey struct{}

var requestIDKey ctxKey

// WithRequestID stashes a request id on the context.
func WithRequestID(ctx context.Context, id string) context.Context {
	return context.WithValue(ctx, requestIDKey, id)
}

// RequestID returns the request id on the context, or "" if none.
func RequestID(ctx context.Context) string {
	v, _ := ctx.Value(requestIDKey).(string)
	return v
}

// RequestIDMiddleware attaches a fresh request id to every request's context,
// echoes it in X-Request-ID, and emits per-request begin/end debug records.
// It logs only the presence of identity headers, never their values.
func RequestIDMiddleware(logger *slog.Logger, next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		id := NewULID()
		ctx := WithRequestID(r.Context(), id)
		w.Header().Set("X-Request-ID", id)

		start := time.Now()
		logger.LogAttrs(ctx, slog.LevelDebug, "request.begin",
			slog.String("request_id", id),
			slog.String("method", r.Method),
			slog.String("path", r.URL.Path),
			slog.String("remote", r.RemoteAddr),
			slog.Bool("has_owner_email", r.Header.Get("X-Owner-Email") != ""),
		)

		rw := &statusRecorder{ResponseWriter: w, status: http.StatusOK}
		next.ServeHTTP(rw, r.WithContext(ctx))

		logger.LogAttrs(ctx, slog.LevelDebug, "request.end",
			slog.String("request_id", id),
			slog.Int("status", rw.status),
			slog.Duration("duration", time.Since(start)),
		)
	})
}

type statusRecorder struct {
	http.ResponseWriter
	status int
	wrote  bool
}

func (s *statusRecorder) WriteHeader(code int) {
	if !s.wrote {
		s.status = code
		s.wrote = true
	}
	s.ResponseWriter.WriteHeader(code)
}

func (s *statusRecorder) Write(b []byte) (int, error) {
	s.wrote = true
	return s.ResponseWriter.Write(b)
}

// Unwrap lets http.NewResponseController reach the underlying writer.
func (s *statusRecorder) Unwrap() http.ResponseWriter { return s.ResponseWriter }
