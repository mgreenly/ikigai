package markdown

import (
	"bytes"
	"html/template"
	"regexp"

	"github.com/microcosm-cc/bluemonday"
	"github.com/yuin/goldmark"
	"github.com/yuin/goldmark/extension"
	"github.com/yuin/goldmark/parser"
	"github.com/yuin/goldmark/renderer/html"
)

var (
	renderer = goldmark.New(
		goldmark.WithExtensions(extension.GFM, extension.TaskList),
		goldmark.WithParserOptions(parser.WithAutoHeadingID()),
		goldmark.WithRendererOptions(html.WithUnsafe()),
	)

	safeHTML = newPolicy()
)

// Render converts Markdown into sanitized HTML suitable for browser display.
func Render(source string) template.HTML {
	var out bytes.Buffer
	if err := renderer.Convert([]byte(source), &out); err != nil {
		return ""
	}
	return template.HTML(safeHTML.Sanitize(out.String()))
}

func newPolicy() *bluemonday.Policy {
	policy := bluemonday.UGCPolicy()

	identifier := regexp.MustCompile(`^[A-Za-z][A-Za-z0-9_.:-]*$`)
	policy.AllowAttrs("id").Matching(identifier).OnElements("h1", "h2", "h3", "h4", "h5", "h6")

	languageClass := regexp.MustCompile(`^language-[A-Za-z0-9_-]+$`)
	policy.AllowAttrs("class").Matching(languageClass).OnElements("code")

	tableAlign := regexp.MustCompile(`^(left|right|center)$`)
	policy.AllowAttrs("align").Matching(tableAlign).OnElements("th", "td")

	policy.AllowElements("input")
	policy.AllowAttrs("type").Matching(regexp.MustCompile(`^checkbox$`)).OnElements("input")
	policy.AllowAttrs("checked", "disabled").OnElements("input")

	return policy
}
