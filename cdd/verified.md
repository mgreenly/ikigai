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
