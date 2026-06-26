# Brief — Phase 06: Add the logged-out name-origin colophon while preserving the control-plane login framing

phase: 06
realizes: D7
decision_files:
  - dashboard/project/design/D07.md

## Ids to cover
R-DB17-ORIG
R-DB18-KEEP
R-DB19-LAND

## Files to touch
- dashboard/internal/server/*_test.go
- dashboard/ui/html/index.html
- dashboard/ui/static/app.css

## Dependency surface (copied from design — do not open design files)
Decision scope:
- Logged-out `GET /`, the `.signin-wall` branch of `ui/html/index.html`.
- Keep existing wordmark, heading, body line, and CTA verbatim.
- No new route, link, control, view-model field, schema, `/services` change, install-script change, landing/home composition change, profile change, or routing change.
- The colophon appears only in the logged-out `{{else}}` branch of `index.html`; signed-in `GET /` landing/home omits it.

Existing login framing that must remain verbatim:
- Heading: `Your account's control plane`
- Body line: `Sign in to manage access tokens, connected agents, and the box's MCP services.`
- CTA text: `Sign in with Google`
- CTA href: `/login`

Markup to add after the existing sign-in `<a>`:
```html
<aside class="name-origin" aria-label="What ikigenba means">
  <p class="name-origin-lede"><b>ikigenba</b> — where your livelihood actually gets done. A portmanteau of two Japanese words:</p>
  <dl class="name-origin-parts">
    <div>
      <dt><b class="seam">iki</b>gai <span lang="ja">生き甲斐</span></dt>
      <dd>&ldquo;reason for being&rdquo; — the work worth doing; your business.</dd>
    </div>
    <div>
      <dt><b class="seam">genba</b> <span lang="ja">現場</span></dt>
      <dd>&ldquo;the actual place&rdquo; — the floor where the work really happens.</dd>
    </div>
  </dl>
</aside>
```

Structural names the tests should assert:
- `.signin-wall`
- `.name-origin`
- `.name-origin-lede`
- `.name-origin-parts`
- `.seam`
- `span[lang="ja"]`
- One `<p class="name-origin-lede">` naming `ikigenba`, saying `where your livelihood actually gets done`, and calling it `A portmanteau of two Japanese words:`.
- One `<dl class="name-origin-parts">` with exactly two items: `ikigai` / `生き甲斐` and `genba` / `現場`.
- The surviving fragments are marked by `<b class="seam">iki</b>` and `<b class="seam">genba</b>`.
- There is no third sibling explanation line; the two `dt/dd` pairs are children of the name, not peers of it.

CSS names to add after the `.signin-wall` block in `ui/static/app.css`:
- `.name-origin`
- `.name-origin .name-origin-lede`
- `.name-origin .name-origin-lede b`
- `.name-origin-parts`
- `.name-origin-parts dt`
- `.name-origin-parts dt .seam`
- `.name-origin-parts dt span[lang="ja"]`
- `.name-origin-parts dd`
- Use semantic tokens only.
- Use muted/subtle ink plus full ink for the name and `.seam` fragments.
- Use one `--color-border` top rule on `.name-origin`.
- Use `max-width: 420px`.
- Make `.name-origin-parts` subordinate to the lede with a left hairline rule and indent.
- Write the lede override as `.name-origin .name-origin-lede` so it out-specifies `.signin-wall p`.
- Use no accent color anywhere in the name-origin block.

Test harness surface:
- Drive real `GET /` route behavior through the package's existing route table via `(*app).routes()` / `httptest`.
- Use a real temp-SQLite session store and an injected session cookie for the signed-in `GET /` case.

## Done bar
- Every id under "Ids to cover" is covered by a genuinely-asserting test tagged with a `// R-DBxx-xxxx` comment (Phase 05's `R-DB16-DOCS` is verified by a text check on dashboard/AGENTS.md instead — name that explicitly).
- **Test placement — co-locate.** Tests live in
  `dashboard/internal/server/<name>_test.go`, `package server`, each named for the
  behavior it asserts — never a root-level or `phaseNN_test.go` file. They drive
  the real route table via the package's existing test harness
  (`(*app).routes()` / `httptest`), with a real temp-SQLite session store and an
  injected session cookie for "signed in".
- The suite is green:
    cd dashboard && go build ./...
    cd dashboard && go vet ./...
    cd dashboard && gofmt -l .          # prints nothing
    cd dashboard && go test ./...
    bin/check-migrations dashboard
