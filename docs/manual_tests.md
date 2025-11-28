# Manual Testing Checklist

This document contains comprehensive manual testing procedures for validating the ikigai LLM integration implementation. These tests supplement automated unit and integration tests by verifying the complete user experience.

## Prerequisites

Before running manual tests:

1. **Build the application:**
   ```bash
   make clean && make
   ```

2. **Configure API key:**
   - Edit `~/.config/ikigai/config.json`
   - Set `"openai_api_key": "sk-your-actual-key-here"`
   - Verify other fields have reasonable defaults:
     ```json
     {
       "openai_api_key": "sk-...",
       "openai_model": "gpt-4o-mini",
       "openai_temperature": 0.7,
       "openai_max_tokens": 4096,
       "openai_system_message": null
     }
     ```

3. **Ensure network connectivity** to api.openai.com

4. **Monitor system resources** (optional but recommended):
   ```bash
   # In separate terminal
   watch -n 1 'ps aux | grep ikigai'
   ```

## Test Session 1: Basic Functionality

**Goal:** Verify core message sending, streaming, and spinner animation

### Test 1.1: First Message

**Steps:**
1. Run `bin/ikigai`
2. Type "Hello!" in the input area
3. Press Enter

**Expected Behavior:**
- Input text moves to scrollback buffer immediately
- Input area is replaced with animated spinner
- Spinner shows rotating frames: `⠋ ⠙ ⠹ ⠸ ⠼ ⠴ ⠦ ⠧ ⠇ ⠏`
- Spinner animates smoothly at ~80ms intervals
- Response begins streaming into scrollback (may start with small delay)
- Text appears incrementally as tokens arrive
- When complete, spinner disappears
- Input area returns and cursor is ready
- Scrollback shows complete conversation:
  ```
  User: Hello!
  Assistant: [response text]
  ```

**Troubleshooting:**
- **No response:** Check API key validity, network connectivity
- **Spinner doesn't animate:** Check terminal supports Unicode
- **Response not streaming:** OpenAI API should stream by default; check config
- **Cursor stuck:** Press Ctrl+C to interrupt, check logs

### Test 1.2: Follow-up Message (Context Preservation)

**Steps:**
1. After Test 1.1 completes
2. Type "What's 2+2?" (or another follow-up question)
3. Press Enter

**Expected Behavior:**
- Same spinner and streaming behavior as Test 1.1
- Response should acknowledge conversation context
- If asked "What did I just say?", LLM should reference "Hello!" message
- Scrollback accumulates all messages chronologically

**Troubleshooting:**
- **No context:** Check session_messages array is populated
- **Old messages forgotten:** May indicate message serialization issue

## Test Session 2: Multi-line Input & Scrolling

**Goal:** Verify text wrapping, viewport scrolling, and large content handling

### Test 2.1: Multi-line Input

**Steps:**
1. Type a message with multiple lines using Shift+Enter:
   ```
   This is line 1
   [Shift+Enter]
   This is line 2
   [Shift+Enter]
   This is line 3
   [Enter to send]
   ```

**Expected Behavior:**
- Input area expands to show all lines
- Message sends with newlines preserved
- Scrollback displays multi-line message correctly
- Response streams normally

**Troubleshooting:**
- **Newlines not preserved:** Check input buffer handles \n correctly
- **Input area doesn't expand:** Layer height calculation issue

### Test 2.2: Long Response with Auto-scrolling

**Steps:**
1. Send question that generates long response:
   ```
   Explain the history of the C programming language in detail.
   ```
2. Watch response stream in

**Expected Behavior:**
- As tokens arrive, scrollback grows
- Viewport stays at bottom, showing new content as it arrives
- Can press Page Up to scroll up while response streams
- Scrolling up doesn't stop streaming
- When response completes, viewport remains at current position

**Troubleshooting:**
- **Viewport jumps:** Check auto-scroll logic in layer_cake
- **Can't scroll up:** Verify Page Up handler not disabled during streaming

### Test 2.3: Scrolling Through History

**Steps:**
1. After several messages exchanged
2. Use Page Up to scroll to top
3. Use Page Down to scroll to bottom

**Expected Behavior:**
- Page Up: Viewport moves up smoothly
- Page Down: Viewport moves down smoothly
- Can see entire conversation history
- Top line wraps correctly at terminal width
- Separator line visible when input area in viewport

**Troubleshooting:**
- **Scrolling choppy:** Performance issue, check rendering efficiency
- **Content cut off:** Wrapping calculation bug

## Test Session 3: Commands

**Goal:** Verify all REPL commands work correctly

### Test 3.1: Help Command

**Steps:**
1. Type `/help`
2. Press Enter

**Expected Behavior:**
- Help text appears in scrollback (NOT sent to LLM)
- Shows all available commands:
  - `/clear` - Clear conversation history
  - `/mark [label]` - Create checkpoint
  - `/rewind [label]` - Rollback to checkpoint
  - `/help` - Show this help
  - `/model <name>` - Switch model
  - `/system <text>` - Set system message
- Input returns immediately (no spinner)

**Troubleshooting:**
- **Help sent to LLM:** Command dispatch failed, check prefix matching
- **Missing commands:** Update help text in repl_commands.c

### Test 3.2: Model Switching

**Steps:**
1. Type `/model gpt-3.5-turbo`
2. Press Enter
3. Send a test message

**Expected Behavior:**
- Confirmation message: "Model changed to: gpt-3.5-turbo"
- Subsequent messages use new model
- Response style may differ (GPT-3.5 vs GPT-4)
- Session context preserved across model change

**Troubleshooting:**
- **Model not changed:** Check config update in /model handler
- **Context lost:** Session messages should not be cleared
- **Invalid model:** Error message should appear

### Test 3.3: System Message

**Steps:**
1. Type `/system You are a helpful pirate assistant who speaks like a pirate`
2. Press Enter
3. Send message: "Hello, who are you?"

**Expected Behavior:**
- Confirmation: "System message set"
- Response should reflect new personality (pirate speak)
- System message persists for session
- Subsequent messages continue with personality

**Troubleshooting:**
- **No personality change:** Check system message added to session_messages
- **Personality lost after first message:** System message should be first in array

### Test 3.4: Mark and Rewind

**Steps:**
1. Exchange 2-3 messages with LLM
2. Type `/mark checkpoint1`
3. Exchange 2-3 more messages
4. Type `/rewind checkpoint1`
5. Send new message

**Expected Behavior:**
- After `/mark checkpoint1`: Scrollback shows mark indicator
- After additional messages: Scrollback contains all messages
- After `/rewind checkpoint1`:
  - Scrollback truncated to checkpoint position
  - Later messages removed from view
  - Session messages array also truncated
- New message after rewind: Conversation continues from checkpoint
- Previous messages after checkpoint no longer in LLM context

**Troubleshooting:**
- **Mark not visible:** Check mark rendering in scrollback
- **Rewind doesn't truncate:** Check mark index lookup
- **Context still includes removed messages:** Verify session_messages truncation

### Test 3.5: Clear Command

**Steps:**
1. After several messages exchanged
2. Type `/clear`
3. Press Enter

**Expected Behavior:**
- Scrollback cleared completely (empty)
- Input area shows empty buffer
- Marks cleared
- Session messages cleared
- Next message starts fresh conversation (no context)

**Troubleshooting:**
- **Context not cleared:** Check session_messages freed/reset
- **Marks remain:** Verify marks array freed

## Test Session 4: Error Handling

**Goal:** Verify graceful error handling and recovery

### Test 4.1: Invalid API Key

**Steps:**
1. Edit `~/.config/ikigai/config.json`
2. Set invalid API key: `"openai_api_key": "sk-invalid"`
3. Restart ikigai (or reload config if supported)
4. Send message: "Hello"

**Expected Behavior:**
- Spinner appears briefly
- Error message appears in scrollback:
  ```
  Error: HTTP 401 Unauthorized
  [Response body with error details]
  ```
- Input area returns (can type again)
- Can restore valid key and retry

**Troubleshooting:**
- **App crashes:** HTTP error handling bug
- **No error shown:** Check error rendering in scrollback
- **Spinner stuck:** Ensure error path clears spinner state

### Test 4.2: Network Disconnection

**Steps:**
1. With valid API key configured
2. Disable network (unplug ethernet, disable wifi, or use firewall)
3. Send message: "Hello"
4. Wait for timeout

**Expected Behavior:**
- Spinner appears
- After timeout (~30s), connection error appears:
  ```
  Error: Failed to connect to api.openai.com
  [Connection timeout or network unreachable]
  ```
- Input returns
- Re-enable network
- Next message works normally

**Troubleshooting:**
- **Timeout too long:** Check libcurl timeout settings
- **App hangs:** Non-blocking I/O issue
- **Can't recover:** Check error cleanup path

### Test 4.3: Malformed Command

**Steps:**
1. Type `/invalidcommand arg1 arg2`
2. Press Enter

**Expected Behavior:**
- Error message in scrollback: "Unknown command: /invalidcommand"
- Suggestion to use `/help`
- Input returns immediately

**Troubleshooting:**
- **Sent to LLM:** Command prefix detection failed
- **App crashes:** Command parsing bug

## Test Session 5: Stress Testing

**Goal:** Verify performance, memory management, and stability under load

### Test 5.1: Rapid Message Sequence

**Steps:**
1. Send 20-30 short messages back-to-back:
   ```
   What is 1+1?
   [wait for response]
   What is 2+2?
   [wait for response]
   ... (repeat)
   ```
2. Monitor with `top` or `htop` in another terminal

**Expected Behavior:**
- All messages process successfully
- Memory usage grows but stabilizes (no continuous growth)
- No memory leaks detected
- Response time remains consistent
- UI remains responsive between messages

**Troubleshooting:**
- **Memory leak:** Check session_messages cleanup
- **Slowdown over time:** Performance regression in scrollback
- **Crash after N messages:** Memory exhaustion, check limits

### Test 5.2: Very Long Input

**Steps:**
1. Type or paste a very long message (500+ words, multi-paragraph)
2. Send message

**Expected Behavior:**
- Input area handles large text
- Message sends successfully
- Response streams normally
- Scrollback displays full input and output

**Troubleshooting:**
- **Input truncated:** Buffer size limit
- **Request fails:** Check API payload size limits
- **Rendering issues:** Wrapping calculation overflow

### Test 5.3: Very Long Response

**Steps:**
1. Send: "Write a detailed 2000-word essay about the history of computing"
2. Wait for complete response

**Expected Behavior:**
- Response streams continuously
- Scrollback grows to accommodate content
- Can scroll up/down during streaming
- Memory usage proportional to content size
- No crashes or hangs
- Response completes successfully

**Troubleshooting:**
- **Streaming stops mid-response:** Check SSE parser buffer handling
- **UI freezes:** Rendering performance issue
- **Out of memory:** May need pagination strategy (future enhancement)

### Test 5.4: Extended Session Valgrind Check

**Steps:**
1. Exit ikigai
2. Run under valgrind:
   ```bash
   valgrind --leak-check=full --show-leak-kinds=all bin/ikigai
   ```
3. Exchange 10+ messages
4. Use various commands (/mark, /rewind, /clear)
5. Exit with Ctrl+D

**Expected Behavior:**
- Valgrind reports:
  ```
  All heap blocks were freed -- no leaks are possible
  ERROR SUMMARY: 0 errors from 0 contexts
  ```
- No memory leaks
- No invalid memory accesses

**Troubleshooting:**
- **Leaks detected:** Check talloc cleanup paths
- **Invalid reads/writes:** Buffer overflow or use-after-free

## Common Issues and Fixes

### Application Won't Start

**Symptoms:** Crashes immediately or shows error message

**Possible Causes:**
- Missing config file
- Invalid JSON in config
- Terminal doesn't support required features

**Fixes:**
1. Delete config: `rm ~/.config/ikigai/config.json` (will regenerate defaults)
2. Validate JSON: `jq . ~/.config/ikigai/config.json`
3. Check terminal: Ensure UTF-8 support, 256 colors minimum

### Spinner Doesn't Animate

**Symptoms:** Spinner appears but doesn't rotate

**Possible Causes:**
- Terminal doesn't support Unicode
- Timer events not firing
- Event loop blocked

**Fixes:**
1. Test terminal: `echo "⠋ ⠙ ⠹ ⠸ ⠼ ⠴ ⠦ ⠧ ⠇ ⠏"` (should show spinner frames)
2. Check terminal type: `echo $TERM` (should be xterm-256color or similar)
3. Try different terminal emulator

### Streaming Doesn't Work

**Symptoms:** Full response appears all at once instead of incrementally

**Possible Causes:**
- API doesn't support streaming
- Streaming disabled in config
- SSE parser not detecting chunks

**Fixes:**
1. Check config: `"stream": true` should be sent in request
2. Verify OpenAI API version supports streaming
3. Check network buffering (some proxies buffer SSE)

### Text Wrapping Issues

**Symptoms:** Lines cut off or wrapped incorrectly

**Possible Causes:**
- Terminal width detection wrong
- Wrapping algorithm bug
- Unicode character width issues

**Fixes:**
1. Resize terminal and restart
2. Check width detection: Add debug output for `term->width`
3. Test with ASCII-only text to isolate Unicode issues

### Context Not Preserved

**Symptoms:** LLM doesn't remember previous messages

**Possible Causes:**
- Session messages not being stored
- Messages not included in API request
- /clear or /rewind unintentionally executed

**Fixes:**
1. Add debug output for session_messages array size
2. Verify request JSON includes all messages
3. Check for accidental command execution

### High Memory Usage

**Symptoms:** Memory grows continuously or excessively

**Possible Causes:**
- Memory leak in message storage
- Scrollback not truncating old content
- Large responses accumulating

**Fixes:**
1. Run under valgrind to detect leaks
2. Use `/clear` periodically in long sessions
3. Check talloc memory reports (if enabled)

### API Errors

**Symptoms:** HTTP errors (429, 500, etc.)

**Possible Causes:**
- Rate limiting (429)
- Server issues (500, 502, 503)
- Invalid request format (400)

**Fixes:**
1. For 429: Wait and retry, or upgrade API tier
2. For 5xx: OpenAI server issue, wait and retry
3. For 400: Check request JSON format in logs

## Regression Testing

When making changes to the codebase, run this quick regression suite:

1. **Quick smoke test** (2 minutes):
   - Start app, send one message, verify streaming works
   - Test `/help` and `/clear`
   - Exit cleanly

2. **Command test** (5 minutes):
   - Test all six commands: /clear, /mark, /rewind, /help, /model, /system
   - Verify expected behavior for each

3. **Error test** (3 minutes):
   - Invalid API key test
   - Malformed command test

4. **Memory test** (5 minutes):
   - Send 10 messages under valgrind
   - Verify no leaks

**Total time: ~15 minutes**

Run full test suite (Sessions 1-5) before major releases or when changing core functionality.

## Automated Testing Complement

These manual tests complement automated tests:

- **Unit tests:** Test individual functions in isolation
- **Integration tests:** Test component interactions
- **Manual tests:** Test complete user experience, UI/UX, edge cases

Always run `make check && make coverage` before manual testing to ensure base functionality is solid.

## Test Recording Template

When reporting manual test results, use this template:

```
Test Session: [1-5]
Test Number: [e.g., 3.4]
Tester: [name]
Date: [YYYY-MM-DD]
Build: [git commit hash]
Result: [PASS/FAIL]

Expected Behavior:
[What should happen]

Actual Behavior:
[What actually happened]

Screenshots/Logs:
[If applicable]

Issues Found:
[List any bugs or unexpected behavior]

Notes:
[Additional observations]
```

## Next Steps After Manual Testing

After completing all manual test sessions:

1. Document any issues found in GitHub issues or tasks.md
2. Fix critical bugs before proceeding
3. Run Phase 1.8 (Mock Verification & Polish)
4. Prepare for final acceptance test (Task 8.6)
5. Consider Phase 2 (Database Integration) requirements
