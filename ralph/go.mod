module ralph

go 1.26

require (
	agentkit v0.0.0
	appkit v0.0.0
	eventplane v0.0.0
	modernc.org/sqlite v1.50.1
)

replace agentkit => ../agentkit

replace appkit => ../appkit

// ralph is now an event-plane CONSUMER (of cron's /feed) and uses
// eventplane/consumer directly; appkit's feed package also resolves it. The
// committed replace (a sibling source tree, never tagged) keeps the build
// deterministic under GOWORK=off.
replace eventplane => ../eventplane

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
