# wiki — Design Index

Each Decision maps to its `docs/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `docs/design/D01.md` — Dependency on the external agentkit (the prod-build wiring) — owns R-MV3L-QS7I, R-MWBI-4JY7
- D2 → `docs/design/D02.md` — Service skeleton: package layout, Spec wiring, config/secret composition root — owns R-6RVX-P1IG
- D3 → `docs/design/D03.md` — The phase-1 data model — owns R-7SNG-0G9A, R-7TVC-E7ZZ, R-7V38-RZQO, R-7WB5-5RHD
- D4 → `docs/design/D04.md` — The ingest pipeline and worker — owns R-M8RN-87WV, R-M9ZJ-LZNK, R-MB7F-ZRE9, R-MCFC-DJ4Y, R-MDN8-RAVN, R-MG31-IUD1
- D5 → `docs/design/D05.md` — The LLM seam (`internal/llm`): json-mode helper — owns R-4BCC-0EHJ, R-J8QP-BETB, R-J9YL-P6K0, R-JCEE-GQ1E, R-JDMA-UHS3, R-JEU7-89IS
- D6 → `docs/design/D06.md` — The extract stage (`internal/extract`) — owns R-4CK8-E688, R-VYU0-BPAX, R-W19T-38SB, R-W2HP-H0J0, R-XJBY-H8JZ, R-XKJU-V0AO
- D7 → `docs/design/D07.md` — The compile stage (`internal/compile`): full recompile, 12k cap — owns R-4DS4-RXYX, R-FQLB-QWS6, R-FT14-IG9K, R-FU90-W809, R-FVGX-9ZQY, R-FWOT-NRHN
- D8 → `docs/design/D08.md` — No retrieval lane this release: keyword search removed, hybrid deferred — owns R-PH8Z-YHNX, R-PIGW-C9EM
- D9 → `docs/design/D09.md` — `ask` (`internal/ask`): subject-extraction pipeline, grounded/cited/honest-empty — owns R-5UPD-VVNA, R-5VXA-9NDZ, R-5X56-NF4O, R-644V-3WUS, R-65CR-HOLH, R-66KN-VGC6, R-67SK-982V, R-690G-MZTK, R-6A8D-0RK9, R-05CG-3H6Y
- D10 → `docs/design/D10.md` — The MCP tool surface (`internal/mcp`) + identity — owns R-MUQ4-K1JS, R-MVY0-XTAH, R-MX5X-BL16, R-MYDT-PCRV, R-MZLQ-34IK, R-N4KO-2WTZ, R-01OQ-Y5YV, R-02WN-BXPK, R-044J-PPG9, R-03GW-PX5K, R-04HB-QM7T
- D11 → `docs/design/D11.md` — Subject addressing: the `type/slug` public path — owns R-ZO9U-QOT8, R-ZQPN-I8AM, R-ZRXJ-W01B, R-ZT5G-9RS0
- D12 → `docs/design/D12.md` — Page links: read-time mention detection + markdown footer — owns R-ZUDC-NJIP, R-ZVL9-1B9E, R-ZWT5-F303, R-ZY11-SUQS, R-ZZ8Y-6MHH, R-00GU-KE86

## Verification ids → Decision

- R-00GU-KE86 → D12 → `docs/design/D12.md`
- R-01OQ-Y5YV → D10 → `docs/design/D10.md`
- R-02WN-BXPK → D10 → `docs/design/D10.md`
- R-03GW-PX5K → D10 → `docs/design/D10.md`
- R-044J-PPG9 → D10 → `docs/design/D10.md`
- R-04HB-QM7T → D10 → `docs/design/D10.md`
- R-05CG-3H6Y → D9 → `docs/design/D09.md`
- R-4BCC-0EHJ → D5 → `docs/design/D05.md`
- R-4CK8-E688 → D6 → `docs/design/D06.md`
- R-4DS4-RXYX → D7 → `docs/design/D07.md`
- R-5UPD-VVNA → D9 → `docs/design/D09.md`
- R-5VXA-9NDZ → D9 → `docs/design/D09.md`
- R-5X56-NF4O → D9 → `docs/design/D09.md`
- R-644V-3WUS → D9 → `docs/design/D09.md`
- R-65CR-HOLH → D9 → `docs/design/D09.md`
- R-66KN-VGC6 → D9 → `docs/design/D09.md`
- R-67SK-982V → D9 → `docs/design/D09.md`
- R-690G-MZTK → D9 → `docs/design/D09.md`
- R-6A8D-0RK9 → D9 → `docs/design/D09.md`
- R-6RVX-P1IG → D2 → `docs/design/D02.md`
- R-7SNG-0G9A → D3 → `docs/design/D03.md`
- R-7TVC-E7ZZ → D3 → `docs/design/D03.md`
- R-7V38-RZQO → D3 → `docs/design/D03.md`
- R-7WB5-5RHD → D3 → `docs/design/D03.md`
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
- R-N4KO-2WTZ → D10 → `docs/design/D10.md`
- R-PH8Z-YHNX → D8 → `docs/design/D08.md`
- R-PIGW-C9EM → D8 → `docs/design/D08.md`
- R-VYU0-BPAX → D6 → `docs/design/D06.md`
- R-W19T-38SB → D6 → `docs/design/D06.md`
- R-W2HP-H0J0 → D6 → `docs/design/D06.md`
- R-XJBY-H8JZ → D6 → `docs/design/D06.md`
- R-XKJU-V0AO → D6 → `docs/design/D06.md`
- R-ZO9U-QOT8 → D11 → `docs/design/D11.md`
- R-ZQPN-I8AM → D11 → `docs/design/D11.md`
- R-ZRXJ-W01B → D11 → `docs/design/D11.md`
- R-ZT5G-9RS0 → D11 → `docs/design/D11.md`
- R-ZUDC-NJIP → D12 → `docs/design/D12.md`
- R-ZVL9-1B9E → D12 → `docs/design/D12.md`
- R-ZWT5-F303 → D12 → `docs/design/D12.md`
- R-ZY11-SUQS → D12 → `docs/design/D12.md`
- R-ZZ8Y-6MHH → D12 → `docs/design/D12.md`
