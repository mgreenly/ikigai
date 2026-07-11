# prompts — Plan

**Authority: construction order and history.** This document and the `project/plan/` directory it heads own the build order and the record of what is built. The plan is **append-only**: completed phases are never rewritten or deleted, so it doubles as construction history. To extend the plan, update product and design in place, then **append** a new phase — a new `project/plan/phase-NN.md` plus a new line at the end of `project/plan/STATUS.md` — never edit a finished phase except to flip its status marker from `⬜` to `✅`.

**One phase = one package = one accumulating context.** Each phase is a single coherent unit — almost always one package — built in one accumulating context against product and design, reading only that package's design Decisions and the *interfaces* (not internals) of the packages it depends on. This is what keeps every phase the size of a small standalone tool no matter how large the project grows. If a single package must split across phases to fit one context, the affected phase files state the partial-Decision split explicitly.

**Done bar.** A phase is **done** when every Verification item (the `R-XXXX-XXXX` ids) in the design Decisions it realizes — or the slice of those ids assigned to it — is covered by a clearly-named test and the suite is green. See the design's *Conventions* section for what "green" concretely means and each Decision's Verification list for what "covered" means.

## Layout

`project/plan/STATUS.md` is the manifest and the **only** home of status markers; `project/plan/phase-NN.md` is one body file per phase (zero-padded; sub-phases keep their suffix, e.g. `phase-07a.md`); `project/plan/README.md` is these static rules. The append-only rule applies to the layout: never rewrite or delete a `phase-NN.md`, never delete a `STATUS.md` line; the only build-time mutation is flipping one phase's `⬜ → ✅` in `STATUS.md`.
