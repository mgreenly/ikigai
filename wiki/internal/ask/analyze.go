package ask

import (
	"context"
	"fmt"
	"strings"

	"wiki/internal/llm"
	"wiki/internal/wiki"
)

const maxAnalysisSubQueries = 4

// Analyze runs one ask-subject call and returns the parsed, capped analysis.
func Analyze(ctx context.Context, c *llm.Client, site llm.CallSite, question string) (wiki.QueryAnalysis, error) {
	out, err := llm.JSON[wiki.QueryAnalysis](ctx, c, site, analysisPrompt(question), func(out *wiki.QueryAnalysis) error {
		if out == nil {
			return fmt.Errorf("analysis required")
		}
		normalizeQueryAnalysis(out)
		return nil
	})
	if err != nil {
		return wiki.QueryAnalysis{}, err
	}
	return out, nil
}

func normalizeQueryAnalysis(out *wiki.QueryAnalysis) {
	if out == nil {
		return
	}
	out.SubQueries = cleanAnalysisStrings(out.SubQueries, maxAnalysisSubQueries)
	out.Keywords = cleanAnalysisStrings(out.Keywords, 0)
	out.Aliases = cleanAnalysisStrings(out.Aliases, 0)
}

func cleanAnalysisStrings(in []string, cap int) []string {
	out := make([]string, 0, len(in))
	seen := map[string]struct{}{}
	for _, raw := range in {
		v := strings.TrimSpace(raw)
		if v == "" {
			continue
		}
		key := strings.ToLower(v)
		if _, ok := seen[key]; ok {
			continue
		}
		seen[key] = struct{}{}
		out = append(out, v)
		if cap > 0 && len(out) == cap {
			break
		}
	}
	return out
}
