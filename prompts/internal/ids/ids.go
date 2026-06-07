// Package ids generates opaque ULID-shaped identifiers used for session ids,
// state ids, binding-cookie values, and any other "random opaque token" the
// service needs. 48 bits of millisecond time + 80 bits of cryptographic
// randomness, Crockford base32-encoded (26 chars).
package ids

import (
	"crypto/rand"
	"encoding/base32"
	"time"
)

var enc = base32.StdEncoding.WithPadding(base32.NoPadding)

// NewULID returns a 26-char opaque identifier.
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
		panic("crypto/rand failed: " + err.Error())
	}
	return enc.EncodeToString(b[:])
}
