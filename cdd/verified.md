# Verified Issues

This file tracks issues found and resolved in CDD plan documents.

## 2026-01-04: Multi-Provider Integration Gap

**Issue:** Plan documents only specified OpenAI provider integration. Anthropic and Google providers were not covered, which would cause compilation failures when `ik_tool_build_all()` is deleted.

**Resolution:** Added multi-provider integration specifications to:
- `cdd/plan/integration-specification.md` - Added "Multi-Provider Integration" section covering:
  - Anthropic provider function signature changes (`ik_anthropic_serialize_request_stream`)
  - Google provider function signature changes (`ik_google_serialize_request`)
  - Call chains showing registry flow from shared context
  - Registry build functions for each provider
  - Schema transformation differences table
- `cdd/plan/removal-specification.md` - Added Anthropic and Google stub specifications:
  - Section 5a: Anthropic request.c stub (empty tools array)
  - Section 5b: Google request.c stub (no-op serialize_tools)
  - Updated Summary section with provider stub table

**Status:** Resolved

## 2026-01-04: Comprehensive Plan Review

**Issue:** Conducted top-down alignment review: README.md → user-stories/ → plan/

**Findings:**
- All 6 tools from README scope are fully specified in `tool-specifications.md`
- All 6 user stories have corresponding plan coverage
- Migration phases (1-6) align with README's "Migration Order" section
- Blocking vs async discovery correctly documented (Phases 2-5 blocking, Phase 6 async)
- Multi-provider integration previously verified and resolved

**Items reviewed:**
1. README scope (8 deliverables) → All present in plan
2. User stories (6 stories) → All behaviors specified in architecture.md
3. Integration points → Fully specified in integration-specification.md
4. Removal specification → Complete file/function lists with stubs
5. Error codes → Comprehensive coverage (ikigai + tool-level)

**Status:** No critical alignment gaps found. Plan is ready for task authoring

## 2026-01-04: Schema Building Integration Point Mismatch

**Issue:** Plan documents specified incorrect integration point for schema building. They claimed provider serializers (e.g., `src/providers/openai/request_chat.c`) call `ik_tool_build_all()`, but this code pattern does not exist in the current codebase.

**Reality discovered:**
- Tool definitions are hard-coded in `src/providers/request_tools.c` (lines 22-79)
- `ik_request_build_from_conversation()` (lines 283-298) populates `req->tools[]` array
- Provider serializers simply iterate over pre-populated `req->tools[]` - they never call any tool-building function
- The old `ik_tool_build_all()` in `src/tool.c` exists but is NOT used by the main request building flow

**Resolution:** Updated three plan documents:

1. **`cdd/plan/integration-specification.md`:**
   - Changed schema building integration point from provider serializers to `src/providers/request_tools.c:ik_request_build_from_conversation()`
   - Updated function signature changes to target the correct function
   - Updated call chain diagrams to show actual data flow
   - Clarified that provider serializers just read from `req->tools[]`

2. **`cdd/plan/removal-specification.md`:**
   - Replaced incorrect Section 5 (OpenAI request_chat.c calling ik_tool_build_all) with actual location (request_tools.c)
   - Updated Sections 5a-5c to note provider serializers need no changes (they already handle empty tools)
   - Updated Summary to reflect correct files being modified

3. **`cdd/plan/architecture-current.md`:**
   - Updated Integration Point A to show `request_tools.c` as actual location
   - Updated Data Flow to show request building populates `req->tools[]`
   - Added note that `ik_tool_build_all()` in `src/tool.c` is legacy/unused

**Status:** Resolved

## 2026-01-04: Missing Registry Iterator Specification

**Issue:** `integration-specification.md` referenced `ik_tool_registry_iter()` function for iterating registry entries, but `architecture.md` never declared this function. Sub-agents implementing request building would find no specification for how to iterate.

**Locations affected:**
- `cdd/plan/integration-specification.md` line 64: referenced `ik_tool_registry_iter()`
- `cdd/plan/integration-specification.md` line 277: referenced `ik_tool_registry_iter()` in call chain
- `cdd/plan/architecture.md`: no iterator function declared

**Resolution:**
1. Added iterator documentation to `architecture.md` clarifying that `entries[]` and `count` are public for direct array iteration
2. Updated `integration-specification.md` to use direct array iteration pattern (`registry->entries[0..count-1]`) instead of non-existent function

**Files modified:**
- `cdd/plan/architecture.md` - Added iteration usage comment documenting public access to entries/count
- `cdd/plan/integration-specification.md` - Changed two references from `ik_tool_registry_iter()` to direct array iteration

**Status:** Resolved

## 2026-01-04: Missing Function Signature Change Specification in discovery-infrastructure.md

**Issue:** The task file `cdd/tasks/discovery-infrastructure.md` showed code using a `registry` variable in the request_tools.c section, but never specified:
1. The function signature change required for `ik_request_build_from_conversation()` to accept a registry parameter
2. The header update required in `src/providers/request.h`
3. The three call sites that must be updated: `repl_actions_llm.c:148`, `repl_tool_completion.c:55`, `commands_fork.c:109`

This would cause the sub-agent to either fail compilation (undefined `registry` variable) or invent an incorrect solution (global variable, ad-hoc mechanism). Even if the agent inferred the need for a signature change, missing call site updates would cause "too few arguments" compilation errors in subsequent tasks.

**Resolution:** Added comprehensive specification to `cdd/tasks/discovery-infrastructure.md`:
- Added "Function Signature Change: ik_request_build_from_conversation" section with old/new signatures
- Added "Call Site Updates (ALL THREE REQUIRED)" section with exact code changes for each call site
- Updated request_tools.c section to show complete function signature in implementation
- Added postconditions to verify signature change and call site updates

**Files modified:**
- `cdd/tasks/discovery-infrastructure.md` - Added function signature change specification, call site updates, and updated postconditions

**Status:** Resolved

## 2026-01-04: Incorrect Wrapper Function Call Sites in discovery-infrastructure.md

**Issue:** The task file `cdd/tasks/discovery-infrastructure.md` showed incorrect function names for call sites 2 and 3. The task specified updating calls to `ik_request_build_from_conversation()` but the actual source code uses the wrapper function `ik_request_build_from_conversation_()` (with trailing underscore).

**Locations affected:**
- `cdd/tasks/discovery-infrastructure.md` lines 279-286: Showed `ik_request_build_from_conversation(ctx, agent, &req)` for call site 2
- Actual code at `src/repl_tool_completion.c:55`: Uses `ik_request_build_from_conversation_(agent, agent, (void **)&req)`
- `cdd/tasks/discovery-infrastructure.md` lines 288-295: Showed `ik_request_build_from_conversation(ctx, agent, &req)` for call site 3
- Actual code at `src/commands_fork.c:109`: Uses `ik_request_build_from_conversation_(repl->current, repl->current, (void **)&req)`

Additionally, the wrapper function signature change was only vaguely mentioned ("If there's a wrapper function...") without providing explicit old/new signatures.

**Why critical:** Sub-agent would:
1. Look for the wrong function name in source code
2. Not know how to update the wrapper function signature
3. Fail to compile due to mismatched function signatures

**Resolution:** Updated `cdd/tasks/discovery-infrastructure.md`:
1. Corrected call site 2 to show actual wrapper function: `ik_request_build_from_conversation_(agent, agent, (void **)&req)`
2. Corrected call site 3 to show actual wrapper function: `ik_request_build_from_conversation_(repl->current, repl->current, (void **)&req)`
3. Added explicit item 4 with complete wrapper function signature changes for:
   - `src/wrapper_internal.h` inline implementation (~line 52-55)
   - `src/wrapper_internal.h` declaration (~line 111)
   - `src/wrapper_internal.c` implementation (~line 59-62)
4. Changed section title from "ALL THREE REQUIRED" to "ALL FOUR REQUIRED"
5. Updated postconditions to include wrapper_internal.h/c in the 4 call sites

**Files modified:**
- `cdd/tasks/discovery-infrastructure.md` - Corrected wrapper function names, added explicit signature changes

**Status:** Resolved
