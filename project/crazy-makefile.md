# Crazy Makefile Idea: Make as Query Engine

Status: **Idea** - may consider in the future

## Problem

Parsing `make` output is painful for LLM-driven development:
- Compiler warnings and errors interleaved
- Entering/leaving directory messages
- Progress indicators
- Interleaved stdout/stderr with parallel builds
- Models spend tokens re-learning how to parse output

Current workaround: custom `check-*` targets with simplified output that scripts parse into structured JSON. Works, but ugly.

## Proposed Solution

Use Make only for what it's good at: **dependency resolution**.

```bash
make -n target    # dry-run: prints commands without executing
```

This gives us:
- Predictable gcc command output
- Only stale files (incremental builds work)
- No compiler noise, warnings, or errors
- Easy to parse

Scripts handle actual execution with controlled output formatting.

## Experiment Results

### Basic dry-run output

```
$ make -n
gcc -Wall -g -c main.c -o main.o
gcc -Wall -g -c foo.c -o foo.o
gcc -Wall -g -c bar.c -o bar.o
gcc main.o foo.o bar.o -o program
```

Clean, one command per line.

### Incremental builds work

```
$ make              # build everything
$ touch foo.c
$ make -n           # only shows stale files
gcc -Wall -g -c foo.c -o foo.o
gcc main.o foo.o bar.o -o program
```

### Parallel flag doesn't change output

```
$ make -n           # same output
$ make -j4 -n       # same output
```

No parallelism hints in dry-run output.

### Output format depends on Makefile

| Makefile pattern | Output |
|------------------|--------|
| Single command per recipe line | One per line (ideal) |
| Backslash continuation | Multi-line with `\` preserved |
| Semicolons | Single line |
| For loops | Single line |

Standard pattern rules (`%.o: %.c`) produce one command per line.

## How Scripts Would Use This

```bash
# Get commands for stale files
commands=$(make -n target 2>/dev/null)

# Separate compile and link stages
compile_cmds=$(echo "$commands" | grep '\-c')
link_cmd=$(echo "$commands" | grep -v '\-c')

# Run compiles in parallel
echo "$compile_cmds" | parallel

# Run link
eval "$link_cmd"

# Report structured JSON
```

## Trade-offs

**Gains:**
- Makefile stays standard (humans, CI use `make` normally)
- Scripts get clean, predictable input
- Incremental builds preserved
- Output format controlled by scripts, not Make
- Easy to return structured JSON for LLMs

**Loses:**
- Make's built-in parallelism (but recoverable via `parallel` or `xargs -P`)
- Must infer parallelism from command structure

**Parallelism recovery:**

All `-c` (compile) commands are independent. Link depends on all objects. Two stages:

```bash
make -n | grep '\-c' | parallel      # compile stage
make -n | grep -v '\-c' | sh         # link stage
```

**Ruby's GIL is not a problem:**

The GIL only affects Ruby code executing in parallel, not external processes. When Ruby spawns `gcc` subprocesses, the GIL is released while waiting. Compilation runs in separate OS processes, outside Ruby entirely.

```ruby
# Threads work fine - GIL released during subprocess waits
threads = files.map do |f|
  Thread.new { system("gcc -c #{f}") }
end
threads.each(&:join)

# Or use Process.spawn for more control
pids = files.map { |f| Process.spawn("gcc -c #{f}") }
pids.each { |pid| Process.wait(pid) }
```

## Considerations

1. **Makefile discipline** - Avoid backslash continuations in rules we parse
2. **CI compatibility** - CI can still use `make` directly
3. **Hybrid approach** - `make -n` for LLM tooling, `make -j` for human speed
4. **Error handling** - Script must capture and format compiler errors from actual execution

## Open Questions

- Worth the complexity vs current `check-*` approach?
- How much token savings in practice?
- Does this generalize beyond C builds?
