module wiki

go 1.26

require (
	agentkit v0.0.0-00010101000000-000000000000
	appkit v0.0.0
	eventplane v0.0.0
	golang.org/x/net v0.12.0
	modernc.org/sqlite v1.50.1
)

// Shared sibling source trees, not published modules. go.work resolves them for
// local dev; these committed replaces keep bin/build deterministic with or
// without the workspace. eventplane is the event-plane library wiki consumes as a
// dropbox file-lifecycle consumer (Task 6.1); agentkit is the agent machinery.
replace eventplane => ../eventplane

replace agentkit => ../agentkit

replace appkit => ../appkit

require (
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
