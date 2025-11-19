# Inter-Agent Communication

**Core Innovation:** Agents can discover each other, communicate asynchronously, and automate responses through filtered message handlers.

## The Concept

Agents are no longer just isolated conversations sharing a memory store. They become **active participants** that can:
- Advertise their identity and capabilities
- Send messages to other agents
- Respond to messages automatically based on filters
- Coordinate work without constant human intervention

Think of it as **agent-to-agent RPC** + **event-driven automation** + **filtered subscriptions**.

## Agent Business Cards

Every agent has an **identity** that other agents can discover and query.

### Core Identity

```yaml
name: oauth-impl
project: ikigai
role: Feature Implementation
status: active
created_at: 2024-01-15T10:30:00Z
current_focus: "Implementing OAuth 2.0 authentication"
tags:
  - oauth
  - authentication
  - security
capabilities:
  - code_implementation
  - testing
  - git_operations
location:
  branch: feature/oauth
  worktree: .worktrees/oauth-impl
  path: /home/mgreenly/projects/ikigai/main/.worktrees/oauth-impl
owner:
  user: mgreenly
  machine: workstation
```

### Querying Agents

```bash
# List all agents
/agents list

# List agents by project
/agents list --project=ikigai

# List agents by capability
/agents list --capability=testing

# Show specific agent's card
/agents show oauth-impl

# Show all agents working on authentication
/agents find --tag=authentication
```

### Agent Roles (Optional Categorization)

Agents can declare their role to help with discovery:

- **Root** - Main coordinating agent
- **Feature** - Implementing a specific feature
- **Research** - Gathering information, creating memory docs
- **Testing** - Writing/running tests
- **Review** - Code review and quality checks
- **Refactor** - Improving existing code
- **Custom** - User-defined roles

## Inter-Agent Messaging

Agents can send messages to other agents. Messages are **asynchronous** and stored in the database.

### Sending Messages

From within a conversation:

```bash
# Send message to specific agent
/send oauth-impl "OAuth research complete. Memory doc at #mem-oauth/patterns ready for your implementation."

# Send to multiple agents
/send testing,review "Feature implementation complete. Ready for testing and review."

# Send with metadata
/send oauth-impl --priority=high --tag=blocker "Need decision on refresh token storage before proceeding."
```

Or agents can send messages autonomously (when prompted by user):

```
[You] Research OAuth patterns and notify the oauth-impl agent when done.
[Assistant researches, creates memory doc, sends message to oauth-impl agent]
Sent message to oauth-impl: "Research complete. See #mem-oauth/patterns"
```

### Receiving Messages

**Key Principle:** Messages from other agents are treated like user input - they can cause the receiving agent to do actual work.

When a message arrives, it enters the agent's conversation context just like user input:

```
[System] 📨 Message from research-agent:
         OAuth research complete. Memory doc at #mem-oauth/patterns ready for implementation.
         Tags: oauth, research-complete
         Priority: normal

[Assistant] I received the OAuth research from research-agent. Let me review #mem-oauth/patterns and start implementing...

[Assistant reads memory doc, creates implementation plan, writes code, runs tests...]
```

**The agent treats the message as a prompt and acts on it**, potentially:
- Writing code
- Running tests
- Creating files
- Making git commits
- Creating memory docs
- Sending messages to other agents
- Making decisions
- Any other work it would do for a user

This is **agent-driven automation** - agents can truly delegate work to each other.

### Message Structure

```c
typedef struct ik_agent_message_t {
    int64_t id;
    int64_t from_agent_id;      // Sending agent
    int64_t to_agent_id;        // Receiving agent (or NULL for broadcast)
    char *subject;              // Brief subject line
    char *content;              // Full message content
    char **tags;                // Message tags for filtering
    char *priority;             // "low", "normal", "high", "urgent"
    timestamp_t sent_at;
    timestamp_t read_at;        // NULL if unread
    char *in_reply_to;          // NULL or message_id being replied to

    // Metadata
    char *project;              // Scope to project
    json_t *metadata;           // Arbitrary structured data
} ik_agent_message_t;
```

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

## Use Cases

### 1. Research → Implementation Handoff

**Scenario:** User asks root agent to implement OAuth. Root agent delegates.

```bash
# In root agent (main)
[You] Implement OAuth 2.0 authentication

[Assistant] I'll coordinate this across multiple agents:
1. Creating research agent to gather OAuth patterns
2. Creating implementation agent to build the feature

/agent new oauth-research
[You] Research OAuth 2.0 patterns, best practices, and security considerations.
      Create memory doc when done and notify oauth-impl agent.

/agent new oauth-impl --worktree
[You] Wait for research from oauth-research agent, then implement OAuth 2.0.

# oauth-research completes and sends message
# oauth-impl has handler that auto-responds, acknowledges, creates plan
# oauth-impl notifies user "Research received, starting implementation"
```

### 2. Continuous Testing Agent

**Scenario:** A testing agent monitors implementation agents and runs tests.

```yaml
# testing-agent handler
handlers:
  - name: code-committed
    filters:
      tags: [code-complete, ready-for-test]
    prompt: |
      {sender_name} reports code completion: {message_content}

      1. Switch to their worktree
      2. Run the test suite
      3. Report results back to them
      4. If failures, include details
    actions:
      - respond: true
      - notify_user: true
```

Implementation agent sends message when done:
```bash
/send testing-agent --tag=ready-for-test "OAuth implementation complete in feature/oauth branch"
```

Testing agent automatically:
1. Receives message
2. Handler fires
3. Runs tests
4. Sends results back

### 3. Code Review Workflow

**Scenario:** Review agent automatically reviews PRs.

```yaml
# review-agent handler
handlers:
  - name: review-request
    filters:
      tags: [review-request]
      from_agent: "*-impl"      # Any implementation agent
    prompt: |
      Review request from {sender_name}:
      {message_content}

      1. Check out their branch
      2. Review code changes
      3. Check for security issues, best practices
      4. Provide detailed feedback
      5. Approve or request changes
    actions:
      - respond: true
      - create_memory: "review-{sender_name}-{timestamp}"
```

### 4. Blocker Escalation

**Scenario:** Helper agents escalate blockers to root agent.

```yaml
# main (root) agent handler
handlers:
  - name: blocker-escalation
    filters:
      tags: [blocker]
      priority: [high, urgent]
    prompt: |
      BLOCKER from {sender_name}:
      {message_content}

      1. Understand the blocker
      2. Provide solution or decision
      3. Unblock the agent so they can continue
    actions:
      - respond: true
      - notify_user: urgent      # Interrupt user
      - priority: urgent
```

Any agent can escalate:
```bash
/send main --priority=urgent --tag=blocker "Need decision: store refresh tokens in DB or filesystem?"
```

### 5. Cross-Project Collaboration

**Scenario:** Agent in project-a needs info from project-b.

```bash
# In project-a agent
/send @project-b/main "Do you have a memory doc about OAuth patterns? Need to implement similar feature."

# project-b agent has handler that searches memory docs and responds
# Response: "Yes! See #mem-oauth/patterns-v2. Also #mem-oauth/security-checklist"
```

## Broadcast Messages

In addition to point-to-point messages, agents can broadcast to multiple recipients.

### Topic-Based Broadcast

```bash
# Broadcast to all agents with specific tags
/broadcast --tags=oauth,implementation "Breaking: OAuth library upgraded to v3.0. Review #mem-oauth/migration-guide"

# All agents tagged with 'oauth' AND 'implementation' receive the message
```

### Project-Wide Broadcast

```bash
# Broadcast to all agents in a project
/broadcast --project=ikigai "FYI: Switching to new error handling pattern. See #mem-errors/new-pattern"
```

### Filtered Broadcast

Handlers can filter broadcasts just like direct messages:

```yaml
handlers:
  - name: security-alerts
    filters:
      is_broadcast: true
      tags: [security, alert]
    prompt: |
      Security alert broadcast: {message_content}

      1. Review the security issue
      2. Check if your code is affected
      3. Respond with impact assessment
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

## Agent Discovery

How do agents find out about each other?

### Active Agents

```bash
/agents active

# Shows currently running agents (with active client connections)
# vs agents that exist in database but no active client
```

### Agent Registry

```bash
/agents registry

# Shows all agents ever created in database
# With metadata: project, last active, message count, etc.
```

### Capability-Based Discovery

```bash
/agents find --capability=testing --project=ikigai

# Returns agents that:
# - Have 'testing' in their capabilities
# - Are associated with ikigai project
# - May be active or dormant
```

### Smart Discovery

```bash
[You] I need help with OAuth testing

[Assistant] Found these agents that might help:
- testing-oauth (active, project: ikigai, 23 messages)
- oauth-impl (active, knows about OAuth implementation)

Would you like me to send a message to one of them?
```

## Future Possibilities

### Handler Marketplace

Share handler definitions:

```bash
/handler install @community/test-automation
/handler install @mgreenly/security-review
```

### Agent Specialization

Agents could advertise specialized knowledge:

```yaml
specializations:
  - OAuth 2.0 implementation
  - PostgreSQL query optimization
  - React component patterns
```

Other agents can query: "Which agent knows about OAuth?"

### Negotiation Protocols

Agents could negotiate:

```
Agent A: "Can you review my PR?"
Agent B: "I'm busy with high-priority work. Can it wait 2 hours?"
Agent A: "Yes, I'll mark it for later review."
```

### Workflow Orchestration

Complex multi-agent workflows:

```yaml
workflow: feature-implementation
steps:
  - agent: research
    outputs: [memory-doc]
    notify: implementation

  - agent: implementation
    depends_on: [research]
    outputs: [code, tests]
    notify: testing

  - agent: testing
    depends_on: [implementation]
    outputs: [test-results]
    notify: [implementation, review]

  - agent: review
    depends_on: [testing]
    outputs: [approval]
    notify: main
```

### Agent Reputation

Track agent reliability:

- Response time
- Quality of responses
- Success rate of automated actions
- User satisfaction ratings

Use reputation for prioritization and discovery.

## Message Delivery Timing

**The Hard Problem:** What happens when a message arrives while an agent is actively conversing with the user?

This is the most challenging aspect of inter-agent messaging because it involves interrupting or coordinating with an active human interaction.

### Scenarios

**1. Agent is Idle (Easy Case)**

Agent has no active client or conversation is paused. Message arrives:
- Handler runs immediately
- Handler can take actions freely
- Response sent back to sender
- When user next interacts, they see: "While you were away, handled message from X"

**2. Agent is Active (Hard Case)**

User is actively conversing with agent. Message arrives mid-conversation:
- Can't just interrupt the conversation
- Can't ignore the message (defeats the purpose)
- Handler might want to take actions that affect conversation state
- User experience matters

### Possible Approaches

#### Approach 1: Queue Until Idle

**Mechanism:**
- Messages queue when agent is active
- Handler runs when agent becomes idle (user pauses/disconnects)
- Message shows as "pending" in status line: `[Agent: oauth-impl | 2 pending]`

**Pros:**
- Never interrupts user
- Clean separation between user interaction and inter-agent messaging
- Handlers run in quiet time

**Cons:**
- Urgent messages might be delayed
- No real-time collaboration between agents
- "Idle" is hard to detect (user thinking? went to lunch?)

#### Approach 2: Priority-Based Interruption

**Mechanism:**
- Low/normal priority: queue until idle
- High priority: show notification, don't interrupt
- Urgent priority: interrupt with prominent alert

```
[You] Implement OAuth token refresh

[Agent is typing response...]

[URGENT MESSAGE from testing-agent]
Critical security issue found in OAuth implementation!
See #mem-oauth/security-vuln-CVE-2024-1234

[Continue?] [Handle Message] [Read Details]
```

**Pros:**
- Urgent issues get immediate attention
- User stays in control
- Clear priority levels

**Cons:**
- Interruptions are disruptive
- User has to context-switch
- Might interrupt at bad time (mid-thought)

#### Approach 3: Background Handler Execution

**Mechanism:**
- Handler runs in background regardless of agent state
- Handler can take "safe" actions (respond, log, create memory doc)
- Handler CANNOT take "disruptive" actions (tag, focus, execute) during active conversation
- User sees summary at next natural break

```
[You] Implement OAuth token refresh

[Assistant provides implementation...]

[Assistant] (By the way, while we were talking, I received and handled 2 messages from testing-agent and research-agent. See /messages for details.)
```

**Pros:**
- Doesn't interrupt user
- Handlers still run in real-time
- Agents can collaborate in background

**Cons:**
- Limited actions during active conversation
- User might miss important messages
- Complex state management

#### Approach 4: Conversation Injection (Subtle)

**Mechanism:**
- Message arrives during active conversation
- If priority is high/urgent AND related to current work:
  - Agent naturally incorporates message into response
  - Attributed to sending agent

```
[You] Implement OAuth token refresh logic

[Assistant] I'm implementing the OAuth token refresh logic.

Actually, I just received a message from research-agent with relevant information: they've documented the recommended refresh token pattern in #mem-oauth/refresh-patterns. Let me incorporate that approach...

[Implementation incorporating the research...]
```

**Pros:**
- Seamless integration
- Relevant information arrives at perfect time
- Feels natural, not disruptive

**Cons:**
- Only works for contextually relevant messages
- Agent must decide relevance (complex)
- Could be confusing (where did this info come from?)

#### Approach 5: Micro-Interruption with Auto-Resume

**Mechanism:**
- Message arrives during conversation
- Brief notification appears: `[📨 from testing-agent: urgent test failure]`
- User can:
  - Ignore (handler queues for later)
  - Quick peek (show message without switching context)
  - Handle now (pause current conversation, handle message, auto-resume)

**Pros:**
- User awareness without full interruption
- Progressive disclosure (can ignore or dive in)
- Auto-resume preserves conversation flow

**Cons:**
- Still somewhat disruptive
- UI complexity
- Need to manage pause/resume state

### Recommended Hybrid Approach

Combine multiple strategies based on message metadata and agent state:

```python
def deliver_message(msg, agent):
    if agent.is_idle():
        # Easy case: run handler immediately
        run_handler(msg)

    elif msg.priority == "urgent":
        # Interrupt with option to handle now or later
        show_urgent_notification(msg)
        user_choice = prompt_user("[Handle Now] [Later]")
        if user_choice == "now":
            pause_conversation()
            run_handler(msg)
            resume_conversation()
        else:
            queue_message(msg)

    elif msg.priority == "high":
        # Non-intrusive notification
        show_notification_banner(msg)
        queue_message(msg)
        # Handler runs when agent becomes idle

    elif is_contextually_relevant(msg, agent.current_conversation):
        # Inject into conversation naturally
        inject_into_conversation(msg)

    else:
        # Normal/low priority: queue silently
        queue_message(msg)
        update_status_line(f"[{agent.name} | {len(queue)} pending]")
```

### Handler Action Restrictions

Some actions should be restricted based on agent state:

**Always Safe (Even During Active Conversation):**
- `respond: true` - Send reply to sender
- `log: message` - Log to system
- `create_memory: title` - Create memory document
- `notify_user: true` - Show non-intrusive notification

**Restricted During Active Conversation:**
- `tag: value` - Could confuse current conversation context
- `focus: value` - Changes agent's declared focus
- `execute: /command` - Might interfere with current work
- `mark: label` - Conversation checkpoint
- `priority: urgent` - Priority changes

**Never Automated (Always Requires User Confirmation):**
- `execute: /git commit` - Git operations
- `execute: /agent close` - Agent lifecycle
- `execute: /clear` - Context clearing

### Message Context Windows

Perhaps handlers have different "context windows" based on delivery timing:

**Idle Delivery:**
- Full context available
- Can access entire conversation history
- Can take any allowed action

**Active Delivery (Queued):**
- Limited context (message content + sender info only)
- Cannot access current conversation (it's in progress)
- Restricted actions

**Active Delivery (Urgent Interrupt):**
- Full context snapshot at time of interruption
- User must approve disruptive actions
- Can prompt user for decisions

### Agent Message Handling Policy

**Key Insight:** Each agent controls how it handles incoming messages via a policy flag.

#### Handling Modes

**1. `interrupt` - Interrupt Me**

Messages are delivered immediately, potentially interrupting active conversation:

```bash
/agent config message-handling interrupt

# Now when messages arrive:
# - Handler runs immediately
# - User sees notification/injection into conversation
# - Best for coordinating agents that need real-time updates
```

**Use case:** Root coordinating agent that needs to stay aware of all helper agents' status.

**2. `queue` - Wait Until Idle**

Messages queue until agent is idle (no active conversation for N seconds):

```bash
/agent config message-handling queue --idle-after=30s

# Messages queue silently
# When user stops typing for 30s, handlers run
# User sees: "Handled 3 messages while you were idle"
```

**Use case:** Implementation agents focused on deep work, don't want interruptions.

**3. `fork` - Fork Me**

Spawn a new agent instance to handle the message while original continues:

```bash
/agent config message-handling fork

# When message arrives:
# 1. System creates oauth-impl-fork-1 (temporary agent)
# 2. Fork handles the message with its own context
# 3. Fork can respond to sender
# 4. Fork auto-merges insights back to parent or auto-closes
# 5. Parent agent continues uninterrupted
```

**Use case:** Agents that need to handle messages without disrupting main work.

#### How Fork Works

When a message arrives to an agent with `fork` policy:

```
┌──────────────────────────────────────────┐
│  oauth-impl (original agent)             │
│  Status: Active conversation with user   │
│  Working on: Token refresh implementation│
└──────────────────────────────────────────┘
                    │
                    │ Message arrives from testing-agent
                    │
                    ▼
┌──────────────────────────────────────────┐
│  oauth-impl-fork-1 (spawned)             │
│  Status: Handling message                │
│  Context: Message + sender info          │
│  Lifetime: Temporary (auto-closes)       │
└──────────────────────────────────────────┘
                    │
                    │ 1. Runs handler
                    │ 2. Responds to sender
                    │ 3. Creates memory doc if needed
                    │ 4. Closes itself
                    ▼
           Message handled ✓
```

**Fork characteristics:**
- **Temporary**: Auto-closes after handling message (or after N minutes idle)
- **Isolated**: Own conversation context, doesn't pollute parent
- **No worktree**: Forks don't create git worktrees (unless needed)
- **Limited scope**: Can read parent's state but can't modify it directly
- **Communication**: Can send messages back to parent with findings

**Fork naming:**
- `oauth-impl-fork-1`, `oauth-impl-fork-2`, etc.
- Or semantic: `oauth-impl-testing-response`, `oauth-impl-review-handler`

**Fork outcomes:**

```yaml
# After handling message, fork can:
outcomes:
  - auto_close: true                          # Default: just close
  - create_memory: "findings-from-testing"    # Document findings
  - send_to_parent: "FYI: handled test results" # Notify parent
  - escalate: main                            # Forward to root agent
```

#### Hybrid Policies

Agents can have sophisticated policies:

```yaml
# .agents/oauth-impl.yml
message_handling:
  default: queue           # Default behavior

  # Override for specific senders
  from:
    main: interrupt        # Root agent can interrupt
    testing-*: fork        # Testing agents trigger fork

  # Override for specific priorities
  priority:
    urgent: interrupt      # Urgent always interrupts
    high: fork            # High priority forks
    normal: queue         # Normal queues

  # Override for specific tags
  tags:
    blocker: interrupt    # Blockers interrupt
    fyi: queue           # FYIs queue

  # Override for specific times
  schedule:
    weekdays:
      9am-5pm: fork       # During work hours, fork
      5pm-9am: queue      # Off hours, queue
    weekends: queue       # Weekends always queue
```

#### Fork vs Queue vs Interrupt Trade-offs

| Aspect | Interrupt | Queue | Fork |
|--------|-----------|-------|------|
| User disruption | High | None | None |
| Real-time response | Immediate | Delayed | Immediate |
| Context preservation | Breaks flow | Preserves flow | Preserves flow |
| Resource usage | Low | Low | Medium (new agent) |
| Complexity | Low | Low | High |
| Best for | Coordination | Deep work | Async collaboration |

### User Control

User can override agent's default policy:

```bash
# Override current agent's policy temporarily
/messages mode queue          # Queue until I say otherwise
/messages mode interrupt      # Allow interruptions
/messages mode fork           # Fork for all messages

# Reset to agent's configured default
/messages mode default

# Per-sender override (overrides agent policy)
/messages from testing-agent queue
/messages from main interrupt-urgent-only

# Global quiet mode (highest priority, overrides everything)
/messages quiet              # Queue everything until `/messages resume`
```

### Visual Indicators

Status line shows message state:

```
[Agent: oauth-impl | 2 pending] [Branch: feature/oauth]
                    ^^^^^^^^^^
                    2 unhandled messages

[Agent: oauth-impl | 📨] [Branch: feature/oauth]
                    ^^
                    New message notification

[Agent: oauth-impl | 🔴 urgent] [Branch: feature/oauth]
                    ^^^^^^^^^^^
                    Urgent message waiting
```

### The Ideal Experience

**For low-priority messages:**
1. Message arrives while user is working
2. Silent queue, tiny indicator in status line
3. When conversation pauses (user stops typing for 30s), handler runs
4. User sees: "Handled message from research-agent while you were thinking"

**For urgent messages:**
1. Message arrives with urgent priority
2. Non-modal notification appears: `[📨 URGENT from testing-agent: test failures]`
3. User can click to handle or dismiss
4. If dismissed, queues for later but stays prominent

**For contextually relevant messages:**
1. Message arrives that's directly relevant to current conversation topic
2. Agent naturally mentions: "I just got an update from X about this..."
3. Incorporates information seamlessly
4. Cites source for transparency

### Implementation Complexity

This is complex because it requires:
- **State tracking**: Is agent idle? Active? How long idle?
- **Priority evaluation**: What counts as urgent?
- **Relevance detection**: Is message related to current work?
- **UI coordination**: Notifications, status updates, interruption handling
- **Action restriction**: Prevent handlers from disrupting active conversations
- **User preferences**: Respect user's workflow preferences

**Recommendation:** Start simple (Approach 1: Queue Until Idle), evolve to hybrid as we learn usage patterns.

## Open Design Questions

### Coordination & Creation Patterns

**Agent Creation & Coordination:**
- Who creates helper agents? User manually, or main agent orchestrates?
- Can agents create sub-agents?
- If so, what's the ownership model? (agent tree? flat list?)
- Does user explicitly coordinate agents, or do agents coordinate themselves?
- How much autonomy should agents have in delegation?

**Discovery & Awareness:**
- How do agents discover each other?
- Should agents be able to query for other agents by capability/tag/role?
- Can agents see all agents, or only related ones (same project, etc.)?
- Should agents be aware of what other agents are working on?

### Message Flows

**Notification & Delivery:**
- How does a receiving agent know a message arrived?
  - Status line indicator only?
  - Active notification?
  - Depends on priority?
- When research agent finishes, how does impl agent find out?
  - Manual: user switches and tells impl agent
  - Message: research sends message to impl
  - Discovery: impl queries for completed research
  - Broadcast: research announces completion
- Should agents be able to wait/block for messages from specific agents?

**Message Processing:**
- `/messages next` vs `/messages wait` vs automatic processing?
- Can user preview message without agent processing it?
- Can user modify/edit incoming messages before agent processes them?
- What if agent is processing a message and another arrives?
- Should there be message priorities that affect queue ordering?

### User Interaction Models

**Control & Visibility:**
- How much should user see of inter-agent communication?
  - Everything (transparent)?
  - Only summaries (agent handled 3 messages)?
  - User configurable?
- When agents are working autonomously, how does user monitor/intervene?
- Can user intercept messages between agents?
- Should user be able to "overhear" agent conversations?

**Manual vs Automatic:**
- Start with everything manual (`/messages next`) and evolve?
- Or build automatic from start with manual override?
- What triggers the transition from manual to automatic?
- How does user configure automation preferences?

### Communication Patterns

**Message Types:**
- Point-to-point: agent A → agent B
- Broadcast: agent A → all agents with tag X
- Publish/subscribe: agents subscribe to topics
- Request/response: agent A asks, expects reply from agent B
- Fire-and-forget: agent A sends, doesn't care about reply
- Which patterns are essential vs nice-to-have?

**Agent Business Cards:**
- What information goes on a business card?
  - Name, role, capabilities, current focus, tags?
  - Status (active/idle/busy)?
  - Message preferences?
- How/when are business cards created?
- Can agents update their own business cards?
- How long do business cards persist (agent lifetime? forever?)

### Technical Concerns

**Synchronous vs Asynchronous:**
- Should agents ever be able to wait for responses?
- Or always async message passing?
- Can handlers be synchronous (block until response)?

**Message Threading:**
- Can messages be part of threads (like email)?
- Reply chains, conversations between agents?
- Or always independent messages?

**Handler Language & Definition:**
- YAML as shown in examples?
- Custom DSL?
- Python/Lua scripts?
- Just structured prompts?
- Database-stored or file-based?
- How does user edit/test handlers?

**Handler Execution Context:**
- Same LLM model as agent?
- Smaller/faster model for handlers?
- Different system prompt?
- Access to full agent context or limited?

**Message Persistence:**
- How long are inter-agent messages kept?
- Are they part of RAG corpus?
- Can they be searched like conversation history?
- Do they live in same messages table or separate?

**Broadcast Scaling:**
- What happens when broadcasting to 50 agents?
- Rate limiting?
- Priority queues?
- Should broadcasts be opt-in (subscribe) vs opt-out?

**Agent Lifecycle & Messages:**
- Can dormant agents (no active client) receive messages?
- Do handlers run even if agent has no active client?
- How to handle agent deletion with pending messages?
- What happens to messages sent to closed/deleted agents?

**Cross-User & Security:**
- Can agents from different users communicate?
- Shared project agents across users?
- Security implications of inter-agent messaging?
- Message authentication/verification?
- Can malicious agent spam others with messages?

### UX & Workflow

**Fork Mode:**
- How does fork actually work in practice?
- What context does fork inherit from parent?
- How does fork communicate findings back to parent?
- When does fork auto-close vs stay around?
- Can user switch to a fork and interact with it?

**Message Queue UI:**
- What does `/messages show` actually display?
- How are messages sorted (time? priority? sender?)
- Can user delete/archive messages?
- Can user mark messages as read/unread?
- Keyboard shortcuts for message navigation?

**Status Line:**
- What message indicators in status line?
  - Count: `| 3 msgs`
  - Priority: `| 🔴 urgent`
  - Sender: `| msg from testing`
- How much info before it's cluttered?
- Should status line be clickable to show messages?

### Future Considerations

**Agent Reputation:**
- Should system track agent reliability/quality?
- How is reputation calculated?
- Does reputation affect message routing/priority?
- User rating of agent responses?

**Workflow Orchestration:**
- Should there be explicit workflow definitions?
- Or emergent workflows from agent interactions?
- Can workflows be saved/reused?
- Template workflows for common patterns?

**Handler Marketplace:**
- Can users share handler definitions?
- Trusted handler sources?
- Versioning of handlers?
- Handler dependencies?

## Message Mechanics: Simple Tool-Based Approach

**Core insight:** Messages are delivered by injecting fake user input. Agents use tools to read/respond.

### How It Works

When a message arrives for an agent:

1. Message queues in database
2. Status line updates: `[Agent: oauth-impl | 1 msg]`
3. User decides when to process: `/messages next`
4. System injects fake user message into conversation:
   ```
   [System as User] You have a message from research-agent. Message #2345.
   ```
5. Agent uses tools to read and respond:
   ```
   [Assistant] Let me read that message.
   [Uses ReadMessage(2345)]
   [Message content: "OAuth patterns documented in #mem-oauth/patterns"]

   [Assistant] Great! I'll review the patterns and start implementing...
   [Reads memory doc, writes code, etc.]
   ```

### Message Tools

Agents have access to message tools (like file tools, git tools, etc.):

**`ReadMessage(message_id)`**
- Returns message content, sender info, metadata
- Like reading a file

**`SendMessage(to_agent, content, [metadata])`**
- Sends message to another agent
- Optional priority, tags, etc.

**`ListMessages([filter])`**
- Shows pending messages
- Can filter by sender, unread, priority

**`ReplyToMessage(message_id, content)`**
- Reply to specific message
- Creates message thread

### Example Flow

```
[oauth-impl agent, working on implementation]

[User runs: /messages next]

[System injects as user] You have a message from research-agent. Message #847.

[Assistant] Let me check that message.

[Calls ReadMessage(847)]
Tool result: {
  id: 847,
  from: "research-agent",
  to: "oauth-impl",
  content: "OAuth security patterns documented. See #mem-oauth/security for PKCE flow, token storage recommendations, and common vulnerabilities to avoid.",
  sent_at: "2 minutes ago",
  tags: ["oauth", "security", "research-complete"]
}

[Assistant] Excellent! The research agent has documented OAuth security patterns. Let me review #mem-oauth/security and incorporate these patterns into my implementation.

[Agent reads memory doc, updates code, adds security measures...]

[Assistant] I've incorporated the PKCE flow and secure token storage from the research. Let me send a reply confirming this.

[Calls SendMessage("research-agent", "Thanks for the security patterns. I've incorporated PKCE and secure storage into the implementation. See branch feature/oauth.")]

[Assistant] Done! I've updated the implementation with the security patterns and notified the research agent.
```

### User Commands

**Manual message processing:**
```bash
/messages next          # Process next queued message (injects fake user input)
/messages wait          # Process all queued messages sequentially
/messages show          # Display message queue without processing
/messages show 2345     # Show specific message content
```

**Sending messages:**
```bash
/send oauth-impl "Implementation ready for testing"
/send testing-agent --priority=high "Critical bug found, need retest"
```

Alternatively, user can tell current agent to send:
```bash
[You] Send a message to testing-agent that implementation is ready
[Agent uses SendMessage tool]
```

### No Complex Handler Infrastructure

This approach eliminates the need for:
- Handler YAML files
- Filter expressions
- Template variables
- Handler execution context
- Automatic handler invocation

Instead: **messages are just data, tools read them, agents act naturally**.

The sophistication comes from the agent's understanding and decision-making, not from infrastructure.

### Future Automation

Later, if automation is needed:
- Agent can be configured to auto-process certain messages
- System auto-injects "you have mail" for specific senders/priorities
- Agent still uses same tools, same flow
- Just happens without user typing `/messages next`

But start manual, evolve as needed.

## Implementation Philosophy: Manual First

**Start manual, evolve to automatic.**

Rather than building complex automatic handlers first, start with simple manual message processing and learn how it's actually used.

### Phase 1: Manual Message Mode

**Core principle:** Messages arrive and queue. Agent processes them only when user explicitly says so.

```bash
# Messages queue automatically, shown in status line
[Agent: oauth-impl | 3 msgs] [Branch: feature/oauth]

# User decides when to process
/messages next         # Process next message
/messages wait         # Process all queued messages
/messages show         # Show queued messages without processing

# Agent treats message like user input and acts on it
```

**Example flow:**

```
[oauth-impl agent]
[You] Implement token refresh

[Agent starts working...]

[Status line updates: oauth-impl | 1 msg]

[You finish current work, then:]
/messages next

[System] Message from research-agent (2 minutes ago):
         OAuth security patterns documented in #mem-oauth/security

[Assistant] I'll review the security patterns from research-agent...
[Reads memory doc, incorporates patterns, continues work...]
```

**Benefits:**
- User stays in control
- No surprising automatic behavior
- Learn actual usage patterns before automating
- Simple to implement and understand
- Easy to debug

### Phase 2: Opt-In Automation

Once manual mode works well, add **opt-in** automation:

```bash
# Agent responds automatically only when explicitly configured
/agent config auto-respond research-agent    # Auto-respond to this sender
/agent config auto-respond --tag=fyi         # Auto-respond to FYI messages

# Still defaults to manual queue for everything else
```

### Phase 3: Handler Patterns

After learning common manual workflows, codify them as handlers:

```bash
# User notices they always respond the same way to testing-agent
# So they create a handler
/handler new test-results

# Handler captures the pattern
# But still requires manual approval initially
/handler test-results --require-approval
```

### Phase 4: Full Automation (Future)

Only after extensive use of manual and opt-in modes, consider full automation with all the sophisticated features (fork, complex filters, etc.).

**Key insight:** We need to **use the system manually** to understand how inter-agent communication actually works in practice before we try to automate it.

## Implementation Phases (Revised)

**Phase 1: Manual Messaging**
- Agent business cards (`/agents` commands)
- Point-to-point messages (`/send`)
- Manual message queue (`/messages next`, `/messages wait`)
- Messages treated like user input
- Status line indicators for pending messages

**Phase 2: Message Management**
- View message details (`/messages show`)
- Mark messages as read/unread
- Delete/archive messages
- Filter message list
- Message threading (replies)

**Phase 3: Opt-In Automation**
- Simple auto-respond rules (`/agent config auto-respond <sender>`)
- Priority-based auto-processing
- Tag-based routing
- Still requires explicit configuration

**Phase 4: Basic Handlers (Future)**
- File-based handler definitions
- Simple prompt templates
- Manual handler invocation
- Handler testing tools

**Phase 5: Advanced Automation (Future)**
- Automatic handler execution
- Complex filter expressions
- Fork mode
- Broadcast messaging
- Agent specialization

**Phase 6: Ecosystem (Future)**
- Database-stored handlers
- Handler marketplace
- Workflow orchestration
- Agent reputation

## Related Documentation

- [multi-agent.md](multi-agent.md) - Multi-agent conversation design
- [database-architecture.md](database-architecture.md) - How agents are stored
- [workflows.md](workflows.md) - Multi-agent workflows
- [commands.md](commands.md) - Command reference
