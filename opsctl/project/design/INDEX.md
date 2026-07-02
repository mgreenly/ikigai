# opsctl — Design Index

Each Decision maps to its `DNN.md`; every `R-XXXX-XXXX` id maps to its
Decision/file. Resolving an id is a grep against this index (or the Decision files
directly). Regenerate this manifest whenever a Decision is added or its
Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — Restore reconstructs `cache/` owned by the service user — owns `R-WP3M-PO1V`, `R-WQBJ-3FSK`, `R-WRJF-H7J9`
- D2 → `project/design/D02.md` — Stage unpacks into a temp dir on the OPSCTL_ROOT filesystem — owns `R-65MT-7QEK`, `R-66UP-LI59`
- D3 → `project/design/D03.md` — opsctl loads the box env file at startup — owns `R-6AIE-QTDC`, `R-6BQB-4L41`, `R-6CY7-ICUQ`, `R-6FE0-9WC4`

## Verification ids → Decision

- R-65MT-7QEK → D2 — `project/design/D02.md`
- R-66UP-LI59 → D2 — `project/design/D02.md`
- R-6AIE-QTDC → D3 — `project/design/D03.md`
- R-6BQB-4L41 → D3 — `project/design/D03.md`
- R-6CY7-ICUQ → D3 — `project/design/D03.md`
- R-6FE0-9WC4 → D3 — `project/design/D03.md`
- R-WP3M-PO1V → D1 — `project/design/D01.md`
- R-WQBJ-3FSK → D1 — `project/design/D01.md`
- R-WRJF-H7J9 → D1 — `project/design/D01.md`
