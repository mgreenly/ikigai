// Package crm is the sales-CRM domain: five entities (organization, contact,
// deal, interaction, task) behind a fixed six-verb polymorphic surface
// (PLAN.md §2). The dispatcher in service.go owns the transaction and is the one
// place that decodes the loose `fields` map into a typed <Type>Input, normalizes
// and validates it (PLAN.md §4 "the dispatcher seam"). Entity stores never see
// map[string]any — they take clean typed inputs and run pure SQL against a
// passed *sql.Tx (PLAN.md §8).
package crm

import (
	"errors"
	"time"
)

// timeFormat is the canonical wire/storage timestamp rendering used across cards,
// summaries, and event payloads (matches the prior service's format so consumers
// see a stable shape).
const timeFormat = "2006-01-02T15:04:05.000000000Z07:00"

// Error sentinels mapped onto the closed error vocabulary (PLAN.md §4). Entity
// stores return these (wrapped with %w + context); the dispatcher does the single
// sentinel→envelope translation. Entities never write wire JSON.
var (
	// ErrValidation — bad/missing field (incl. a rejected derived `status`).
	ErrValidation = errors.New("crm: validation")
	// ErrNotFound — id doesn't resolve, or resolves to a soft-deleted row.
	ErrNotFound = errors.New("crm: not found")
	// ErrConflict — invariant violation (uniqueness race, no-primary).
	ErrConflict = errors.New("crm: conflict")
)

// DuplicateError is the dedup-probe carrier (PLAN.md §4). It is the only error
// meaning "retry with force:true or update"; it carries the id of the existing
// row so the agent can choose update vs force. The dispatcher extracts it with
// errors.As and renders the `duplicate` envelope with `existing_id`.
type DuplicateError struct {
	ExistingID string
	Message    string
}

func (e *DuplicateError) Error() string {
	if e.Message != "" {
		return e.Message
	}
	return "crm: duplicate (existing_id=" + e.ExistingID + ")"
}

// ValidationError carries a corrective message and, optionally, the offending
// field name. It wraps ErrValidation so errors.Is(err, ErrValidation) holds and
// the dispatcher can still surface `field`.
type ValidationError struct {
	Field   string
	Message string
}

func (e *ValidationError) Error() string { return e.Message }
func (e *ValidationError) Unwrap() error  { return ErrValidation }

// invalid builds a ValidationError with a corrective message (PLAN.md §4: the
// message states the fix, not just the defect).
func invalid(field, msg string) error { return &ValidationError{Field: field, Message: msg} }

// ── typed inputs (one per mutable entity) ────────────────────────────────────
//
// Pointer-means-provided throughout (the existing contacts convention): a nil
// pointer = field absent = untouched on update; a non-nil pointer (even to "")
// is a provided value. For nullable columns a provided empty string clears the
// column to NULL. Set-valued fields are *[]T: nil = untouched, &[]{} = clear all
// (PLAN.md §4).

// OrganizationInput drives crm_save(type:"organization").
type OrganizationInput struct {
	Name   *string // required on create
	Domain *string // nullable; "" clears
}

// EmailInput / PhoneInput are inline child values. Emails and phones are
// declarative-replace sets on a contact (the array is the complete desired set;
// the first entry is primary).
type EmailInput struct {
	Email string
	Label *string
}

type PhoneInput struct {
	Phone string
	Label *string
}

// ContactInput drives crm_save(type:"contact").
type ContactInput struct {
	GivenName   *string
	FamilyName  *string
	DisplayName *string // derived if absent on create
	OrgID       *string // FK; "" clears
	Title       *string
	Lifecycle   *string // subscriber|lead|opportunity|customer; default lead on create
	Emails      *[]EmailInput
	Phones      *[]PhoneInput
	Tags        *[]string // declarative set; the diff emits contact.tagged/untagged
}

// DealContactInput is one deal participant (the {id, role} shape, PLAN.md §3).
type DealContactInput struct {
	ID   string
	Role *string
}

// DealInput drives crm_save(type:"deal"). There is deliberately no Status field:
// status is derived from stage, and a client-supplied status is rejected at the
// decode seam (PLAN.md §3).
type DealInput struct {
	Name        *string // required on create
	OrgID       *string
	Stage       *string // lead|qualified|proposal|negotiation|won|lost; default lead
	AmountCents *int64
	Currency    *string // default USD on create
	CloseDate   *string // RFC3339 date; "" clears
	Contacts    *[]DealContactInput
}

// TaskInput drives crm_save(type:"task").
type TaskInput struct {
	Title     *string // required on create
	Status    *string // open|done; default open
	DueAt     *string
	DoneAt    *string
	ContactID *string
	OrgID     *string
	DealID    *string
}

// LogInput drives crm_log (PLAN.md §2). subject_id is resolved to a contact/org/
// deal FK by probe-by-id in the dispatcher before the interaction is appended.
type LogInput struct {
	SubjectID  string
	Kind       string
	Body       string
	OccurredAt *string
}

// ── shared result shapes ─────────────────────────────────────────────────────

// Summary is the trimmed cross-entity search row (PLAN.md §2): id, type, a display
// label, and a few key fields to disambiguate. sortKey is unexported and carries
// updated_at for the cross-entity recency merge.
type Summary struct {
	ID        string         `json:"id"`
	Type      string         `json:"type"`
	Label     string         `json:"label"`
	UpdatedAt string         `json:"updated_at"`
	Fields    map[string]any `json:"fields,omitempty"`

	sortKey time.Time

	// Side-band for the dispatcher's first-wave event emission (PLAN.md §6) —
	// not part of the wire summary. A Save sets isCreate to distinguish
	// contact.created from contact.updated; contactStore.Save populates the
	// tag diff (added/removed) that becomes contact.tagged/untagged.
	isCreate    bool
	tagsAdded   []string
	tagsRemoved []string
}

// Card is the rich per-type read shape (PLAN.md §4): each entity's Get hook
// composes its own map — self fields plus the relations that entity attaches.
type Card map[string]any

// SearchParams drive crm_search (PLAN.md §2): substring (LIKE) match, recency
// ordered (updated_at DESC). Type scopes to one entity; Filters carry
// entity-specific predicates (subject_id, tag, lifecycle, stage, …).
type SearchParams struct {
	Query   string
	Type    string
	Filters map[string]any
	Limit   int
	AfterID string
}

const defaultSearchLimit = 50
const maxSearchLimit = 200

func (p SearchParams) limit() int {
	switch {
	case p.Limit <= 0:
		return defaultSearchLimit
	case p.Limit > maxSearchLimit:
		return maxSearchLimit
	default:
		return p.Limit
	}
}
