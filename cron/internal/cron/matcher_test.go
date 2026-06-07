package cron

import (
	"testing"
	"time"
)

// ut builds a UTC time at the given fields (year fixed where day-of-week matters).
func ut(year, month, day, hour, min int) time.Time {
	return time.Date(year, time.Month(month), day, hour, min, 0, 0, time.UTC)
}

// TestParse_Invalid asserts the parser fails loudly on structurally bad input,
// naming the field. We assert rejection (err != nil), not the exact message.
func TestParse_Invalid(t *testing.T) {
	bad := []string{
		"",                  // empty
		"* * * *",           // 4 fields
		"* * * * * *",       // 6 fields
		"60 * * * *",        // minute out of range
		"* 24 * * *",        // hour out of range
		"* * 0 * *",         // dom below 1
		"* * 32 * *",        // dom above 31
		"* * * 0 *",         // month below 1
		"* * * 13 *",        // month above 12
		"* * * * 8",         // dow above 7
		"*/0 * * * *",       // zero step
		"*/-1 * * * *",      // negative step
		"5-1 * * * *",       // descending range
		"a * * * *",         // non-numeric value
		"1-b * * * *",       // non-numeric range bound
		"1,,2 * * * *",      // empty list element
		"1- * * * *",        // dangling range
		"/5 * * * *",        // step with no range
		"1-5/x * * * *",     // non-numeric step
		"* * * * 1-2-3",     // malformed range
	}
	for _, expr := range bad {
		if _, err := Parse(expr); err == nil {
			t.Errorf("Parse(%q): expected error, got nil", expr)
		}
	}
}

// TestParse_Valid asserts a spread of valid forms parse without error.
func TestParse_Valid(t *testing.T) {
	good := []string{
		"* * * * *",
		"0 0 1 1 0",
		"*/15 * * * *",
		"0-30/10 * * * *",
		"1,2,3 * * * *",
		"0 9-17 * * 1-5",
		"0 0 * * 7", // 7 = Sunday
		"59 23 31 12 6",
		"0 0 1,15 * *",
		"5 4 * * 0",
	}
	for _, expr := range good {
		if _, err := Parse(expr); err != nil {
			t.Errorf("Parse(%q): unexpected error: %v", expr, err)
		}
	}
}

func mustParse(t *testing.T, expr string) *Schedule {
	t.Helper()
	s, err := Parse(expr)
	if err != nil {
		t.Fatalf("Parse(%q): %v", expr, err)
	}
	return s
}

// TestMatches_Star: `* * * * *` matches every minute.
func TestMatches_Star(t *testing.T) {
	s := mustParse(t, "* * * * *")
	times := []time.Time{
		ut(2026, 6, 6, 0, 0),
		ut(2026, 1, 1, 23, 59),
		ut(2024, 2, 29, 12, 30), // leap day
	}
	for _, tm := range times {
		if !s.Matches(tm) {
			t.Errorf("`* * * * *` should match %v", tm)
		}
	}
}

// TestMatches_Exact pins a specific minute and rejects neighbours.
func TestMatches_Exact(t *testing.T) {
	s := mustParse(t, "30 14 6 6 *") // 14:30 on June 6
	if !s.Matches(ut(2026, 6, 6, 14, 30)) {
		t.Error("should match 2026-06-06 14:30")
	}
	misses := []time.Time{
		ut(2026, 6, 6, 14, 31), // minute off
		ut(2026, 6, 6, 13, 30), // hour off
		ut(2026, 6, 7, 14, 30), // day off
		ut(2026, 7, 6, 14, 30), // month off
	}
	for _, tm := range misses {
		if s.Matches(tm) {
			t.Errorf("should NOT match %v", tm)
		}
	}
}

// TestMatches_Step exercises */n and a-b/n.
func TestMatches_Step(t *testing.T) {
	s := mustParse(t, "*/15 * * * *") // minutes 0,15,30,45
	for _, m := range []int{0, 15, 30, 45} {
		if !s.Matches(ut(2026, 6, 6, 10, m)) {
			t.Errorf("*/15 should match minute %d", m)
		}
	}
	for _, m := range []int{1, 14, 16, 31, 44, 59} {
		if s.Matches(ut(2026, 6, 6, 10, m)) {
			t.Errorf("*/15 should NOT match minute %d", m)
		}
	}

	// a-b/n: 0-30/10 → 0,10,20,30
	s2 := mustParse(t, "0-30/10 * * * *")
	for _, m := range []int{0, 10, 20, 30} {
		if !s2.Matches(ut(2026, 6, 6, 10, m)) {
			t.Errorf("0-30/10 should match minute %d", m)
		}
	}
	for _, m := range []int{5, 15, 25, 35, 40} {
		if s2.Matches(ut(2026, 6, 6, 10, m)) {
			t.Errorf("0-30/10 should NOT match minute %d", m)
		}
	}
}

// TestMatches_Range exercises a-b inclusivity at both endpoints.
func TestMatches_Range(t *testing.T) {
	s := mustParse(t, "0 9-17 * * *") // hours 9..17 inclusive
	for _, h := range []int{9, 12, 17} {
		if !s.Matches(ut(2026, 6, 6, h, 0)) {
			t.Errorf("9-17 should match hour %d", h)
		}
	}
	for _, h := range []int{8, 18, 0, 23} {
		if s.Matches(ut(2026, 6, 6, h, 0)) {
			t.Errorf("9-17 should NOT match hour %d", h)
		}
	}
}

// TestMatches_List exercises a,b,c.
func TestMatches_List(t *testing.T) {
	s := mustParse(t, "0 0 1,15,28 * *")
	for _, d := range []int{1, 15, 28} {
		if !s.Matches(ut(2026, 6, d, 0, 0)) {
			t.Errorf("1,15,28 should match day %d", d)
		}
	}
	for _, d := range []int{2, 14, 16, 27, 29} {
		if s.Matches(ut(2026, 6, d, 0, 0)) {
			t.Errorf("1,15,28 should NOT match day %d", d)
		}
	}
}

// TestMatches_SevenIsSunday: both 0 and 7 mean Sunday. 2026-06-07 is a Sunday.
func TestMatches_SevenIsSunday(t *testing.T) {
	sunday := ut(2026, 6, 7, 0, 0)
	if sunday.Weekday() != time.Sunday {
		t.Fatalf("test fixture wrong: 2026-06-07 is %v, expected Sunday", sunday.Weekday())
	}
	for _, expr := range []string{"0 0 * * 0", "0 0 * * 7"} {
		s := mustParse(t, expr)
		if !s.Matches(sunday) {
			t.Errorf("%q should match Sunday", expr)
		}
		// Monday 2026-06-08 must not match.
		if s.Matches(ut(2026, 6, 8, 0, 0)) {
			t.Errorf("%q should NOT match Monday", expr)
		}
	}
	// A range that includes 7 (e.g. 6-7) must include Sunday.
	s := mustParse(t, "0 0 * * 6-7")
	if !s.Matches(sunday) {
		t.Error("6-7 should match Sunday (7 folds to 0)")
	}
	sat := ut(2026, 6, 6, 0, 0)
	if sat.Weekday() != time.Saturday {
		t.Fatalf("fixture: 2026-06-06 is %v, expected Saturday", sat.Weekday())
	}
	if !s.Matches(sat) {
		t.Error("6-7 should match Saturday")
	}
}

// TestMatches_VixieOR is the crux: when BOTH DOM and DOW are restricted, fire on
// EITHER. 2026-06 — the 1st is a Monday, the 7th/14th/21st/28th are Sundays.
func TestMatches_VixieOR(t *testing.T) {
	// DOM=15 AND DOW=Sunday(0), both restricted → fires on the 15th OR any Sunday.
	s := mustParse(t, "0 0 15 * 0")

	if d := ut(2026, 6, 1, 0, 0); d.Weekday() != time.Monday {
		t.Fatalf("fixture: 2026-06-01 is %v, want Monday", d.Weekday())
	}

	// The 15th (a Monday) — matches via DOM.
	if !s.Matches(ut(2026, 6, 15, 0, 0)) {
		t.Error("Vixie OR: should match the 15th (DOM hit)")
	}
	// The 7th (a Sunday, not the 15th) — matches via DOW.
	if !s.Matches(ut(2026, 6, 7, 0, 0)) {
		t.Error("Vixie OR: should match Sunday the 7th (DOW hit)")
	}
	// The 10th (Wednesday, not the 15th) — matches neither → no fire.
	if s.Matches(ut(2026, 6, 10, 0, 0)) {
		t.Error("Vixie OR: should NOT match the 10th (neither DOM nor DOW)")
	}
}

// TestMatches_VixieOR_StepCountsAsRestricted: `*/2` is restricted, so a `*/2`
// DOM with a restricted DOW triggers the OR rule (decisions §2).
func TestMatches_VixieOR_StepCountsAsRestricted(t *testing.T) {
	// DOM=*/2 (even days... actually 1,3,5.. since dom starts at 1) AND DOW=Sunday.
	// Both restricted → OR. We verify a Sunday on an odd (non-DOM) day still fires.
	s := mustParse(t, "0 0 */2 * 0")
	// dom */2 over 1..31 → 1,3,5,...; the 7th (odd) is in DOM AND is Sunday.
	// Pick a Sunday NOT in the */2 set to prove DOW-only fires.
	// 2026-06-14 is a Sunday and 14 is even → not in 1,3,5,... so DOM misses.
	d := ut(2026, 6, 14, 0, 0)
	if d.Weekday() != time.Sunday {
		t.Fatalf("fixture: 2026-06-14 is %v, want Sunday", d.Weekday())
	}
	if !s.Matches(d) {
		t.Error("*/2 DOM (restricted) + DOW Sunday: Sunday the 14th should fire via DOW (OR rule)")
	}
}

// TestMatches_OneRestricted: when only DOM is restricted (DOW=*), it is a plain
// AND — DOW matches everything, so DOM alone governs (no OR).
func TestMatches_OneRestricted_DOM(t *testing.T) {
	s := mustParse(t, "0 0 15 * *") // DOM=15, DOW=*
	if !s.Matches(ut(2026, 6, 15, 0, 0)) {
		t.Error("DOM=15, DOW=*: should match the 15th")
	}
	// A Sunday that is not the 15th must NOT fire (DOW is unrestricted, no OR).
	if s.Matches(ut(2026, 6, 7, 0, 0)) {
		t.Error("DOM=15, DOW=*: should NOT match Sunday the 7th (no OR when one is *)")
	}
}

// TestMatches_OneRestricted_DOW: only DOW restricted (DOM=*) → AND, DOW governs.
func TestMatches_OneRestricted_DOW(t *testing.T) {
	s := mustParse(t, "0 0 * * 1") // every Monday
	if d := ut(2026, 6, 1, 0, 0); d.Weekday() != time.Monday {
		t.Fatalf("fixture: 2026-06-01 is %v, want Monday", d.Weekday())
	}
	if !s.Matches(ut(2026, 6, 1, 0, 0)) {
		t.Error("DOM=*, DOW=Mon: should match Monday the 1st")
	}
	if s.Matches(ut(2026, 6, 2, 0, 0)) {
		t.Error("DOM=*, DOW=Mon: should NOT match Tuesday the 2nd")
	}
}

// TestMatches_NeitherRestricted: both `*` → matches every day (plain AND of two
// always-true fields).
func TestMatches_NeitherRestricted(t *testing.T) {
	s := mustParse(t, "0 0 * * *")
	for d := 1; d <= 28; d++ {
		if !s.Matches(ut(2026, 6, d, 0, 0)) {
			t.Errorf("`* *` day fields should match day %d", d)
		}
	}
}

// TestMatches_MonthAndEdges hits field boundaries: minute 59, hour 23, dom 31,
// month 12.
func TestMatches_MonthAndEdges(t *testing.T) {
	s := mustParse(t, "59 23 31 12 *")
	if !s.Matches(ut(2026, 12, 31, 23, 59)) {
		t.Error("edge schedule should match 2026-12-31 23:59")
	}
	if s.Matches(ut(2026, 12, 31, 23, 58)) {
		t.Error("edge schedule should NOT match 23:58")
	}
	if s.Matches(ut(2026, 11, 30, 23, 59)) {
		t.Error("edge schedule should NOT match November")
	}
}

// TestMatches_EvaluatesInUTC: a time in a non-UTC zone is converted to UTC before
// field comparison. 03:30 in UTC+5 is 22:30 UTC the previous day.
func TestMatches_EvaluatesInUTC(t *testing.T) {
	s := mustParse(t, "30 22 * * *") // 22:30 UTC
	zone := time.FixedZone("UTC+5", 5*3600)
	local := time.Date(2026, 6, 7, 3, 30, 0, 0, zone) // == 2026-06-06 22:30 UTC
	if !s.Matches(local) {
		t.Errorf("matcher should convert to UTC: %v -> 22:30 UTC", local)
	}
	// And the naive (non-converted) reading 03:30 must not spuriously match a
	// 03:30 schedule.
	s2 := mustParse(t, "30 3 * * *")
	if s2.Matches(local) {
		t.Error("03:30 local (22:30 UTC) should NOT match a 03:30 UTC schedule")
	}
}

// TestMatches_IgnoresSeconds: a non-minute-truncated time still matches.
func TestMatches_IgnoresSeconds(t *testing.T) {
	s := mustParse(t, "30 14 * * *")
	tm := time.Date(2026, 6, 6, 14, 30, 47, 123, time.UTC)
	if !s.Matches(tm) {
		t.Error("matcher should ignore sub-minute components")
	}
}
