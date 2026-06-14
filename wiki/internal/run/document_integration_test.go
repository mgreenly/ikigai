//go:build integration

// The standing integration tier's first FULL document-pass slice — see
// "Integration testing" in docs/wiki-redesign-plan.md. This is P7a2's phase-owned
// checkpoint (the SECOND of three): extract + match + merge now run their live
// pinned (prompt, model, effort) triples end-to-end against a BLUNT fixture, and
// the result is committed through the real end-of-run transaction. It asserts the
// output is STRUCTURALLY valid — a page row with ≥1 [inbox-id] citation matching
// its source row, and the registry subject + alias — never whether the prose is
// GOOD (quality is Part II's graded sweep).
//
// Build-tag gated (`-tags=integration`) so it is always in the tree but never in
// the unit gate, which must stay deterministic, free, and offline. With no
// key/network it emits the visible `INTEGRATION CHECKPOINT SKIPPED — no keys` line
// and skips — never passing as if it ran. A red run pauses the march for
// investigation (advisory, not a deploy gate).
package run

import (
	"context"
	"crypto/rand"
	"encoding/base32"
	"os"
	"regexp"
	"testing"
	"time"

	"agentkit/model"
	"agentkit/provider/anthropic"
	"agentkit/provider/openai"

	"wiki/internal/config"
	"wiki/internal/integrate"
	"wiki/internal/llm"
	"wiki/internal/page"
)

func liveDocFactory() llm.ClientFactory {
	return func(r model.Resolved) (llm.Client, error) {
		switch r.Provider {
		case model.ProviderOpenAI:
			return openai.New(os.Getenv("OPENAI_API_KEY"), r.BareID)
		default:
			return anthropic.New(os.Getenv("ANTHROPIC_API_KEY"), r.BareID)
		}
	}
}

var intgULIDEnc = base32.StdEncoding.WithPadding(base32.NoPadding)

// mintULID mints a real time-ordered ULID for the live assembler (the unit gate
// uses a deterministic stub; the live checkpoint needs real, unique ids).
func mintULID() string {
	var b [16]byte
	ms := uint64(time.Now().UnixMilli())
	b[0] = byte(ms >> 40)
	b[1] = byte(ms >> 32)
	b[2] = byte(ms >> 24)
	b[3] = byte(ms >> 16)
	b[4] = byte(ms >> 8)
	b[5] = byte(ms)
	var r [10]byte
	_, _ = rand.Read(r[:])
	copy(b[6:], r[:])
	return intgULIDEnc.EncodeToString(b[:])
}

func TestDocumentPassIntegration(t *testing.T) {
	if os.Getenv("ANTHROPIC_API_KEY") == "" && os.Getenv("OPENAI_API_KEY") == "" {
		t.Log("INTEGRATION CHECKPOINT SKIPPED — no keys")
		t.Skip("no provider keys present")
	}

	cfg, err := config.Load(os.Getenv)
	if err != nil {
		t.Fatalf("config.Load: %v", err)
	}

	s, conn := newStore(t)
	const inboxID = "01HXBLUNTINBOX0000000000001"
	insertInbox(t, conn, inboxID, "document", "mcp:x")

	store := page.NewStore(conn)
	w := llm.New(liveDocFactory(), nil)
	caller := integrate.NewWrapperCaller(w)

	ex := integrate.NewExtractor(caller, cfg.LLM.Extract)
	res := integrate.NewResolver(store, cfg.CandidateLimit)
	matcher := integrate.NewMatcher(caller, store, cfg.LLM.Match, cfg.MatchExcerptChars)
	asm := integrate.NewAssembler(matcher, mintULID)
	merger := integrate.NewMerger(caller, store, cfg.LLM.Merge)

	// A blunt fixture: one obvious subject with a corroborating claim.
	doc := integrate.NewDocument(fakeDocSource{
		row: integrate.DocumentRow{
			ID: inboxID, Source: "mcp", Title: "Apple memo",
			ReceivedAt: time.Date(2024, 1, 2, 0, 0, 0, 0, time.UTC),
		},
		payload: []byte("Apple Inc. is a technology company headquartered in Cupertino, California. " +
			"Tim Cook is its chief executive officer."),
	}, ex, res, asm, merger)

	ctx, cancel := context.WithTimeout(context.Background(), 180*time.Second)
	defer cancel()

	runID, err := s.Begin(ctx, doc.Job(), inboxID)
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	m, err := doc.Integrate(ctx, integrate.Unit{CausedBy: inboxID})
	if err != nil {
		t.Fatalf("live document pass failed (checkpoint RED — investigate): %v", err)
	}
	if err := s.Commit(ctx, runID, inboxID, m, true); err != nil {
		t.Fatalf("commit (checkpoint RED — investigate): %v", err)
	}

	// Structural: at least one page row exists.
	var nPages int
	conn.QueryRow(`SELECT COUNT(1) FROM pages`).Scan(&nPages)
	if nPages < 1 {
		t.Fatalf("no page row written (checkpoint RED): nPages=%d", nPages)
	}

	// Structural: at least one page body carries a [inbox-id] citation matching the
	// causing source row (provenance on real output — never asserting content).
	cite := regexp.MustCompile(`\[` + regexp.QuoteMeta(inboxID) + `\]`)
	rows, err := conn.Query(`SELECT body FROM pages`)
	if err != nil {
		t.Fatalf("read pages: %v", err)
	}
	defer rows.Close()
	var cited bool
	for rows.Next() {
		var body string
		if err := rows.Scan(&body); err != nil {
			t.Fatalf("scan page: %v", err)
		}
		if cite.MatchString(body) {
			cited = true
		}
	}
	if !cited {
		t.Errorf("no page cites the source inbox id %q (checkpoint RED — provenance missing)", inboxID)
	}

	// Structural: the registry has at least one subject and at least one alias.
	var nSubj, nAlias int
	conn.QueryRow(`SELECT COUNT(1) FROM subjects`).Scan(&nSubj)
	conn.QueryRow(`SELECT COUNT(1) FROM aliases`).Scan(&nAlias)
	if nSubj < 1 || nAlias < 1 {
		t.Errorf("registry not populated (checkpoint RED): subjects=%d aliases=%d", nSubj, nAlias)
	}

	t.Logf("document-pass checkpoint GREEN: pages=%d subjects=%d aliases=%d cited=%v",
		nPages, nSubj, nAlias, cited)
}
