package mcp

import (
	"context"
	"testing"

	"wiki/internal/ingest"
	"wiki/internal/store"
)

// stubIngester is a test double for the Ingester seam: it records the last call
// and returns canned results, proving the verb → core wiring (arg parsing,
// owner threading, result shaping) without spawning a real agent job.
type stubIngester struct {
	gotOwner   string
	gotContent string
	gotURL     string
	gotMeta    store.RawMeta
	result     ingest.Result
	status     ingest.Status
	statusErr  error
}

func (s *stubIngester) Ingest(_ context.Context, owner, _ string, content []byte, meta store.RawMeta) (ingest.Result, error) {
	s.gotOwner = owner
	s.gotContent = string(content)
	s.gotMeta = meta
	return s.result, nil
}

func (s *stubIngester) IngestURL(_ context.Context, owner, _, url string, meta store.RawMeta) (ingest.Result, error) {
	s.gotOwner = owner
	s.gotURL = url
	s.gotMeta = meta
	return s.result, nil
}

func (s *stubIngester) JobStatus(_ context.Context, _, _, jobID string) (ingest.Status, error) {
	if s.statusErr != nil {
		return ingest.Status{}, s.statusErr
	}
	s.status.JobID = jobID
	return s.status, nil
}

func TestIngestText_Verb(t *testing.T) {
	stub := &stubIngester{result: ingest.Result{JobID: "job-xyz", Sha256: "abc123", RawRelPath: "raw/abc123.md", AlreadyHad: false}}
	h := NewHandler(stub, nil, nil, "1.2.3", "wiki", nil)

	p, isErr := callTool(t, h, "ikigenba_wiki_ingest_text",
		`{"content":"record this","title":"T","source":"chat","tags":["a","b"]}`)
	if isErr {
		t.Fatalf("ingest_text returned isError: %v", p)
	}
	if p["job_id"] != "job-xyz" || p["sha256"] != "abc123" {
		t.Fatalf("ingest_text result = %v", p)
	}
	if stub.gotOwner != "me@example.com" {
		t.Fatalf("owner threaded = %q, want me@example.com", stub.gotOwner)
	}
	if stub.gotContent != "record this" {
		t.Fatalf("content = %q", stub.gotContent)
	}
	if stub.gotMeta.Title != "T" || stub.gotMeta.Source != "chat" || len(stub.gotMeta.Tags) != 2 {
		t.Fatalf("meta = %+v", stub.gotMeta)
	}
}

func TestIngestText_RequiresContent(t *testing.T) {
	h := NewHandler(&stubIngester{}, nil, nil, "1.2.3", "wiki", nil)
	_, isErr := callTool(t, h, "ikigenba_wiki_ingest_text", `{"title":"no body"}`)
	if !isErr {
		t.Fatal("ingest_text with empty content should be a tool-error")
	}
}

func TestIngestText_NilCoreUnavailable(t *testing.T) {
	h := NewHandler(nil, nil, nil, "1.2.3", "wiki", nil)
	_, isErr := callTool(t, h, "ikigenba_wiki_ingest_text", `{"content":"x"}`)
	if !isErr {
		t.Fatal("ingest_text with nil core should be a tool-error")
	}
}

func TestIngestURL_Verb(t *testing.T) {
	stub := &stubIngester{result: ingest.Result{JobID: "job-url", Sha256: "def456", RawRelPath: "raw/def456.md", AlreadyHad: false}}
	h := NewHandler(stub, nil, nil, "1.2.3", "wiki", nil)

	p, isErr := callTool(t, h, "ikigenba_wiki_ingest_url",
		`{"url":"https://example.com/page","tags":["web"]}`)
	if isErr {
		t.Fatalf("ingest_url returned isError: %v", p)
	}
	if p["job_id"] != "job-url" || p["sha256"] != "def456" {
		t.Fatalf("ingest_url result = %v", p)
	}
	if stub.gotOwner != "me@example.com" {
		t.Fatalf("owner threaded = %q, want me@example.com", stub.gotOwner)
	}
	if stub.gotURL != "https://example.com/page" {
		t.Fatalf("url threaded = %q", stub.gotURL)
	}
	if len(stub.gotMeta.Tags) != 1 || stub.gotMeta.Tags[0] != "web" {
		t.Fatalf("meta tags = %+v", stub.gotMeta.Tags)
	}
}

func TestIngestURL_RequiresURL(t *testing.T) {
	h := NewHandler(&stubIngester{}, nil, nil, "1.2.3", "wiki", nil)
	_, isErr := callTool(t, h, "ikigenba_wiki_ingest_url", `{"title":"no url"}`)
	if !isErr {
		t.Fatal("ingest_url with empty url should be a tool-error")
	}
}

func TestIngestURL_NilCoreUnavailable(t *testing.T) {
	h := NewHandler(nil, nil, nil, "1.2.3", "wiki", nil)
	_, isErr := callTool(t, h, "ikigenba_wiki_ingest_url", `{"url":"https://example.com"}`)
	if !isErr {
		t.Fatal("ingest_url with nil core should be a tool-error")
	}
}

func TestJobStatus_Verb(t *testing.T) {
	stub := &stubIngester{status: ingest.Status{Status: "succeeded", Terminal: true, StartedAt: "2026-06-04T00:00:00Z"}}
	h := NewHandler(stub, nil, nil, "1.2.3", "wiki", nil)
	p, isErr := callTool(t, h, "ikigenba_wiki_job_status", `{"job_id":"job-xyz"}`)
	if isErr {
		t.Fatalf("job_status isError: %v", p)
	}
	if p["job_id"] != "job-xyz" || p["status"] != "succeeded" || p["terminal"] != true {
		t.Fatalf("job_status result = %v", p)
	}
}

func TestJobStatus_NotFound(t *testing.T) {
	stub := &stubIngester{statusErr: ingest.ErrJobNotFound}
	h := NewHandler(stub, nil, nil, "1.2.3", "wiki", nil)
	_, isErr := callTool(t, h, "ikigenba_wiki_job_status", `{"job_id":"ghost"}`)
	if !isErr {
		t.Fatal("job_status for unknown id should be a tool-error")
	}
}
