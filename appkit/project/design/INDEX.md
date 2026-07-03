# appkit — Design Index (manifest read-path through `current`)

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to
its Decision/file. Resolve an id by grepping this index (or the Decision files
directly). Regenerate this manifest whenever a Decision is added or its
Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — Manifest readers resolve *through* the per-app `current` symlink (`appkit/inventory`) — owns R-YO06-9I18, R-YP82-N9RX
- D2 → `project/design/D02.md` — `bin/registry` resolves through `current` — owns R-YQFZ-11IM
- D3 → `project/design/D03.md` — Local dev runtime layout mirrors the box (`bin/start` stages a prod-shaped manifest root) — owns R-YRNV-ET9B
- D4 → `project/design/D04.md` — Retire the stable sibling path and its hand-placed artifacts — owns R-YSVR-SL00, R-YU3O-6CQP

## Verification ids → Decision

- R-YO06-9I18 → D1 → `project/design/D01.md`
- R-YP82-N9RX → D1 → `project/design/D01.md`
- R-YQFZ-11IM → D2 → `project/design/D02.md`
- R-YRNV-ET9B → D3 → `project/design/D03.md`
- R-YSVR-SL00 → D4 → `project/design/D04.md`
- R-YU3O-6CQP → D4 → `project/design/D04.md`
