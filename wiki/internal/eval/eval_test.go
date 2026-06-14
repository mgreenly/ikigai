package eval

import (
	"context"
	"encoding/json"
	"path/filepath"
	"sync/atomic"
	"testing"

	"wiki/internal/config"
)

func TestLoadDatasetBareArray(t *testing.T) {
	ds, err := LoadDataset(filepath.Join("..", "..", "testsets", "match", "datasets", "gen-1.json"))
	if err != nil {
		t.Fatalf("LoadDataset: %v", err)
	}
	// P15 grew the match set beyond P13's seed pair (blunt -> subtle), so assert the
	// floor and find the seed case by id rather than pinning an exact count/order.
	if len(ds.Cases) < 2 {
		t.Fatalf("want >=2 cases, got %d", len(ds.Cases))
	}
	var seed *Case
	for i := range ds.Cases {
		if ds.Cases[i].CaseID == "match-0001" {
			seed = &ds.Cases[i]
		}
		if ds.Cases[i].Site != "match" {
			t.Errorf("case %s: site = %q, want match", ds.Cases[i].CaseID, ds.Cases[i].Site)
		}
		if ds.Cases[i].Generation != 1 {
			t.Errorf("case %s: generation = %d, want 1", ds.Cases[i].CaseID, ds.Cases[i].Generation)
		}
	}
	if seed == nil {
		t.Fatal("seed case match-0001 not found")
	}
	// The seed input must round-trip as the match site's shape.
	var mi matchInput
	if err := json.Unmarshal(seed.Input, &mi); err != nil {
		t.Fatalf("decode case input: %v", err)
	}
	if mi.Incoming.Name != "Apple Inc." || len(mi.Candidates) != 2 {
		t.Errorf("unexpected decoded match input: %+v", mi)
	}
}

func TestLoadBundle(t *testing.T) {
	siteDir := filepath.Join("..", "..", "testsets", "match")
	b, ds, dsBytes, promptBytes, err := LoadBundle(siteDir, filepath.Join(siteDir, "bundles", "gen-1.json"))
	if err != nil {
		t.Fatalf("LoadBundle: %v", err)
	}
	if b.Dataset != "datasets/gen-1.json" {
		t.Errorf("bundle dataset = %q", b.Dataset)
	}
	if len(ds.Cases) < 2 {
		t.Errorf("want >=2 cases, got %d", len(ds.Cases))
	}
	if len(dsBytes) == 0 {
		t.Error("dataset bytes empty")
	}
	// P15's match bundle now names a prompt artifact (the production prompt is just a
	// prompt artifact — eval design q3), so prompt bytes are present.
	if len(promptBytes) == 0 {
		t.Error("expected the bundle's named prompt artifact bytes, got none")
	}
}

func TestCacheRoundTripAndMiss(t *testing.T) {
	c, err := NewCache(t.TempDir())
	if err != nil {
		t.Fatalf("NewCache: %v", err)
	}
	key := CacheKey{DatasetHash: "d", CaseID: "match-0001", PromptHash: "p", Model: "claude-haiku-4-5", Effort: ""}
	if _, err := c.Get(key); err != ErrCacheMiss {
		t.Fatalf("want ErrCacheMiss, got %v", err)
	}
	want := CachedOutput{Raw: json.RawMessage(`{"same":"x"}`), CostUSD: 0.0012, LatencyMS: 345}
	if err := c.Put(key, want); err != nil {
		t.Fatalf("Put: %v", err)
	}
	got, err := c.Get(key)
	if err != nil {
		t.Fatalf("Get after Put: %v", err)
	}
	if got.CostUSD != want.CostUSD || got.LatencyMS != want.LatencyMS || string(got.Raw) != string(want.Raw) {
		t.Errorf("round-trip mismatch: got %+v want %+v", got, want)
	}
	// A different effort is a distinct key (the cache key includes effort).
	if _, err := c.Get(CacheKey{DatasetHash: "d", CaseID: "match-0001", PromptHash: "p", Model: "claude-haiku-4-5", Effort: "high"}); err != ErrCacheMiss {
		t.Errorf("distinct effort should miss, got %v", err)
	}
}

func TestCacheKeyHashDistinct(t *testing.T) {
	a := CacheKey{DatasetHash: "ab", CaseID: "c", PromptHash: "p", Model: "m", Effort: ""}.Hash()
	// Field-boundary collision guard: shifting a char across the boundary must not collide.
	b := CacheKey{DatasetHash: "a", CaseID: "bc", PromptHash: "p", Model: "m", Effort: ""}.Hash()
	if a == b {
		t.Error("cache key hash collides across field boundary")
	}
}

// fakeCall counts invocations and returns a fixed output — it proves the runner's
// cache reproduces a second run entirely from cache (zero calls), the P13 Verify.
type fakeCall struct{ n int32 }

func (f *fakeCall) call(_ context.Context, _ config.CallSite, _ json.RawMessage) (json.RawMessage, error) {
	atomic.AddInt32(&f.n, 1)
	return json.RawMessage(`{"same":"01HXCANDIDATEAPPLEINC000001","dup_pairs":[]}`), nil
}

func TestRunnerCachesSecondRun(t *testing.T) {
	siteDir := filepath.Join("..", "..", "testsets", "match")
	_, ds, dsBytes, promptBytes, err := LoadBundle(siteDir, filepath.Join(siteDir, "bundles", "gen-1.json"))
	if err != nil {
		t.Fatalf("LoadBundle: %v", err)
	}
	cache, err := NewCache(t.TempDir())
	if err != nil {
		t.Fatalf("NewCache: %v", err)
	}
	cap := NewCaptureHandler()
	fc := &fakeCall{}
	grid := []ModelEffort{{Model: "claude-haiku-4-5"}}

	want := int32(len(ds.Cases)) // one call per case for the single grid point

	r1 := NewRunner("match", fc.call, cache, cap, dsBytes, promptBytes, config.DefaultMatchPrompt)
	res1, err := r1.Run(context.Background(), ds, grid)
	if err != nil {
		t.Fatalf("first Run: %v", err)
	}
	if len(res1) != len(ds.Cases) {
		t.Fatalf("want %d results, got %d", len(ds.Cases), len(res1))
	}
	if got := atomic.LoadInt32(&fc.n); got != want {
		t.Fatalf("first run should make %d calls, made %d", want, got)
	}
	for _, r := range res1 {
		if r.Cached {
			t.Errorf("first run case %s should not be cached", r.CaseID)
		}
	}

	// Second run, fresh runner + same cache: zero paid calls (the P13 Verify).
	r2 := NewRunner("match", fc.call, cache, cap, dsBytes, promptBytes, config.DefaultMatchPrompt)
	res2, err := r2.Run(context.Background(), ds, grid)
	if err != nil {
		t.Fatalf("second Run: %v", err)
	}
	if got := atomic.LoadInt32(&fc.n); got != want {
		t.Errorf("second run made paid calls (total %d, want %d) — cache not reproducing", got, want)
	}
	for _, r := range res2 {
		if !r.Cached {
			t.Errorf("second run case %s should be cached", r.CaseID)
		}
	}
}

func TestRunnerRejectsBadSweepConfig(t *testing.T) {
	siteDir := filepath.Join("..", "..", "testsets", "match")
	_, ds, dsBytes, promptBytes, _ := LoadBundle(siteDir, filepath.Join(siteDir, "bundles", "gen-1.json"))
	cache, _ := NewCache(t.TempDir())
	fc := &fakeCall{}
	r := NewRunner("match", fc.call, cache, NewCaptureHandler(), dsBytes, promptBytes, config.DefaultMatchPrompt)
	_, err := r.Run(context.Background(), ds, []ModelEffort{{Model: "no-such-model-xyz"}})
	if err == nil {
		t.Fatal("expected an error for an unresolvable sweep model")
	}
	if got := atomic.LoadInt32(&fc.n); got != 0 {
		t.Errorf("a bad sweep config must fail before any paid call, made %d", got)
	}
}

func TestBuildTablePerConfig(t *testing.T) {
	results := []CaseResult{
		{CaseID: "a", Model: "m1", Effort: "", CostUSD: 0.001, LatencyMS: 100},
		{CaseID: "b", Model: "m1", Effort: "", CostUSD: 0.003, LatencyMS: 300, Cached: true},
		{CaseID: "a", Model: "m2", Effort: "high", CostUSD: 0.010, LatencyMS: 500},
	}
	tbl := BuildTable(1, "match", results)
	if tbl.Generation != 1 || tbl.Site != "match" {
		t.Errorf("caption mismatch: %+v", tbl)
	}
	if len(tbl.Rows) != 2 {
		t.Fatalf("want 2 config rows, got %d", len(tbl.Rows))
	}
	// m1 row: 2 cases, 1 cached, total 0.004.
	var m1 *Row
	for i := range tbl.Rows {
		if tbl.Rows[i].Model == "m1" {
			m1 = &tbl.Rows[i]
		}
	}
	if m1 == nil {
		t.Fatal("missing m1 row")
	}
	if m1.Cases != 2 || m1.Cached != 1 {
		t.Errorf("m1 coverage wrong: %+v", *m1)
	}
	if m1.TotalCostUSD < 0.0039 || m1.TotalCostUSD > 0.0041 {
		t.Errorf("m1 total cost = %v", m1.TotalCostUSD)
	}
	if got := tbl.Render(); !contains(got, "generation=gen-1") || !contains(got, "m1") {
		t.Errorf("render missing caption/config: %q", got)
	}
}

func TestCaptureHandlerSumsCostLatency(t *testing.T) {
	h := NewCaptureHandler()
	lg := h.Logger()
	lg.Info("api_call", "cost_usd", 0.002, "duration_ms", int64(120))
	lg.Info("api_call", "cost_usd", 0.003, "duration_ms", int64(80))
	cost, dur, calls := h.Result()
	if calls != 2 {
		t.Errorf("calls = %d, want 2", calls)
	}
	if cost < 0.0049 || cost > 0.0051 {
		t.Errorf("cost = %v, want ~0.005", cost)
	}
	if dur != 200 {
		t.Errorf("dur = %d, want 200", dur)
	}
	h.Reset()
	cost, dur, calls = h.Result()
	if cost != 0 || dur != 0 || calls != 0 {
		t.Errorf("after Reset: cost=%v dur=%d calls=%d", cost, dur, calls)
	}
}

func contains(s, sub string) bool {
	return len(s) >= len(sub) && (func() bool {
		for i := 0; i+len(sub) <= len(s); i++ {
			if s[i:i+len(sub)] == sub {
				return true
			}
		}
		return false
	})()
}
