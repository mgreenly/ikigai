// Package cron is the hand-rolled 5-field cron-expression matcher (no external
// dependency, in the spirit of the hand-rolled SSE parser — decisions §2). It
// parses + validates an expression once into a Schedule, then answers
// matches(t) for any time. Evaluation is in UTC; callers truncate to the minute
// before testing. Vixie day-of-month / day-of-week OR semantics are implemented:
// when both DOM and DOW are restricted (neither field is literally `*`), a row
// fires when EITHER matches.
//
// Supported per field: `*`, a number, lists `a,b,c`, ranges `a-b`, and steps
// `*/n` and `a-b/n`. Field bounds: minute 0-59, hour 0-23, day-of-month 1-31,
// month 1-12, day-of-week 0-7 (both 0 and 7 mean Sunday). Field-name aliases
// (mon/jan) are deliberately NOT supported yet (decisions §2).
package cron

import (
	"fmt"
	"strconv"
	"strings"
	"time"
)

// Schedule is a parsed, validated 5-field cron expression. Each field is a
// 64-bit-or-wider bitmask of the values it matches (kept as a uint64 since every
// field's domain is <= 60). domRestricted / dowRestricted record whether the
// source token was literally `*`, which drives the Vixie OR rule.
type Schedule struct {
	min   uint64 // bits 0..59
	hour  uint64 // bits 0..23
	dom   uint64 // bits 1..31
	month uint64 // bits 1..12
	dow   uint64 // bits 0..6 (Sunday=0; 7 folds to 0 at parse)

	domRestricted bool
	dowRestricted bool

	raw string
}

// fieldSpec describes one field's parse domain. dow uses an extended max (7) at
// parse time so a literal 7 is accepted; it is folded to 0 (Sunday) in the mask.
type fieldSpec struct {
	name string
	min  int
	max  int // inclusive parse maximum
}

var (
	specMin   = fieldSpec{"minute", 0, 59}
	specHour  = fieldSpec{"hour", 0, 23}
	specDOM   = fieldSpec{"day-of-month", 1, 31}
	specMonth = fieldSpec{"month", 1, 12}
	specDOW   = fieldSpec{"day-of-week", 0, 7}
)

// Parse validates a 5-field cron expression and returns its compiled Schedule.
// It fails loudly, naming the offending field, on any structural error — this is
// the authority the MCP create/update path relies on (decisions §2).
func Parse(expr string) (*Schedule, error) {
	fields := strings.Fields(strings.TrimSpace(expr))
	if len(fields) != 5 {
		return nil, fmt.Errorf("cron expression must have exactly 5 fields (minute hour day-of-month month day-of-week), got %d", len(fields))
	}

	minMask, _, err := parseField(fields[0], specMin)
	if err != nil {
		return nil, err
	}
	hourMask, _, err := parseField(fields[1], specHour)
	if err != nil {
		return nil, err
	}
	domMask, domRestricted, err := parseField(fields[2], specDOM)
	if err != nil {
		return nil, err
	}
	monthMask, _, err := parseField(fields[3], specMonth)
	if err != nil {
		return nil, err
	}
	dowMask, dowRestricted, err := parseField(fields[4], specDOW)
	if err != nil {
		return nil, err
	}

	return &Schedule{
		min:           minMask,
		hour:          hourMask,
		dom:           domMask,
		month:         monthMask,
		dow:           dowMask,
		domRestricted: domRestricted,
		dowRestricted: dowRestricted,
		raw:           strings.Join(fields, " "),
	}, nil
}

// Matches reports whether the schedule fires at time t. t is evaluated in UTC;
// the second/nanosecond components are ignored (a schedule fires for the whole
// matching minute). Callers truncate to the minute before testing for the
// double-emit guard, but Matches itself is robust to a non-truncated t.
func (s *Schedule) Matches(t time.Time) bool {
	t = t.UTC()

	if !bit(s.min, t.Minute()) {
		return false
	}
	if !bit(s.hour, t.Hour()) {
		return false
	}
	if !bit(s.month, int(t.Month())) {
		return false
	}

	domHit := bit(s.dom, t.Day())
	// time.Weekday: Sunday=0 .. Saturday=6 — matches our mask's convention.
	dowHit := bit(s.dow, int(t.Weekday()))

	// Vixie OR rule: when BOTH day fields are restricted, fire on EITHER. When
	// only one (or neither) is restricted, the unrestricted (`*`) field matches
	// everything, so a plain AND yields the intuitive result.
	if s.domRestricted && s.dowRestricted {
		return domHit || dowHit
	}
	return domHit && dowHit
}

// Raw returns the normalized (single-space-joined) source expression.
func (s *Schedule) Raw() string { return s.raw }

// ── field parsing ────────────────────────────────────────────────────────────

// parseField compiles one comma-list field into a value mask and reports whether
// the field is "restricted" (the source token is not literally `*`; `*/2` IS
// restricted, decisions §2). It rejects anything structurally invalid, naming
// the field.
func parseField(field string, spec fieldSpec) (mask uint64, restricted bool, err error) {
	if field == "" {
		return 0, false, fmt.Errorf("%s field is empty", spec.name)
	}
	restricted = field != "*"

	for _, term := range strings.Split(field, ",") {
		if term == "" {
			return 0, false, fmt.Errorf("%s field has an empty list element in %q", spec.name, field)
		}
		m, err := parseTerm(term, spec)
		if err != nil {
			return 0, false, err
		}
		mask |= m
	}
	return mask, restricted, nil
}

// parseTerm compiles one comma-separated term: `*`, `*/n`, `a`, `a-b`, or
// `a-b/n`. Day-of-week value 7 folds to 0 (Sunday).
func parseTerm(term string, spec fieldSpec) (uint64, error) {
	rangePart := term
	step := 1

	if slash := strings.IndexByte(term, '/'); slash >= 0 {
		rangePart = term[:slash]
		stepStr := term[slash+1:]
		if rangePart == "" {
			return 0, fmt.Errorf("%s step %q is missing its range", spec.name, term)
		}
		n, err := strconv.Atoi(stepStr)
		if err != nil || n <= 0 {
			return 0, fmt.Errorf("%s step in %q must be a positive integer", spec.name, term)
		}
		step = n
	}

	var lo, hi int
	switch {
	case rangePart == "*":
		lo, hi = spec.min, spec.max
	case strings.IndexByte(rangePart, '-') >= 0:
		dash := strings.IndexByte(rangePart, '-')
		a, errA := strconv.Atoi(rangePart[:dash])
		b, errB := strconv.Atoi(rangePart[dash+1:])
		if errA != nil || errB != nil {
			return 0, fmt.Errorf("%s range %q must be numeric", spec.name, rangePart)
		}
		lo, hi = a, b
	default:
		n, err := strconv.Atoi(rangePart)
		if err != nil {
			return 0, fmt.Errorf("%s value %q must be numeric", spec.name, rangePart)
		}
		lo, hi = n, n
	}

	if lo > hi {
		return 0, fmt.Errorf("%s range %q is descending (low > high)", spec.name, rangePart)
	}
	if lo < spec.min || hi > spec.max {
		return 0, fmt.Errorf("%s value out of range %d-%d in %q", spec.name, spec.min, spec.max, term)
	}

	var mask uint64
	for v := lo; v <= hi; v += step {
		mask |= bitOf(foldDOW(v, spec))
	}
	return mask, nil
}

// foldDOW maps day-of-week 7 to 0 (Sunday); other fields and values pass through.
func foldDOW(v int, spec fieldSpec) int {
	if spec.name == specDOW.name && v == 7 {
		return 0
	}
	return v
}

func bitOf(v int) uint64 { return uint64(1) << uint(v) }

func bit(mask uint64, v int) bool {
	if v < 0 || v > 63 {
		return false
	}
	return mask&bitOf(v) != 0
}
