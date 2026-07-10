# sites — Research

Collected external ground truth the design references. Non-contractual: the
build loop never reads this; design cites these facts instead of re-deriving
them.

## chromedp (browser automation from Go)

`github.com/chromedp/chromedp` is a pure-Go library that drives a Chrome/Chromium
browser over the **Chrome DevTools Protocol** (the same wire protocol Chrome's
own devtools use). No node, no npm, no driver server, no cgo — a Go test talks
TCP/websocket to a Chrome process it spawns itself.

**The API footprint the design uses** (all of it — the library is much larger):

- `chromedp.NewExecAllocator(ctx, opts...)` — launches and owns a Chrome
  process. `chromedp.DefaultExecAllocatorOptions` includes `headless` (the
  modern `--headless=new` engine — the real browser minus the window), a
  **fresh temporary `--user-data-dir`** (no profile, no cookies, no history,
  fully isolated from any desktop Chrome), and sandbox/GPU flags suitable for
  unattended runs. It finds the browser binary by looking up well-known names
  (`google-chrome`, `chromium`, …) on `PATH` unless `chromedp.ExecPath` pins
  one.
- `chromedp.NewContext(allocCtx)` — one browser tab. Everything hangs off
  `context.Context`: cancelling the context kills the tab/browser (cleanup is
  the `defer cancel()` stack), and `context.WithTimeout` bounds any scenario so
  a hung page fails instead of hanging `go test`.
- `chromedp.Run(ctx, actions...)` — executes actions sequentially:
  - `chromedp.Navigate(url)` — load a page.
  - `chromedp.WaitVisible(sel)` — poll until the CSS-selected element exists
    **and is visible**. The idiomatic no-sleep way to wait for JS to act; doubles
    as an assertion that it did.
  - `chromedp.SendKeys(sel, text)` — dispatch genuine trusted key events
    character-by-character; the page's real `input`/`keydown` listeners fire.
    Requires the target to be visible/interactable — typing into a `hidden`
    element fails.
  - `chromedp.Click(sel)` — a real click on the selected element.
  - `chromedp.Evaluate(js, &out)` — run a JS expression in the page and marshal
    its JSON result into a Go value (the DOM read-back channel).

**Costs and characteristics:**

- The Chrome launch is the expensive step (~300–800 ms once per allocator);
  each action afterward is milliseconds. Multi-step scenarios amortize the
  launch by sharing one session.
- The dominant flake mode is the **launch**, not the scenario; a single launch
  retry distinguishes "Chrome hiccuped" from "Chrome broken/absent".
- Transitive deps: `chromedp/cdproto` (large machine-generated DevTools
  protocol bindings — a chunky `go.sum` diff, build-cache absorbed),
  `gobwas/ws` (websocket), small utilities. All pure Go.
- The browser binary itself is an **environment assumption** `go.mod` cannot
  express — like a C compiler. It must be documented as part of the suite's
  green definition.
- Debug escape hatch: dropping the `headless` flag runs the same test headful
  (a visible window) for diagnosis. Never the default.

## Environment facts (verified on this box, 2026-07-10)

- `/usr/bin/google-chrome` is installed (the binary chromedp finds on `PATH`).
- `node` v24 / `npx` exist but nothing in this repo uses them; no Playwright
  package is installed (npm/pip/CLI all absent). A stale `~/.cache/ms-playwright`
  browser cache exists but is unused.
- The ralph build loop runs on this box; the deploy box never runs the test
  suite; there is no CI. Every environment that runs `go test ./...` has Chrome.

## Alternatives evaluated and not chosen

- **Playwright (node).** Would drag a second-language toolchain into a pure-Go
  repo: `package.json`, `node_modules`, a version-churning driver. Everything it
  offers that this design needs, chromedp does over the same DevTools protocol
  with zero node dependency. Rejected.
- **A goja DOM shim.** Hand-rolling a fake `document`/event system to test the
  controller in goja is a mock that passes whatever it is taught to pass — it
  cannot falsify real browser wiring. Rejected on verification-substrate
  grounds.
- **`t.Skip` when Chrome is absent.** Keeps the suite pure-Go-green anywhere but
  makes the gate soft: an environment misconfiguration silently un-proves the
  wiring, and a skipped test reads as green to the verify step. Rejected in
  favor of a hard requirement.
