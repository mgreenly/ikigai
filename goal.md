Story: #280

## Objective

Add an optional `--depends` flag to `goal-create` that appends a `Depends:` line to the goal body.

## Changes to `.claude/harness/goal-create/run`

### 1. Parse new flag

Add to the flag parsing loop:
```ruby
when '--depends'
  depends = args.shift
```

### 2. Append to body

After the `Story: #N` prepend (line 87), if depends is present:
```ruby
if depends
  dep_refs = depends.split(',').map(&:strip).map { |d| "##{d.delete('#')}" }.join(', ')
  body = "#{body}\nDepends: #{dep_refs}"
end
```

### 3. Update help text

Add `--depends` to the usage and examples:
```
--depends "277,278"    Goal numbers this depends on (optional)
```

## Acceptance Criteria

- `goal-create --story 15 --title "X" --depends 277` produces body with `Depends: #277`
- `goal-create --story 15 --title "X" --depends "277,278"` produces body with `Depends: #277, #278`
- Omitting `--depends` has no effect (backwards compatible)

Story: #280