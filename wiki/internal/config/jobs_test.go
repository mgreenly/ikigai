package config

import (
	"strings"
	"testing"
)

func twoDigests() Jobs {
	return Jobs{Entries: []Job{
		{Name: "crm-digest", Kind: JobDigest, Trigger: "cron.daily", SourcePrefixes: []string{"crm"}},
		{Name: "ledger-digest", Kind: JobDigest, Trigger: "cron.daily", SourcePrefixes: []string{"ledger"}},
		{Name: "lint-dups", Kind: JobLint, Trigger: "cron.weekly"},
	}}
}

// TestPartitionCheckPasses: disjoint selectors covering every consumed source boot.
func TestPartitionCheckPasses(t *testing.T) {
	if err := twoDigests().PartitionCheck([]string{"crm", "ledger"}); err != nil {
		t.Fatalf("partition check should pass: %v", err)
	}
}

// TestPartitionCheckOverlapRefuses: two digests matching one source → boot refusal.
func TestPartitionCheckOverlapRefuses(t *testing.T) {
	j := Jobs{Entries: []Job{
		{Name: "a", Kind: JobDigest, Trigger: "cron.daily", SourcePrefixes: []string{"crm"}},
		{Name: "b", Kind: JobDigest, Trigger: "cron.daily", SourcePrefixes: []string{"crm"}},
	}}
	err := j.PartitionCheck([]string{"crm"})
	if err == nil || !strings.Contains(err.Error(), "overlap") {
		t.Fatalf("overlap must refuse to boot, got: %v", err)
	}
}

// TestPartitionCheckUnmatchedRefuses: a consumed source no digest matches is
// surfaced (events would accumulate forever, never compiled).
func TestPartitionCheckUnmatchedRefuses(t *testing.T) {
	j := Jobs{Entries: []Job{
		{Name: "crm-digest", Kind: JobDigest, Trigger: "cron.daily", SourcePrefixes: []string{"crm"}},
	}}
	err := j.PartitionCheck([]string{"crm", "ledger"})
	if err == nil || !strings.Contains(err.Error(), "no digest selector") {
		t.Fatalf("unmatched consumed source must be surfaced, got: %v", err)
	}
}

// TestPartitionCheckUnusedSelectorRefuses: a selector matching no consumed source
// is a config error (a digest that would silently never fire).
func TestPartitionCheckUnusedSelectorRefuses(t *testing.T) {
	j := Jobs{Entries: []Job{
		{Name: "crm-digest", Kind: JobDigest, Trigger: "cron.daily", SourcePrefixes: []string{"crm"}},
		{Name: "ghost-digest", Kind: JobDigest, Trigger: "cron.daily", SourcePrefixes: []string{"nope"}},
	}}
	err := j.PartitionCheck([]string{"crm"})
	if err == nil || !strings.Contains(err.Error(), "no consumed source") {
		t.Fatalf("unused selector must be surfaced, got: %v", err)
	}
}

// TestPartitionCheckDigestWithoutSelectorRefuses: a digest with no selector is
// illegal (only lint entries omit a selector — §6).
func TestPartitionCheckDigestWithoutSelectorRefuses(t *testing.T) {
	j := Jobs{Entries: []Job{
		{Name: "crm-digest", Kind: JobDigest, Trigger: "cron.daily"},
	}}
	if err := j.PartitionCheck([]string{"crm"}); err == nil {
		t.Fatal("a digest with no selector must refuse to boot")
	}
}

// TestBindingsFanOut: a cron schedule binds the entries triggered by it (the cron
// fan-out the worker reads to run each bound entry as its own run).
func TestBindingsFanOut(t *testing.T) {
	b := twoDigests().Bindings()
	daily := b["daily"]
	if len(daily) != 2 || daily[0] != "crm-digest" || daily[1] != "ledger-digest" {
		t.Fatalf("daily bindings = %v, want sorted [crm-digest ledger-digest]", daily)
	}
	if len(b["weekly"]) != 1 || b["weekly"][0] != "lint-dups" {
		t.Fatalf("weekly bindings = %v, want [lint-dups]", b["weekly"])
	}
}

// TestDigestEntriesFiltersLint: DigestEntries returns only digest entries.
func TestDigestEntriesFiltersLint(t *testing.T) {
	d := twoDigests().DigestEntries()
	if len(d) != 2 {
		t.Fatalf("digest entries = %d, want 2 (lint excluded)", len(d))
	}
	for _, e := range d {
		if e.Kind != JobDigest {
			t.Errorf("non-digest in DigestEntries: %+v", e)
		}
	}
}

// TestLintEntriesFiltersDigest: LintEntries returns only lint entries (no
// selector), and the lint entry's trigger is bound by the cron fan-out so a
// cron.weekly tick runs it (the shared lint plumbing, P9a).
func TestLintEntriesFiltersDigest(t *testing.T) {
	l := twoDigests().LintEntries()
	if len(l) != 1 || l[0].Name != "lint-dups" {
		t.Fatalf("lint entries = %+v, want just lint-dups", l)
	}
	if len(l[0].SourcePrefixes) != 0 {
		t.Errorf("a lint entry must carry no selector: %+v", l[0])
	}
	// The lint entry participates in the cron fan-out like any other job.
	b := twoDigests().Bindings()
	if got := b["weekly"]; len(got) != 1 || got[0] != "lint-dups" {
		t.Errorf("weekly bindings = %v, want [lint-dups]", got)
	}
}

// TestStandardLintEntriesRegistersSweep: lint-sweep is a registered no-selector
// lint entry bound to a cron trigger (design §6's jobs yaml), so the worker spine
// selects it like any other job.
func TestStandardLintEntriesRegistersSweep(t *testing.T) {
	entries := StandardLintEntries()
	var sweep *Job
	for i := range entries {
		if entries[i].Kind != JobLint {
			t.Fatalf("standard lint entry must be JobLint: %+v", entries[i])
		}
		if len(entries[i].SourcePrefixes) != 0 {
			t.Fatalf("a lint entry has no selector: %+v", entries[i])
		}
		if entries[i].Name == "lint-sweep" {
			sweep = &entries[i]
		}
	}
	if sweep == nil {
		t.Fatal("lint-sweep not registered in StandardLintEntries")
	}
	if scheduleName(sweep.Trigger) == "" {
		t.Fatalf("lint-sweep must bind a cron schedule, got trigger %q", sweep.Trigger)
	}
}
