// Package eval loads extract evaluation cases and runs production extraction.
package eval

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"

	"wiki/internal/extract"
	"wiki/internal/llm"
	wikidomain "wiki/internal/wiki"
)

// Case is one extract evaluation fixture.
type Case struct {
	Name       string
	Difficulty string
	Header     extract.DocumentHeader
	Text       string
	Gold       []GoldSubject
}

// GoldSubject is one blessed expected subject in an eval case.
type GoldSubject struct {
	Type   string
	Name   string
	Claims []string
}

// SubjectScore partitions predicted subjects against the gold subject set.
type SubjectScore struct {
	Found        []string
	Missed       []string
	Hallucinated []string
}

// ClaimScore totals claim verdicts.
type ClaimScore struct {
	Covered int
	Missed  int
	Extra   int
}

// ClaimMatch records one predicted claim judged to cover a gold claim.
type ClaimMatch struct {
	Gold      string
	Predicted string
}

// SubjectClaimResult keeps per-subject claim text for human review.
type SubjectClaimResult struct {
	Subject string
	Covered []ClaimMatch
	Missed  []string
	Extra   []string
}

// CaseResult is the score for one eval case.
type CaseResult struct {
	Case       string
	Difficulty string
	Subjects   SubjectScore
	Claims     ClaimScore
	ClaimText  []SubjectClaimResult
}

// Metrics carries precision and recall for one subject or claim partition.
type Metrics struct {
	Precision float64
	Recall    float64
}

// Totals aggregates case results overall and by difficulty.
type Totals struct {
	Overall struct {
		Subjects Metrics
		Claims   Metrics
		Cases    int
	}
	ByDifficulty map[string]struct {
		Subjects Metrics
		Claims   Metrics
		Cases    int
	}
}

// Score partitions subjects and asks the judge to score claims for matched subjects.
func Score(ctx context.Context, j *Judge, c Case, predicted []extract.ExtractedSubject) (CaseResult, error) {
	goldByID := make(map[string]GoldSubject, len(c.Gold))
	predictedByID := make(map[string]extract.ExtractedSubject, len(predicted))
	for _, subject := range c.Gold {
		goldByID[subjectID(subject.Type, subject.Name)] = subject
	}
	for _, subject := range predicted {
		predictedByID[subjectID(subject.Type, subject.Name)] = subject
	}

	result := CaseResult{
		Case:       c.Name,
		Difficulty: c.Difficulty,
	}
	ids := unionSubjectIDs(goldByID, predictedByID)
	for _, id := range ids {
		gold, inGold := goldByID[id]
		pred, inPredicted := predictedByID[id]
		switch {
		case inGold && inPredicted:
			result.Subjects.Found = append(result.Subjects.Found, id)
			detail, err := judgeClaims(ctx, j, id, gold.Claims, pred.Claims)
			if err != nil {
				return CaseResult{}, err
			}
			result.Claims.Covered += len(detail.Covered)
			result.Claims.Missed += len(detail.Missed)
			result.Claims.Extra += len(detail.Extra)
			result.ClaimText = append(result.ClaimText, detail)
		case inGold:
			result.Subjects.Missed = append(result.Subjects.Missed, id)
			result.Claims.Missed += len(gold.Claims)
			result.ClaimText = append(result.ClaimText, SubjectClaimResult{
				Subject: id,
				Missed:  append([]string(nil), gold.Claims...),
			})
		case inPredicted:
			result.Subjects.Hallucinated = append(result.Subjects.Hallucinated, id)
			result.Claims.Extra += len(pred.Claims)
			result.ClaimText = append(result.ClaimText, SubjectClaimResult{
				Subject: id,
				Extra:   append([]string(nil), pred.Claims...),
			})
		}
	}
	return result, nil
}

// Aggregate rolls case scores into overall and per-difficulty precision/recall.
func Aggregate(results []CaseResult) Totals {
	type counts struct {
		cases               int
		subjectFound        int
		subjectMissed       int
		subjectHallucinated int
		claimCovered        int
		claimMissed         int
		claimExtra          int
	}
	var overall counts
	byDifficulty := make(map[string]counts)
	for _, result := range results {
		add := counts{
			cases:               1,
			subjectFound:        len(result.Subjects.Found),
			subjectMissed:       len(result.Subjects.Missed),
			subjectHallucinated: len(result.Subjects.Hallucinated),
			claimCovered:        result.Claims.Covered,
			claimMissed:         result.Claims.Missed,
			claimExtra:          result.Claims.Extra,
		}
		overall = addCounts(overall, add)
		byDifficulty[result.Difficulty] = addCounts(byDifficulty[result.Difficulty], add)
	}

	var totals Totals
	totals.Overall = bucketFromCounts(overall)
	totals.ByDifficulty = make(map[string]struct {
		Subjects Metrics
		Claims   Metrics
		Cases    int
	}, len(byDifficulty))
	for difficulty, counts := range byDifficulty {
		totals.ByDifficulty[difficulty] = bucketFromCounts(counts)
	}
	return totals
}

// LoadCase parses and validates document.txt and gold.json from dir.
func LoadCase(dir string) (Case, error) {
	name := filepath.Base(filepath.Clean(dir))
	rawText, err := os.ReadFile(filepath.Join(dir, "document.txt"))
	if err != nil {
		return Case{}, fmt.Errorf("load case %s document.txt: %w", name, err)
	}
	text := string(rawText)
	if strings.TrimSpace(text) == "" {
		return Case{}, fmt.Errorf("load case %s: document.txt required", name)
	}

	rawGold, err := os.ReadFile(filepath.Join(dir, "gold.json"))
	if err != nil {
		return Case{}, fmt.Errorf("load case %s gold.json: %w", name, err)
	}
	var gold goldFile
	dec := json.NewDecoder(strings.NewReader(string(rawGold)))
	dec.DisallowUnknownFields()
	if err := dec.Decode(&gold); err != nil {
		return Case{}, fmt.Errorf("load case %s gold.json: %w", name, err)
	}
	if err := validateGold(name, gold); err != nil {
		return Case{}, err
	}

	receivedAt, err := time.Parse(time.RFC3339, gold.Header.ReceivedAt)
	if err != nil {
		return Case{}, fmt.Errorf("load case %s: header.received_at must be RFC3339: %w", name, err)
	}
	return Case{
		Name:       name,
		Difficulty: gold.Difficulty,
		Header: extract.DocumentHeader{
			Source:     gold.Header.Source,
			Title:      gold.Header.Title,
			Tags:       append([]string(nil), gold.Header.Tags...),
			ReceivedAt: receivedAt,
		},
		Text: text,
		Gold: copyGold(gold.Gold),
	}, nil
}

// LoadDataset loads each immediate subdirectory below root as one case.
func LoadDataset(root string) ([]Case, error) {
	entries, err := os.ReadDir(root)
	if err != nil {
		return nil, fmt.Errorf("load dataset %s: %w", root, err)
	}
	var cases []Case
	for _, entry := range entries {
		if !entry.IsDir() {
			continue
		}
		c, err := LoadCase(filepath.Join(root, entry.Name()))
		if err != nil {
			return nil, err
		}
		cases = append(cases, c)
	}
	return cases, nil
}

// Run executes production extract over a loaded case.
func Run(ctx context.Context, ex *extract.Extractor, c Case) ([]extract.ExtractedSubject, error) {
	return ex.Extract(ctx, c.Header, c.Text)
}

func subjectID(subjectType, name string) string {
	return wikidomain.Path(wikidomain.Subject{
		Type:     subjectType,
		NormName: wikidomain.Normalize(name),
	})
}

func unionSubjectIDs(gold map[string]GoldSubject, predicted map[string]extract.ExtractedSubject) []string {
	seen := make(map[string]struct{}, len(gold)+len(predicted))
	for id := range gold {
		seen[id] = struct{}{}
	}
	for id := range predicted {
		seen[id] = struct{}{}
	}
	ids := make([]string, 0, len(seen))
	for id := range seen {
		ids = append(ids, id)
	}
	sort.Strings(ids)
	return ids
}

func judgeClaims(ctx context.Context, j *Judge, subject string, goldClaims, predictedClaims []string) (SubjectClaimResult, error) {
	if j == nil {
		return SubjectClaimResult{}, fmt.Errorf("eval judge: nil judge")
	}
	verdict, err := llm.JSON[judgeVerdict](ctx, j.c, j.site, renderJudgePrompt(subject, goldClaims, predictedClaims), validateJudgeVerdict(goldClaims, predictedClaims))
	if err != nil {
		return SubjectClaimResult{}, err
	}
	detail := SubjectClaimResult{
		Subject: subject,
		Missed:  append([]string(nil), verdict.Missed...),
		Extra:   append([]string(nil), verdict.Extra...),
	}
	for _, covered := range verdict.Covered {
		detail.Covered = append(detail.Covered, ClaimMatch{
			Gold:      covered.Gold,
			Predicted: covered.Predicted,
		})
	}
	return detail, nil
}

type judgeVerdict struct {
	Covered []judgeCovered `json:"covered"`
	Missed  []string       `json:"missed"`
	Extra   []string       `json:"extra"`
}

type judgeCovered struct {
	Gold      string `json:"gold"`
	Predicted string `json:"predicted"`
}

func renderJudgePrompt(subject string, goldClaims, predictedClaims []string) string {
	type payload struct {
		Subject         string   `json:"subject"`
		GoldClaims      []string `json:"gold_claims"`
		PredictedClaims []string `json:"predicted_claims"`
	}
	raw, _ := json.MarshalIndent(payload{
		Subject:         subject,
		GoldClaims:      goldClaims,
		PredictedClaims: predictedClaims,
	}, "", "  ")
	return "Judge whether predicted claims cover gold claims for one matched subject.\n" +
		"Return JSON with shape {\"covered\":[{\"gold\":\"...\",\"predicted\":\"...\"}],\"missed\":[\"...\"],\"extra\":[\"...\"]}.\n" +
		"Use exact claim strings from the supplied arrays.\n\n" +
		string(raw)
}

func validateJudgeVerdict(goldClaims, predictedClaims []string) func(*judgeVerdict) error {
	return func(v *judgeVerdict) error {
		if v == nil {
			return fmt.Errorf("verdict required")
		}
		goldSet := stringSet(goldClaims)
		predictedSet := stringSet(predictedClaims)
		seenGold := make(map[string]string, len(goldClaims))
		seenPredicted := make(map[string]string, len(predictedClaims))
		for i, covered := range v.Covered {
			if _, ok := goldSet[covered.Gold]; !ok {
				return fmt.Errorf("covered[%d].gold is not a gold claim", i)
			}
			if _, ok := predictedSet[covered.Predicted]; !ok {
				return fmt.Errorf("covered[%d].predicted is not a predicted claim", i)
			}
			if prev, ok := seenGold[covered.Gold]; ok {
				return fmt.Errorf("gold claim appears in both %s and covered[%d]", prev, i)
			}
			seenGold[covered.Gold] = fmt.Sprintf("covered[%d]", i)
			if prev, ok := seenPredicted[covered.Predicted]; ok {
				return fmt.Errorf("predicted claim appears in both %s and covered[%d]", prev, i)
			}
			seenPredicted[covered.Predicted] = fmt.Sprintf("covered[%d]", i)
		}
		for i, missed := range v.Missed {
			if _, ok := goldSet[missed]; !ok {
				return fmt.Errorf("missed[%d] is not a gold claim", i)
			}
			if prev, ok := seenGold[missed]; ok {
				return fmt.Errorf("gold claim appears in both %s and missed[%d]", prev, i)
			}
			seenGold[missed] = fmt.Sprintf("missed[%d]", i)
		}
		for i, extra := range v.Extra {
			if _, ok := predictedSet[extra]; !ok {
				return fmt.Errorf("extra[%d] is not a predicted claim", i)
			}
			if prev, ok := seenPredicted[extra]; ok {
				return fmt.Errorf("predicted claim appears in both %s and extra[%d]", prev, i)
			}
			seenPredicted[extra] = fmt.Sprintf("extra[%d]", i)
		}
		if len(seenGold) != len(goldSet) {
			return fmt.Errorf("verdict must classify every gold claim")
		}
		if len(seenPredicted) != len(predictedSet) {
			return fmt.Errorf("verdict must classify every predicted claim")
		}
		return nil
	}
}

func stringSet(values []string) map[string]struct{} {
	set := make(map[string]struct{}, len(values))
	for _, value := range values {
		set[value] = struct{}{}
	}
	return set
}

func addCounts(a, b struct {
	cases               int
	subjectFound        int
	subjectMissed       int
	subjectHallucinated int
	claimCovered        int
	claimMissed         int
	claimExtra          int
}) struct {
	cases               int
	subjectFound        int
	subjectMissed       int
	subjectHallucinated int
	claimCovered        int
	claimMissed         int
	claimExtra          int
} {
	a.cases += b.cases
	a.subjectFound += b.subjectFound
	a.subjectMissed += b.subjectMissed
	a.subjectHallucinated += b.subjectHallucinated
	a.claimCovered += b.claimCovered
	a.claimMissed += b.claimMissed
	a.claimExtra += b.claimExtra
	return a
}

func bucketFromCounts(c struct {
	cases               int
	subjectFound        int
	subjectMissed       int
	subjectHallucinated int
	claimCovered        int
	claimMissed         int
	claimExtra          int
}) struct {
	Subjects Metrics
	Claims   Metrics
	Cases    int
} {
	return struct {
		Subjects Metrics
		Claims   Metrics
		Cases    int
	}{
		Subjects: metrics(c.subjectFound, c.subjectHallucinated, c.subjectMissed),
		Claims:   metrics(c.claimCovered, c.claimExtra, c.claimMissed),
		Cases:    c.cases,
	}
}

func metrics(found, extra, missed int) Metrics {
	return Metrics{
		Precision: ratio(found, found+extra),
		Recall:    ratio(found, found+missed),
	}
}

func ratio(n, d int) float64 {
	if d == 0 {
		return 0
	}
	return float64(n) / float64(d)
}

type goldFile struct {
	Difficulty string        `json:"difficulty"`
	Header     goldHeader    `json:"header"`
	Gold       []GoldSubject `json:"gold"`
}

type goldHeader struct {
	Source     string   `json:"source"`
	Title      string   `json:"title"`
	Tags       []string `json:"tags"`
	ReceivedAt string   `json:"received_at"`
}

func validateGold(caseName string, gold goldFile) error {
	switch gold.Difficulty {
	case "easy", "medium", "hard":
	default:
		return fmt.Errorf("load case %s: difficulty must be easy, medium, or hard", caseName)
	}
	if strings.TrimSpace(gold.Header.Source) == "" {
		return fmt.Errorf("load case %s: header.source required", caseName)
	}
	if strings.TrimSpace(gold.Header.Title) == "" {
		return fmt.Errorf("load case %s: header.title required", caseName)
	}
	if strings.TrimSpace(gold.Header.ReceivedAt) == "" {
		return fmt.Errorf("load case %s: header.received_at required", caseName)
	}
	if len(gold.Gold) == 0 {
		return fmt.Errorf("load case %s: gold subjects required", caseName)
	}
	for i, subject := range gold.Gold {
		if err := validateGoldSubject(i, subject); err != nil {
			return fmt.Errorf("load case %s: %w", caseName, err)
		}
	}
	return nil
}

func validateGoldSubject(i int, subject GoldSubject) error {
	switch subject.Type {
	case "entity", "event", "concept":
	default:
		return fmt.Errorf("gold[%d].type must be entity, event, or concept", i)
	}
	if strings.TrimSpace(subject.Name) == "" {
		return fmt.Errorf("gold[%d].name required", i)
	}
	if len(subject.Claims) == 0 {
		return fmt.Errorf("gold[%d].claims required", i)
	}
	for j, claim := range subject.Claims {
		if strings.TrimSpace(claim) == "" {
			return fmt.Errorf("gold[%d].claims[%d] required", i, j)
		}
	}
	return nil
}

func copyGold(in []GoldSubject) []GoldSubject {
	out := make([]GoldSubject, len(in))
	for i := range in {
		out[i] = GoldSubject{
			Type:   in[i].Type,
			Name:   in[i].Name,
			Claims: append([]string(nil), in[i].Claims...),
		}
	}
	return out
}
