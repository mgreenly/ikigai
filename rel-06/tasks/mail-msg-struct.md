# Task: Mail Message Structure

## Target
User Stories: 15-send-mail.md, 16-check-mail.md, 17-read-mail.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- project/agent-process-model.md (Mailbox section)
- project/memory.md (talloc ownership)

## Pre-read Source (patterns)
- READ: src/agent.h (ik_agent_ctx_t)

## Pre-read Tests (patterns)
- READ: tests/unit structures (e.g., tests/unit/agent/agent_test.c)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- nav-parent-child.md complete

## Task
Create the mail message structure for inter-agent communication.

**Structure:**
```c
typedef struct ik_mail_msg {
    int64_t id;           // Database ID
    char *from_uuid;      // Sender agent UUID
    char *to_uuid;        // Recipient agent UUID
    char *body;           // Message content
    int64_t timestamp;    // Unix epoch
    bool read;            // Has been read
} ik_mail_msg_t;
```

**API:**
```c
// Create a mail message
ik_mail_msg_t *ik_mail_msg_create(TALLOC_CTX *ctx,
                                   const char *from_uuid,
                                   const char *to_uuid,
                                   const char *body);
```

## TDD Cycle

### Red
1. CREATE: `src/mail/msg.h`:
   ```c
   #pragma once

   #include <stdbool.h>
   #include <stdint.h>
   #include <talloc.h>

   typedef struct ik_mail_msg {
       int64_t id;
       char *from_uuid;
       char *to_uuid;
       char *body;
       int64_t timestamp;
       bool read;
   } ik_mail_msg_t;

   ik_mail_msg_t *ik_mail_msg_create(TALLOC_CTX *ctx,
                                      const char *from_uuid,
                                      const char *to_uuid,
                                      const char *body);
   ```

2. CREATE: `tests/unit/mail/msg_test.c`:
   - Test create allocates message
   - Test fields copied correctly
   - Test timestamp set to current time
   - Test read defaults to false
   - Test id defaults to 0 (set on insert)
   - Test freed with talloc_free

3. Run `make check` - expect test failures

### Green
1. CREATE: `src/mail/msg.c`:
   ```c
   #include "msg.h"
   #include "../panic.h"
   #include "../wrapper.h"
   #include <time.h>

   ik_mail_msg_t *ik_mail_msg_create(TALLOC_CTX *ctx,
                                      const char *from_uuid,
                                      const char *to_uuid,
                                      const char *body)
   {
       ik_mail_msg_t *msg = talloc_zero_(ctx, sizeof(ik_mail_msg_t));
       if (msg == NULL) PANIC("Out of memory");

       msg->from_uuid = talloc_strdup(msg, from_uuid);
       msg->to_uuid = talloc_strdup(msg, to_uuid);
       msg->body = talloc_strdup(msg, body);
       msg->timestamp = time(NULL);
       msg->read = false;
       msg->id = 0;

       return msg;
   }
   ```

2. MODIFY: Makefile (add src/mail/msg.c and tests/unit/mail/msg_test.c)

3. Run `make check` - expect pass
4. Commit changes (use haiku sub-agent for git commit)

### Refactor
1. Verify talloc ownership (strings child of msg)
2. Run `make lint` - verify clean (use haiku sub-agent for verification)
3. If refactoring needed, commit changes (use haiku sub-agent for git commit)

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- `ik_mail_msg_t` structure defined
- Create function works
- Proper memory management
- Working tree is clean (all changes committed)
