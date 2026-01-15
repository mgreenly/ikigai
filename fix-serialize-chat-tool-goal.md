## Objective

OpenAI tool calls fail with: `Invalid schema for function 'file_read': In context=(), 'additionalProperties' is required to be supplied and to be false`

Fix `serialize_chat_tool()` in `src/providers/openai/request_chat.c` to add `additionalProperties: false` to parameter schemas. OpenAI's strict mode (line 117) requires this field.

## Reference

- `project/archive/rel-07/diagnosis-tool-schema-additionalProperties.md` - full diagnosis

## Acceptance

All quality gates pass:
- compile
- filesize
- unit
- complexity
- integration
- sanitize
- tsan
- helgrind
- valgrind
- coverage
