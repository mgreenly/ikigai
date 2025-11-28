# Phase Planner

## Description
Strategic understanding of project phases and their relationships. Use this skill to understand where the project is in its roadmap and how new work aligns with planned phases.

## Project Phase Hierarchy

**Ikigai** follows a structured release cycle with clear phases defined in `docs/README.md`:

### Phase Levels

1. **Phases** - High-level milestones aligned with version releases
   - Example: "LLM Integration (rel-02)", "Database Integration (v0.3.0)"
   - Defined in docs/README.md under "Roadmap to v1.0"
   - Each phase has clear objectives and deliverables

2. **Tasks** - Deliverable work units that implement phase requirements
   - Stored in `.tasks/` directory with decimal numbering
   - Example: `1-openai-client.md`, `2-conversation-state.md`
   - Each task represents a cohesive piece of functionality

3. **Steps** - Individual concrete changes within a task
   - Defined in task files under `## Steps`
   - Example: "Step 1: Write failing tests", "Step 2: Implement code"
   - Each step has Documentation/Context/Execute/Success Criteria

## Understanding Project Context

**Read `docs/README.md` to understand project context:**
- Completed phases (marked with ✅) - what's already done
- Documented upcoming phases - the planned roadmap
- Overall v1.0 architecture and vision - long-term direction
- Existing documentation and design decisions

**Key sections to understand:**
- "Roadmap to v1.0" - Sequential phases with status
- Each phase section contains:
  - Objective (what this phase achieves)
  - Tasks (what needs to be done)
  - Documentation references (where to learn more)
  - Dependencies (what must be complete first)

**BUT:** The roadmap is a living document. You can:
- Define new phases not yet documented
- Modify phase scope based on discussion
- Identify work that doesn't fit existing phases
- Propose changes to the roadmap

## Defining Work Scope

When translating discussions or requirements into work:

1. **Understand the discussion context:**
   - Is this work aligned with an existing documented phase?
   - Is this defining a NEW phase not yet in docs/README.md?
   - Does it modify or extend an existing phase?
   - Does it cut across multiple phases?

2. **Define phase boundaries as needed:**
   - Existing phases have clear objectives - align if appropriate
   - New work may require defining new phase objectives
   - Don't force work into existing phases if it doesn't fit
   - Consider dependencies between phases (existing and new)

3. **Map requirements to deliverables:**
   - What does this work need to accomplish?
   - What are the key deliverables?
   - How does it relate to overall project direction?

## Phase-to-Task Decomposition

Your role as task strategist is to identify **what tasks** are needed, not **how to implement them**.

**Good task identification:**
- "OpenAI API client with streaming support"
- "Conversation state management with checkpoints"
- "Slash command implementation (/clear, /mark, /rewind)"

**Avoid premature detail:**
- Not: "Write tests for OpenAI client JSON parsing"
- Not: "Implement curl_easy_setopt with CURLOPT_WRITEFUNCTION"
- Not: "Add rollback logic to conversation.c:234"

**Focus on deliverables:**
- What functionality needs to exist?
- What integration points are required?
- What are the major components?

## Understanding Phase Documentation

Each phase typically has dedicated documentation in `docs/`:
- Architecture documentation (e.g., `v1-llm-integration.md`)
- Design decisions (e.g., `v1-conversation-management.md`)
- Implementation notes (e.g., `v1-database-design.md`)

**Read phase documentation to understand:**
- Technical approach and design decisions
- Integration with existing systems
- Key components and their responsibilities
- Success criteria for the phase

## Strategic Questions to Ask

When planning tasks for a phase:

1. **Scope:** What's included in this phase vs. deferred to later?
2. **Dependencies:** What must be completed first?
3. **Integration:** How does this connect to existing systems?
4. **Validation:** How will we know this phase is complete?
5. **Risks:** What could block or complicate this work?

## Common Phase Patterns

### Setup Phases
- Environment configuration
- Dependency installation
- Schema creation
- Infrastructure setup

### Integration Phases
- API client implementation
- Protocol handling
- Data flow integration
- Error handling

### Feature Phases
- User-facing functionality
- UI components
- Command implementation
- Workflow support

### Refinement Phases
- Code organization
- Performance optimization
- Documentation
- Polish and cleanup

## Delegation to Task Architects

Once you've identified tasks, delegate detailed planning to task-architect sub-agents:

**For each task:**
1. Provide task number and name
2. Specify phase type (TDD, setup, integration, custom)
3. Give high-level description and objectives
4. Reference relevant documentation
5. Note integration points and context

**The task-architect will:**
- Create detailed step-by-step instructions
- Specify documentation to read for each step
- Define concrete success criteria
- Handle TDD workflow templates
- Write the actual task file

## Example: Phase to Tasks

**Phase:** LLM Integration (rel-02)
**Objective:** Stream LLM responses directly to the terminal

**Reading docs/README.md and docs/v1-llm-integration.md reveals:**
- Need HTTP client with streaming
- Conversation state management
- Slash command support
- Status indicators

**Strategic task breakdown:**
1. `1-openai-client.md` - HTTP client with libcurl streaming (TDD)
2. `2-conversation-state.md` - In-memory message management (TDD)
3. `3-slash-commands.md` - Command parsing and execution (TDD)
4. `4-status-indicators.md` - Spinner and error display (TDD)
5. `5-integration.md` - Wire everything together (Integration)

**Each task:**
- Aligns with phase objectives
- Has clear deliverable
- Can be assigned to task-architect for detailed planning
- Will use TDD workflow or custom steps as appropriate

## Related Documentation

- `docs/README.md` - Primary roadmap and phase definitions
- `docs/v1-*.md` - Phase-specific architecture and design
- `.agents/skills/task-decomposition.md` - How to delegate to task-architects
- `.agents/skills/task-system.md` - Task execution mechanics

## Summary

As a phase planner, you:
- Understand project context and roadmap from docs/README.md
- Can define NEW phases or work within existing ones
- Think strategically about what needs to be done
- Identify tasks (deliverables), not steps (implementation)
- Delegate detailed planning to task-architects
- Maintain focus on the macro view
- Shape the roadmap based on discussions and requirements

You bridge discussions and requirements to executable task breakdown by understanding where the project has been, where it's documented to go, and where it actually needs to go based on current requirements.
