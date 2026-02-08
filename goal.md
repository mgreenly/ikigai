Story: #280

## Objective

Modify the orchestrator's slot-filling logic to respect goal dependencies declared in goal bodies.

## Changes to `.claude/harness/orchestrator/run`

### 1. Add dependency parsing helper

```ruby
def parse_depends(body)
  return [] unless body
  match = body.match(/^Depends:\s*(.+)$/i)
  return [] unless match
  match[1].scan(/#(\d+)/).flatten.map(&:to_i)
end
```

### 2. Add dependency check helper

```ruby
def dependencies_met?(depends_on)
  return true if depends_on.empty?
  depends_on.all? do |dep_number|
    result = run_script(GOAL_GET_SCRIPT, dep_number.to_s)
    result && result['ok'] && result['labels']&.include?('goal:done')
  end
end
```

Note: `goal-get` must return labels in its response. Check if it already does; if not, this goal should add that field.

### 3. Filter queued goals in slot-filling loop

In the `queued.first(available_slots).each` block (around line 255), before cloning and spawning:

1. Fetch the goal body (already done at line 274 but after cloning — move the dependency check before cloning)
2. Parse dependencies from the body
3. If dependencies are not met, skip this goal (leave it queued)
4. Log when skipping: `"Goal #N waiting on dependencies: #X, #Y"`

## Acceptance Criteria

- Goals with `Depends: #N` stay queued until #N has `goal:done` label
- Goals without `Depends:` line are unaffected
- Multiple dependencies supported (`Depends: #1, #2, #3`)
- Skipped goals are logged but not re-labeled
- No unnecessary API calls (only check deps when filling slots)

Story: #280