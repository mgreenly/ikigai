# Task: Dim Styling for Notifications in Scrollback

## Target
Phase 3: Inter-Agent Mailboxes - Step 14 (Notification visual styling)

Supports User Stories:
- 39 (notification visible in scrollback) - Notifications styled with ANSI dim to de-emphasize visually

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/coverage.md
- .agents/skills/mocking.md

### Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (Notification System section - visual styling)
- docs/rel-05/user-stories/39-notification-visible-in-scrollback.md (complete walkthrough)
- docs/memory.md (talloc ownership patterns)
- docs/return_values.md (res_t patterns)

### Pre-read Source (patterns)
- src/event_render.h (ik_event_render, ik_event_renders_visible)
- src/event_render.c (render_content_event, apply_style patterns)
- src/scrollback.h (ik_scrollback_append_line)
- src/scrollback.c (line rendering, ANSI handling)
- src/ansi.h (IK_ANSI_RESET, color constants, ik_ansi_colors_enabled)
- src/ansi.c (ANSI sequence building patterns)

### Pre-read Tests (patterns)
- tests/unit/event_render/styling_test.c (color styling test patterns)
- tests/unit/event_render/basic_test.c (event rendering test patterns)
- tests/unit/scrollback/scrollback_test.c (scrollback append patterns)

## Pre-conditions
- `make check` passes
- `make lint` passes
- mail-notification-inject.md complete:
  - Notification injection working on IDLE transition
  - Notifications added to scrollback as "notification" kind
  - `mail_notification_pending` flag managed
  - `ik_event_render()` handles "notification" kind
  - `ik_event_renders_visible()` returns true for "notification"
- `ik_ansi_colors_enabled()` and ANSI infrastructure available
- `ik_scrollback_append_line()` handles ANSI sequences correctly

## Task
Implement dim styling for mail notifications in scrollback. When notifications are rendered via `ik_event_render()` with kind "notification", wrap the content in ANSI dim escape sequences to visually de-emphasize while keeping fully readable.

**Visual example (from user story 39):**
```
I've finished analyzing the repository structure.

[Notification: You have 1 unread message in your inbox]   <- dimmed

> /mail
```

**ANSI dim codes:**
```c
#define IK_ANSI_DIM    "\x1b[2m"   // SGR code 2 = dim/faint
#define IK_ANSI_RESET  "\x1b[0m"   // SGR code 0 = reset all attributes
```

**Styling requirements:**

1. **Detection**: Identify notification events by kind "notification" in `ik_event_render()`

2. **Dim application**: Wrap notification content with `\x1b[2m` prefix and `\x1b[0m` suffix
   ```
   Input:  "[Notification: You have 2 unread messages in your inbox]"
   Output: "\x1b[2m[Notification: You have 2 unread messages in your inbox]\x1b[0m"
   ```

3. **Color respect**: Only apply dim when `ik_ansi_colors_enabled()` returns true
   - Colors disabled: Output plain text without ANSI sequences
   - Colors enabled: Output with dim wrapping

4. **Reset after line**: Ensure ANSI reset is applied after notification text to prevent dim bleeding into subsequent content

5. **LLM context**: Plain text for LLM (conversation messages have no ANSI codes)
   - Scrollback stores styled text (for terminal display)
   - Conversation stores plain text (for LLM context)
   - This separation already exists from mail-notification-inject.md

6. **Graceful degradation**: Terminals that don't support dim attribute will display text normally (no visual difference, but no corruption)

**Key insight from existing code:**

The current `render_content_event()` in `src/event_render.c` uses `apply_style()` for color:
```c
static char *apply_style(TALLOC_CTX *ctx, const char *content, uint8_t color)
{
    if (!ik_ansi_colors_enabled() || color == 0) {
        return talloc_strdup(ctx, content);
    }
    char color_seq[16];
    ik_ansi_fg_256(color_seq, sizeof(color_seq), color);
    return talloc_asprintf(ctx, "%s%s%s", color_seq, content, IK_ANSI_RESET);
}
```

For notifications, we need a similar approach but with dim attribute instead of foreground color:
```c
static char *apply_dim_style(TALLOC_CTX *ctx, const char *content)
{
    if (!ik_ansi_colors_enabled()) {
        return talloc_strdup(ctx, content);
    }
    return talloc_asprintf(ctx, "%s%s%s", IK_ANSI_DIM, content, IK_ANSI_RESET);
}
```

**Rendering flow:**

```
Notification injection (mail-notification-inject.md)
    |
    v
ik_event_render(scrollback, "notification", notification_text, NULL)
    |
    +---> kind == "notification"?
    |         |
    |         +---> ik_ansi_colors_enabled()?
    |         |         |
    |         |         +---> Yes: Wrap with \x1b[2m ... \x1b[0m
    |         |         +---> No:  Use plain text
    |         |
    |         +---> ik_scrollback_append_line(scrollback, styled_text, len)
    |
    v
Text in scrollback buffer (styled or plain depending on colors)
```

**Width calculation:**

ANSI escape sequences have zero display width. The scrollback width calculation in `src/scrollback.c` already handles this via `ik_ansi_skip_csi()`:
- `\x1b[2m` - 4 bytes, 0 display width
- `\x1b[0m` - 4 bytes, 0 display width

This is already implemented, so no changes needed to width calculation.

## TDD Cycle

### Red
1. Add `IK_ANSI_DIM` constant to `src/ansi.h`:
   ```c
   // SGR sequence literals
   #define IK_ANSI_RESET "\x1b[0m"
   #define IK_ANSI_DIM   "\x1b[2m"
   ```

2. Create tests in `tests/unit/event_render/notification_style_test.c`:
   ```c
   /**
    * @file notification_style_test.c
    * @brief Tests for notification dim styling in event render
    *
    * Tests the visual styling of notifications:
    * - Dim attribute applied when colors enabled
    * - Plain text when colors disabled
    * - Proper reset after notification
    * - Display width calculation with dim codes
    */

   #include "../../../src/event_render.h"
   #include "../../../src/scrollback.h"
   #include "../../../src/ansi.h"
   #include "../../../src/error.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <stdlib.h>
   #include <string.h>
   #include <talloc.h>

   // ========== Test Fixture ==========

   static TALLOC_CTX *ctx;
   static ik_scrollback_t *scrollback;

   static void setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);

       scrollback = ik_scrollback_create(ctx, 80);
       ck_assert_ptr_nonnull(scrollback);

       // Ensure colors are enabled by default
       unsetenv("NO_COLOR");
       ik_ansi_init();
   }

   static void teardown(void)
   {
       talloc_free(ctx);
       ctx = NULL;
       scrollback = NULL;
   }

   // ========== Dim Styling Tests ==========

   // Test: Notification has dim prefix when colors enabled
   START_TEST(test_notification_dim_prefix)
   {
       res_t result = ik_event_render(scrollback, "notification",
           "[Notification: You have 1 unread message in your inbox]", NULL);
       ck_assert(!is_err(&result));

       const char *text;
       size_t length;
       ik_scrollback_get_line_text(scrollback, 0, &text, &length);

       // Should start with dim escape sequence
       ck_assert_ptr_nonnull(strstr(text, "\x1b[2m"));
       // Dim should be at the start
       ck_assert(strncmp(text, "\x1b[2m", 4) == 0);
   }
   END_TEST

   // Test: Notification has reset suffix when colors enabled
   START_TEST(test_notification_reset_suffix)
   {
       res_t result = ik_event_render(scrollback, "notification",
           "[Notification: Test]", NULL);
       ck_assert(!is_err(&result));

       const char *text;
       size_t length;
       ik_scrollback_get_line_text(scrollback, 0, &text, &length);

       // Should end with reset escape sequence
       ck_assert_ptr_nonnull(strstr(text, "\x1b[0m"));
       // Reset should be at the end
       ck_assert(length >= 4);
       ck_assert(strncmp(text + length - 4, "\x1b[0m", 4) == 0);
   }
   END_TEST

   // Test: Notification content preserved in styled output
   START_TEST(test_notification_content_preserved)
   {
       const char *notification = "[Notification: You have 2 unread messages in your inbox]";
       res_t result = ik_event_render(scrollback, "notification", notification, NULL);
       ck_assert(!is_err(&result));

       const char *text;
       size_t length;
       ik_scrollback_get_line_text(scrollback, 0, &text, &length);

       // Original notification text should be contained within
       ck_assert_ptr_nonnull(strstr(text, notification));
   }
   END_TEST

   // Test: Notification uses dim, not foreground color
   START_TEST(test_notification_uses_dim_not_color)
   {
       res_t result = ik_event_render(scrollback, "notification",
           "[Notification: Test]", NULL);
       ck_assert(!is_err(&result));

       const char *text;
       size_t length;
       ik_scrollback_get_line_text(scrollback, 0, &text, &length);

       // Should have dim \x1b[2m but NOT foreground color \x1b[38;5;
       ck_assert_ptr_nonnull(strstr(text, "\x1b[2m"));
       ck_assert_ptr_null(strstr(text, "\x1b[38;5;"));
   }
   END_TEST

   // Test: Complete styled format is correct
   START_TEST(test_notification_complete_format)
   {
       const char *content = "[Notification: Message]";
       res_t result = ik_event_render(scrollback, "notification", content, NULL);
       ck_assert(!is_err(&result));

       const char *text;
       size_t length;
       ik_scrollback_get_line_text(scrollback, 0, &text, &length);

       // Expected format: \x1b[2m[Notification: Message]\x1b[0m
       char expected[128];
       snprintf(expected, sizeof(expected), "\x1b[2m%s\x1b[0m", content);

       ck_assert_str_eq(text, expected);
   }
   END_TEST

   // ========== Colors Disabled Tests ==========

   // Test: No ANSI codes when colors disabled
   START_TEST(test_notification_no_ansi_when_disabled)
   {
       // Disable colors
       setenv("NO_COLOR", "1", 1);
       ik_ansi_init();

       const char *content = "[Notification: Test]";
       res_t result = ik_event_render(scrollback, "notification", content, NULL);
       ck_assert(!is_err(&result));

       const char *text;
       size_t length;
       ik_scrollback_get_line_text(scrollback, 0, &text, &length);

       // Should NOT contain any ANSI escape sequences
       ck_assert_ptr_null(strstr(text, "\x1b["));
       // Should be plain text
       ck_assert_str_eq(text, content);

       // Cleanup
       unsetenv("NO_COLOR");
       ik_ansi_init();
   }
   END_TEST

   // Test: Plain text identical to input when colors disabled
   START_TEST(test_notification_plain_when_disabled)
   {
       setenv("NO_COLOR", "1", 1);
       ik_ansi_init();

       const char *content = "[Notification: You have 5 unread messages in your inbox]";
       res_t result = ik_event_render(scrollback, "notification", content, NULL);
       ck_assert(!is_err(&result));

       const char *text;
       size_t length;
       ik_scrollback_get_line_text(scrollback, 0, &text, &length);

       ck_assert_uint_eq(length, strlen(content));
       ck_assert_str_eq(text, content);

       unsetenv("NO_COLOR");
       ik_ansi_init();
   }
   END_TEST

   // Test: TERM=dumb also disables styling
   START_TEST(test_notification_no_ansi_term_dumb)
   {
       setenv("TERM", "dumb", 1);
       ik_ansi_init();

       res_t result = ik_event_render(scrollback, "notification",
           "[Notification: Test]", NULL);
       ck_assert(!is_err(&result));

       const char *text;
       size_t length;
       ik_scrollback_get_line_text(scrollback, 0, &text, &length);

       ck_assert_ptr_null(strstr(text, "\x1b["));

       unsetenv("TERM");
       ik_ansi_init();
   }
   END_TEST

   // ========== Display Width Tests ==========

   // Test: ANSI dim codes have zero display width
   START_TEST(test_notification_display_width_excludes_ansi)
   {
       const char *content = "Test"; // 4 display characters
       res_t result = ik_event_render(scrollback, "notification", content, NULL);
       ck_assert(!is_err(&result));

       // Get the layout which includes display_width
       ck_assert_uint_eq(scrollback->layouts[0].display_width, 4);
   }
   END_TEST

   // Test: Multi-byte content width calculated correctly
   START_TEST(test_notification_unicode_width)
   {
       // Unicode content: "Hello" = 5 chars
       const char *content = "[Notification: Hello]"; // 21 display chars
       res_t result = ik_event_render(scrollback, "notification", content, NULL);
       ck_assert(!is_err(&result));

       // Display width should be content width, not byte count
       ck_assert_uint_eq(scrollback->layouts[0].display_width, 21);
   }
   END_TEST

   // Test: Long notification display width
   START_TEST(test_notification_long_content_width)
   {
       const char *content = "[Notification: You have 99 unread messages in your inbox]";
       res_t result = ik_event_render(scrollback, "notification", content, NULL);
       ck_assert(!is_err(&result));

       // Width = strlen(content) = 57 (all ASCII)
       ck_assert_uint_eq(scrollback->layouts[0].display_width, 57);
   }
   END_TEST

   // ========== Edge Case Tests ==========

   // Test: Empty notification content
   START_TEST(test_notification_empty_content)
   {
       res_t result = ik_event_render(scrollback, "notification", "", NULL);
       ck_assert(!is_err(&result));

       // Empty content should not add any line
       ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);
   }
   END_TEST

   // Test: Null notification content
   START_TEST(test_notification_null_content)
   {
       res_t result = ik_event_render(scrollback, "notification", NULL, NULL);
       ck_assert(!is_err(&result));

       // Null content should not add any line
       ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);
   }
   END_TEST

   // Test: Notification with embedded newlines
   START_TEST(test_notification_with_newlines)
   {
       const char *content = "[Notification: Line1\nLine2]";
       res_t result = ik_event_render(scrollback, "notification", content, NULL);
       ck_assert(!is_err(&result));

       const char *text;
       size_t length;
       ik_scrollback_get_line_text(scrollback, 0, &text, &length);

       // Content should be preserved including newline
       ck_assert_ptr_nonnull(strstr(text, "Line1\nLine2"));
   }
   END_TEST

   // Test: Notification renders as visible kind
   START_TEST(test_notification_is_visible_kind)
   {
       ck_assert(ik_event_renders_visible("notification"));
   }
   END_TEST

   // Test: Multiple notifications maintain styling
   START_TEST(test_multiple_notifications_styled)
   {
       ik_event_render(scrollback, "notification", "[Notification: First]", NULL);
       ik_event_render(scrollback, "notification", "[Notification: Second]", NULL);

       const char *text1;
       const char *text2;
       size_t len1, len2;

       // First notification (line 0, blank line at 1)
       ik_scrollback_get_line_text(scrollback, 0, &text1, &len1);
       // Second notification (line 2, blank line at 3)
       ik_scrollback_get_line_text(scrollback, 2, &text2, &len2);

       // Both should have dim styling
       ck_assert_ptr_nonnull(strstr(text1, "\x1b[2m"));
       ck_assert_ptr_nonnull(strstr(text2, "\x1b[2m"));

       // Both should have reset
       ck_assert_ptr_nonnull(strstr(text1, "\x1b[0m"));
       ck_assert_ptr_nonnull(strstr(text2, "\x1b[0m"));
   }
   END_TEST

   // ========== Style Isolation Tests ==========

   // Test: Notification dim does not affect subsequent lines
   START_TEST(test_notification_style_isolated)
   {
       ik_event_render(scrollback, "notification", "[Notification: Test]", NULL);
       ik_event_render(scrollback, "user", "User message after notification", NULL);

       // Find user message line (after notification + blank line)
       const char *user_text;
       size_t user_len;
       ik_scrollback_get_line_text(scrollback, 2, &user_text, &user_len);

       // User message should have no ANSI codes
       ck_assert_ptr_null(strstr(user_text, "\x1b["));
       ck_assert_ptr_nonnull(strstr(user_text, "User message"));
   }
   END_TEST

   // Test: User message before notification unaffected
   START_TEST(test_preceding_content_unaffected)
   {
       ik_event_render(scrollback, "user", "User message", NULL);
       ik_event_render(scrollback, "notification", "[Notification: Test]", NULL);

       const char *user_text;
       size_t user_len;
       ik_scrollback_get_line_text(scrollback, 0, &user_text, &user_len);

       // User message should have no ANSI codes
       ck_assert_ptr_null(strstr(user_text, "\x1b["));
   }
   END_TEST

   // Test: Assistant message after notification uses its own color
   START_TEST(test_assistant_after_notification_own_style)
   {
       ik_event_render(scrollback, "notification", "[Notification: Test]", NULL);
       ik_event_render(scrollback, "assistant", "Assistant response", NULL);

       // Find assistant line (after notification + blank + blank)
       const char *assistant_text;
       size_t assistant_len;
       ik_scrollback_get_line_text(scrollback, 2, &assistant_text, &assistant_len);

       // Assistant should have its own color (249), not dim
       ck_assert_ptr_nonnull(strstr(assistant_text, "\x1b[38;5;249m"));
       // Should NOT have dim
       ck_assert_ptr_null(strstr(assistant_text, "\x1b[2m"));
   }
   END_TEST

   // ========== Suite Configuration ==========

   static Suite *notification_style_suite(void)
   {
       Suite *s = suite_create("NotificationStyle");

       TCase *tc_dim = tcase_create("DimStyling");
       tcase_add_checked_fixture(tc_dim, setup, teardown);
       tcase_add_test(tc_dim, test_notification_dim_prefix);
       tcase_add_test(tc_dim, test_notification_reset_suffix);
       tcase_add_test(tc_dim, test_notification_content_preserved);
       tcase_add_test(tc_dim, test_notification_uses_dim_not_color);
       tcase_add_test(tc_dim, test_notification_complete_format);
       suite_add_tcase(s, tc_dim);

       TCase *tc_disabled = tcase_create("ColorsDisabled");
       tcase_add_checked_fixture(tc_disabled, setup, teardown);
       tcase_add_test(tc_disabled, test_notification_no_ansi_when_disabled);
       tcase_add_test(tc_disabled, test_notification_plain_when_disabled);
       tcase_add_test(tc_disabled, test_notification_no_ansi_term_dumb);
       suite_add_tcase(s, tc_disabled);

       TCase *tc_width = tcase_create("DisplayWidth");
       tcase_add_checked_fixture(tc_width, setup, teardown);
       tcase_add_test(tc_width, test_notification_display_width_excludes_ansi);
       tcase_add_test(tc_width, test_notification_unicode_width);
       tcase_add_test(tc_width, test_notification_long_content_width);
       suite_add_tcase(s, tc_width);

       TCase *tc_edge = tcase_create("EdgeCases");
       tcase_add_checked_fixture(tc_edge, setup, teardown);
       tcase_add_test(tc_edge, test_notification_empty_content);
       tcase_add_test(tc_edge, test_notification_null_content);
       tcase_add_test(tc_edge, test_notification_with_newlines);
       tcase_add_test(tc_edge, test_notification_is_visible_kind);
       tcase_add_test(tc_edge, test_multiple_notifications_styled);
       suite_add_tcase(s, tc_edge);

       TCase *tc_isolation = tcase_create("StyleIsolation");
       tcase_add_checked_fixture(tc_isolation, setup, teardown);
       tcase_add_test(tc_isolation, test_notification_style_isolated);
       tcase_add_test(tc_isolation, test_preceding_content_unaffected);
       tcase_add_test(tc_isolation, test_assistant_after_notification_own_style);
       suite_add_tcase(s, tc_isolation);

       return s;
   }

   int main(void)
   {
       Suite *s = notification_style_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       int number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

3. Update Makefile:
   - Verify `tests/unit/event_render/notification_style_test.c` is picked up by wildcard

4. Run `make check` - expect test failures (dim styling not implemented)

### Green
1. Add `IK_ANSI_DIM` constant to `src/ansi.h`:
   ```c
   // SGR sequence literals
   #define IK_ANSI_RESET "\x1b[0m"
   #define IK_ANSI_DIM   "\x1b[2m"
   ```

2. Add dim style helper to `src/event_render.c`:
   ```c
   // Helper: apply dim styling to notification content
   static char *apply_dim_style(TALLOC_CTX *ctx, const char *content)
   {
       if (!ik_ansi_colors_enabled()) {
           return talloc_strdup(ctx, content);
       }

       char *styled = talloc_asprintf(ctx, "%s%s%s",
           IK_ANSI_DIM, content, IK_ANSI_RESET);
       if (styled == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       return styled;
   }
   ```

3. Add notification rendering to `src/event_render.c` `ik_event_render()`:
   ```c
   // Handle notification events (mail notifications with dim styling)
   if (strcmp(kind, "notification") == 0) {
       // Skip empty content
       if (content == NULL || content[0] == '\0') {
           return OK(NULL);
       }

       TALLOC_CTX *tmp = talloc_new(NULL);
       if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Trim trailing whitespace
       char *trimmed = ik_scrollback_trim_trailing(tmp, content, strlen(content));

       // Skip if empty after trimming
       if (trimmed[0] == '\0') {
           talloc_free(tmp);
           return OK(NULL);
       }

       // Apply dim styling
       char *styled = apply_dim_style(tmp, trimmed);

       // Append to scrollback
       res_t result = ik_scrollback_append_line(scrollback, styled, strlen(styled));
       if (is_err(&result)) {
           talloc_free(tmp);
           return result;
       }

       // Append blank line for spacing
       result = ik_scrollback_append_line(scrollback, "", 0);

       talloc_free(tmp);
       return result;
   }
   ```

4. Ensure `ik_event_renders_visible()` includes "notification":
   ```c
   bool ik_event_renders_visible(const char *kind)
   {
       if (kind == NULL) {
           return false;
       }

       // These kinds render visible content
       if (strcmp(kind, "user") == 0 ||
           strcmp(kind, "assistant") == 0 ||
           strcmp(kind, "system") == 0 ||
           strcmp(kind, "mark") == 0 ||
           strcmp(kind, "notification") == 0) {  // Add notification
           return true;
       }

       return false;
   }
   ```

5. Run `make check` - expect all tests pass

### Refactor
1. Verify include order follows style guide in event_render.c:
   - Own header first (`event_render.h`)
   - Project headers next (`ansi.h`, `panic.h`, `scrollback.h`, etc.)
   - Vendor headers (`vendor/yyjson/yyjson.h`)
   - System headers last (`<assert.h>`, `<string.h>`, `<talloc.h>`)

2. Verify `// comments` style used (not `/* */`)

3. Verify assert() statements have LCOV_EXCL_BR_LINE comments where used

4. Consider: Is `apply_dim_style()` too similar to `apply_style()`?
   - Current: Separate functions for color vs dim
   - Alternative: Unify with a style parameter
   - Decision: Keep separate for clarity - color is 256-color palette, dim is SGR attribute

5. Run `make lint` - verify clean

6. Run `make coverage` - verify 100% coverage on new code

7. Run `make check-valgrind` - verify no memory leaks

8. Manual verification:
   - Start ikigai with debug mode
   - Send mail to current agent from another agent
   - Let agent complete a task and go IDLE
   - Verify notification appears in scrollback (dimmed)
   - Verify dim is visually distinct from regular text
   - Verify text after notification is NOT dimmed
   - Set NO_COLOR=1, verify notification appears without ANSI codes

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100% on modified code in:
  - `src/ansi.h` (new IK_ANSI_DIM constant)
  - `src/event_render.c` (notification kind handling, apply_dim_style)
- Notifications styled with ANSI dim when colors enabled
- Format: `\x1b[2m{content}\x1b[0m`
- Plain text output when colors disabled (NO_COLOR, TERM=dumb)
- Display width calculation excludes ANSI sequences
- Style reset prevents bleeding into subsequent content
- `ik_event_renders_visible("notification")` returns true
- Tests cover:
  - Dim styling application (prefix, suffix, content)
  - Colors disabled behavior (plain text)
  - Display width calculation (excludes ANSI)
  - Edge cases (empty, null, newlines)
  - Style isolation (doesn't affect other messages)
- src/ansi.h updated with IK_ANSI_DIM constant
- src/event_render.c updated with notification styling

## Notes

### ANSI Dim Attribute

The SGR (Select Graphic Rendition) code 2 sets the "dim" or "faint" attribute:
- Standard sequence: `\x1b[2m` (ESC [ 2 m)
- Reset: `\x1b[0m` (ESC [ 0 m) - resets all attributes

**Terminal support:**
- Most modern terminals support dim
- Terminals that don't support it display text normally (graceful degradation)
- Some terminals render dim as slightly darker text, others as lower intensity

**Why dim instead of gray color?**
- Dim is semantically correct (de-emphasize, not colorize)
- Works with terminal themes (adapts to light/dark backgrounds)
- Stacks with other attributes if needed later
- Simpler (no color palette index)

### Visual Hierarchy

After this task, the visual hierarchy in scrollback is:
1. **User input** - Default terminal color (most prominent)
2. **Assistant output** - Gray 249 (slightly subdued)
3. **System/tool messages** - Gray 242 (very subdued)
4. **Notifications** - Dim attribute (de-emphasized but readable)

Notifications should be visually lower in the hierarchy than conversation content but still fully readable.

### LLM Context Separation

The LLM context uses plain text without ANSI codes:
- Conversation messages: Created in `ik_repl_check_mail_notification()` as plain text
- Scrollback: Rendered with ANSI codes via `ik_event_render()`

This separation exists from the notification injection task. The notification text is:
1. Added to conversation as plain text (for LLM)
2. Rendered to scrollback with dim styling (for terminal)

### Testing Strategy

Tests are organized by behavior:

1. **Dim styling tests**: Verify ANSI codes applied correctly
   - Prefix present
   - Suffix present
   - Content preserved
   - Uses dim, not color
   - Complete format matches expected

2. **Colors disabled tests**: Verify graceful degradation
   - NO_COLOR environment variable
   - TERM=dumb
   - Plain text output

3. **Display width tests**: Verify ANSI doesn't affect layout
   - Zero width for escape sequences
   - Correct content width
   - Long content handling

4. **Edge case tests**: Verify robustness
   - Empty content
   - Null content
   - Embedded newlines
   - Multiple notifications

5. **Style isolation tests**: Verify no bleeding
   - Subsequent messages unaffected
   - Preceding messages unaffected
   - Different message kinds maintain their styling

### Relationship to Other Tasks

```
mail-notification-inject.md  (injects notification as "notification" kind)
        |
mail-notification-style.md   (this task - applies dim styling)
        |
        v
User sees dimmed notification in scrollback
```

This task depends on the notification injection being complete and working. The injection task creates the "notification" kind event; this task applies the visual styling.

### Memory Ownership

```
event_render() call
    |
    +-> tmp (TALLOC_CTX) - temporary context
    |     |
    |     +-> trimmed - trimmed content
    |     |
    |     +-> styled - styled content with ANSI codes
    |
    +-> styled text copied to scrollback->text_buffer
    |
    +-> tmp freed (releases trimmed and styled)
```

The styled text is copied into the scrollback's text buffer. The temporary allocations are freed before returning.

### Future Considerations

1. **Configurable styling**: Could add config option to change notification style (dim, color, underline, etc.)

2. **Highlighting for urgent**: Future priority mail could use different styling (e.g., yellow highlight)

3. **Theme support**: Notifications could respect terminal color themes more explicitly

4. **Accessibility**: Screen reader announcements for notifications (separate feature)

5. **Rich formatting**: Future notifications could include icons or other formatting (outside v1 scope)
