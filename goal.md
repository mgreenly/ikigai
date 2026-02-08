Story: #276

## Objective

Wire up the `story-try-close` helper into the three places that transition goals to `goal:done`.

## Changes

### 1. `.claude/harness/orchestrator/run`

After line 221 (`transition_label(number, 'goal:running', 'goal:done')`), call:
```ruby
system(STORY_TRY_CLOSE_SCRIPT, number.to_s, out: '/dev/null', err: '/dev/null')
```

Add the constant:
```ruby
STORY_TRY_CLOSE_SCRIPT = File.join(SCRIPT_DIR, '..', 'story-try-close', 'run')
```

### 2. `.claude/harness/goal-approve/run`

After line 119 (where it adds `goal:done` label), call:
```ruby
system(File.join(PROJECT_ROOT, '.claude', 'scripts', 'story-try-close'), number.to_s,
       out: '/dev/null', err: '/dev/null')
```

### 3. `.claude/harness/goal-spot-check/run`

After line 83 (where it adds `goal:done` label on approve), call:
```ruby
system(File.join(File.dirname(File.realpath(__FILE__)), '..', 'story-try-close', 'run'),
       number.to_s, out: '/dev/null', err: '/dev/null')
```

## Acceptance Criteria

- All three `goal:done` transitions trigger `story-try-close`
- Failure of `story-try-close` is non-fatal (goal still completes)
- No other behavior changes

Story: #276
