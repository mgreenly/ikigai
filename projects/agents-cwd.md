# Agent Working Directory (`agent.cwd`)

## Summary

Add a new per-agent property, `agent.cwd`, to give each agent its own working directory. The goal is for different agents to appear to exist and operate in different parts of the filesystem, even though they run inside the same ikigai process tree. Every external tool call made on behalf of an agent should begin execution in that agent's `cwd`.

This should be implemented by treating `agent.cwd` as part of the agent's durable identity and execution context, not as a temporary UI setting.

## Desired Behavior

- Each agent has a current working directory stored as `agent.cwd`.
- All external tool calls for that agent start in `agent.cwd`.
- Child agents inherit the parent agent's `cwd` by default unless explicitly overridden later.
- Restored agents keep the same `cwd` they had before shutdown.
- One agent changing directories must not affect any other agent.

## Why This Design

Changing the main process working directory is not acceptable because multiple agents may be active concurrently. If one shared process cwd were mutated globally, agents would interfere with each other.

Instead, the cwd must be applied only at tool execution time, inside the forked child process that runs the external tool. That gives each tool invocation the correct filesystem perspective while keeping agents isolated from one another.

## Implementation Shape

### 1. Agent model

Add `cwd` to `ik_agent_ctx_t` so it is available anywhere agent execution decisions are made.

### 2. Persistence

Add a `cwd` column to the agents table and thread it through:

- agent insert
- agent lookup
- agent restore
- agent list/query row structs as needed

This ensures agent cwd survives restart and replay.

### 3. Initialization

Set the root agent cwd at creation time from the process startup directory or chosen project root. On fork, copy the parent's cwd into the child agent.

### 4. Tool execution

Thread `agent.cwd` through the tool execution stack:

- `ik_tool_execute_from_registry(...)`
- `ik_tool_external_exec(...)`

Then, in `apps/ikigai/tool_external.c`, in the child process after pipe setup and before `execl(...)`, call:

```c
chdir(agent_cwd);
```

If `chdir()` fails, exit nonzero so the parent reports tool execution failure.

## Important Constraints

- Do not change the parent process cwd.
- Use per-child `chdir()` only.
- Prefer absolute paths for tool executables so changing cwd does not break execution.
- Internal tools may also need to respect `agent.cwd` conceptually, but the critical path is external tool execution.

## Result

With this change, each agent gets a stable filesystem point of view. Tool calls naturally run "from" the agent's directory, child agents inherit location from parents, and concurrent agents can work in different directories safely.
