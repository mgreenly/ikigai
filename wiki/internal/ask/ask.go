package ask

import (
	"bytes"
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"time"

	"agentkit/agent"
	"agentkit/job"
	"agentkit/provider"
	"agentkit/wire"

	"wiki/internal/ids"
	"wiki/internal/ingest"
	"wiki/internal/jobstore"
	"wiki/internal/search"
	"wiki/internal/store"
)

// ClientFactory builds a provider.Client for the ask model. main.go supplies the
// real anthropic client factory (which closes over ANTHROPIC_API_KEY); tests
// supply a stub. Returning an error lets ask fail the job cleanly when no client
// can be built (e.g. a missing key at run time), rather than panicking. Identical
// in shape to ingest.ClientFactory / lint.ClientFactory.
type ClientFactory func() (provider.Client, error)

// Config carries the injected ask knobs (model + cost ceiling + TTL), read at
// cmd/wiki/main.go's composition root (PLAN Decision 3: model + cost ceiling are
// config, not hardcoded). A zero MaxTokens applies the package default; a zero
// TTL means the job is bounded only by Cancel and the model.
type Config struct {
	Model     string        // ask model id (default DefaultModel)
	MaxTokens int           // per-job output-token ceiling (cost knob)
	JobTTL    time.Duration // per-run wall-clock TTL (0 = no deadline)
}

// withDefaults fills unset Config fields with package defaults.
func (c Config) withDefaults() Config {
	if c.Model == "" {
		c.Model = DefaultModel
	}
	if c.MaxTokens <= 0 {
		c.MaxTokens = DefaultMaxTokens
	}
	return c
}

// Asker is the wiki's agentic synthesis pass. It owns the injected collaborators
// and exposes Ask (the wiki_ask entrypoint) and JobStatus (the owner-scoped status
// read, identical in contract to ingest's). It is owner-agnostic: every method
// takes owner+collection.
//
// Like ingest.Core / lint.Linter, Asker constructs a fresh owner-scoped job.Runner
// per Ask, each over an owner-bound jobstore.Store; the shared single-writer
// *sql.DB + the global partial-unique running index enforce single-flight ACROSS
// runners — and across packages, since ask reuses ingest's flight key (ask files a
// synthesis page and re-indexes, so it is a write-pass like ingest/lint). Crash
// recovery is the ingest core's one-shot boot sweep over the whole table (ask
// shares the wiki_jobs table), so Asker does not need its own Recover.
type Asker struct {
	store     *store.Store
	search    search.Index
	db        *sql.DB
	newClient ClientFactory
	cfg       Config
	now       func() time.Time
}

// New builds an Asker. It takes the same collaborators as ingest.New / lint.New
// (the filesystem store, the BM25 index, the shared single-writer DB, a client
// factory, and config), so main.go constructs it from the same dependency graph.
func New(st *store.Store, idx search.Index, db *sql.DB, newClient ClientFactory, cfg Config) *Asker {
	return &Asker{
		store:     st,
		search:    idx,
		db:        db,
		newClient: newClient,
		cfg:       cfg.withDefaults(),
		now:       func() time.Time { return time.Now().UTC() },
	}
}

// Result is what Ask returns: the spawned job id. Poll it with JobStatus (the
// same wiki_job_status path ingest/lint use — ask rows live in the same wiki_jobs
// table). The synthesized, cited answer is filed as a synthesis page in the
// collection (and is therefore immediately findable via wiki_search once the job
// reaches succeeded), the compounding artifact GOALS describes.
type Result struct {
	JobID string
}

// Ask is the wiki_ask entrypoint. It spawns an async agentkit job that runs the
// ask agent (read+glob+grep for index-first navigation, plus write to file the
// synthesis page) over the existing owner+collection page tree and, on success,
// re-indexes the collection so the new synthesis page stays searchable (explorations
// compound). It returns the job id immediately; the synthesis runs in the
// background.
//
// Single-flight: the job uses ingest.FlightKey(owner, collection) — the SAME key
// ingest and lint use — so an ask while an ingest/lint is running (or a second
// write-pass) is rejected with job.ErrFlightInUse. Only one write-pass touches a
// given wiki at a time. On rejection, no job is launched and an empty job id is
// returned alongside the error.
func (a *Asker) Ask(ctx context.Context, owner, collection, question string) (Result, error) {
	if collection == "" {
		collection = store.DefaultCollection
	}

	// Ensure the collection root + page dirs (incl. synthesis/) exist so the
	// confined agent loop has a sandbox to run in and its write tool (which does
	// NOT create parent dirs) can file the synthesis page (idempotent; an ask over
	// a never-ingested wiki just finds an empty tree).
	root, err := a.store.EnsureLayout(owner, collection)
	if err != nil {
		return Result{}, fmt.Errorf("ask: ensure layout: %w", err)
	}

	jobID := ids.NewULID()
	runner := job.New(jobstore.New(a.db, owner, collection), a.cfg.JobTTL)
	j := &askJob{
		asker:       a,
		owner:       owner,
		collection:  collection,
		sandboxRoot: root,
		question:    question,
	}

	rec := job.Record{
		ID:        jobID,
		FlightKey: ingest.FlightKey(owner, collection), // shared write-pass key.
		StartedAt: a.now(),
	}
	if _, err := runner.Spawn(rec, j); err != nil {
		// Single-flight rejection (ErrFlightInUse) or any Insert error: no job was
		// launched. Return an empty job id with the error so the caller learns it.
		return Result{}, err
	}

	return Result{JobID: jobID}, nil
}

// JobStatus reads the ask job's owner-scoped status, reusing ingest's Status
// projection and the same wiki_jobs table the wiki_job_status verb already reads.
// A missing or foreign-owned id returns ingest.ErrJobNotFound. Ask jobs are thus
// observable through the exact same status path as ingest and lint jobs.
func (a *Asker) JobStatus(ctx context.Context, owner, collection, jobID string) (ingest.Status, error) {
	// Delegate to a throwaway ingest.Core over the same db: the status read is
	// owner-scoped over wiki_jobs and carries no ingest-specific state, so an ask
	// job id reads identically. (ingest.Core.JobStatus only touches the db + the
	// jobstore.)
	return ingest.New(a.store, a.search, a.db, a.ingestClientShim(), ingest.Config{}).
		JobStatus(ctx, owner, collection, jobID)
}

// ingestClientShim adapts ask's ClientFactory to ingest.ClientFactory so the
// throwaway ingest.Core used solely for JobStatus is constructible. It is never
// invoked (JobStatus runs no agent), so a nil newClient is tolerated.
func (a *Asker) ingestClientShim() ingest.ClientFactory {
	if a.newClient == nil {
		return func() (provider.Client, error) { return nil, fmt.Errorf("ask: no client") }
	}
	nc := a.newClient
	return func() (provider.Client, error) { return nc() }
}

// askJob is the unit of work the runner spawns: it runs the real agent loop
// (freeform, sch=nil) with the ask toolset + ask prompt, confined to the owner+
// collection root, then on success re-indexes the collection so the freshly-filed
// synthesis page is searchable. It implements agentkit/job.Job — the SAME
// interface the ingest integrationJob and the lintJob implement.
type askJob struct {
	asker       *Asker
	owner       string
	collection  string
	sandboxRoot string
	question    string

	// stream captures the agent's wire output so usage can be extracted (mirrors
	// ingest's integrationJob.stream / lint's lintJob.stream).
	stream bytes.Buffer
}

// Run executes the ask/synthesis pass. It builds the provider client, runs the
// agent loop over a fresh wire session confined to sandboxRoot with the ask
// toolset/prompt, and on success re-indexes the collection. A reindex failure
// fails the job (the synthesis page landed but is not searchable — surfaced, not
// silent). Usage is captured from the wire stream regardless of outcome.
func (j *askJob) Run(ctx context.Context) (string, error) {
	client, err := j.asker.newClient()
	if err != nil {
		return "", fmt.Errorf("ask job: build client: %w", err)
	}

	req := provider.Request{
		Model:        j.asker.cfg.Model,
		MaxTokens:    j.asker.cfg.MaxTokens,
		SystemPrompt: systemPrompt(),
		Messages: []provider.Message{{
			Role: provider.RoleUser,
			Blocks: []provider.Block{provider.TextBlock{
				Text: userMessage(j.owner, j.collection, j.question),
			}},
		}},
		Tools: askToolset(),
	}

	sess := wire.NewSession(&j.stream)
	if err := agent.Run(ctx, client, sess, req, nil /* freeform */, j.sandboxRoot, nil /* no tracer */); err != nil {
		return captureUsage(j.stream.Bytes()), fmt.Errorf("ask job: agent run: %w", err)
	}

	// On success, re-index so the freshly-filed synthesis page is searchable —
	// this is what makes explorations COMPOUND (a subsequent wiki_search finds it).
	if err := search.ReindexCollection(ctx, j.asker.search, storeAdapter{j.asker.store}, j.owner, j.collection); err != nil {
		return captureUsage(j.stream.Bytes()), fmt.Errorf("ask job: reindex: %w", err)
	}

	return captureUsage(j.stream.Bytes()), nil
}

// storeAdapter wraps *store.Store to satisfy search.PageSource (store.PageEntry →
// search.PageEntry). Mirrors ingest's / lint's adapter; lives here so the ask job
// can call ReindexCollection with the real store.
type storeAdapter struct{ s *store.Store }

func (a storeAdapter) WalkPages(owner, collection string) ([]search.PageEntry, error) {
	entries, err := a.s.WalkPages(owner, collection)
	if err != nil {
		return nil, err
	}
	out := make([]search.PageEntry, len(entries))
	for i, e := range entries {
		out[i] = search.PageEntry{RelPath: e.RelPath}
	}
	return out, nil
}

func (a storeAdapter) ReadPage(owner, collection, relPath string) ([]byte, error) {
	return a.s.ReadPage(owner, collection, relPath)
}

// captureUsage extracts the accounting blob from the last result event in the
// wire stream — the same best-effort scan ingest's / lint's core uses, reused
// here to populate Record.UsageJSON.
func captureUsage(streamed []byte) string {
	var out json.RawMessage
	for _, line := range bytes.Split(streamed, []byte("\n")) {
		line = bytes.TrimSpace(line)
		if len(line) == 0 {
			continue
		}
		var ev struct {
			Type  string          `json:"type"`
			Usage json.RawMessage `json:"usage"`
		}
		if err := json.Unmarshal(line, &ev); err != nil || ev.Type != "result" {
			continue
		}
		if len(ev.Usage) > 0 && !bytes.Equal(bytes.TrimSpace(ev.Usage), []byte("null")) {
			out = ev.Usage
		}
	}
	if len(out) == 0 {
		return ""
	}
	b, _ := json.Marshal(map[string]json.RawMessage{"usage": out})
	return string(b)
}
