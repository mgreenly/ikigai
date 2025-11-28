# Storage and Database Schema

## Overview

PostgreSQL stores conversation structure. Added in Phase 3.

## Schema

### Conversations Table

One row per WebSocket connection.

**Fields:**
- `id` (UUID, primary key)
- `user_identity` (text) - `hostname@username`
- `started_at` (timestamp)
- `ended_at` (timestamp, nullable)

### Exchanges Table

One row per query-response cycle.

**Fields:**
- `id` (UUID, primary key) - correlation_id from protocol
- `conversation_id` (UUID, foreign key)
- `started_at` (timestamp)
- `completed_at` (timestamp, nullable)

### Messages Table

Stores user queries, assistant responses, tool delegations, tool results.

**Fields:**
- `id` (UUID, primary key)
- `exchange_id` (UUID, foreign key)
- `message_type` (enum: user_query, assistant_response, tool_delegation, tool_result)
- `content` (jsonb)
- `created_at` (timestamp)
- `sequence` (integer) - order within exchange

**Relationships:**
- Messages belong to exchanges
- Exchanges belong to conversations

## Incomplete Exchanges

Exchanges may be incomplete due to errors:
- No `assistant_response`: LLM request failed before response
- Partial `assistant_response`: Streaming interrupted
- `completed_at` NULL: Exchange never finished

All sent/received messages are saved, even in failed exchanges.
