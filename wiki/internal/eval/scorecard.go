package eval

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"sort"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/extract"
	"wiki/internal/llm"
)

// Scorecard is one extract-eval run with its resolved model configuration.
type Scorecard struct {
	Dataset string       `json:"dataset"`
	Model   string       `json:"model"`
	Config  string       `json:"config"`
	Judge   string       `json:"judge"`
	Cases   []CaseResult `json:"cases"`
	Totals  Totals       `json:"totals"`
}

// RunDataset loads all cases, runs production extraction, scores them, and stamps the run.
func RunDataset(ctx context.Context, dataset string, ex *extract.Extractor, extractSite llm.CallSite, j *Judge, judgeSite llm.CallSite) (Scorecard, error) {
	cases, err := LoadDataset(dataset)
	if err != nil {
		return Scorecard{}, err
	}
	results := make([]CaseResult, 0, len(cases))
	for _, c := range cases {
		predicted, err := Run(ctx, ex, c)
		if err != nil {
			return Scorecard{}, fmt.Errorf("eval extract %s: %w", c.Name, err)
		}
		result, err := Score(ctx, j, c, predicted)
		if err != nil {
			return Scorecard{}, fmt.Errorf("eval score %s: %w", c.Name, err)
		}
		results = append(results, result)
	}
	return Scorecard{
		Dataset: dataset,
		Model:   extractSite.Model,
		Config:  CallSiteParamsJSON(extractSite),
		Judge:   JudgeStampJSON(judgeSite),
		Cases:   results,
		Totals:  Aggregate(results),
	}, nil
}

// WriteHuman emits a stable, readable scorecard report.
func (s Scorecard) WriteHuman(w io.Writer) {
	fmt.Fprintf(w, "dataset: %s\n", s.Dataset)
	fmt.Fprintf(w, "model: %s\n", s.Model)
	fmt.Fprintf(w, "config: %s\n", s.Config)
	fmt.Fprintf(w, "judge: %s\n\n", s.Judge)
	fmt.Fprintln(w, "cases:")
	for _, c := range s.Cases {
		fmt.Fprintf(w, "- %s (%s)\n", c.Case, c.Difficulty)
		writeMetrics(w, "  subjects", len(c.Subjects.Found), len(c.Subjects.Hallucinated), len(c.Subjects.Missed))
		writeMetrics(w, "  claims", c.Claims.Covered, c.Claims.Extra, c.Claims.Missed)
		for _, detail := range c.ClaimText {
			fmt.Fprintf(w, "  subject %s covered=%d missed=%d extra=%d\n", detail.Subject, len(detail.Covered), len(detail.Missed), len(detail.Extra))
		}
	}
	fmt.Fprintln(w, "\naggregate:")
	writeMetricValues(w, "  subjects", s.Totals.Overall.Subjects, s.Totals.Overall.Cases)
	writeMetricValues(w, "  claims", s.Totals.Overall.Claims, s.Totals.Overall.Cases)
	fmt.Fprintln(w, "by difficulty:")
	for _, difficulty := range sortedDifficulties(s.Totals.ByDifficulty) {
		bucket := s.Totals.ByDifficulty[difficulty]
		fmt.Fprintf(w, "- %s\n", difficulty)
		writeMetricValues(w, "  subjects", bucket.Subjects, bucket.Cases)
		writeMetricValues(w, "  claims", bucket.Claims, bucket.Cases)
	}
}

// WriteJSON emits the machine-readable scorecard form.
func (s Scorecard) WriteJSON(w io.Writer) {
	enc := json.NewEncoder(w)
	enc.SetIndent("", "  ")
	_ = enc.Encode(s)
}

// CallSiteParamsJSON returns the resolved generation parameters for a call site.
func CallSiteParamsJSON(site llm.CallSite) string {
	return compactJSON(callSiteParams{
		Temperature: site.Temperature,
		Reasoning:   reasoningStamp(site.Reasoning),
		MaxTokens:   site.MaxTokens,
	})
}

// JudgeStampJSON returns the judge model and generation parameters.
func JudgeStampJSON(site llm.CallSite) string {
	return compactJSON(judgeStamp{
		Model:  site.Model,
		Params: json.RawMessage(CallSiteParamsJSON(site)),
	})
}

type callSiteParams struct {
	Temperature *float64 `json:"temperature,omitempty"`
	Reasoning   any      `json:"reasoning,omitempty"`
	MaxTokens   int      `json:"max_tokens,omitempty"`
}

type judgeStamp struct {
	Model  string          `json:"model"`
	Params json.RawMessage `json:"params"`
}

func writeMetrics(w io.Writer, label string, found, extra, missed int) {
	writeMetricValues(w, label, metrics(found, extra, missed), 0)
	fmt.Fprintf(w, "%s counts found=%d extra=%d missed=%d\n", label, found, extra, missed)
}

func writeMetricValues(w io.Writer, label string, m Metrics, cases int) {
	if cases > 0 {
		fmt.Fprintf(w, "%s precision=%.3f recall=%.3f cases=%d\n", label, m.Precision, m.Recall, cases)
		return
	}
	fmt.Fprintf(w, "%s precision=%.3f recall=%.3f\n", label, m.Precision, m.Recall)
}

func sortedDifficulties(in map[string]struct {
	Subjects Metrics
	Claims   Metrics
	Cases    int
}) []string {
	out := make([]string, 0, len(in))
	for difficulty := range in {
		out = append(out, difficulty)
	}
	sort.Strings(out)
	return out
}

func reasoningStamp(reasoning any) any {
	if reasoning == nil {
		return nil
	}
	if v, ok := reasoning.(agentkit.ReasoningValue); ok {
		if level, ok := v.Level(); ok {
			return level
		}
		if budget, ok := v.Budget(); ok {
			return budget
		}
		if v.Disabled() {
			return "disabled"
		}
		return nil
	}
	return "disabled"
}

func compactJSON(v any) string {
	raw, err := json.Marshal(v)
	if err != nil {
		return "{}"
	}
	return string(raw)
}
