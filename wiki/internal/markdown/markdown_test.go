package markdown

import (
	"strings"
	"testing"
)

func TestRenderConvertsHeadingMarkup(t *testing.T) {
	// R-SS0J-U7PG
	got := render("## Acme")

	assertContains(t, got, `<h2 id="acme">Acme</h2>`)
	assertNotContains(t, got, "## Acme")
}

func TestRenderConvertsStrongAndEmphasisMarkup(t *testing.T) {
	// R-ST8G-7ZG5
	got := render("**bold** and *italic*")

	assertContains(t, got, `<strong>bold</strong>`)
	assertContains(t, got, `<em>italic</em>`)
	assertNotContains(t, got, "**")
	assertNotContains(t, got, "*italic*")
}

func TestRenderConvertsBulletedAndOrderedLists(t *testing.T) {
	// R-SUGC-LR6U
	got := render("- one\n- two\n\n1. first\n2. second")

	assertContains(t, got, "<ul>")
	assertContains(t, got, "<li>one</li>")
	assertContains(t, got, "<li>two</li>")
	assertContains(t, got, "<ol>")
	assertContains(t, got, "<li>first</li>")
	assertContains(t, got, "<li>second</li>")
	assertNotContains(t, got, "- one")
	assertNotContains(t, got, "1. first")
}

func TestRenderConvertsInlineAndFencedCode(t *testing.T) {
	// R-SVO8-ZIXJ
	got := render("Inline `x`.\n\n```go\nfmt.Println(\"x\")\n```")

	assertContains(t, got, `<code>x</code>`)
	assertContains(t, got, `<pre><code class="language-go">fmt.Println(&#34;x&#34;)`)
	assertNotContains(t, got, "```")
	assertNotContains(t, got, "`x`")
}

func TestRenderConvertsBlockquotes(t *testing.T) {
	// R-SWW5-DAO8
	got := render("> quoted")

	assertContains(t, got, "<blockquote>")
	assertContains(t, got, "<p>quoted</p>")
	assertNotContains(t, got, "> quoted")
}

func TestRenderConvertsGFMTables(t *testing.T) {
	// R-SY41-R2EX
	got := render("| A | B |\n| - | - |\n| 1 | 2 |")

	assertContains(t, got, "<table>")
	assertContains(t, got, "<th>A</th>")
	assertContains(t, got, "<th>B</th>")
	assertContains(t, got, "<td>1</td>")
	assertContains(t, got, "<td>2</td>")
	assertNotContains(t, got, "| A | B |")
	assertNotContains(t, got, "| 1 | 2 |")
}

func TestRenderPreservesSafeHTTPSLinks(t *testing.T) {
	// R-SZBY-4U5M
	got := render("[Acme](https://x.test)")

	assertContains(t, got, `<a href="https://x.test" rel="nofollow">Acme</a>`)
}

func TestRenderStripsScriptElements(t *testing.T) {
	// R-T0JU-ILWB
	got := strings.ToLower(render("<script>alert(1)</script>"))

	assertNotContains(t, got, "<script")
	assertNotContains(t, got, "</script")
	assertNotContains(t, got, "alert(1)")
}

func TestRenderRemovesDangerousLinkSchemes(t *testing.T) {
	// R-T1RQ-WDN0
	got := render("[x](javascript:alert(1))")

	assertNotContains(t, strings.ToLower(got), `href="javascript:`)
	assertNotContains(t, got, "javascript:alert(1)")
}

func TestRenderWrapsPlainProseInParagraph(t *testing.T) {
	// R-T2ZN-A5DP
	got := render("Just a sentence.")

	if got != "<p>Just a sentence.</p>\n" {
		t.Fatalf("Render(prose) = %q, want paragraph-wrapped prose", got)
	}
}

func render(source string) string {
	return string(Render(source))
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
