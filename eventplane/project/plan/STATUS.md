# eventplane — Plan Status

One line per phase in build order; this file is the only place a phase's
marker lives. Each phase line is a Markdown bullet beginning with `- Phase`
carrying `✅` (done) or `⬜` (not started). The build loop finds its next work
with `grep -nE '^- Phase .* ⬜' project/plan/STATUS.md | head -1` and reads
only that phase's body file. No bare status glyph appears outside phase lines,
so the anchored grep matches only phase lines.

- Phase 01 ✅ realizes R-3FIX-KJG7 R-3GQT-YB6W R-3HYQ-C2XL R-3J6M-PUOA R-3KEJ-3MEZ R-3LMF-HE5O R-3MUB-V5WD R-41H4-GESP — the `routing` package: canonical key rendering, address validity, and the hand-rolled glob matcher
- Phase 02 ✅ realizes R-39FF-NOQQ R-3ANC-1GHF R-3BV8-F884 R-3D34-SZYT R-3EB1-6RPI R-42P0-U6JE — outbox envelope and wire cutover: kind/subject in Event, Append validation, revised SchemaSQL, keyed SSE frames
- Phase 03 ✅ realizes R-3O28-8XN2 R-3QI1-0H4G R-3RPX-E8V5 R-3SXT-S0LU R-3U5Q-5SCJ — producer families: Registry/Family, reflection index and detail, Append kind gating, CouldMatch validation helper
- Phase 04 ✅ realizes R-3VDM-JK38 R-3WLI-XBTX R-3XTF-B3KM R-4098-2N20 R-3Z1B-OVBB R-95KP-1QIO — consumer surface: Kind/Subject on consumer.Event, Event.Key(), Subscription cutover (Filter as key glob, Match deleted), end-to-end keyed delivery and filtering
