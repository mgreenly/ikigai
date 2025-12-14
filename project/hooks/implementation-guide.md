# Implementing Claude Code-Compatible Hooks in Ikigai

This guide covers implementing the Claude Code hook system in ikigai.

## Overview

Claude Code hooks provide an event-driven extension mechanism that allows external scripts to react to session lifecycle events. Implementing this in ikigai will enable users to customize and extend behavior without modifying ikigai's core.

## Architecture Components

### 1. Hook Configuration Loader

Load and parse hooks from `.claude/settings.json`:

```typescript
interface HookConfig {
  type: 'command';
  command: string;
  timeout: number;
}

interface HookMatcher {
  matcher: string;
  hooks: HookConfig[];
}

interface Settings {
  hooks?: {
    SessionStart?: HookMatcher[];
    UserPromptSubmit?: HookMatcher[];
    Stop?: HookMatcher[];
  };
}

function loadSettings(projectRoot: string): Settings {
  const settingsPath = path.join(projectRoot, '.claude', 'settings.json');
  if (!fs.existsSync(settingsPath)) {
    return {};
  }
  return JSON.parse(fs.readFileSync(settingsPath, 'utf-8'));
}
```

### 2. Hook Executor

Execute hooks with JSON input via stdin:

```typescript
import { spawn } from 'child_process';

interface HookExecutionResult {
  success: boolean;
  error?: string;
  stdout?: string;
  stderr?: string;
}

async function executeHook(
  hook: HookConfig,
  data: object,
  projectRoot: string
): Promise<HookExecutionResult> {
  return new Promise((resolve) => {
    const hookPath = path.resolve(projectRoot, hook.command);

    // Check if hook exists and is executable
    if (!fs.existsSync(hookPath)) {
      resolve({
        success: false,
        error: `Hook not found: ${hook.command}`
      });
      return;
    }

    const child = spawn(hookPath, [], {
      cwd: projectRoot,
      timeout: hook.timeout || 30000,
      stdio: ['pipe', 'pipe', 'pipe']
    });

    let stdout = '';
    let stderr = '';

    child.stdout.on('data', (data) => {
      stdout += data.toString();
    });

    child.stderr.on('data', (data) => {
      stderr += data.toString();
    });

    // Send JSON data to stdin
    child.stdin.write(JSON.stringify(data));
    child.stdin.end();

    child.on('close', (code) => {
      if (code === 0) {
        resolve({ success: true, stdout, stderr });
      } else {
        resolve({
          success: false,
          error: `Failed with exit code ${code}`,
          stdout,
          stderr
        });
      }
    });

    child.on('error', (err) => {
      resolve({
        success: false,
        error: err.message
      });
    });
  });
}
```

### 3. Hook Trigger Points

Integrate hooks at key lifecycle events:

```typescript
class IkigaiSession {
  private settings: Settings;
  private projectRoot: string;
  private sessionId: string;
  private transcriptPath: string;

  async start() {
    // Generate session ID
    this.sessionId = crypto.randomUUID();

    // Initialize transcript
    this.transcriptPath = this.initializeTranscript();

    // Trigger SessionStart hooks
    await this.triggerHooks('SessionStart', {
      source: 'cli',
      session_id: this.sessionId
    });
  }

  async handleUserPrompt(prompt: string) {
    // Trigger UserPromptSubmit hooks
    await this.triggerHooks('UserPromptSubmit', {
      prompt
    });

    // Add to transcript
    this.appendToTranscript({ role: 'user', content: prompt });

    // Process prompt...
    const response = await this.processPrompt(prompt);

    // Add to transcript
    this.appendToTranscript({ role: 'assistant', content: response });

    // Trigger Stop hooks
    await this.triggerHooks('Stop', {
      transcript_path: this.transcriptPath
    });

    return response;
  }

  private async triggerHooks(hookType: string, data: object) {
    const hooks = this.settings.hooks?.[hookType];
    if (!hooks || hooks.length === 0) {
      return;
    }

    for (const matcher of hooks) {
      // Check matcher (for now, only support "*")
      if (matcher.matcher !== '*') {
        continue;
      }

      for (const hook of matcher.hooks) {
        const result = await executeHook(hook, data, this.projectRoot);

        if (!result.success) {
          // Non-blocking: log error but continue
          console.error(
            `Hook error (${hook.command}): ${result.error}`
          );
        }
      }
    }
  }
}
```

### 4. Transcript Management

Maintain conversation transcript in JSONL format:

```typescript
interface Message {
  role: 'user' | 'assistant';
  content: string | ContentBlock[];
}

interface ContentBlock {
  type: string;
  text?: string;
  [key: string]: any;
}

class TranscriptManager {
  private transcriptPath: string;
  private writeStream: fs.WriteStream;

  constructor(sessionId: string, sessionDir: string) {
    this.transcriptPath = path.join(
      sessionDir,
      `transcript-${sessionId}.jsonl`
    );
    this.writeStream = fs.createWriteStream(this.transcriptPath, {
      flags: 'a'
    });
  }

  append(message: Message) {
    this.writeStream.write(JSON.stringify(message) + '\n');
  }

  close() {
    this.writeStream.end();
  }

  getPath(): string {
    return this.transcriptPath;
  }
}
```

---

## Implementation Checklist

### Phase 1: Core Infrastructure

- [ ] Create settings loader
- [ ] Implement hook executor with timeout
- [ ] Add stdin JSON passing
- [ ] Implement non-blocking error handling
- [ ] Add logging for hook execution

### Phase 2: Hook Triggers

- [ ] SessionStart trigger on session initialization
- [ ] UserPromptSubmit trigger before prompt processing
- [ ] Stop trigger after response completion
- [ ] Transcript JSONL writer

### Phase 3: Configuration

- [ ] Support matcher patterns (start with "*" wildcard)
- [ ] Validate hook configuration on load
- [ ] Handle missing/malformed settings.json
- [ ] Support both relative and absolute hook paths

### Phase 4: Error Handling

- [ ] Non-blocking hook failures
- [ ] Timeout enforcement
- [ ] Permission error handling
- [ ] User-friendly error messages

### Phase 5: Testing

- [ ] Unit tests for hook executor
- [ ] Integration tests for each hook type
- [ ] Test timeout behavior
- [ ] Test error conditions
- [ ] Test transcript format

---

## Key Design Decisions

### Non-Blocking Execution

Hooks must never block or crash the main session:
- Always use timeouts
- Catch and log all errors
- Continue even if hooks fail
- Provide clear error messages to users

### Security Considerations

1. **Path Validation**: Ensure hook paths don't escape project directory
2. **Permission Checks**: Verify hooks are executable
3. **Timeout Enforcement**: Kill runaway hooks
4. **Input Sanitization**: Validate JSON data before passing to hooks
5. **No Arbitrary Code**: Only execute configured hooks

```typescript
function validateHookPath(hookPath: string, projectRoot: string): boolean {
  const resolved = path.resolve(projectRoot, hookPath);

  // Ensure path is within project
  if (!resolved.startsWith(projectRoot)) {
    return false;
  }

  // Check exists
  if (!fs.existsSync(resolved)) {
    return false;
  }

  // Check executable
  try {
    fs.accessSync(resolved, fs.constants.X_OK);
    return true;
  } catch {
    return false;
  }
}
```

### Performance

- **Lazy Load**: Only load settings once at session start
- **Async Execution**: Don't block on hook completion
- **Timeout Defaults**: Reasonable defaults (5-30s)
- **Stream Transcript**: Write incrementally, not all at once

### Compatibility

Match Claude Code behavior exactly:
- Same JSON schemas
- Same hook types and names
- Same error handling (non-blocking)
- Same transcript format (JSONL)

---

## Testing Strategy

### Unit Tests

```typescript
describe('Hook Executor', () => {
  it('should execute hook with JSON stdin', async () => {
    const hook = {
      type: 'command',
      command: 'test/fixtures/echo_stdin.sh',
      timeout: 1000
    };

    const data = { test: 'value' };
    const result = await executeHook(hook, data, '/project');

    expect(result.success).toBe(true);
    expect(result.stdout).toContain(JSON.stringify(data));
  });

  it('should timeout long-running hooks', async () => {
    const hook = {
      type: 'command',
      command: 'test/fixtures/sleep_forever.sh',
      timeout: 100
    };

    const result = await executeHook(hook, {}, '/project');

    expect(result.success).toBe(false);
    expect(result.error).toContain('timeout');
  });

  it('should handle missing hooks gracefully', async () => {
    const hook = {
      type: 'command',
      command: 'nonexistent.sh',
      timeout: 1000
    };

    const result = await executeHook(hook, {}, '/project');

    expect(result.success).toBe(false);
    expect(result.error).toContain('not found');
  });
});
```

### Integration Tests

```typescript
describe('Hook Integration', () => {
  it('should trigger SessionStart hooks', async () => {
    const session = new IkigaiSession('/project');
    await session.start();

    // Verify hook was called (check log file, etc.)
    const log = fs.readFileSync('/project/test.log', 'utf-8');
    expect(log).toContain('Session started');
  });

  it('should pass correct data to UserPromptSubmit', async () => {
    const session = new IkigaiSession('/project');
    await session.start();

    await session.handleUserPrompt('test prompt');

    const log = fs.readFileSync('/project/test.log', 'utf-8');
    expect(log).toContain('test prompt');
  });
});
```

---

## Migration Path

For existing ikigai users:

1. **Backward Compatible**: Hooks are optional, no changes required
2. **Documentation**: Provide examples and templates
3. **Default Hooks**: Ship with basic logging hooks disabled by default
4. **Migration Script**: Help users move existing logging to hooks

---

## Future Enhancements

### Advanced Matchers

Beyond "*" wildcard:
- Regex patterns on prompt content
- Conditional execution based on context
- User-defined matcher functions

### Additional Hook Types

- **PreResponse**: Before Claude generates response
- **Error**: When errors occur
- **SessionEnd**: When session terminates

### Hook Capabilities

- **Bidirectional**: Allow hooks to modify data
- **Blocking**: Optional blocking mode for validation
- **Async**: Webhook/HTTP endpoints as hooks

### Performance

- **Parallel Execution**: Run independent hooks concurrently
- **Caching**: Cache hook results for performance
- **Batching**: Batch multiple events to reduce overhead
