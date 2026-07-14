package gh

import (
	"encoding/json"
	"errors"
	"net/http"
	"strconv"
	"strings"
)

// PRHandler returns the loopback GET /pr handler.
func (c *Client) PRHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		repo := strings.TrimSpace(r.URL.Query().Get("repo"))
		number, err := strconv.Atoi(strings.TrimSpace(r.URL.Query().Get("number")))
		if repo == "" || err != nil || number <= 0 {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}

		pr, err := c.PRGet(r.Context(), repo, number)
		if err != nil {
			if errors.Is(err, ErrNotFound) {
				http.NotFound(w, r)
				return
			}
			http.Error(w, "github request failed", http.StatusBadGateway)
			return
		}

		w.Header().Set("Content-Type", "application/json")
		if err := json.NewEncoder(w).Encode(pr); err != nil {
			http.Error(w, "encode response", http.StatusInternalServerError)
		}
	})
}
