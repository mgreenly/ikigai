package retrieve

import (
	"context"
	"fmt"
	"math"
	"reflect"
	"sort"
	"strings"
)

const (
	defaultRRFK    = 60
	defaultPerLane = 60
	defaultFinalK  = DefaultLimit
)

// FusionConfig controls hybrid retrieval fanout and final result count.
type FusionConfig struct {
	RRFk    int
	PerLane int
	FinalK  int
}

// NewHybridRetriever returns a retriever that fuses keyword and vector lanes.
func NewHybridRetriever(keyword, vector Retriever, resolver, pages any, cfg FusionConfig) *hybridRetriever {
	return &hybridRetriever{
		keyword:  keyword,
		vector:   vector,
		resolver: resolver,
		pages:    pages,
		cfg:      cfg,
	}
}

type hybridRetriever struct {
	keyword  Retriever
	vector   Retriever
	resolver any
	pages    any
	cfg      FusionConfig
}

func (r *hybridRetriever) Search(ctx context.Context, query string, limits SearchLimits) (Result, error) {
	return r.search(ctx, []hybridLaneQuery{{
		meaning: strings.TrimSpace(query),
		keyword: strings.TrimSpace(query),
	}}, []string{query}, limits)
}

// SearchAnalyzed fuses every analyzed sub-query lane list in one pass.
func (r *hybridRetriever) SearchAnalyzed(ctx context.Context, qa any, limits SearchLimits) (Result, error) {
	analyzed := extractQueryAnalysis(qa)
	keywordQuery := joinAnalyzedTerms(analyzed.keywords, analyzed.aliases)
	queries := make([]hybridLaneQuery, 0, len(analyzed.subQueries))
	for _, sub := range analyzed.subQueries {
		sub = strings.TrimSpace(sub)
		if sub == "" {
			continue
		}
		queries = append(queries, hybridLaneQuery{meaning: sub, keyword: keywordQuery})
	}
	if len(queries) == 0 {
		fallback := keywordQuery
		queries = append(queries, hybridLaneQuery{meaning: fallback, keyword: fallback})
	}

	pinCandidates := append([]string(nil), analyzed.subQueries...)
	pinCandidates = append(pinCandidates, analyzed.aliases...)
	return r.search(ctx, queries, pinCandidates, limits)
}

type hybridLaneQuery struct {
	meaning string
	keyword string
}

func (r *hybridRetriever) search(ctx context.Context, queries []hybridLaneQuery, pinCandidates []string, limits SearchLimits) (Result, error) {
	cfg := r.cfg.resolve(limits)
	acc := map[string]*fusedHit{}
	var order int
	var topDense float64
	var sawDense bool

	for _, q := range queries {
		if r.keyword != nil && strings.TrimSpace(q.keyword) != "" {
			result, err := r.keyword.Search(ctx, q.keyword, SearchLimits{Limit: cfg.PerLane})
			if err != nil {
				return Result{}, err
			}
			order = addFused(acc, result.Hits, cfg.RRFk, order)
		}
		if r.vector != nil && strings.TrimSpace(q.meaning) != "" {
			result, err := r.vector.Search(ctx, q.meaning, SearchLimits{Limit: cfg.PerLane})
			if err != nil {
				return Result{}, err
			}
			for _, hit := range result.Hits {
				if !sawDense || hit.Score > topDense {
					topDense = hit.Score
					sawDense = true
				}
			}
			order = addFused(acc, result.Hits, cfg.RRFk, order)
		}
	}

	hits := sortedFusedHits(acc)
	if len(hits) > cfg.FinalK {
		hits = hits[:cfg.FinalK]
	}

	pinned, ok, err := r.pinnedHit(ctx, pinCandidates)
	if err != nil {
		return Result{}, err
	}
	if ok {
		hits = pinFirst(hits, pinned, cfg.FinalK)
	}
	return Result{Hits: hits, TopDense: topDense, Pinned: ok}, nil
}

type resolvedFusionConfig struct {
	RRFk    int
	PerLane int
	FinalK  int
}

func (c FusionConfig) resolve(limits SearchLimits) resolvedFusionConfig {
	out := resolvedFusionConfig{RRFk: c.RRFk, PerLane: c.PerLane, FinalK: c.FinalK}
	if out.RRFk <= 0 {
		out.RRFk = defaultRRFK
	}
	if out.PerLane <= 0 {
		out.PerLane = defaultPerLane
	}
	if out.FinalK <= 0 {
		out.FinalK = defaultFinalK
	}
	if limits.Limit > 0 && limits.Resolve().Limit < out.FinalK {
		out.FinalK = limits.Resolve().Limit
	}
	if out.FinalK < 1 {
		out.FinalK = 1
	}
	return out
}

type fusedHit struct {
	hit   Hit
	score float64
	order int
}

func addFused(acc map[string]*fusedHit, hits []Hit, rrfK int, nextOrder int) int {
	for i, hit := range hits {
		if strings.TrimSpace(hit.PageID) == "" {
			continue
		}
		contribution := 1 / (float64(rrfK) + float64(i+1))
		existing := acc[hit.PageID]
		if existing == nil {
			hit.Score = contribution
			acc[hit.PageID] = &fusedHit{hit: hit, score: contribution, order: nextOrder}
			nextOrder++
			continue
		}
		existing.score += contribution
		existing.hit.Score = existing.score
		mergeHitFields(&existing.hit, hit)
	}
	return nextOrder
}

func mergeHitFields(dst *Hit, src Hit) {
	if dst.Path == "" {
		dst.Path = src.Path
	}
	if dst.Title == "" {
		dst.Title = src.Title
	}
	if dst.Snippet == "" {
		dst.Snippet = src.Snippet
	}
}

func sortedFusedHits(acc map[string]*fusedHit) []Hit {
	fused := make([]*fusedHit, 0, len(acc))
	for _, item := range acc {
		fused = append(fused, item)
	}
	sort.SliceStable(fused, func(i, j int) bool {
		if fused[i].score != fused[j].score {
			return fused[i].score > fused[j].score
		}
		if fused[i].hit.Title != fused[j].hit.Title {
			return fused[i].hit.Title < fused[j].hit.Title
		}
		if fused[i].hit.PageID != fused[j].hit.PageID {
			return fused[i].hit.PageID < fused[j].hit.PageID
		}
		return fused[i].order < fused[j].order
	})
	out := make([]Hit, 0, len(fused))
	for _, item := range fused {
		item.hit.Score = item.score
		out = append(out, item.hit)
	}
	return out
}

func pinFirst(hits []Hit, pinned Hit, limit int) []Hit {
	out := make([]Hit, 0, limit)
	out = append(out, pinned)
	for _, hit := range hits {
		if hit.PageID == pinned.PageID {
			continue
		}
		out = append(out, hit)
		if len(out) == limit {
			break
		}
	}
	return out
}

func (r *hybridRetriever) pinnedHit(ctx context.Context, candidates []string) (Hit, bool, error) {
	if r == nil || r.resolver == nil || r.pages == nil {
		return Hit{}, false, nil
	}
	seen := map[string]struct{}{}
	for _, candidate := range candidates {
		candidate = strings.TrimSpace(candidate)
		if candidate == "" {
			continue
		}
		key := strings.ToLower(candidate)
		if _, ok := seen[key]; ok {
			continue
		}
		seen[key] = struct{}{}

		subject, ok, err := callResolveByName(ctx, r.resolver, candidate)
		if err != nil {
			return Hit{}, false, err
		}
		if !ok {
			continue
		}
		page, ok, err := callGetBySubject(ctx, r.pages, subject.id)
		if err != nil {
			return Hit{}, false, err
		}
		if !ok {
			continue
		}
		title := page.title
		if title == "" {
			title = subject.name
		}
		pageID := page.id
		if pageID == "" {
			pageID = subject.id
		}
		return Hit{
			PageID:  pageID,
			Path:    subject.path(),
			Title:   title,
			Snippet: page.body,
			Score:   math.Inf(1),
		}, true, nil
	}
	return Hit{}, false, nil
}

type reflectedSubject struct {
	id       string
	name     string
	normName string
	typ      string
}

func (s reflectedSubject) path() string {
	if s.typ == "" || s.normName == "" {
		return ""
	}
	return s.typ + "/" + s.normName
}

type reflectedPage struct {
	id    string
	title string
	body  string
}

func callResolveByName(ctx context.Context, resolver any, name string) (reflectedSubject, bool, error) {
	out, ok, err := callStoreMethod(ctx, resolver, "ResolveByName", name)
	if err != nil || !ok {
		return reflectedSubject{}, false, err
	}
	subject := reflectedSubject{
		id:       fieldString(out, "ID"),
		name:     fieldString(out, "Name"),
		normName: fieldString(out, "NormName"),
		typ:      fieldString(out, "Type"),
	}
	if subject.id == "" {
		return reflectedSubject{}, false, fmt.Errorf("retrieve: resolved subject missing ID")
	}
	return subject, true, nil
}

func callGetBySubject(ctx context.Context, pages any, subjectID string) (reflectedPage, bool, error) {
	out, ok, err := callStoreMethod(ctx, pages, "GetBySubject", subjectID)
	if err != nil || !ok {
		return reflectedPage{}, false, err
	}
	return reflectedPage{
		id:    fieldString(out, "ID"),
		title: fieldString(out, "Title"),
		body:  fieldString(out, "Body"),
	}, true, nil
}

func callStoreMethod(ctx context.Context, receiver any, name string, arg string) (reflect.Value, bool, error) {
	v := reflect.ValueOf(receiver)
	if !v.IsValid() {
		return reflect.Value{}, false, nil
	}
	method := v.MethodByName(name)
	if !method.IsValid() {
		return reflect.Value{}, false, fmt.Errorf("retrieve: %T has no %s method", receiver, name)
	}
	out := method.Call([]reflect.Value{reflect.ValueOf(ctx), reflect.ValueOf(arg)})
	if len(out) != 2 {
		return reflect.Value{}, false, fmt.Errorf("retrieve: %T.%s returned %d values, want 2", receiver, name, len(out))
	}
	if err, ok := out[1].Interface().(error); ok && err != nil {
		if strings.Contains(err.Error(), "not found") || strings.Contains(err.Error(), "no rows") {
			return reflect.Value{}, false, nil
		}
		return reflect.Value{}, false, err
	}
	return out[0], true, nil
}

func fieldString(v reflect.Value, name string) string {
	if v.Kind() == reflect.Pointer {
		if v.IsNil() {
			return ""
		}
		v = v.Elem()
	}
	if v.Kind() != reflect.Struct {
		return ""
	}
	f := v.FieldByName(name)
	if !f.IsValid() || f.Kind() != reflect.String {
		return ""
	}
	return f.String()
}

type reflectedQueryAnalysis struct {
	subQueries []string
	keywords   []string
	aliases    []string
}

func extractQueryAnalysis(qa any) reflectedQueryAnalysis {
	v := reflect.ValueOf(qa)
	if v.Kind() == reflect.Pointer {
		if v.IsNil() {
			return reflectedQueryAnalysis{}
		}
		v = v.Elem()
	}
	if v.Kind() != reflect.Struct {
		return reflectedQueryAnalysis{}
	}
	return reflectedQueryAnalysis{
		subQueries: fieldStringSlice(v, "SubQueries"),
		keywords:   fieldStringSlice(v, "Keywords"),
		aliases:    fieldStringSlice(v, "Aliases"),
	}
}

func fieldStringSlice(v reflect.Value, name string) []string {
	f := v.FieldByName(name)
	if !f.IsValid() || f.Kind() != reflect.Slice || f.Type().Elem().Kind() != reflect.String {
		return nil
	}
	out := make([]string, 0, f.Len())
	for i := 0; i < f.Len(); i++ {
		out = append(out, f.Index(i).String())
	}
	return out
}

func joinAnalyzedTerms(keywords, aliases []string) string {
	var terms []string
	seen := map[string]struct{}{}
	for _, term := range append(append([]string(nil), keywords...), aliases...) {
		term = strings.TrimSpace(term)
		if term == "" {
			continue
		}
		key := strings.ToLower(term)
		if _, ok := seen[key]; ok {
			continue
		}
		seen[key] = struct{}{}
		terms = append(terms, term)
	}
	return strings.Join(terms, " OR ")
}
