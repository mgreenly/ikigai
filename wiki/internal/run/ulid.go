package run

import (
	"crypto/rand"
	"encoding/base32"
	"time"
)

var ulidEnc = base32.StdEncoding.WithPadding(base32.NoPadding)

// newULID returns a 26-char lexicographically-time-ordered identifier (48 bits of
// millisecond time + 80 bits of randomness, the suite's standard ULID shape). It
// is the run row's id — the provenance key that threads runs.id ↔
// inbox.integrated_by (design §4.5).
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
