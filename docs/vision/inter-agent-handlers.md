# Inter-Agent Communication: Message Handlers

This document covers the message handler system for automated inter-agent responses.

See also:
- [inter-agent-communication.md](inter-agent-communication.md) - Overview and index
- [inter-agent-core.md](inter-agent-core.md) - Core concepts and messaging
- [inter-agent-delivery.md](inter-agent-delivery.md) - Message delivery mechanics
- [inter-agent-implementation.md](inter-agent-implementation.md) - Use cases and implementation

## Message Handlers

The most powerful feature: **agents can define handlers that automatically respond to messages**.

### Handler Concept

A handler is:
- A **prompt** that gets invoked when a message matches filters
- **Filters** that determine when it runs
- **Actions** it can take (respond, create memory doc, execute command, etc.)
- **Context** it has access to (sender info, message content, current state)

Think of it like:
- Email filters that auto-respond
- GitHub Actions that trigger on events
- Unix mail filters + procmail
- Event-driven microservices

### Defining Handlers

Handlers could be defined in a configuration language:

```yaml
# .agents/handlers/oauth-impl.yml
handlers:
  - name: research-complete
    description: "Auto-respond when research agent completes OAuth research"

    filters:
      from_agent: research-*           # Any agent starting with "research-"
      tags: [oauth, research-complete]  # Must have both tags
      project: ikigai                   # Only for this project

    prompt: |
      You received a research completion message from {sender_name}.

      Message: {message_content}

      Review the referenced memory documents and:
      1. Acknowledge receipt
      2. Summarize key findings
      3. Create implementation plan based on research
      4. Send response back to {sender_name}

    actions:
      - respond: true              # Send reply to sender
      - notify_user: true          # Show notification in UI
      - log: "Received research completion"

  - name: test-failure
    description: "Handle test failure notifications"

    filters:
      from_agent: testing-*
      tags: [test-failure]
      priority: [high, urgent]

    prompt: |
      A test agent reported failures: {message_content}

      1. Analyze the test failures
      2. If they're in your code, create a fix plan
      3. If they're not your responsibility, route to correct agent
      4. Respond with analysis and action plan

    actions:
      - respond: true
      - tag: bug-fix               # Tag conversation from this point
      - notify_user: true

  - name: coordinator-request
    description: "Handle requests from root agent"

    filters:
      from_agent: main             # Only from root agent
      project: ikigai

    prompt: |
      The root agent sent you a message: {message_content}

      This is a coordination message. Respond with:
      1. Current status of your work
      2. Any blockers
      3. Estimated completion (if applicable)
      4. Questions or needs

    actions:
      - respond: true
      - priority: high
```

### Handler Context

When a handler prompt runs, it has access to rich context:

```python
# Template variables available in handler prompts
{
  # Message details
  "message_id": 12345,
  "message_content": "OAuth research complete...",
  "message_tags": ["oauth", "research-complete"],
  "message_priority": "normal",

  # Sender information
  "sender_name": "research-agent",
  "sender_id": 42,
  "sender_project": "ikigai",
  "sender_focus": "researching OAuth patterns",
  "sender_tags": ["research", "oauth"],

  # Receiver (this agent) information
  "my_name": "oauth-impl",
  "my_focus": "implementing OAuth 2.0",
  "my_current_branch": "feature/oauth",
  "my_tags": ["oauth", "implementation"],

  # Conversation context
  "recent_messages": [...],  # Last N messages in this agent's conversation
  "memory_docs": [...],       # Referenced memory docs

  # Project context
  "project": "ikigai",
  "git_status": {...},
}
```

### Handler Actions

Handlers can take various actions:

**Communication:**
- `respond: true` - Send reply to sender
- `forward: agent-name` - Forward to another agent
- `broadcast: [tag-list]` - Broadcast to agents with tags

**State Changes:**
- `tag: bug-fix` - Add tag to conversation
- `focus: "Fixing test failures"` - Update focus
- `priority: high` - Change priority

**Automation:**
- `create_memory: title` - Create memory document from response
- `execute: /command` - Run a slash command
- `mark: before-fix` - Create checkpoint

**Notification:**
- `notify_user: true` - Show in UI (default for most handlers)
- `log: "message"` - Log to system log
- `silent: true` - Process without notifying user

## Filter Expressions

Handlers need powerful filtering to determine when they should run.

### Simple Filters

```yaml
filters:
  from_agent: research-agent        # Exact match
  to_agent: oauth-impl              # If sent specifically to this agent
  tags: [oauth, complete]           # Must have ALL these tags
  priority: high                    # Exact priority
  project: ikigai                   # Only this project
```

### Pattern Matching

```yaml
filters:
  from_agent: "research-*"          # Wildcard
  tags: ["oauth-*"]                 # Tag patterns
  content: "/urgent|blocker/i"      # Regex on content
```

### Boolean Logic

```yaml
filters:
  any_of:
    - from_agent: research-agent
    - from_agent: testing-agent

  all_of:
    - tags: [oauth]
    - priority: [high, urgent]

  not:
    - from_agent: main              # Not from root agent
```

### Compound Filters

```yaml
filters:
  # From research agents OR testing agents
  # AND tagged with oauth
  # AND high priority
  # AND NOT from the user 'bot'

  any_of:
    - from_agent: "research-*"
    - from_agent: "testing-*"

  all_of:
    - tags: [oauth]
    - priority: [high, urgent]

  not:
    - from_user: bot
```

## Handler Locations

Where are handlers defined?

### Option 1: Per-Agent Files

```
.agents/
  handlers/
    oauth-impl.yml
    testing-agent.yml
    main.yml
```

Each agent has its own handler file.

### Option 2: Global + Per-Agent

```
.agents/
  handlers/
    global.yml           # Handlers for all agents
    oauth-impl.yml       # Agent-specific overrides
```

Global handlers apply to all agents, can be overridden per-agent.

### Option 3: Database-Stored

Handlers stored in database, editable via commands:

```bash
/handler new research-complete
# Opens editor for handler definition

/handler list
# Shows all handlers for current agent

/handler enable research-complete
/handler disable research-complete
```

**Recommended:** Start with file-based (Option 1), evolve to database (Option 3).

## Handler Development Experience

### Testing Handlers

```bash
# Test a handler with mock message
/handler test research-complete --from=research-agent --message="Test message" --tags=oauth,complete

# Shows what the handler would do without actually doing it
```

### Handler Debugging

```bash
# Enable handler debug mode
/handler debug on

# All handler invocations show:
# - Which filters matched
# - Template variable values
# - Actions taken
# - Full prompt sent to LLM
```

### Handler Dry Run

```bash
# Run handlers in dry-run mode (no actions executed)
/handler dry-run on

# Handlers process messages but don't take actions
# Useful for testing new handlers
```

## Security and Privacy

### Handler Permissions

Handlers should require opt-in for sensitive actions:

```yaml
handlers:
  - name: auto-committer
    filters:
      tags: [ready-to-commit]
    prompt: "Review changes and create commit"
    actions:
      - execute: "/git commit"    # Requires permission!
    permissions:
      - git_commit                # User must grant this
```

User grants permissions:
```bash
/handler grant oauth-impl git_commit
/handler revoke oauth-impl git_commit
```

### Message Privacy

Messages should respect project boundaries:

- Can't send messages to agents in other users' projects (unless shared)
- Can't read messages from other users (unless shared project)
- Sensitive messages can be marked private (not in memory/RAG)

### Rate Limiting

Prevent handler spam:

```yaml
handlers:
  - name: auto-responder
    rate_limit:
      max_invocations: 10
      per: hour
```

## Handler Prompt Patterns

Handler prompts can instruct the agent to **do actual work**, not just respond with text.

### Work Request Handler

```yaml
prompt: |
  Work request from {sender_name}: {message_content}

  This is a work request. Complete the requested work:

  1. Review the request and any referenced memory docs
  2. Perform the implementation work (write code, tests, etc.)
  3. Run tests to verify your work
  4. Send results back to {sender_name}
  5. If you encounter blockers, escalate to root agent

  Treat this as if the user directly requested this work.
```

**Example:** Research agent sends "Implement OAuth token refresh using patterns in #mem-oauth/patterns". Implementation agent receives this, reads the memory doc, writes the code, runs tests, and reports back - all automatically.

### Test and Report

```yaml
prompt: |
  Test request from {sender_name} for branch: {message_metadata.branch}

  1. Check out the branch: {message_metadata.branch}
  2. Run the full test suite
  3. If tests pass:
     - Send success message to {sender_name}
     - Tag the message as ready-for-review
  4. If tests fail:
     - Analyze the failures
     - Create detailed failure report
     - Send to {sender_name} with fixes needed
  5. Return to your original branch when done
```

**Example:** Implementation agent completes feature, sends message to testing agent. Testing agent automatically checks out branch, runs tests, reports results.

### Code Review Handler

```yaml
prompt: |
  Code review request from {sender_name}: {message_content}

  Branch to review: {message_metadata.branch}
  Files changed: {message_metadata.files}

  Perform a thorough code review:

  1. Check out the branch
  2. Review all changed files
  3. Check for:
     - Security vulnerabilities
     - Performance issues
     - Code style violations
     - Test coverage gaps
     - Documentation needs
  4. Create detailed review in memory doc
  5. Send review to {sender_name}
  6. If serious issues, escalate to root agent
```

**Example:** Developer completes feature, agent sends to review agent. Review agent automatically performs full code review and reports findings.

### Acknowledge and Analyze

```yaml
prompt: |
  Message from {sender_name}: {message_content}

  1. Acknowledge receipt
  2. Analyze the message content
  3. Determine if action is needed
  4. If action needed: perform it
  5. Send response with your analysis and what you did
```

### Delegate Work

```yaml
prompt: |
  Message from {sender_name}: {message_content}

  This work should be handled by a different agent.

  1. Determine which agent is best suited
  2. Create new agent if needed (e.g., /agent new testing-oauth)
  3. Forward the work request to them with full context
  4. Respond to {sender_name} that you've delegated to <agent-name>
  5. Monitor progress and relay updates
```

### Conditional Action

```yaml
prompt: |
  Test results from {sender_name}: {message_content}

  IF tests passed:
    1. Create git commit with test results
    2. Push to remote
    3. Create pull request
    4. Send PR link to {sender_name} and review-agent
    5. Tag message as ready-for-review

  IF tests failed:
    1. Analyze the failures in detail
    2. Create fixes for the failing tests
    3. Run tests again
    4. If still failing, escalate to {sender_name}
    5. If now passing, send success message
```

### Gather Context and Execute

```yaml
prompt: |
  Request from {sender_name}: {message_content}

  This is a work request. Before executing:

  1. Review recent conversation history
  2. Load and review any referenced memory docs
  3. Check current git status and branch
  4. Understand the full context

  Then perform the requested work:
  - Write/modify code as requested
  - Add tests for changes
  - Run tests to verify
  - Create commits if appropriate
  - Document in memory if significant

  Finally, send comprehensive response to {sender_name}:
  - What you did
  - Test results
  - Any issues or blockers
  - Next steps if any
```

### Autonomous Research and Implement

```yaml
prompt: |
  Feature request from {sender_name}: {message_content}

  You are being asked to autonomously implement this feature.

  Phase 1 - Research:
  1. Research the feature requirements
  2. Investigate existing code and patterns
  3. Create implementation plan
  4. Document plan in memory doc

  Phase 2 - Implementation:
  1. Create feature branch
  2. Implement the feature following the plan
  3. Write comprehensive tests
  4. Ensure tests pass

  Phase 3 - Review:
  1. Self-review your implementation
  2. Check for issues
  3. Create PR

  Phase 4 - Report:
  1. Send detailed report to {sender_name}
  2. Include: memory doc, branch, PR link, test results
  3. Ask for feedback or approval
```

**Example:** Root agent sends "Implement OAuth refresh token handling" to helper agent. Helper agent autonomously researches, plans, implements, tests, and reports back - minimal human intervention needed.
