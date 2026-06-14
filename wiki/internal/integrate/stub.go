package integrate

import (
	"context"
	"errors"
)

// Stub is a no-LLM Integrator the P4 spine exercises end-to-end: it satisfies the
// Integrator interface and emits a Manifest the end-of-run transaction
// round-trips, without doing any real extract/merge work. It is the spine's proof
// that claim → run → commit → stamp works before any real integrator exists, and
// it can be told to fail (FailWith) so P5's failure policy has something to drive.
//
// The document-pass stub and the cron/no-op stub are both just a Stub with a
// different Job name and Manifest factory; the spine treats them identically (it
// "can't tell which integrator ran").
type Stub struct {
	job string
	// build produces the Manifest for a unit; nil means a minimal empty Manifest.
	build func(Unit) *Manifest
	// failWith, if non-nil, is returned instead of running (P5 drives the failure
	// path through it).
	failWith error
}

// NewDocumentStub returns a document-pass stub Integrator (Job "document-pass")
// that emits a minimal but POPULATED Manifest — one subject with a target page,
// a base version slot, and a single claim citing the causing row — so the P4
// round-trip test proves a populated Manifest (including the per-page base
// version slot) survives the end-of-run transaction.
func NewDocumentStub() *Stub {
	return &Stub{
		job: "document-pass",
		build: func(u Unit) *Manifest {
			return &Manifest{
				Subjects: []Subject{{
					Type:        TypeEntity,
					Kind:        "org",
					Name:        "stub-subject",
					Aliases:     []string{"stub-subject"},
					SubjectID:   u.CausedBy, // deterministic, addressable
					TargetPage:  u.CausedBy,
					BaseVersion: 0,
					Claims:      []Claim{{Text: "stub claim", Cites: []string{u.CausedBy}}},
				}},
			}
		},
	}
}

// NewCronStub returns a cron/no-op stub Integrator (Job job, e.g. "crm-digest")
// that emits a minimal empty Manifest — a no-op run that still threads the runs
// lifecycle and the cron completion-time stamp.
func NewCronStub(job string) *Stub {
	return &Stub{job: job, build: func(Unit) *Manifest { return &Manifest{} }}
}

// FailWith makes the stub's Integrate return err (the run fails cleanly; the
// causing row stays pending). Used by P5 to drive the failure policy.
func (s *Stub) FailWith(err error) *Stub {
	s.failWith = err
	return s
}

// Job returns the stub's job name (runs.job; the cron/lint TryLock key).
func (s *Stub) Job() string { return s.job }

// Integrate runs the stub: it returns the configured failure if set, else the
// built Manifest (a minimal empty one if no builder was supplied).
func (s *Stub) Integrate(ctx context.Context, unit Unit) (*Manifest, error) {
	if s.failWith != nil {
		return nil, s.failWith
	}
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if s.build == nil {
		return &Manifest{}, nil
	}
	m := s.build(unit)
	if m == nil {
		return &Manifest{}, nil
	}
	return m, nil
}

// ErrStubFailure is a convenient sentinel for tests/P5 to drive the failure path.
var ErrStubFailure = errors.New("integrate: stub forced failure")
