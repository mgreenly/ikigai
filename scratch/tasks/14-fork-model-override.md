# Task: Update /fork Command for Model Override

**Phase:** 4 - Commands
**Depends on:** 07-agent-provider-fields, 08-model-command

## Objective

Update `/fork` command to support `--model MODEL/THINKING` override for child agents.

## Deliverables

1. Update `src/commands_fork.c`:
   - Parse `--model MODEL/THINKING` flag
   - Default: inherit parent's provider/model/thinking
   - Override: use specified model/thinking

2. Argument parsing:
   - `/fork` - Inherit all from parent
   - `/fork "prompt"` - Inherit + assign task
   - `/fork --model NAME/THINKING` - Override
   - `/fork --model NAME/THINKING "prompt"` - Override + task

3. Child agent creation:
   - Copy parent's provider/model/thinking if not overridden
   - Apply override if specified
   - Save to database with new values

## Reference

- `scratch/README.md` - Fork Command Integration section

## Examples

```
# Parent using claude-sonnet-4-5/med

/fork
# Child inherits: claude-sonnet-4-5/med

/fork --model o3-mini/high "Solve complex problem"
# Child uses: o3-mini/high
```

## Verification

- Inheritance works correctly
- Override applies to child
- Database stores correct values
