# dashboard — Design Index (web pages restructure)

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to
its Decision/file. Resolve an id by grepping this index (or the Decision files
directly). Regenerate this manifest whenever a Decision is added or its
Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — Three-page topology and the route map — owns R-DB01-PG3A, R-DB02-LND7, R-DB03-PRF9
- D2 → `project/design/D02.md` — The profile route is session-gated in-process (redirect when signed out) — owns R-DB04-GATE, R-DB05-SESS
- D3 → `project/design/D03.md` — Personal-access-token management moves to the profile page — owns R-DB06-PATM, R-DB07-PATR, R-DB08-PATX
- D4 → `project/design/D04.md` — OAuth grant management moves to the profile page — owns R-DB09-GRNT, R-DB10-GRVK, R-DB11-GRNX
- D5 → `project/design/D05.md` — Landing composition: service-name links, email→profile nav, sign-out — owns R-DB12-LINK, R-DB13-MAIL, R-DB14-SOUT, R-DB15-INST
- D6 → `project/design/D06.md` — Purge the stale "single hybrid page / don't split" doc rule — owns R-DB16-DOCS
- D7 → `project/design/D07.md` — Login composition: the sign-in line ("Sign in to access your services.") is the `<h1>` (the "Your account's control plane" line removed); name-origin colophon below the CTA that explains the portmanteau as a name + its two subcomponents + a pronunciation guide at its foot — owns R-DB17-ORIG, R-DB18-KEEP, R-DB19-LAND, R-O7K1-XEN7
- D8 → `project/design/D08.md` — Eliminate the web-font FOUT (font-display: optional + font preload; apex src already origin-correct) — owns R-P97M-GIJ1, R-PAFI-UA9Q, R-PBNF-820F
- D9 → `project/design/D09.md` — No site footer: remove the `.site-footer` element from every page (index login + landing, profile, pat_created) and delete its CSS — owns R-EFJZ-FRQ1

## Verification ids → Decision

- R-DB01-PG3A → D1 → `project/design/D01.md`
- R-DB02-LND7 → D1 → `project/design/D01.md`
- R-DB03-PRF9 → D1 → `project/design/D01.md`
- R-DB04-GATE → D2 → `project/design/D02.md`
- R-DB05-SESS → D2 → `project/design/D02.md`
- R-DB06-PATM → D3 → `project/design/D03.md`
- R-DB07-PATR → D3 → `project/design/D03.md`
- R-DB08-PATX → D3 → `project/design/D03.md`
- R-DB09-GRNT → D4 → `project/design/D04.md`
- R-DB10-GRVK → D4 → `project/design/D04.md`
- R-DB11-GRNX → D4 → `project/design/D04.md`
- R-DB12-LINK → D5 → `project/design/D05.md`
- R-DB13-MAIL → D5 → `project/design/D05.md`
- R-DB14-SOUT → D5 → `project/design/D05.md`
- R-DB15-INST → D5 → `project/design/D05.md`
- R-DB16-DOCS → D6 → `project/design/D06.md`
- R-DB17-ORIG → D7 → `project/design/D07.md`
- R-DB18-KEEP → D7 → `project/design/D07.md`
- R-DB19-LAND → D7 → `project/design/D07.md`
- R-EFJZ-FRQ1 → D9 → `project/design/D09.md`
- R-O7K1-XEN7 → D7 → `project/design/D07.md`
- R-P97M-GIJ1 → D8 → `project/design/D08.md`
- R-PAFI-UA9Q → D8 → `project/design/D08.md`
- R-PBNF-820F → D8 → `project/design/D08.md`
