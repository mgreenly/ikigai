package gh

import (
	"encoding/json"
	"net/http"
	"time"
)

// TokenHandler returns the loopback GET /token handler.
func (c *Client) TokenHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		token, expiresAt, err := c.Token(r.Context())
		if err != nil {
			http.Error(w, "github token unavailable", http.StatusBadGateway)
			return
		}

		w.Header().Set("Content-Type", "application/json")
		if err := json.NewEncoder(w).Encode(struct {
			Token     string `json:"token"`
			ExpiresAt string `json:"expires_at"`
		}{
			Token:     token,
			ExpiresAt: expiresAt.Format(time.RFC3339),
		}); err != nil {
			http.Error(w, "encode response", http.StatusInternalServerError)
		}
	})
}
