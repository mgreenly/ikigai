module sites

go 1.26

require (
	agentkit v0.0.0
	appkit v0.0.0
	modernc.org/sqlite v1.50.1
)

require (
	eventplane v0.0.0 // indirect
	github.com/dustin/go-humanize v1.0.1 // indirect
	github.com/google/uuid v1.6.0 // indirect
	github.com/mattn/go-isatty v0.0.20 // indirect
	github.com/ncruces/go-strftime v1.0.0 // indirect
	github.com/remyoudompheng/bigfft v0.0.0-20230129092748-24d4a6f8daec // indirect
	golang.org/x/sys v0.42.0 // indirect
	modernc.org/libc v1.72.3 // indirect
	modernc.org/mathutil v1.7.1 // indirect
	modernc.org/memory v1.11.0 // indirect
)

// The shared chassis (appkit) and event-plane (eventplane) libraries are sibling
// source trees, not published modules. go.work resolves them for local dev; these
// committed replaces make the prod build deterministic with or without the
// workspace (PLAN §1.6: never a versioned require for an in-repo library).
replace appkit => ../appkit

replace eventplane => ../eventplane

replace agentkit => ../agentkit
