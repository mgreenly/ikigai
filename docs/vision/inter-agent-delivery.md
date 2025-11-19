# Inter-Agent Communication: Message Delivery

This document covers message delivery timing, mechanics, and policies.

**Note:** This content has been split into two focused documents for better organization.

See also:
- [inter-agent-communication.md](inter-agent-communication.md) - Overview and index
- [inter-agent-core.md](inter-agent-core.md) - Core concepts and messaging
- [inter-agent-handlers.md](inter-agent-handlers.md) - Message handler system
- [inter-agent-implementation.md](inter-agent-implementation.md) - Use cases and implementation

## Documentation Structure

### 1. [Delivery Timing](inter-agent-delivery-timing.md)

The hard problem of when and how messages are delivered.

**Topics:**
- Message delivery timing scenarios (idle vs active)
- Five delivery approaches:
  - Queue Until Idle
  - Priority-Based Interruption
  - Background Handler Execution
  - Conversation Injection
  - Micro-Interruption with Auto-Resume
- Recommended hybrid approach
- Handler action restrictions based on agent state
- Message context windows
- Implementation complexity considerations

**Key concepts:** Delivery timing, interruption strategies, context windows.

### 2. [Delivery Policies & Mechanics](inter-agent-delivery-policies.md)

How agents control message handling and the simple tool-based approach.

**Topics:**
- Agent message handling policies (interrupt, queue, fork)
- Fork mode mechanics and characteristics
- Hybrid policies (sender-based, priority-based, time-based)
- Policy trade-offs comparison
- User control and overrides
- Visual indicators in status line
- The ideal user experience
- Simple tool-based message mechanics
- Message tools (ReadMessage, SendMessage, ListMessages, ReplyToMessage)
- Example flows and user commands

**Key concepts:** Fork mode, handling policies, message tools, user control.

## Quick Reference

**For understanding delivery timing issues:**
→ Read [Delivery Timing](inter-agent-delivery-timing.md)

**For implementing message handling:**
→ Read [Delivery Policies & Mechanics](inter-agent-delivery-policies.md)

**For seeing it in action:**
→ Read the example flows in [Delivery Policies](inter-agent-delivery-policies.md#example-flow)
