package ingest

import (
	"context"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"

	"golang.org/x/net/html"
	"golang.org/x/net/html/atom"
)

// URL fetch limits. The HTTP client times the whole request out and the body
// read is capped so a runaway / hostile response cannot exhaust memory.
const (
	urlFetchTimeout = 30 * time.Second
	maxBodyBytes    = 8 << 20 // 8 MiB cap on the fetched body
)

// FetchFunc fetches a URL and extracts it to markdown. Core holds one as an
// injectable field so tests can stub the network (an httptest.Server URL works
// with the default impl too, but a stub keeps unit tests hermetic). It returns
// the extracted markdown bytes and a derived title.
type FetchFunc func(ctx context.Context, rawURL string) (markdown []byte, title string, err error)

// FetchAndExtract is the default FetchFunc: it fetches rawURL over HTTP(S) and
// extracts HTML to markdown with a pure-Go (no CGO) path (golang.org/x/net/html).
// Non-HTML bodies (text/markdown/plain) are passed through verbatim.
//
// Scheme allow-list: only http/https are permitted; anything else (file://,
// data:, ftp://, gopher://, …) is rejected. NOTE: this does NOT defend against
// SSRF to internal/loopback/link-local addresses — a fetch of http://169.254.…
// or http://127.0.0.1 still proceeds. That is a known, deliberately-deferred
// limitation; the Phase-7 OS-level sandbox is where egress is confined. Do not
// rely on this function for SSRF protection.
func FetchAndExtract(ctx context.Context, rawURL string) ([]byte, string, error) {
	u, err := url.Parse(strings.TrimSpace(rawURL))
	if err != nil {
		return nil, "", fmt.Errorf("parse url: %w", err)
	}
	switch strings.ToLower(u.Scheme) {
	case "http", "https":
		// allowed
	default:
		return nil, "", fmt.Errorf("unsupported url scheme %q: only http and https are allowed", u.Scheme)
	}

	client := &http.Client{Timeout: urlFetchTimeout}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, u.String(), nil)
	if err != nil {
		return nil, "", fmt.Errorf("build request: %w", err)
	}
	req.Header.Set("User-Agent", "ikigenba-wiki/1 (+https://ikigenba.com)")
	req.Header.Set("Accept", "text/html,text/markdown,text/plain;q=0.9,*/*;q=0.8")

	resp, err := client.Do(req)
	if err != nil {
		return nil, "", fmt.Errorf("fetch %s: %w", u.String(), err)
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, "", fmt.Errorf("fetch %s: unexpected status %s", u.String(), resp.Status)
	}

	body, err := io.ReadAll(io.LimitReader(resp.Body, maxBodyBytes))
	if err != nil {
		return nil, "", fmt.Errorf("read body: %w", err)
	}

	ct := resp.Header.Get("Content-Type")
	if !isHTMLContentType(ct, body) {
		// Already plain text / markdown: pass through untouched, deriving a title
		// from the URL path since there is no <title> to mine.
		return body, titleFromURL(u), nil
	}

	md, title := htmlToMarkdown(body)
	if title == "" {
		title = titleFromURL(u)
	}
	return md, title, nil
}

// isHTMLContentType decides whether to run the HTML→markdown extractor. It trusts
// an explicit HTML content type; when the header is missing/ambiguous it sniffs
// the first bytes for an HTML signature so a server that mislabels HTML as
// text/plain still gets extracted, while genuine plain text/markdown passes
// through.
func isHTMLContentType(contentType string, body []byte) bool {
	ct := strings.ToLower(strings.TrimSpace(contentType))
	if i := strings.IndexByte(ct, ';'); i >= 0 {
		ct = strings.TrimSpace(ct[:i])
	}
	switch ct {
	case "text/html", "application/xhtml+xml":
		return true
	case "text/markdown", "text/x-markdown", "text/plain", "application/json":
		return false
	}
	// Unknown/empty content type: sniff for an HTML signature.
	head := strings.ToLower(strings.TrimSpace(string(body)))
	if len(head) > 512 {
		head = head[:512]
	}
	return strings.HasPrefix(head, "<!doctype html") ||
		strings.HasPrefix(head, "<html") ||
		strings.Contains(head, "<head>") ||
		strings.Contains(head, "<body")
}

// titleFromURL derives a human-ish title from a URL when no <title> is available:
// the last non-empty path segment (de-slugged), else the host.
func titleFromURL(u *url.URL) string {
	p := strings.Trim(u.Path, "/")
	if p != "" {
		segs := strings.Split(p, "/")
		last := segs[len(segs)-1]
		last = strings.TrimSuffix(last, ".html")
		last = strings.TrimSuffix(last, ".htm")
		last = strings.NewReplacer("-", " ", "_", " ").Replace(last)
		last = strings.TrimSpace(last)
		if last != "" {
			return last
		}
	}
	return u.Host
}

// htmlToMarkdown parses an HTML document with golang.org/x/net/html (pure-Go) and
// walks the DOM into reasonable markdown: headings → #..######, links →
// [text](href), paragraphs/line-breaks, list items → "- ", code/pre fenced.
// script/style/head/nav/noscript subtrees are dropped as noise. It also returns
// the document <title>. Extraction quality only needs to be reasonable — the
// downstream ingest agent re-reads and structures the markdown.
func htmlToMarkdown(body []byte) (markdown []byte, title string) {
	doc, err := html.Parse(strings.NewReader(string(body)))
	if err != nil {
		// Parse is extremely lenient and effectively never errors on real input;
		// if it somehow does, fall back to passing the raw bytes through.
		return body, ""
	}

	var b strings.Builder
	w := &mdWalker{out: &b}
	w.walk(doc)

	// Mine <title> with a dedicated scan: <head> (where <title> lives) is dropped
	// from the body walk, so the title must be found independently of it.
	return []byte(collapseBlankLines(b.String())), strings.TrimSpace(findTitle(doc))
}

// findTitle returns the text of the first <title> element anywhere in the tree.
func findTitle(n *html.Node) string {
	if n.Type == html.ElementNode && n.DataAtom == atom.Title {
		return strings.TrimSpace(textContent(n))
	}
	for c := n.FirstChild; c != nil; c = c.NextSibling {
		if t := findTitle(c); t != "" {
			return t
		}
	}
	return ""
}

// mdWalker accumulates markdown as it walks the parsed DOM. The <title> is mined
// separately (findTitle), since <head> is dropped from the body walk.
type mdWalker struct {
	out *strings.Builder
}

// dropped is the set of element subtrees treated as non-content noise.
var dropped = map[atom.Atom]bool{
	atom.Script:   true,
	atom.Style:    true,
	atom.Head:     true,
	atom.Nav:      true,
	atom.Noscript: true,
	atom.Template: true,
}

func (w *mdWalker) walk(n *html.Node) {
	switch n.Type {
	case html.TextNode:
		// Inline text. html already unescaped entities (&amp; → &) during parse.
		text := normalizeInlineWS(n.Data)
		if text != "" {
			w.out.WriteString(text)
		}
		return
	case html.ElementNode:
		// <head> (and its <title>) is dropped from the body walk; the title is
		// mined separately via findTitle.
		if dropped[n.DataAtom] {
			return
		}
		w.element(n)
		return
	default:
		// Document/comment/doctype nodes: just descend.
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			w.walk(c)
		}
	}
}

// element renders a single element node into markdown.
func (w *mdWalker) element(n *html.Node) {
	switch n.DataAtom {
	case atom.H1, atom.H2, atom.H3, atom.H4, atom.H5, atom.H6:
		level := int(n.DataAtom - atom.H1 + 1) // h1→1 … h6→6
		w.block(strings.Repeat("#", level)+" "+strings.TrimSpace(textContent(n)), true)
		return
	case atom.P:
		w.newlineBlock()
		w.children(n)
		w.newlineBlock()
		return
	case atom.Br:
		w.out.WriteString("\n")
		return
	case atom.A:
		text := strings.TrimSpace(textContent(n))
		href := attr(n, "href")
		switch {
		case text == "":
			// no anchor text — nothing useful to emit
		case href == "" || strings.HasPrefix(href, "#"):
			w.out.WriteString(text)
		default:
			fmt.Fprintf(w.out, "[%s](%s)", text, href)
		}
		return
	case atom.Li:
		w.newlineBlock()
		w.out.WriteString("- ")
		w.children(n)
		w.out.WriteString("\n")
		return
	case atom.Pre, atom.Code:
		code := textContent(n)
		if n.DataAtom == atom.Pre {
			w.block("```\n"+strings.TrimRight(code, "\n")+"\n```", true)
		} else {
			fmt.Fprintf(w.out, "`%s`", strings.TrimSpace(code))
		}
		return
	case atom.Ul, atom.Ol, atom.Div, atom.Section, atom.Article, atom.Main,
		atom.Blockquote, atom.Table, atom.Tr, atom.Header, atom.Footer:
		// Structural blocks: descend, separating with a blank line.
		w.newlineBlock()
		w.children(n)
		w.newlineBlock()
		return
	default:
		// Unknown inline/structural element: just descend, keeping content.
		w.children(n)
	}
}

// children walks an element's children in order.
func (w *mdWalker) children(n *html.Node) {
	for c := n.FirstChild; c != nil; c = c.NextSibling {
		w.walk(c)
	}
}

// block writes text as its own block, ensuring a blank line before and after.
func (w *mdWalker) block(text string, _ bool) {
	w.newlineBlock()
	w.out.WriteString(text)
	w.newlineBlock()
}

// newlineBlock ensures the builder ends with a newline (block separation is
// normalized to single blank lines afterward by collapseBlankLines).
func (w *mdWalker) newlineBlock() {
	s := w.out.String()
	if s == "" {
		return
	}
	if !strings.HasSuffix(s, "\n") {
		w.out.WriteString("\n")
	}
}

// textContent returns the concatenated text of a subtree (entities already
// unescaped by the parser), excluding dropped noise subtrees.
func textContent(n *html.Node) string {
	var b strings.Builder
	var rec func(*html.Node)
	rec = func(nn *html.Node) {
		if nn.Type == html.ElementNode && dropped[nn.DataAtom] {
			return
		}
		if nn.Type == html.TextNode {
			b.WriteString(nn.Data)
		}
		for c := nn.FirstChild; c != nil; c = c.NextSibling {
			rec(c)
		}
	}
	rec(n)
	return normalizeInlineWS(b.String())
}

// attr returns the value of the named attribute, or "".
func attr(n *html.Node, name string) string {
	for _, a := range n.Attr {
		if a.Key == name {
			return a.Val
		}
	}
	return ""
}

// normalizeInlineWS collapses runs of whitespace (incl. newlines) into single
// spaces — HTML treats inter-tag whitespace as insignificant.
func normalizeInlineWS(s string) string {
	return strings.Join(strings.Fields(s), " ")
}

// collapseBlankLines trims each line and collapses runs of blank lines to a
// single blank line, yielding tidy markdown.
func collapseBlankLines(s string) string {
	lines := strings.Split(s, "\n")
	out := make([]string, 0, len(lines))
	blank := false
	for _, ln := range lines {
		ln = strings.TrimRight(ln, " \t")
		if strings.TrimSpace(ln) == "" {
			if blank {
				continue
			}
			blank = true
			out = append(out, "")
			continue
		}
		blank = false
		out = append(out, ln)
	}
	res := strings.Join(out, "\n")
	return strings.TrimSpace(res) + "\n"
}
