package gh

import (
	"context"
	"time"
)

// Token returns a cached GitHub App installation token and its expiry,
// refreshing through the client's existing token source when needed.
func (c *Client) Token(ctx context.Context) (token string, expiresAt time.Time, err error) {
	return c.ts.tokenWithExpiry(ctx, false)
}
