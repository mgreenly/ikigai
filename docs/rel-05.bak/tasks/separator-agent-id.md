# Task: Display Agent ID in Separator

## Target
User Story: 12-separator-shows-agent.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/patterns/composite.md
- .agents/skills/patterns/vtable.md

## Pre-read Docs
- docs/backlog/manual-top-level-agents.md (Visual Feedback section)
- docs/rel-05/user-stories/12-separator-shows-agent.md

## Pre-read Source (patterns)
- src/layer.h (layer vtable interface)
- src/layer.c (layer implementation)
- src/layer_wrappers.h (separator layer)
- src/layer_wrappers.c (separator layer implementation)
- src/render.h (render context)
- src/render.c (rendering logic)
- src/repl.h (access to current agent)
- src/agent.h (agent_id field)
- src/ansi.h (ANSI escape codes for formatting)

## Pre-read Tests (patterns)
- tests/unit/layer/layer_test.c (layer tests)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Separator layer exists and renders horizontal line
- DI-1 complete: agents have agent_id field
- repl-agent-array.md complete: can access current agent
- agent-switch.md complete: switching triggers re-render

## Task
Update the separator layer to display the current agent ID centered in the horizontal line.

**Current separator appearance:**
```
────────────────────────────────────────────────────────────
```

**New separator appearance:**
```
───────────────────────── agent 0/ ─────────────────────────
```

**Format specification:**
- Dashes (`─`, Unicode box-drawing) fill the line
- Agent label: ` agent {id} ` (space-padded)
- Label centered horizontally
- Adjusts to terminal width
- Future: will show additional status (Phase 2+)

**Rendering logic:**
```c
void separator_render(ik_layer_t *layer, ik_render_ctx_t *render)
{
    ik_repl_ctx_t *repl = layer->user_ctx;
    ik_agent_ctx_t *agent = CURRENT_AGENT(repl);

    // Format label
    char label[32];
    snprintf(label, sizeof(label), " agent %s ", agent->agent_id);
    size_t label_len = strlen(label);

    // Calculate dash counts
    int32_t width = render->term_width;
    int32_t left_dashes = (width - label_len) / 2;
    int32_t right_dashes = width - label_len - left_dashes;

    // Render: dashes + label + dashes
    for (int32_t i = 0; i < left_dashes; i++) {
        render_char(render, L'─');
    }
    render_string(render, label);
    for (int32_t i = 0; i < right_dashes; i++) {
        render_char(render, L'─');
    }
}
```

**Edge cases:**
- Very narrow terminal: truncate dashes, keep label
- Very long agent_id (future hierarchical): may need truncation

## TDD Cycle

### Red
1. Update separator layer interface to accept agent context or repl context

2. Create/update tests in `tests/unit/layer/separator_agent_test.c`:
   - Test separator renders with agent ID centered
   - Test label format is " agent 0/ " for agent 0
   - Test label format is " agent 1/ " for agent 1
   - Test dashes fill remaining width
   - Test narrow terminal (width 20) still shows label
   - Test re-render after switch shows new agent ID

3. Run `make check` - expect test failures

### Green
1. Update separator layer in `src/layer_wrappers.c`:
   - Add pointer to repl context (for accessing current agent)
   - Update render function to include agent ID
   - Calculate centered positioning

2. Update separator layer creation to receive repl context:
   ```c
   ik_layer_t *ik_separator_layer_create(TALLOC_CTX *ctx, ik_repl_ctx_t *repl);
   ```

3. Ensure separator re-renders on agent switch (already happens via full redraw)

4. Run `make check` - expect pass

### Refactor
1. Verify Unicode box-drawing character renders correctly
2. Verify terminal width changes update separator
3. Consider: dim/color styling for agent ID? (Keep simple for now)
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- Separator displays current agent ID centered
- Format: `──────── agent N/ ────────`
- Updates on agent switch
- Adjusts to terminal width
- No additional status info yet (Phase 1 scope)
- Working tree is clean (all changes committed)
