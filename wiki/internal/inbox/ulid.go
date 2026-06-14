package inbox

import (
	"crypto/rand"
	"encoding/base32"
	"time"
)

var ulidEnc = base32.StdEncoding.WithPadding(base32.NoPadding)

// newULID returns a 26-char opaque, lexicographically-time-ordered identifier:
// 48 bits of millisecond time + 80 bits of cryptographic randomness, Crockford-
// shaped base32 (the suite's standard ids.NewULID shape). It is the inbox row's
// id — the ARRIVAL identity that threads the whole provenance chain (§2.2).
func newULID() string {
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
	return ulidEnc.EncodeToString(b[:])
}
