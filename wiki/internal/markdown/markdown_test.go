package markdown

import (
	"strings"
	"testing"
)

func TestRenderConvertsHeadingsParagraphsAndInlineFormatting(t *testing.T) {
	// R-SS0J-U7PG
	got := render(t, "# Launch Plan\n\nHello **wiki** and _team_.")

	assertContains(t, got, `<h1 id="launch-plan">Launch Plan</h1>`)
	assertContains(t, got, `<p>Hello <strong>wiki</strong> and <em>team</em>.</p>`)
}

func TestRenderConvertsListsAndBlockquotes(t *testing.T) {
	// R-ST8G-7ZG5
	got := render(t, "> Notes\n\n- first\n- second")

	assertContains(t, got, "<blockquote>")
	assertContains(t, got, "<p>Notes</p>")
	assertContains(t, got, "<ul>")
	assertContains(t, got, "<li>first</li>")
	assertContains(t, got, "<li>second</li>")
}

func TestRenderConvertsFencedCodeBlocksAndEscapesCodeContent(t *testing.T) {
	// R-SUGC-LR6U
	got := render(t, "```go\nfmt.Println(\"<tag>\")\n```")

	assertContains(t, got, `<pre><code class="language-go">fmt.Println(&#34;&lt;tag&gt;&#34;)`)
	assertNotContains(t, got, "<tag>")
}

func TestRenderConvertsTablesWithGFM(t *testing.T) {
	// R-SVO8-ZIXJ
	got := render(t, "| Name | Count |\n| --- | ---: |\n| Alpha | 2 |")

	assertContains(t, got, "<table>")
	assertContains(t, got, "<th>Name</th>")
	assertContains(t, got, "<th>Count</th>")
	assertContains(t, got, "<td>Alpha</td>")
	assertContains(t, got, "<td>2</td>")
}

func TestRenderConvertsTaskListsWithoutUnsafeInputs(t *testing.T) {
	// R-SWW5-DAO8
	got := render(t, "- [x] done\n- [ ] next")

	assertContains(t, got, `<input checked="" disabled="" type="checkbox">`)
	assertContains(t, got, `<input disabled="" type="checkbox">`)
	assertContains(t, got, "done")
	assertContains(t, got, "next")
}

func TestRenderSanitizesScriptElements(t *testing.T) {
	// R-SY41-R2EX
	got := render(t, "Hello<script>alert(1)</script>after")

	assertContains(t, got, "Hello")
	assertContains(t, got, "after")
	assertNotContains(t, got, "<script")
	assertNotContains(t, got, "alert(1)")
}

func TestRenderSanitizesDangerousAttributes(t *testing.T) {
	// R-SZBY-4U5M
	got := render(t, `<img src="https://example.test/a.png" onerror="alert(1)" alt="diagram">`)

	assertContains(t, got, `<img src="https://example.test/a.png" alt="diagram">`)
	assertNotContains(t, got, "onerror")
	assertNotContains(t, got, "alert(1)")
}

func TestRenderSanitizesDangerousLinkProtocols(t *testing.T) {
	// R-T0JU-ILWB
	got := render(t, `[bad](javascript:alert(1)) [good](https://example.test/wiki)`)

	assertContains(t, got, `<a href="https://example.test/wiki" rel="nofollow">good</a>`)
	assertNotContains(t, got, "javascript:")
	assertNotContains(t, got, "alert(1)")
}

func TestRenderPreservesSafeRelativeLinks(t *testing.T) {
	// R-T1RQ-WDN0
	got := render(t, `[Subject](../entity/acme-robotics "profile")`)

	assertContains(t, got, `<a href="../entity/acme-robotics" title="profile" rel="nofollow">Subject</a>`)
}

func TestRenderReturnsEmptyHTMLForEmptyMarkdown(t *testing.T) {
	// R-T2ZN-A5DP
	got := render(t, "")

	if got != "" {
		t.Fatalf("Render(empty) = %q, want empty HTML", got)
	}
}

func render(t *testing.T, source string) string {
	t.Helper()
	got, err := Render(source)
	if err != nil {
		t.Fatalf("Render returned error: %v", err)
	}
	return got
}

func assertContains(t *testing.T, got, want string) {
	t.Helper()
	if !strings.Contains(got, want) {
		t.Fatalf("rendered HTML missing %q\nhtml:\n%s", want, got)
	}
}

func assertNotContains(t *testing.T, got, forbidden string) {
	t.Helper()
	if strings.Contains(got, forbidden) {
		t.Fatalf("rendered HTML contains %q\nhtml:\n%s", forbidden, got)
	}
}
