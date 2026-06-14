package integrate

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"

	"agentkit/provider"

	"wiki/internal/config"
)

// Merge is the document pass's write stage (design §4.4): ONE agent run per
// document that folds the manifest's resolved subjects into prose pages. Input is
// the MANIFEST ONLY (never the original document — the raw document invites
// re-extraction past the salience gate). The write set is the manifest's pages
// EXACTLY (the subjects' target pages); the read set is looser (neighbors for
// context). Per page it folds that subject's claims as prose — new info woven in,
// already-known corroborated with the new citation, contradictions corralled with
// both sides + citations kept.
//
// Merge's read+write-page work is CAPTURED into the manifest (the per-subject
// PageTitle/PageBody slots, the per-page Superseded list, and the manifest's
// StaleNotes carrier) rather than written directly — the end-of-run transaction
// owns the only write, so there are zero mid-run partial writes (§4.5). Merge also
// records, for each page it reads, the base pages.version into the manifest's
// per-subject BaseVersion slot (design §3 "the version merge read"); P7b's
// optimistic-commit guard consumes it.
//
// Merge is a clean, externally-callable function over an injected
// (prompt, model, effort) triple (eval obligation 1): the harness scores it by
// swapping config.CallSite, calling the same function. P7a wires merge against a
// PLACEHOLDER config-default prompt under mocks; merge's REAL config-default prompt
// and its offline gate land in P7a2.

// pageReader is the minimal slice of *page.Store merge needs for its read set: the
// current page content (the read-page tool) and the base version (design §3).
// Declared as an interface so the stage is unit-testable without a live DB.
type pageReader interface {
	ReadPage(ctx context.Context, subject string) (title, body string, ok bool, err error)
	ReadVersion(ctx context.Context, subject string) (int, error)
}

// Merger runs the merge stage with an injected call-site triple. Construct it once
// at the composition root; the document integrator calls Merge per document.
type Merger struct {
	caller structuredCaller
	reg    pageReader
	site   config.CallSite
}

// NewMerger builds a Merger over a structured caller, the page reader, and the
// merge call-site triple. The triple (prompt/model/effort) is injected — Merge
// never reads a constant or env (design §10 / obligation 1).
func NewMerger(caller structuredCaller, reg pageReader, site config.CallSite) *Merger {
	return &Merger{caller: caller, reg: reg, site: site}
}

// rawMergeOutput is merge's wire shape: the rewritten pages (one per target
// subject, with the §6.1 superseded list) plus any stale notes merge surfaced
// while folding (design §4.4 / §6 / §6.1). The agent's write-page tool calls are
// modeled as this captured output so the end-of-run transaction — not merge —
// performs the only write.
type rawMergeOutput struct {
	Pages []struct {
		Subject    string   `json:"subject"`
		Title      string   `json:"title"`
		Body       string   `json:"body"`
		Superseded []string `json:"superseded"`
	} `json:"pages"`
	StaleNotes []struct {
		Subject string   `json:"subject"`
		Note    string   `json:"note"`
		Cites   []string `json:"cites"`
	} `json:"stale_notes"`
}

// Merge folds the manifest's subjects into prose pages. It mutates the passed
// manifest in place: for each subject it records the base version slot (read at
// merge time), and fills the per-subject PageTitle/PageBody + Superseded from
// merge's output; it appends merge's stale notes to the manifest's StaleNotes
// carrier. The manifest's write set is unchanged (the subjects' target pages,
// exactly). Returns the manifest for caller convenience.
//
// The base version is recorded for EVERY target page BEFORE the call (design §3:
// "the manifest records the version merge read") so P7b's optimistic-commit guard
// has it even when merge leaves a page's content unchanged.
func (m *Merger) Merge(ctx context.Context, manifest *Manifest) (*Manifest, error) {
	if manifest == nil {
		return nil, fmt.Errorf("merge: nil manifest")
	}

	// Record the base version for each target page (the slot P7b's guard consumes)
	// and read the current page content for the merge evidence.
	reads := make(map[string]mergeRead, len(manifest.Subjects))
	for i := range manifest.Subjects {
		subj := &manifest.Subjects[i]
		if subj.TargetPage == "" {
			continue
		}
		v, err := m.reg.ReadVersion(ctx, subj.TargetPage)
		if err != nil {
			return nil, fmt.Errorf("merge: read base version %q: %w", subj.TargetPage, err)
		}
		subj.BaseVersion = v
		if _, seen := reads[subj.TargetPage]; !seen {
			title, body, ok, err := m.reg.ReadPage(ctx, subj.TargetPage)
			if err != nil {
				return nil, fmt.Errorf("merge: read page %q: %w", subj.TargetPage, err)
			}
			reads[subj.TargetPage] = mergeRead{title: title, body: body, exists: ok}
		}
	}

	user := renderMergeInput(manifest, reads)
	msgs := []provider.Message{{
		Role:   provider.RoleUser,
		Blocks: []provider.Block{provider.TextBlock{Text: user}},
	}}

	raw, err := m.caller.Structured(ctx, m.site, MergeSchema, msgs)
	if err != nil {
		return nil, fmt.Errorf("merge: structured call: %w", err)
	}
	return ApplyMerge(manifest, raw)
}

// mergeRead is the current page content merge read for one target page (the
// read-page evidence). exists is false for a not-yet-created page.
type mergeRead struct {
	title, body string
	exists      bool
}

// renderMergeInput builds the merge user message from the manifest ONLY (never the
// original document — design §4.4). Per target subject it shows the registry
// identity, the claims to fold, and the current page body (if any) so merge weaves
// rather than overwrites. Deterministic for a fixed manifest + read set.
func renderMergeInput(manifest *Manifest, reads map[string]mergeRead) string {
	var b strings.Builder
	b.WriteString("--- merge work order (manifest) ---\n")
	for i := range manifest.Subjects {
		s := &manifest.Subjects[i]
		fmt.Fprintf(&b, "\nsubject %d:\n", i+1)
		fmt.Fprintf(&b, "  id: %s\n", s.SubjectID)
		fmt.Fprintf(&b, "  type: %s\n", s.Type)
		fmt.Fprintf(&b, "  name: %s\n", s.Name)
		if len(s.Aliases) > 0 {
			fmt.Fprintf(&b, "  aliases: %s\n", strings.Join(s.Aliases, ", "))
		}
		b.WriteString("  claims:\n")
		for _, c := range s.Claims {
			fmt.Fprintf(&b, "    - %s [cites: %s]\n", c.Text, strings.Join(c.Cites, ", "))
		}
		if r, ok := reads[s.TargetPage]; ok && r.exists {
			fmt.Fprintf(&b, "  current page body:\n%s\n", r.body)
		} else {
			b.WriteString("  current page body: (new page)\n")
		}
	}
	b.WriteString("--- end work order ---\n")
	return b.String()
}

// ApplyMerge parses and validates a merge response body and folds it into the
// manifest: per-subject PageTitle/PageBody + Superseded, plus the manifest-level
// StaleNotes carrier. It is separated from the call so the prompt-default gate and
// goldens can exercise the parser + schema offline against a committed fixture,
// with no client (obligation 5 / the standing prompt gate, P7a2). Every page in
// merge's output must name a subject in the manifest's write set (merge may not
// invent a page outside its license — design §4.4 "write set = the manifest's
// pages, exactly").
func ApplyMerge(manifest *Manifest, raw string) (*Manifest, error) {
	var out rawMergeOutput
	if err := json.Unmarshal([]byte(stripCodeFence(raw)), &out); err != nil {
		return nil, fmt.Errorf("merge: parse response: %w", err)
	}

	// Index subjects by their target page for the write-set conformance check and
	// the per-subject fill.
	bySubject := make(map[string]*Subject, len(manifest.Subjects))
	for i := range manifest.Subjects {
		s := &manifest.Subjects[i]
		if s.TargetPage != "" {
			bySubject[s.TargetPage] = s
		}
	}

	seen := make(map[string]struct{}, len(out.Pages))
	for _, p := range out.Pages {
		subj := strings.TrimSpace(p.Subject)
		target, ok := bySubject[subj]
		if !ok {
			return nil, fmt.Errorf("merge: output page %q is not in the manifest's write set", subj)
		}
		if _, dup := seen[subj]; dup {
			return nil, fmt.Errorf("merge: output names page %q twice", subj)
		}
		seen[subj] = struct{}{}
		if strings.TrimSpace(p.Body) == "" {
			return nil, fmt.Errorf("merge: output page %q has an empty body", subj)
		}
		target.PageTitle = strings.TrimSpace(p.Title)
		target.PageBody = p.Body
		target.Superseded = cleanCites(p.Superseded)
	}

	// Every write-set page must be written (merge owes a body for each subject it
	// was handed — the write set is the manifest's pages, exactly).
	for tgt := range bySubject {
		if _, ok := seen[tgt]; !ok {
			return nil, fmt.Errorf("merge: write-set page %q was not written", tgt)
		}
	}

	// Fold merge's stale notes into the manifest's carrier (the end-of-run
	// transaction writes them — §6).
	for _, sn := range out.StaleNotes {
		s := strings.TrimSpace(sn.Subject)
		note := strings.TrimSpace(sn.Note)
		if s == "" || note == "" {
			continue
		}
		manifest.StaleNotes = append(manifest.StaleNotes, StaleNote{
			Subject: s,
			Note:    note,
			Cites:   cleanCites(sn.Cites),
		})
	}
	return manifest, nil
}

// cleanCites trims and drops empty entries from a citation/id list.
func cleanCites(in []string) []string {
	out := make([]string, 0, len(in))
	for _, c := range in {
		if t := strings.TrimSpace(c); t != "" {
			out = append(out, t)
		}
	}
	if len(out) == 0 {
		return nil
	}
	return out
}
