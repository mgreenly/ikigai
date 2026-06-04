package crm

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"
	"unicode"

	"github.com/nyaruka/phonenumbers"

	"eventplane/outbox"
)

// Service is the dispatcher seam (PLAN.md §4, §8): it owns the single *sql.Tx and
// runs decode → entity write → dedup probe → outbox append → commit atomically
// over the one SQLite file. Entity stores are pure SQL against the passed tx and
// never own a transaction.
type Service struct {
	DB  *sql.DB
	Now func() time.Time
	// Outbox, when set, makes the service an event-plane producer: first-wave
	// contact events are appended atomically with the domain write and the feed
	// is rung after commit (PLAN.md §6). Nil disables event emission.
	Outbox *outbox.Outbox

	orgs         organizationStore
	contacts     contactStore
	deals        dealStore
	tasks        taskStore
	interactions interactionStore
}

func NewService(db *sql.DB) *Service {
	return &Service{DB: db, Now: time.Now}
}

// entity is the uniform read/delete contract every entity store satisfies. Save
// is intentionally absent: each entity's Save takes a typed <Type>Input, so the
// polymorphic create/update path is a typed switch in Save below — the one honest
// type-switch (PLAN.md §4). Get/Search/Delete are uniform and are dispatched
// generically here.
type entity interface {
	Get(tx *sql.Tx, id string) (Card, error)
	Search(tx *sql.Tx, p SearchParams) ([]Summary, error)
	Delete(tx *sql.Tx, id string, at time.Time) error
}

// typedEntity pairs an entity store with its type name. The probe order for
// crm_get is fixed (PLAN.md §4: up to five indexed point lookups, one per table).
func (s *Service) entities() []struct {
	name string
	e    entity
} {
	return []struct {
		name string
		e    entity
	}{
		{"organization", s.orgs},
		{"contact", s.contacts},
		{"deal", s.deals},
		{"task", s.tasks},
		{"interaction", s.interactions},
	}
}

// byType returns the entity store for a type name, or ok=false for an unknown
// type. interaction is reachable for Get/Search/Delete but not Save (it is
// created via Log).
func (s *Service) byType(typ string) (entity, bool) {
	for _, te := range s.entities() {
		if te.name == typ {
			return te.e, true
		}
	}
	return nil, false
}

// ── Get: probe-by-id type resolution → per-type card (PLAN.md §4) ────────────

func (s *Service) Get(ctx context.Context, id string) (Card, error) {
	if id == "" {
		return nil, invalid("id", "id is required")
	}
	tx, err := s.DB.BeginTx(ctx, &sql.TxOptions{ReadOnly: true})
	if err != nil {
		return nil, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()
	for _, te := range s.entities() {
		card, err := te.e.Get(tx, id)
		if err == nil {
			return card, nil
		}
		if !errors.Is(err, ErrNotFound) {
			return nil, err
		}
	}
	return nil, ErrNotFound
}

// ── Delete: shallow soft-delete, routed by type (PLAN.md §8) ─────────────────
//
// Delete is shallow: each entity soft-deletes only its own row + owned children.
// It does not cascade to or block on other entities; dangling FKs are tolerated
// because every read path filters deleted_at IS NULL.
func (s *Service) Delete(ctx context.Context, typ, id string) error {
	if id == "" {
		return invalid("id", "id is required")
	}
	e, ok := s.byType(typ)
	if !ok {
		return invalid("type", "unknown type "+typ)
	}
	tx, err := s.DB.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()
	if err := e.Delete(tx, id, s.Now().UTC()); err != nil {
		return err
	}
	return tx.Commit()
}

// ── Search, Save, Log: fleshed out in Phase 2 (PLAN.md §9) ───────────────────

// Search is implemented in search.go (cross-entity filtered/recency search).

// Save is the polymorphic upsert (PLAN.md §4): the one honest type-switch. It
// decodes the loose `fields` blob into the typed <Type>Input, normalizes and
// validates it, runs the exact dedup probe on create (unless force), checks
// cross-entity FK liveness, then routes to the matching entity store's Save on
// the service-owned *sql.Tx. interaction is absent from the enum — it is created
// via Log.
func (s *Service) Save(ctx context.Context, typ, id string, fields []byte, force bool) (Summary, error) {
	if !saveTypes[typ] {
		return Summary{}, invalid("type", "type must be one of organization, contact, deal, task (interactions are created via crm_log); got "+typ)
	}
	now := s.Now().UTC()

	tx, err := s.DB.BeginTx(ctx, nil)
	if err != nil {
		return Summary{}, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()

	var summary Summary
	switch typ {
	case "organization":
		summary, err = s.saveOrganization(tx, id, fields, force, now)
	case "contact":
		summary, err = s.saveContact(tx, id, fields, force, now)
	case "deal":
		summary, err = s.saveDeal(tx, id, fields, now)
	case "task":
		summary, err = s.saveTask(tx, id, fields, now)
	}
	if err != nil {
		return Summary{}, err
	}

	// Phase 4 seam: first-wave event emission (PLAN.md §6). Only contacts are
	// wired: contact.created/updated plus one contact.tagged/untagged per tag in
	// the diff captured on the summary side-band by contactStore.Save. The events
	// are appended onto THIS tx (so they commit atomically with the domain write
	// or not at all), then Ring()'d AFTER commit. The deal/task second wave is
	// documented intent only and is not emitted here. The nil-Outbox guard keeps
	// tests and other callers without an event plane working.
	ring := false
	if s.Outbox != nil && typ == "contact" {
		events, err := contactEvents(tx, summary)
		if err != nil {
			return Summary{}, err
		}
		for _, ev := range events {
			if err := s.Outbox.Append(tx, ev); err != nil {
				return Summary{}, err
			}
		}
		ring = len(events) > 0
	}

	if err := tx.Commit(); err != nil {
		return Summary{}, fmt.Errorf("commit: %w", err)
	}
	if ring {
		s.Outbox.Ring()
	}
	return summary, nil
}

// saveTypes is the closed enum for crm_save (PLAN.md §4); interaction is
// deliberately absent (created via Log).
var saveTypes = map[string]bool{"organization": true, "contact": true, "deal": true, "task": true}

func (s *Service) saveOrganization(tx *sql.Tx, id string, fields []byte, force bool, now time.Time) (Summary, error) {
	in, err := decodeOrganization(fields)
	if err != nil {
		return Summary{}, err
	}
	if id == "" {
		existing, err := probeOrganization(tx, in)
		if err != nil {
			return Summary{}, err
		}
		if existing != "" && !force {
			return Summary{}, &DuplicateError{ExistingID: existing,
				Message: "an organization with this domain/name already exists (existing_id=" + existing + "); update it, or retry with force:true to create a duplicate"}
		}
	}
	return s.orgs.Save(tx, id, in, now)
}

func (s *Service) saveContact(tx *sql.Tx, id string, fields []byte, force bool, now time.Time) (Summary, error) {
	in, primaryEmail, err := decodeContact(fields, id == "")
	if err != nil {
		return Summary{}, err
	}
	if in.OrgID != nil && *in.OrgID != "" {
		ok, err := liveExists(tx, "organizations", *in.OrgID)
		if err != nil {
			return Summary{}, err
		}
		if !ok {
			return Summary{}, invalid("org_id", "org_id does not reference a live organization; create the organization first or omit org_id")
		}
	}
	if id == "" && primaryEmail != "" {
		existing, err := probeContactByEmail(tx, primaryEmail)
		if err != nil {
			return Summary{}, err
		}
		if existing != "" && !force {
			return Summary{}, &DuplicateError{ExistingID: existing,
				Message: "a contact with primary email " + primaryEmail + " already exists (existing_id=" + existing + "); update it, or retry with force:true to create a duplicate"}
		}
	}
	return s.contacts.Save(tx, id, in, now)
}

func (s *Service) saveDeal(tx *sql.Tx, id string, fields []byte, now time.Time) (Summary, error) {
	in, err := decodeDeal(fields)
	if err != nil {
		return Summary{}, err
	}
	if in.OrgID != nil && *in.OrgID != "" {
		ok, err := liveExists(tx, "organizations", *in.OrgID)
		if err != nil {
			return Summary{}, err
		}
		if !ok {
			return Summary{}, invalid("org_id", "org_id does not reference a live organization; create the organization first or omit org_id")
		}
	}
	if in.Contacts != nil {
		for _, c := range *in.Contacts {
			ok, err := liveExists(tx, "contacts", c.ID)
			if err != nil {
				return Summary{}, err
			}
			if !ok {
				return Summary{}, invalid("contacts", "deal participant "+c.ID+" does not reference a live contact")
			}
		}
	}
	return s.deals.Save(tx, id, in, now)
}

func (s *Service) saveTask(tx *sql.Tx, id string, fields []byte, now time.Time) (Summary, error) {
	in, err := decodeTask(fields)
	if err != nil {
		return Summary{}, err
	}
	for _, ref := range []struct {
		field, table string
		val          *string
	}{
		{"contact_id", "contacts", in.ContactID},
		{"org_id", "organizations", in.OrgID},
		{"deal_id", "deals", in.DealID},
	} {
		if ref.val == nil || *ref.val == "" {
			continue
		}
		ok, err := liveExists(tx, ref.table, *ref.val)
		if err != nil {
			return Summary{}, err
		}
		if !ok {
			return Summary{}, invalid(ref.field, ref.field+" does not reference a live "+strings.TrimSuffix(ref.table, "s"))
		}
	}
	return s.tasks.Save(tx, id, in, now)
}

// Log appends an interaction to the timeline (PLAN.md §3, §4). It resolves
// subject_id by probe-by-id to exactly one of contact/org/deal, validates the
// kind vocabulary and the "body required" rule with corrective messages,
// defaults occurred_at to now, then appends on a service-owned tx.
func (s *Service) Log(ctx context.Context, in LogInput) (Summary, error) {
	if strings.TrimSpace(in.Body) == "" {
		return Summary{}, invalid("body", "body is required")
	}
	if !interactionKinds[in.Kind] {
		return Summary{}, invalid("kind", "kind must be one of note, call, email, meeting; got "+in.Kind)
	}
	if in.SubjectID == "" {
		return Summary{}, invalid("subject_id", "subject_id is required (the contact, organization, or deal this interaction is about)")
	}
	occurredAt := s.Now().UTC()
	if in.OccurredAt != nil && strings.TrimSpace(*in.OccurredAt) != "" {
		t, err := time.Parse(time.RFC3339, strings.TrimSpace(*in.OccurredAt))
		if err != nil {
			return Summary{}, invalid("occurred_at", "occurred_at must be an RFC3339 timestamp (e.g. 2026-06-03T12:00:00Z); got "+*in.OccurredAt)
		}
		occurredAt = t.UTC()
	}
	now := s.Now().UTC()

	tx, err := s.DB.BeginTx(ctx, nil)
	if err != nil {
		return Summary{}, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()

	// Probe-by-id to resolve which FK column the subject maps to.
	var contactID, orgID, dealID *string
	switch {
	case liveExistsT(tx, "contacts", in.SubjectID):
		contactID = &in.SubjectID
	case liveExistsT(tx, "organizations", in.SubjectID):
		orgID = &in.SubjectID
	case liveExistsT(tx, "deals", in.SubjectID):
		dealID = &in.SubjectID
	default:
		return Summary{}, fmt.Errorf("%w: subject_id %s does not resolve to a live contact, organization, or deal", ErrNotFound, in.SubjectID)
	}

	summary, err := s.interactions.Insert(tx, in.Kind, in.Body, occurredAt, contactID, orgID, dealID, now)
	if err != nil {
		return Summary{}, err
	}

	// TODO(phase4): second-wave interaction.logged event emission (PLAN.md §6).

	if err := tx.Commit(); err != nil {
		return Summary{}, fmt.Errorf("commit: %w", err)
	}
	return summary, nil
}

// liveExistsT is a thin error-eliding wrapper around liveExists for the Log
// probe switch (a probe error is rare and treated as "not this type"; the final
// default arm surfaces not_found).
func liveExistsT(tx *sql.Tx, table, id string) bool {
	ok, _ := liveExists(tx, table, id)
	return ok
}

// interactionKinds is the closed kind vocabulary (PLAN.md §3, matches the schema
// CHECK).
var interactionKinds = map[string]bool{"note": true, "call": true, "email": true, "meeting": true}

// ── decode / normalize / validate (the dispatcher seam, PLAN.md §4) ──────────
//
// Each decode<Type> unmarshals the loose `fields` JSON into the typed input with
// pointer-means-provided semantics, then normalizes (email lowercase/trim, phone
// → E.164, display_name derivation, label validation) and validates vocabulary
// with corrective messages. Ported from the prior contacts/mcp normalization
// (normalizeEmail/normalizePhone/validateLabel/deriveDisplayName).

// organizationFields is the wire shape for crm_save(type:"organization").
type organizationFields struct {
	Name   *string `json:"name"`
	Domain *string `json:"domain"`
}

func decodeOrganization(fields []byte) (OrganizationInput, error) {
	var f organizationFields
	if err := decodeFields(fields, &f); err != nil {
		return OrganizationInput{}, err
	}
	return OrganizationInput{Name: trimPtr(f.Name), Domain: lowerTrimPtr(f.Domain)}, nil
}

// probeOrganization is the exact dedup probe (PLAN.md §4): by exact domain, else
// by exact name when no domain is supplied. Returns the existing live id or "".
func probeOrganization(tx *sql.Tx, in OrganizationInput) (string, error) {
	if in.Domain != nil && *in.Domain != "" {
		var id string
		err := tx.QueryRow(`SELECT id FROM organizations WHERE domain = ? AND deleted_at IS NULL LIMIT 1`, *in.Domain).Scan(&id)
		switch err {
		case nil:
			return id, nil
		case sql.ErrNoRows:
			return "", nil
		default:
			return "", fmt.Errorf("probe organization by domain: %w", err)
		}
	}
	if in.Name != nil && *in.Name != "" {
		var id string
		err := tx.QueryRow(`SELECT id FROM organizations WHERE name = ? AND deleted_at IS NULL LIMIT 1`, *in.Name).Scan(&id)
		switch err {
		case nil:
			return id, nil
		case sql.ErrNoRows:
			return "", nil
		default:
			return "", fmt.Errorf("probe organization by name: %w", err)
		}
	}
	return "", nil
}

// emailFields / phoneFields are the inline child wire shapes.
type emailFields struct {
	Email string  `json:"email"`
	Label *string `json:"label"`
}

type phoneFields struct {
	Phone string  `json:"phone"`
	Label *string `json:"label"`
}

// contactFields is the wire shape for crm_save(type:"contact").
type contactFields struct {
	GivenName   *string        `json:"given_name"`
	FamilyName  *string        `json:"family_name"`
	DisplayName *string        `json:"display_name"`
	OrgID       *string        `json:"org_id"`
	Title       *string        `json:"title"`
	Lifecycle   *string        `json:"lifecycle"`
	Emails      *[]emailFields `json:"emails"`
	Phones      *[]phoneFields `json:"phones"`
	Tags        *[]string      `json:"tags"`
}

var lifecycles = map[string]bool{"subscriber": true, "lead": true, "opportunity": true, "customer": true}

// decodeContact normalizes a contact and returns the typed input plus the
// normalized primary email (the first email, "" when none) for the dedup probe.
// onCreate gates the display_name derivation (required on create).
func decodeContact(fields []byte, onCreate bool) (ContactInput, string, error) {
	var f contactFields
	if err := decodeFields(fields, &f); err != nil {
		return ContactInput{}, "", err
	}
	in := ContactInput{
		GivenName:  trimPtr(f.GivenName),
		FamilyName: trimPtr(f.FamilyName),
		OrgID:      trimPtr(f.OrgID),
		Title:      trimPtr(f.Title),
		Tags:       f.Tags,
	}
	if f.Lifecycle != nil {
		lc := strings.TrimSpace(*f.Lifecycle)
		if !lifecycles[lc] {
			return ContactInput{}, "", invalid("lifecycle", "lifecycle must be one of subscriber, lead, opportunity, customer; got "+*f.Lifecycle)
		}
		in.Lifecycle = &lc
	}

	var primaryEmail string
	if f.Emails != nil {
		emails := make([]EmailInput, 0, len(*f.Emails))
		for i, e := range *f.Emails {
			norm, err := normalizeEmail(e.Email)
			if err != nil {
				return ContactInput{}, "", invalid("emails", err.Error())
			}
			if err := validateLabel(e.Label); err != nil {
				return ContactInput{}, "", invalid("emails", err.Error())
			}
			if i == 0 {
				primaryEmail = norm
			}
			emails = append(emails, EmailInput{Email: norm, Label: trimPtr(e.Label)})
		}
		in.Emails = &emails
	}
	if f.Phones != nil {
		phones := make([]PhoneInput, 0, len(*f.Phones))
		for _, p := range *f.Phones {
			norm, err := normalizePhone(p.Phone)
			if err != nil {
				return ContactInput{}, "", invalid("phones", err.Error())
			}
			if err := validateLabel(p.Label); err != nil {
				return ContactInput{}, "", invalid("phones", err.Error())
			}
			phones = append(phones, PhoneInput{Phone: norm, Label: trimPtr(p.Label)})
		}
		in.Phones = &phones
	}

	// display_name derivation (PLAN.md §3): supplied → "given family" → primary
	// email. Derived only on create; on update an absent display_name is left
	// untouched (nil).
	if f.DisplayName != nil && strings.TrimSpace(*f.DisplayName) != "" {
		dn := strings.TrimSpace(*f.DisplayName)
		in.DisplayName = &dn
	} else if onCreate {
		dn, err := deriveDisplayName(f.DisplayName, f.GivenName, f.FamilyName, primaryEmail)
		if err != nil {
			return ContactInput{}, "", err
		}
		in.DisplayName = &dn
	}
	return in, primaryEmail, nil
}

// probeContactByEmail is the exact contact dedup probe (PLAN.md §4): by
// normalized primary email. Returns the existing live id or "".
func probeContactByEmail(tx *sql.Tx, email string) (string, error) {
	var id string
	err := tx.QueryRow(`
		SELECT c.id FROM contacts c
		JOIN contact_emails e ON e.contact_id = c.id AND e.is_primary = 1 AND e.deleted_at IS NULL
		WHERE e.email = ? AND c.deleted_at IS NULL LIMIT 1`, email).Scan(&id)
	switch err {
	case nil:
		return id, nil
	case sql.ErrNoRows:
		return "", nil
	default:
		return "", fmt.Errorf("probe contact by email: %w", err)
	}
}

// dealContactFields is the wire shape for one deal participant.
type dealContactFields struct {
	ID   string  `json:"id"`
	Role *string `json:"role"`
}

// dealFields is the wire shape for crm_save(type:"deal"). status is deliberately
// included so a client-supplied status can be rejected at the seam (PLAN.md §3).
type dealFields struct {
	Name        *string              `json:"name"`
	OrgID       *string              `json:"org_id"`
	Stage       *string              `json:"stage"`
	Status      *string              `json:"status"`
	AmountCents *int64               `json:"amount_cents"`
	Currency    *string              `json:"currency"`
	CloseDate   *string              `json:"close_date"`
	Contacts    *[]dealContactFields `json:"contacts"`
}

var dealStages = map[string]bool{"lead": true, "qualified": true, "proposal": true, "negotiation": true, "won": true, "lost": true}

func decodeDeal(fields []byte) (DealInput, error) {
	var f dealFields
	if err := decodeFields(fields, &f); err != nil {
		return DealInput{}, err
	}
	// status is derived from stage, never client-set (PLAN.md §3).
	if f.Status != nil {
		return DealInput{}, invalid("status", "status is derived from stage and cannot be set directly; set stage instead (won/lost yield status won/lost, all others status open)")
	}
	in := DealInput{
		Name:        trimPtr(f.Name),
		OrgID:       trimPtr(f.OrgID),
		AmountCents: f.AmountCents,
		CloseDate:   trimPtr(f.CloseDate),
	}
	if f.Stage != nil {
		st := strings.TrimSpace(*f.Stage)
		if !dealStages[st] {
			return DealInput{}, invalid("stage", "stage must be one of lead, qualified, proposal, negotiation, won, lost; got "+*f.Stage)
		}
		in.Stage = &st
	}
	if f.Currency != nil {
		cur := strings.ToUpper(strings.TrimSpace(*f.Currency))
		in.Currency = &cur
	}
	if f.Contacts != nil {
		participants := make([]DealContactInput, 0, len(*f.Contacts))
		for _, c := range *f.Contacts {
			cid := strings.TrimSpace(c.ID)
			if cid == "" {
				return DealInput{}, invalid("contacts", "each deal participant needs an id")
			}
			participants = append(participants, DealContactInput{ID: cid, Role: trimPtr(c.Role)})
		}
		in.Contacts = &participants
	}
	return in, nil
}

// taskFields is the wire shape for crm_save(type:"task").
type taskFields struct {
	Title     *string `json:"title"`
	Status    *string `json:"status"`
	DueAt     *string `json:"due_at"`
	DoneAt    *string `json:"done_at"`
	ContactID *string `json:"contact_id"`
	OrgID     *string `json:"org_id"`
	DealID    *string `json:"deal_id"`
}

var taskStatuses = map[string]bool{"open": true, "done": true}

func decodeTask(fields []byte) (TaskInput, error) {
	var f taskFields
	if err := decodeFields(fields, &f); err != nil {
		return TaskInput{}, err
	}
	in := TaskInput{
		Title:     trimPtr(f.Title),
		DueAt:     trimPtr(f.DueAt),
		DoneAt:    trimPtr(f.DoneAt),
		ContactID: trimPtr(f.ContactID),
		OrgID:     trimPtr(f.OrgID),
		DealID:    trimPtr(f.DealID),
	}
	if f.Status != nil {
		st := strings.TrimSpace(*f.Status)
		if !taskStatuses[st] {
			return TaskInput{}, invalid("status", "status must be one of open, done; got "+*f.Status)
		}
		in.Status = &st
	}
	return in, nil
}

// decodeFields unmarshals the loose `fields` blob, rejecting unknown fields with
// a corrective message (the loose-schema self-correction bet, PLAN.md §4). An
// empty blob decodes to the zero input (all-absent).
func decodeFields(fields []byte, v any) error {
	if len(fields) == 0 {
		return nil
	}
	dec := json.NewDecoder(strings.NewReader(string(fields)))
	dec.DisallowUnknownFields()
	if err := dec.Decode(v); err != nil {
		return invalid("fields", "fields could not be decoded for this type: "+err.Error())
	}
	return nil
}

// ── normalization helpers (ported from the prior contacts/mcp tools.go) ──────

func normalizeEmail(in string) (string, error) {
	s := strings.ToLower(strings.TrimSpace(in))
	if s == "" {
		return "", errors.New("email must not be empty")
	}
	at := strings.IndexByte(s, '@')
	if at <= 0 || at == len(s)-1 {
		return "", errors.New("email must contain a local-part and a domain (e.g. bob@example.com); got " + in)
	}
	return s, nil
}

func normalizePhone(in string) (string, error) {
	s := strings.TrimSpace(in)
	if s == "" {
		return "", errors.New("phone must not be empty")
	}
	if !strings.HasPrefix(s, "+") {
		return "", errors.New("phone must be fully qualified E.164 starting with '+' (e.g. +14155550123); got " + in)
	}
	parsed, err := phonenumbers.Parse(s, "")
	if err != nil {
		return "", errors.New("phone is not a valid E.164 number; got " + in)
	}
	if !phonenumbers.IsValidNumber(parsed) {
		return "", errors.New("phone is not a valid E.164 number; got " + in)
	}
	return phonenumbers.Format(parsed, phonenumbers.E164), nil
}

func validateLabel(label *string) error {
	if label == nil {
		return nil
	}
	if len(*label) > 40 {
		return errors.New("label must be at most 40 characters")
	}
	for _, r := range *label {
		if unicode.IsControl(r) {
			return errors.New("label must not contain control characters")
		}
	}
	return nil
}

// deriveDisplayName mirrors the prior derivation (PLAN.md §3): supplied →
// "given family" → primary email. primaryEmail is the already-normalized first
// email ("" when none).
func deriveDisplayName(supplied, given, family *string, primaryEmail string) (string, error) {
	if s := ptrTrim(supplied); s != "" {
		return s, nil
	}
	combined := strings.TrimSpace(ptrTrim(given) + " " + ptrTrim(family))
	if combined != "" {
		return combined, nil
	}
	if primaryEmail != "" {
		return primaryEmail, nil
	}
	return "", invalid("display_name", "a contact needs at least one identifying field: display_name, given_name/family_name, or a primary email")
}

func ptrTrim(s *string) string {
	if s == nil {
		return ""
	}
	return strings.TrimSpace(*s)
}

// trimPtr returns nil for a nil pointer, else a pointer to the trimmed value
// (preserving pointer-means-provided: a provided "" stays "" so nullable columns
// can be cleared).
func trimPtr(s *string) *string {
	if s == nil {
		return nil
	}
	v := strings.TrimSpace(*s)
	return &v
}

// lowerTrimPtr is trimPtr plus lowercasing, for the org domain (dedup key).
func lowerTrimPtr(s *string) *string {
	if s == nil {
		return nil
	}
	v := strings.ToLower(strings.TrimSpace(*s))
	return &v
}
