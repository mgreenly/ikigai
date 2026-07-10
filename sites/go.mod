module sites

go 1.26

require (
	appkit v0.0.0
	github.com/chromedp/chromedp v0.15.1
	github.com/dop251/goja v0.0.0-20260701091749-b07b74453ea9
	registry v0.0.0
)

require (
	eventplane v0.0.0 // indirect
	github.com/chromedp/cdproto v0.0.0-20260321001828-e3e3800016bc // indirect
	github.com/chromedp/sysutil v1.1.0 // indirect
	github.com/dlclark/regexp2/v2 v2.2.1 // indirect
	github.com/dustin/go-humanize v1.0.1 // indirect
	github.com/go-json-experiment/json v0.0.0-20260214004413-d219187c3433 // indirect
	github.com/go-sourcemap/sourcemap v2.1.3+incompatible // indirect
	github.com/gobwas/httphead v0.1.0 // indirect
	github.com/gobwas/pool v0.2.1 // indirect
	github.com/gobwas/ws v1.4.0 // indirect
	github.com/google/pprof v0.0.0-20250317173921-a4b03ec1a45e // indirect
	github.com/google/uuid v1.6.0 // indirect
	github.com/mattn/go-isatty v0.0.20 // indirect
	github.com/ncruces/go-strftime v1.0.0 // indirect
	github.com/remyoudompheng/bigfft v0.0.0-20230129092748-24d4a6f8daec // indirect
	golang.org/x/mod v0.34.0 // indirect
	golang.org/x/sys v0.42.0 // indirect
	golang.org/x/text v0.3.8 // indirect
	modernc.org/libc v1.72.3 // indirect
	modernc.org/mathutil v1.7.1 // indirect
	modernc.org/memory v1.11.0 // indirect
	modernc.org/sqlite v1.50.1 // indirect
)

// The shared chassis (appkit) and event-plane (eventplane) libraries are sibling
// source trees, not published modules. go.work resolves them for local dev; these
// committed replaces make the prod build deterministic with or without the
// workspace (PLAN §1.6: never a versioned require for an in-repo library).
replace appkit => ../appkit

replace eventplane => ../eventplane

replace registry => ../registry
