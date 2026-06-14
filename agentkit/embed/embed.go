// Package embed provides a provider-neutral embeddings client. Embeddings
// are not a generation call, so they live in their own library rather than
// the chat [provider.Client] interface: there is no streaming, no tools, no
// reasoning — just text in, vectors out.
//
// The wiki service's index (design §9.3) is the first and only consumer; the
// catch-up worker owns chunking to the provider's array/token limits, so one
// [Embedder.Embed] call maps to exactly one HTTP request over the whole
// texts slice — the library hides no fan-out.
package embed

import (
	"context"
)

// Result is the output of one [Embedder.Embed] call: one vector per input
// text in order, plus the provider-reported input token count (read from the
// response usage so the caller can compute a real cost — P0c).
type Result struct {
	Vectors     [][]float32
	InputTokens int
}

// Embedder is the contract an embeddings backend implements. Embed turns a
// slice of texts into a slice of vectors, one vector per input in order.
// model selects the provider model (e.g. "text-embedding-3-large"); dims is
// the requested output dimensionality, passed through verbatim so the
// provider rejects an illegal value rather than the library second-guessing
// it. On failure it returns a typed *provider.Error, never a raw HTTP error.
type Embedder interface {
	Embed(ctx context.Context, model string, dims int, texts []string) (Result, error)
}
