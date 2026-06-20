# wiki — Design Index

Each Decision maps to its `docs/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `docs/design/D01.md` — Dependency on the external agentkit (the prod-build wiring) — owns R-MV3L-QS7I, R-MWBI-4JY7
- D2 → `docs/design/D02.md` — Service skeleton: package layout, Spec wiring, config/secret composition root — owns R-6RVX-P1IG
- D3 → `docs/design/D03.md` — The phase-1 data model — owns R-7SNG-0G9A, R-7TVC-E7ZZ, R-7V38-RZQO, R-7WB5-5RHD
- D4 → `docs/design/D04.md` — The ingest pipeline and worker — owns R-M8RN-87WV, R-M9ZJ-LZNK, R-MB7F-ZRE9, R-MCFC-DJ4Y, R-MDN8-RAVN, R-MG31-IUD1
- D5 → `docs/design/D05.md` — The LLM seam (`internal/llm`): json-mode helper — owns R-J8QP-BETB, R-J9YL-P6K0, R-JCEE-GQ1E, R-JDMA-UHS3, R-JEU7-89IS
- D6 → `docs/design/D06.md` — The extract stage (`internal/extract`) — owns R-VYU0-BPAX, R-W01W-PH1M, R-W19T-38SB, R-W2HP-H0J0
- D7 → `docs/design/D07.md` — The compile stage (`internal/compile`): full recompile, 12k cap — owns R-FQLB-QWS6, R-FT14-IG9K, R-FU90-W809, R-FVGX-9ZQY, R-FWOT-NRHN
- D8 → `docs/design/D08.md` — The retrieval seam (`internal/retrieve`): keyword now, hybrid later — owns R-CLF2-TMI8, R-CMMZ-7E8X, R-CNUV-L5ZM, R-CP2R-YXQB, R-CQAO-CPH0
- D9 → `docs/design/D09.md` — `ask` (`internal/ask`): grounded, cited, honest-empty agent — owns R-5THH-I3WL, R-5UPD-VVNA, R-5VXA-9NDZ, R-5X56-NF4O, R-5YD3-16VD
- D10 → `docs/design/D10.md` — The MCP tool surface (`internal/mcp`) + identity — owns R-MUQ4-K1JS, R-MVY0-XTAH, R-MX5X-BL16, R-MYDT-PCRV, R-MZLQ-34IK

## Verification ids → Decision

- R-5THH-I3WL → D9 → `docs/design/D09.md`
- R-5UPD-VVNA → D9 → `docs/design/D09.md`
- R-5VXA-9NDZ → D9 → `docs/design/D09.md`
- R-5X56-NF4O → D9 → `docs/design/D09.md`
- R-5YD3-16VD → D9 → `docs/design/D09.md`
- R-6RVX-P1IG → D2 → `docs/design/D02.md`
- R-7SNG-0G9A → D3 → `docs/design/D03.md`
- R-7TVC-E7ZZ → D3 → `docs/design/D03.md`
- R-7V38-RZQO → D3 → `docs/design/D03.md`
- R-7WB5-5RHD → D3 → `docs/design/D03.md`
- R-CLF2-TMI8 → D8 → `docs/design/D08.md`
- R-CMMZ-7E8X → D8 → `docs/design/D08.md`
- R-CNUV-L5ZM → D8 → `docs/design/D08.md`
- R-CP2R-YXQB → D8 → `docs/design/D08.md`
- R-CQAO-CPH0 → D8 → `docs/design/D08.md`
- R-FQLB-QWS6 → D7 → `docs/design/D07.md`
- R-FT14-IG9K → D7 → `docs/design/D07.md`
- R-FU90-W809 → D7 → `docs/design/D07.md`
- R-FVGX-9ZQY → D7 → `docs/design/D07.md`
- R-FWOT-NRHN → D7 → `docs/design/D07.md`
- R-J8QP-BETB → D5 → `docs/design/D05.md`
- R-J9YL-P6K0 → D5 → `docs/design/D05.md`
- R-JCEE-GQ1E → D5 → `docs/design/D05.md`
- R-JDMA-UHS3 → D5 → `docs/design/D05.md`
- R-JEU7-89IS → D5 → `docs/design/D05.md`
- R-M8RN-87WV → D4 → `docs/design/D04.md`
- R-M9ZJ-LZNK → D4 → `docs/design/D04.md`
- R-MB7F-ZRE9 → D4 → `docs/design/D04.md`
- R-MCFC-DJ4Y → D4 → `docs/design/D04.md`
- R-MDN8-RAVN → D4 → `docs/design/D04.md`
- R-MG31-IUD1 → D4 → `docs/design/D04.md`
- R-MUQ4-K1JS → D10 → `docs/design/D10.md`
- R-MV3L-QS7I → D1 → `docs/design/D01.md`
- R-MVY0-XTAH → D10 → `docs/design/D10.md`
- R-MWBI-4JY7 → D1 → `docs/design/D01.md`
- R-MX5X-BL16 → D10 → `docs/design/D10.md`
- R-MYDT-PCRV → D10 → `docs/design/D10.md`
- R-MZLQ-34IK → D10 → `docs/design/D10.md`
- R-VYU0-BPAX → D6 → `docs/design/D06.md`
- R-W01W-PH1M → D6 → `docs/design/D06.md`
- R-W19T-38SB → D6 → `docs/design/D06.md`
- R-W2HP-H0J0 → D6 → `docs/design/D06.md`
