package wiki

import (
	"context"
	"database/sql"
	"fmt"
	"sort"
	"strings"
	"unicode"
	"unicode/utf8"
)

// Ref is a markdown link target for another wiki subject.
type Ref struct {
	Path string
	Name string
}

// LinkedPage is a page plus its read-time link projection.
type LinkedPage struct {
	Page
	Mentions    []Ref
	MentionedBy []Ref
}

// SubjectKeys is a subject plus every normalized name that resolves to it.
type SubjectKeys struct {
	Subject
	Keys []string
}

// Mentions returns every subject whose normalized name appears as a whole
// alphanumeric-bounded phrase in body.
func Mentions(body string, others []SubjectKeys) []Subject {
	normalizedBody := Normalize(body)
	var out []Subject
	for _, subjectKeys := range others {
		for _, key := range subjectKeys.Keys {
			if key == "" {
				continue
			}
			if containsWholePhrase(normalizedBody, key) {
				out = append(out, subjectKeys.Subject)
				break
			}
		}
	}
	return out
}

func containsWholePhrase(body, phrase string) bool {
	for offset := 0; offset <= len(body); {
		i := strings.Index(body[offset:], phrase)
		if i < 0 {
			return false
		}
		start := offset + i
		end := start + len(phrase)
		if phraseBoundaryBefore(body, start) && phraseBoundaryAfter(body, end) {
			return true
		}
		if end >= len(body) {
			return false
		}
		_, width := utf8.DecodeRuneInString(body[start:])
		if width == 0 {
			width = 1
		}
		offset = start + width
	}
	return false
}

func phraseBoundaryBefore(s string, index int) bool {
	if index == 0 {
		return true
	}
	r, _ := utf8.DecodeLastRuneInString(s[:index])
	return !isAlphaNumeric(r)
}

func phraseBoundaryAfter(s string, index int) bool {
	if index == len(s) {
		return true
	}
	r, _ := utf8.DecodeRuneInString(s[index:])
	return !isAlphaNumeric(r)
}

func isAlphaNumeric(r rune) bool {
	return unicode.IsLetter(r) || unicode.IsDigit(r)
}

// PageWithLinks returns a stored page plus read-time outbound and inbound links.
func (s *Service) PageWithLinks(ctx context.Context, subjectID string) (LinkedPage, error) {
	if s == nil {
		return LinkedPage{}, fmt.Errorf("wiki: nil service")
	}
	subjectID = strings.TrimSpace(subjectID)
	subject, err := s.subjects.Get(ctx, subjectID)
	if err != nil {
		return LinkedPage{}, err
	}
	page, err := s.pages.GetBySubject(ctx, subjectID)
	if err != nil {
		return LinkedPage{}, err
	}
	subjects, err := listAllSubjects(ctx, s.subjects, "", "")
	if err != nil {
		return LinkedPage{}, err
	}
	aliases, err := s.aliases.ListAll(ctx)
	if err != nil {
		return LinkedPage{}, err
	}

	subjectKeys := subjectKeysFor(subjects, aliases)
	thisKeys := subjectKeys[subject.ID]
	var others []SubjectKeys
	for _, candidateKeys := range subjectKeys {
		if candidateKeys.ID != subject.ID {
			others = append(others, candidateKeys)
		}
	}

	linked := LinkedPage{
		Page:     page,
		Mentions: refsFor(Mentions(page.Body, others)),
	}
	for _, other := range others {
		otherPage, err := s.pages.GetBySubject(ctx, other.ID)
		if err == sql.ErrNoRows {
			continue
		}
		if err != nil {
			return LinkedPage{}, err
		}
		if len(Mentions(otherPage.Body, []SubjectKeys{thisKeys})) > 0 {
			linked.MentionedBy = append(linked.MentionedBy, refFor(other.Subject))
		}
	}
	linked.MentionedBy = canonicalRefs(linked.MentionedBy)
	return linked, nil
}

// MentionsIn returns public read-surface links for subjects named in text.
func (s *Service) MentionsIn(ctx context.Context, text string) ([]Ref, error) {
	if s == nil {
		return nil, fmt.Errorf("wiki: nil service")
	}
	subjects, err := listAllSubjects(ctx, s.subjects, "", "")
	if err != nil {
		return nil, err
	}
	aliases, err := s.aliases.ListAll(ctx)
	if err != nil {
		return nil, err
	}

	subjectKeysByID := subjectKeysFor(subjects, aliases)
	subjectKeys := make([]SubjectKeys, 0, len(subjectKeysByID))
	for _, keys := range subjectKeysByID {
		subjectKeys = append(subjectKeys, keys)
	}
	return refsFor(Mentions(text, subjectKeys)), nil
}

func subjectKeysFor(subjects []Subject, aliases []Alias) map[string]SubjectKeys {
	byID := make(map[string]SubjectKeys, len(subjects))
	for _, subject := range subjects {
		keys := appendNormalizedKey(nil, subject.NormName)
		byID[subject.ID] = SubjectKeys{Subject: subject, Keys: keys}
	}
	for _, alias := range aliases {
		subjectKeys, ok := byID[alias.SubjectID]
		if !ok {
			continue
		}
		subjectKeys.Keys = appendNormalizedKey(subjectKeys.Keys, alias.NormName)
		byID[alias.SubjectID] = subjectKeys
	}
	return byID
}

func appendNormalizedKey(keys []string, key string) []string {
	key = Normalize(key)
	if key == "" {
		return keys
	}
	for _, existing := range keys {
		if existing == key {
			return keys
		}
	}
	return append(keys, key)
}

func refsFor(subjects []Subject) []Ref {
	refs := make([]Ref, 0, len(subjects))
	for _, subject := range subjects {
		refs = append(refs, refFor(subject))
	}
	return canonicalRefs(refs)
}

func refFor(subject Subject) Ref {
	return Ref{
		Path: Path(subject),
		Name: subject.Name,
	}
}

// RenderFooter appends a deterministic markdown link footer to body.
func RenderFooter(body string, mentions, mentionedBy []Ref) string {
	var b strings.Builder
	b.WriteString(strings.TrimRight(body, "\n"))
	b.WriteString("\n\n---\n\n## Links\n\n")
	writeRefSection(&b, "Mentions", mentions)
	b.WriteString("\n")
	writeRefSection(&b, "Mentioned by", mentionedBy)
	return b.String()
}

func writeRefSection(b *strings.Builder, title string, refs []Ref) {
	b.WriteString("### ")
	b.WriteString(title)
	b.WriteString("\n")
	refs = canonicalRefs(refs)
	if len(refs) == 0 {
		b.WriteString("- None\n")
		return
	}
	for _, ref := range refs {
		b.WriteString("- [")
		b.WriteString(escapeMarkdownLinkText(ref.Name))
		b.WriteString("](")
		b.WriteString(ref.Path)
		b.WriteString(")\n")
	}
}

func canonicalRefs(refs []Ref) []Ref {
	if len(refs) == 0 {
		return nil
	}
	out := append([]Ref(nil), refs...)
	sort.Slice(out, func(i, j int) bool {
		if out[i].Path == out[j].Path {
			return out[i].Name < out[j].Name
		}
		return out[i].Path < out[j].Path
	})
	n := 0
	for _, ref := range out {
		if n > 0 && out[n-1].Path == ref.Path {
			continue
		}
		out[n] = ref
		n++
	}
	return out[:n]
}

func escapeMarkdownLinkText(s string) string {
	s = strings.ReplaceAll(s, `\`, `\\`)
	s = strings.ReplaceAll(s, `]`, `\]`)
	return s
}
