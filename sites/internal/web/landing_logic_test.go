// Package web tests the browser-independent landing-page logic.
package web

import (
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"github.com/dop251/goja"
)

type landingRow struct {
	Slug          string `json:"slug"`
	URL           string `json:"url"`
	Public        bool   `json:"public"`
	CreatedBy     string `json:"createdBy"`
	CreatedAt     string `json:"createdAt"`
	CreatedAtSort string `json:"createdAtSort"`
}

func landingEval(t *testing.T, expression string, out any) {
	t.Helper()
	source, err := os.ReadFile(filepath.Join("..", "..", "share", "www", "static", "landing.js"))
	if err != nil {
		t.Fatal(err)
	}
	runtime := goja.New()
	if _, err := runtime.RunString(string(source)); err != nil {
		t.Fatal(err)
	}
	value, err := runtime.RunString("JSON.stringify(" + expression + ")")
	if err != nil {
		t.Fatal(err)
	}
	if err := json.Unmarshal([]byte(value.String()), out); err != nil {
		t.Fatal(err)
	}
}

func landingControllerEval(t *testing.T, rows []landingRow, expression string, out any) {
	t.Helper()
	source, err := os.ReadFile(filepath.Join("..", "..", "share", "www", "static", "landing.js"))
	if err != nil {
		t.Fatal(err)
	}
	runtime := goja.New()
	prelude := strings.Replace(`
function node() {
  return {
    children: [], dataset: {}, attributes: {}, listeners: {},
    appendChild: function(child) { this.children.push(child); },
    append: function() { for (var i = 0; i < arguments.length; i++) this.children.push(arguments[i]); },
    replaceChildren: function() { this.children = []; },
    toggleAttribute: function(name, present) { if (present && !Object.prototype.hasOwnProperty.call(this.attributes, name)) this.attributes[name] = ""; else if (!present) delete this.attributes[name]; },
    setAttribute: function(name, value) { this.attributes[name] = value; },
    addEventListener: function(name, listener) { this.listeners[name] = listener; }
  };
}
var nodes = {
  data: node(), controls: node(), pager: node(), noMatch: node(), search: node(), clear: node(), previous: node(), next: node(), label: node(), body: node(),
  name: node(), createdBy: node(), createdAt: node()
};
nodes.data.textContent = JSON.stringify(ROWS);
nodes.name.dataset.sortKey = "name";
nodes.createdBy.dataset.sortKey = "createdBy";
nodes.createdAt.dataset.sortKey = "createdAt";
var document = {
  documentElement: { className: "no-js" },
  createElement: node,
  querySelector: function(selector) {
    return {"#sites-data": nodes.data, ".controls": nodes.controls, ".pager": nodes.pager, ".no-match": nodes.noMatch, "#site-search": nodes.search, "#site-clear": nodes.clear, "#pager-prev": nodes.previous, "#pager-next": nodes.next, "#pager-label": nodes.label, ".site-table tbody": nodes.body}[selector];
  },
  querySelectorAll: function() { return [nodes.name, nodes.createdBy, nodes.createdAt]; },
  addEventListener: function(name, listener) { if (name === "DOMContentLoaded") listener(); }
};
`, "ROWS", landingJSON(t, rows), 1)
	if _, err := runtime.RunString(prelude + string(source)); err != nil {
		t.Fatal(err)
	}
	value, err := runtime.RunString("JSON.stringify(" + expression + ")")
	if err != nil {
		t.Fatal(err)
	}
	if err := json.Unmarshal([]byte(value.String()), out); err != nil {
		t.Fatal(err)
	}
}

func landingJSON(t *testing.T, value any) string {
	t.Helper()
	encoded, err := json.Marshal(value)
	if err != nil {
		t.Fatal(err)
	}
	return string(encoded)
}

func rows(count int) []landingRow {
	result := make([]landingRow, count)
	for i := range result {
		result[i] = landingRow{
			Slug:          "site-" + string(rune('a'+i)),
			URL:           "https://example.test/" + string(rune('a'+i)),
			Public:        true,
			CreatedBy:     "owner",
			CreatedAt:     "display",
			CreatedAtSort: "2025-01-" + string(rune('0'+(i/10))) + string(rune('0'+(i%10))) + "T00:00:00Z",
		}
	}
	return result
}

func slugs(rows []landingRow) []string {
	got := make([]string, len(rows))
	for i, row := range rows {
		got[i] = row.Slug
	}
	return got
}

func TestFilterSitesSubsequence(t *testing.T) {
	// R-HU67-LJBW
	input := []landingRow{{Slug: "docs"}, {Slug: "dashboard"}, {Slug: "blog"}}
	var got []landingRow
	landingEval(t, "SitesLanding.filterSites("+landingJSON(t, input)+", 'dsb')", &got)
	if want := []string{"dashboard"}; !reflect.DeepEqual(slugs(got), want) {
		t.Fatalf("filter slugs = %v, want %v", slugs(got), want)
	}
}

func TestFilterSitesCaseInsensitive(t *testing.T) {
	// R-HVE3-ZB2L
	input := []landingRow{{Slug: "docs"}, {Slug: "dashboard"}}
	var docs, dashboard []landingRow
	landingEval(t, "SitesLanding.filterSites("+landingJSON(t, input)+", 'DOCS')", &docs)
	landingEval(t, "SitesLanding.filterSites("+landingJSON(t, input)+", 'DsB')", &dashboard)
	if !reflect.DeepEqual(slugs(docs), []string{"docs"}) || !reflect.DeepEqual(slugs(dashboard), []string{"dashboard"}) {
		t.Fatalf("case-insensitive results = %v, %v", slugs(docs), slugs(dashboard))
	}
}

func TestFilterSitesEmptyPreservesOrder(t *testing.T) {
	// R-HWM0-D2TA
	input := []landingRow{{Slug: "zebra"}, {Slug: "able"}, {Slug: "middle"}}
	var got []landingRow
	landingEval(t, "SitesLanding.filterSites("+landingJSON(t, input)+", '')", &got)
	if !reflect.DeepEqual(slugs(got), slugs(input)) {
		t.Fatalf("filter order = %v, want %v", slugs(got), slugs(input))
	}
}

func TestFilterSitesDoesNotSortMatches(t *testing.T) {
	// R-HXTW-QUJZ
	input := []landingRow{{Slug: "zebra-app"}, {Slug: "able-app"}}
	var got []landingRow
	landingEval(t, "SitesLanding.filterSites("+landingJSON(t, input)+", 'app')", &got)
	if want := []string{"zebra-app", "able-app"}; !reflect.DeepEqual(slugs(got), want) {
		t.Fatalf("filter order = %v, want %v", slugs(got), want)
	}
}

func TestSortRowsNameDescendingReversesAscending(t *testing.T) {
	// R-HZ1T-4MAO
	input := []landingRow{{Slug: "beta"}, {Slug: "alpha"}, {Slug: "gamma"}}
	var asc, desc []landingRow
	landingEval(t, "SitesLanding.sortRows("+landingJSON(t, input)+", 'name', 'asc')", &asc)
	landingEval(t, "SitesLanding.sortRows("+landingJSON(t, input)+", 'name', 'desc')", &desc)
	if want := []string{"gamma", "beta", "alpha"}; !reflect.DeepEqual(slugs(desc), want) || !reflect.DeepEqual(slugs(asc), []string{"alpha", "beta", "gamma"}) {
		t.Fatalf("name sort asc=%v desc=%v", slugs(asc), slugs(desc))
	}
}

func TestSortRowsCreatedAtUsesSortableTimestamp(t *testing.T) {
	// R-I1HL-W5S2
	input := []landingRow{
		{Slug: "new", CreatedAt: "January 2026", CreatedAtSort: "2026-01-01T00:00:00Z"},
		{Slug: "old", CreatedAt: "December 2024", CreatedAtSort: "2024-12-01T00:00:00Z"},
		{Slug: "middle", CreatedAt: "February 2025", CreatedAtSort: "2025-02-01T00:00:00Z"},
	}
	var got []landingRow
	landingEval(t, "SitesLanding.sortRows("+landingJSON(t, input)+", 'createdAt', 'desc')", &got)
	if want := []string{"new", "middle", "old"}; !reflect.DeepEqual(slugs(got), want) {
		t.Fatalf("createdAt sort = %v, want %v", slugs(got), want)
	}
}

func TestSortRowsCreatedByAscending(t *testing.T) {
	// R-I2PI-9XIR
	input := []landingRow{{Slug: "one", CreatedBy: "zoe"}, {Slug: "two", CreatedBy: "amy"}, {Slug: "three", CreatedBy: "mike"}}
	var got []landingRow
	landingEval(t, "SitesLanding.sortRows("+landingJSON(t, input)+", 'createdBy', 'asc')", &got)
	if want := []string{"two", "three", "one"}; !reflect.DeepEqual(slugs(got), want) {
		t.Fatalf("createdBy sort = %v, want %v", slugs(got), want)
	}
}

func TestSortRowsTiesBreakBySlugIndependentOfInputOrder(t *testing.T) {
	// R-I3XE-NP9G
	input := []landingRow{{Slug: "zeta", CreatedAtSort: "2025-01-01T00:00:00Z"}, {Slug: "alpha", CreatedAtSort: "2025-01-01T00:00:00Z"}, {Slug: "middle", CreatedAtSort: "2025-01-01T00:00:00Z"}}
	var first, second []landingRow
	landingEval(t, "SitesLanding.sortRows("+landingJSON(t, input)+", 'createdAt', 'desc')", &first)
	landingEval(t, "SitesLanding.sortRows("+landingJSON(t, []landingRow{input[2], input[0], input[1]})+", 'createdAt', 'desc')", &second)
	if want := []string{"alpha", "middle", "zeta"}; !reflect.DeepEqual(slugs(first), want) || !reflect.DeepEqual(slugs(second), want) {
		t.Fatalf("tied sorts = %v, %v; want %v", slugs(first), slugs(second), want)
	}
}

func TestNextSortToggleAndNewColumn(t *testing.T) {
	// R-I55B-1H05
	var same, different struct{ SortKey, Dir string }
	landingEval(t, "SitesLanding.nextSort({sortKey:'createdAt',dir:'desc'}, 'createdAt')", &same)
	landingEval(t, "SitesLanding.nextSort({sortKey:'createdAt',dir:'desc'}, 'name')", &different)
	if same.SortKey != "createdAt" || same.Dir != "asc" || different.SortKey != "name" || different.Dir != "asc" {
		t.Fatalf("next sorts = %#v, %#v", same, different)
	}
}

func TestPaginateSlicesPages(t *testing.T) {
	// R-I6D7-F8QU
	input := rows(34)
	var first, last, beyond []landingRow
	jsonRows := landingJSON(t, input)
	landingEval(t, "SitesLanding.paginate("+jsonRows+", 1, 10)", &first)
	landingEval(t, "SitesLanding.paginate("+jsonRows+", 4, 10)", &last)
	landingEval(t, "SitesLanding.paginate("+jsonRows+", 5, 10)", &beyond)
	if len(first) != 10 || first[0].Slug != input[0].Slug || len(last) != 4 || last[0].Slug != input[30].Slug || last[3].Slug != input[33].Slug || len(beyond) != 0 {
		t.Fatalf("pages have lengths %d, %d, %d", len(first), len(last), len(beyond))
	}
}

func TestComputeViewShowsPagerOnlyAboveTenRows(t *testing.T) {
	// R-I7L3-T0HJ
	var ten, eleven struct{ ShowPager bool }
	landingEval(t, "SitesLanding.computeView("+landingJSON(t, rows(10))+", SitesLanding.defaultState())", &ten)
	landingEval(t, "SitesLanding.computeView("+landingJSON(t, rows(11))+", SitesLanding.defaultState())", &eleven)
	if ten.ShowPager || !eleven.ShowPager {
		t.Fatalf("showPager for 10 and 11 rows = %t, %t", ten.ShowPager, eleven.ShowPager)
	}
}

func TestComputeViewClampsPageAndReportsRange(t *testing.T) {
	// R-I8T0-6S88
	input := landingJSON(t, rows(34))
	var fourth, clamped struct{ Page, PageCount, RangeFrom, RangeTo int }
	landingEval(t, "SitesLanding.computeView("+input+", {query:'',sortKey:'name',dir:'asc',page:4})", &fourth)
	landingEval(t, "SitesLanding.computeView("+input+", {query:'',sortKey:'name',dir:'asc',page:9})", &clamped)
	if fourth.Page != 4 || fourth.PageCount != 4 || fourth.RangeFrom != 31 || fourth.RangeTo != 34 || !reflect.DeepEqual(fourth, clamped) {
		t.Fatalf("fourth=%#v clamped=%#v", fourth, clamped)
	}
}

func TestComputeViewEmptyAndNoMatchStates(t *testing.T) {
	// R-IA0W-KJYX
	var empty, noMatch struct{ ShowControls, Empty, NoMatch, ShowPager bool }
	landingEval(t, "SitesLanding.computeView([], SitesLanding.defaultState())", &empty)
	landingEval(t, "SitesLanding.computeView("+landingJSON(t, rows(1))+", {query:'missing',sortKey:'name',dir:'asc',page:1})", &noMatch)
	if empty.ShowControls || !empty.Empty || noMatch.Empty || !noMatch.ShowControls || !noMatch.NoMatch || noMatch.ShowPager {
		t.Fatalf("empty=%#v noMatch=%#v", empty, noMatch)
	}
}

func TestDefaultStateAndClear(t *testing.T) {
	// R-IB8S-YBPM
	want := struct {
		Query, SortKey, Dir string
		Page                int
	}{"", "createdAt", "desc", 1}
	var state, cleared struct {
		Query, SortKey, Dir string
		Page                int
	}
	landingEval(t, "SitesLanding.defaultState()", &state)
	landingEval(t, "SitesLanding.reduce({query:'x',sortKey:'name',dir:'asc',page:3}, {type:'clear'})", &cleared)
	if state != want || cleared != want {
		t.Fatalf("default=%#v clear=%#v, want %#v", state, cleared, want)
	}
}

func TestReduceResetsOnlyQueryAndSortPages(t *testing.T) {
	// R-7V8B-GA0T
	var query, sort, page struct {
		Query, SortKey, Dir string
		Page                int
	}
	landingEval(t, "SitesLanding.reduce({query:'old',sortKey:'createdAt',dir:'desc',page:3}, {type:'setQuery',query:'x'})", &query)
	landingEval(t, "SitesLanding.reduce({query:'old',sortKey:'createdAt',dir:'desc',page:3}, {type:'setSort',key:'name'})", &sort)
	landingEval(t, "SitesLanding.reduce({query:'old',sortKey:'createdAt',dir:'desc',page:3}, {type:'setPage',page:2})", &page)
	if query.Page != 1 || query.Query != "x" || sort.Page != 1 || sort.SortKey != "name" || sort.Dir != "asc" || page.Page != 2 || page.Query != "old" || page.SortKey != "createdAt" || page.Dir != "desc" {
		t.Fatalf("query=%#v sort=%#v page=%#v", query, sort, page)
	}
}

func TestLandingControllerRendersAndWiresControls(t *testing.T) {
	// R-863D-5EDF
	input := []landingRow{
		{Slug: "zebra", URL: "/zebra", Public: false, CreatedBy: "zoe", CreatedAt: "Yesterday", CreatedAtSort: "2026-01-02T00:00:00Z"},
		{Slug: "alpha", URL: "/alpha", Public: true, CreatedBy: "amy", CreatedAt: "Today", CreatedAtSort: "2026-01-03T00:00:00Z"},
	}
	var got struct {
		ClassName       string
		ControlsHidden  bool
		NoMatchHidden   bool
		RowCount        int
		FirstSlug       string
		FirstURL        string
		FirstVisibility string
		Sort            string
	}
	landingControllerEval(t, input, `(function () {
	  nodes.search.value = "missing";
	  nodes.search.listeners.input();
	  var noMatch = !Object.prototype.hasOwnProperty.call(nodes.noMatch.attributes, "hidden") && nodes.body.children.length === 0;
	  nodes.clear.listeners.click();
	  nodes.name.listeners.click();
	  var first = nodes.body.children[0];
	  return {
	    ClassName: document.documentElement.className,
	    ControlsHidden: Object.prototype.hasOwnProperty.call(nodes.controls.attributes, "hidden"),
	    NoMatchHidden: noMatch,
	    RowCount: nodes.body.children.length,
	    FirstSlug: first.children[0].children[0].textContent,
	    FirstURL: first.children[0].children[0].href,
	    FirstVisibility: first.children[1].children[0].textContent,
	    Sort: nodes.name.attributes["aria-sort"]
	  };
	}())`, &got)
	if got.ClassName != "js" || got.ControlsHidden || !got.NoMatchHidden || got.RowCount != 2 || got.FirstSlug != "alpha" || got.FirstURL != "/alpha" || got.FirstVisibility != "public" || got.Sort != "ascending" {
		t.Fatalf("controller view = %#v, want rendered, searchable, sortable DOM state", got)
	}
}
