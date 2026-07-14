package dropbox

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
)

// WriteHandler serves the loopback PUT and DELETE /content mutations.
func (s *Service) WriteHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		path := r.URL.Query().Get("path")
		switch r.Method {
		case http.MethodPut:
			row, err := s.Write(context.Background(), path, r.Body, r.Header.Get("X-Client-Id"))
			if err != nil {
				writeMutationError(w, err)
				return
			}
			writeJSON(w, http.StatusOK, map[string]any{"path": row.Path, "size": row.Size, "content_hash": row.ContentHash, "rev": row.Rev})
		case http.MethodDelete:
			if _, err := s.Delete(context.Background(), path, r.Header.Get("X-Client-Id")); err != nil {
				writeMutationError(w, err)
				return
			}
			w.WriteHeader(http.StatusNoContent)
		default:
			w.WriteHeader(http.StatusMethodNotAllowed)
		}
	})
}

// MkdirHandler serves POST /mkdir.
func (s *Service) MkdirHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if err := s.Mkdir(context.Background(), r.URL.Query().Get("path"), r.Header.Get("X-Client-Id")); err != nil {
			writeMutationError(w, err)
			return
		}
		w.WriteHeader(http.StatusNoContent)
	})
}

// MoveHandler serves POST /move.
func (s *Service) MoveHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if err := s.Move(context.Background(), r.URL.Query().Get("from"), r.URL.Query().Get("to"), r.Header.Get("X-Client-Id")); err != nil {
			writeMutationError(w, err)
			return
		}
		w.WriteHeader(http.StatusNoContent)
	})
}

// StatHandler serves GET /stat for either indexed entry kind.
func (s *Service) StatHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		entry, err := s.Stat(r.URL.Query().Get("path"))
		if errors.Is(err, ErrNotFound) {
			http.Error(w, "not found", http.StatusNotFound)
			return
		}
		if err != nil {
			http.Error(w, "internal error", http.StatusInternalServerError)
			return
		}
		writeJSON(w, http.StatusOK, entry)
	})
}

func writeMutationError(w http.ResponseWriter, err error) {
	if errors.Is(err, ErrValidation) || errors.Is(err, ErrPathEscape) {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	if errors.Is(err, ErrNotFound) {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}
	http.Error(w, "internal error", http.StatusInternalServerError)
}

func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(v)
}
