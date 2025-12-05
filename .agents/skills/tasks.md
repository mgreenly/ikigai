# Tasks

Feature/release folder structure and workflow for TDD-based implementation.

## Folder Structure

Feature/release folders follow this structure:

- **README.md** - What this feature/release is about
- **run.md** - Orchestrator prompt for supervising sub-agents
- **user-stories/** - Requirements as numbered story files (01-name.md, 02-name.md)
- **tasks/** - Implementation work derived from stories
- **fixes/** - Issues found during review, works like tasks/

## User Story Format

Each story file contains: Description, Transcript (example interaction), Walkthrough (numbered internal steps), Reference (API contracts as JSON).

## Order Files

Both `tasks/order.md` and `fixes/order.md` list items organized by story. Completed items use ~~strikethrough~~. Execute strictly top-to-bottom.

## Task/Fix File Format

Each file specifies:
- **Target** - Which user story it supports
- **Agent model** - haiku (simple), sonnet (moderate), opus (complex)
- **Pre-read sections** - Skills, docs, source patterns, test patterns
- **Pre-conditions** - What must be true before starting
- **Task** - One clear, testable goal
- **TDD Cycle** - Red (failing test), Green (minimal impl), Refactor (clean up)
- **Post-conditions** - What must be true after completion

## Workflow

**IMPORTANT**: When working on a feature/release, ALWAYS read the `run.md` file first to understand your role and how to process the work.

1. **User stories** define requirements with example transcripts
2. **Tasks** are created from stories (smallest testable units)
3. **Orchestrator** (run.md) supervises sub-agents executing tasks
4. **Review** identifies issues after tasks complete
5. **Fixes** capture rework from review process

## Orchestrator Role

- Launch sub-agents with model specified in task/fix file
- Monitor results, update order.md with ~~strikethrough~~
- Commit after each task/fix completes
- Never run code directly - sub-agents do all work

## Rules

- Semantic filenames (not numbered)
- Sub-agents start with blank context - list all pre-reads
- Pre-conditions of task N = post-conditions of task N-1
- One task/fix per file, one clear goal
- Always verify `make check` at end of each TDD phase
