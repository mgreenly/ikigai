# github — Design Index

Each Decision maps to its `DNN.md`; every `R-XXXX-XXXX` id maps to its
Decision/file. Resolving an id is a grep against this index (or the Decision files
directly). Regenerate this manifest whenever a Decision is added or its
Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — A stateless connector on the appkit chassis — none (structural)
- D2 → `project/design/D02.md` — App authentication: the installation-token source — owns `R-DLMX-CNDL`, `R-DMUT-QF4A`, `R-DO2Q-46UZ`, `R-DPAM-HYLO`, `R-DQII-VQCD`, `R-DRQF-9I32`
- D3 → `project/design/D03.md` — The typed GitHub REST v3 client — owns `R-DVE4-ETB5`, `R-DWM0-SL1U`, `R-DXTX-6CSJ`, `R-DZ1T-K4J8`, `R-E09P-XW9X`, `R-E1HM-BO0M`, `R-E2PI-PFRB`, `R-E3XF-37I0`, `R-E55B-GZ8P`, `R-E6D7-UQZE`, `R-E7L4-8IQ3`, `R-EA0X-027H`, `R-EB8T-DTY6`, `R-ECGP-RLOV`, `R-D0IM-VQ7H`
- D4 → `project/design/D04.md` — The MCP tool surface — owns `R-EEWI-J569`, `R-EHCB-AONN`, `R-EIK7-OGEC`, `R-EJS4-2851`, `R-EL00-FZVQ`, `R-EM7W-TRMF`, `R-ENFT-7JD4`
- D5 → `project/design/D05.md` — The loopback `GET /pr` twin for scripts — owns `R-EPVL-Z2UI`, `R-ER3I-CUL7`, `R-ETJB-4E2L`
- D6 → `project/design/D06.md` — The landing page and nginx fragment — owns `R-EVZ3-VXJZ`, `R-EX70-9PAO`, `R-EYEW-NH1D`, `R-7NJI-UTHM`, `R-7ORF-8L8B`, `R-7PZB-MCZ0`, `R-WYSR-NPL3`, `R-X00O-1HBS`

## Verification ids → Decision

- R-7NJI-UTHM → D6 — `project/design/D06.md`
- R-7ORF-8L8B → D6 — `project/design/D06.md`
- R-7PZB-MCZ0 → D6 — `project/design/D06.md`
- R-D0IM-VQ7H → D3 — `project/design/D03.md`
- R-DLMX-CNDL → D2 — `project/design/D02.md`
- R-DMUT-QF4A → D2 — `project/design/D02.md`
- R-DO2Q-46UZ → D2 — `project/design/D02.md`
- R-DPAM-HYLO → D2 — `project/design/D02.md`
- R-DQII-VQCD → D2 — `project/design/D02.md`
- R-DRQF-9I32 → D2 — `project/design/D02.md`
- R-DVE4-ETB5 → D3 — `project/design/D03.md`
- R-DWM0-SL1U → D3 — `project/design/D03.md`
- R-DXTX-6CSJ → D3 — `project/design/D03.md`
- R-DZ1T-K4J8 → D3 — `project/design/D03.md`
- R-E09P-XW9X → D3 — `project/design/D03.md`
- R-E1HM-BO0M → D3 — `project/design/D03.md`
- R-E2PI-PFRB → D3 — `project/design/D03.md`
- R-E3XF-37I0 → D3 — `project/design/D03.md`
- R-E55B-GZ8P → D3 — `project/design/D03.md`
- R-E6D7-UQZE → D3 — `project/design/D03.md`
- R-E7L4-8IQ3 → D3 — `project/design/D03.md`
- R-EA0X-027H → D3 — `project/design/D03.md`
- R-EB8T-DTY6 → D3 — `project/design/D03.md`
- R-ECGP-RLOV → D3 — `project/design/D03.md`
- R-EEWI-J569 → D4 — `project/design/D04.md`
- R-EHCB-AONN → D4 — `project/design/D04.md`
- R-EIK7-OGEC → D4 — `project/design/D04.md`
- R-EJS4-2851 → D4 — `project/design/D04.md`
- R-EL00-FZVQ → D4 — `project/design/D04.md`
- R-EM7W-TRMF → D4 — `project/design/D04.md`
- R-ENFT-7JD4 → D4 — `project/design/D04.md`
- R-EPVL-Z2UI → D5 — `project/design/D05.md`
- R-ER3I-CUL7 → D5 — `project/design/D05.md`
- R-ETJB-4E2L → D5 — `project/design/D05.md`
- R-EVZ3-VXJZ → D6 — `project/design/D06.md`
- R-EX70-9PAO → D6 — `project/design/D06.md`
- R-EYEW-NH1D → D6 — `project/design/D06.md`
- R-WYSR-NPL3 → D6 — `project/design/D06.md`
- R-X00O-1HBS → D6 — `project/design/D06.md`
