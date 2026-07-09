# Phase 24 — agentkit `v0.2.1`: group-name loading in the framing prompt and the pin

*Realizes design Decision 19 (amended: group-name loading guidance + the `v0.2.1` pin). Depends on Phase 23 (deferred runner wiring). **External precondition:** the published `github.com/ikigenba/agentkit v0.2.1` tag (agentkit's phase 50) — already published.*

agentkit `v0.2.1` widens `load_tools`: a name that fails tool-name resolution now loads a matching group's whole tool set (agentkit's amended D23), so a model's natural "load the crm tools" call succeeds instead of burning a corrective round-trip. This phase trues prompts up to that release: the framing prompt's deferred-tools guidance states that a service's name loads all of its tools, and the dependency pin moves to `v0.2.1`.

End state:

- `prompts/internal/runner/framing_prompt.go`'s guidance paragraph matches the amended D19 wording — names `load_tools`, states that a service's name loads that whole group, enumerates no individual service. `eventPreamble` unchanged.
- `prompts/go.mod` pins `github.com/ikigenba/agentkit v0.2.1` (`go mod tidy`; `go.sum` updates with it).

## Done when

The suite is green (design *Conventions* commands, from `prompts/`) and:

- **R-A69O-ATWI** — a clearly-named test asserts the framing prompt sent as the conversation `System` states that a service's name passed to `load_tools` loads all of that service's tools, while still enumerating no individual service.
- R-9OJA-B2KU stays green on the reworded guidance (still names `load_tools`, still no `ikigenba_` service enumeration).
- `grep -n "agentkit v0.2.1" prompts/go.mod` matches, and the build is green with `GOWORK=off`.
