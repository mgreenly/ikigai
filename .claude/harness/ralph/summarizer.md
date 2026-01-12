You are summarizing progress history for an autonomous agent loop.

## Input

### Current Summary (Iterations 1-<%= summary_end %>)
<%= summary %>

### Recent Iterations (<%= summary_end + 1 %>-<%= current %>)
<%= recent %>

## Task

Produce an updated summary covering iterations 1-<%= current %>.

## Preservation Rules

**Must preserve:**
- Failed approaches and why they failed (prevents retry loops)
- Blockers encountered and how they were resolved
- Key decisions and their rationale
- Files modified or deleted

**Can compress:**
- Routine verification steps ("ran check-build, passed")
- Intermediate states superseded by later progress
- Redundant observations

## Output Format

Return a concise narrative (10-20 sentences) covering:
1. What has been accomplished
2. What was tried but failed (and why)
3. Current state
