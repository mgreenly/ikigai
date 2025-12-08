# Task: Completion Display Layer

## Target
Feature: Tab Completion - UI Rendering

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/patterns/composite.md

### Pre-read Docs
- docs/backlog/readline-features.md (completion display specification)

### Pre-read Source (patterns)
- src/layer.h (layer interface, ik_layer_cake_t)
- src/layer_wrappers.h (existing layer wrappers)
- src/layer_wrappers.c (spinner layer, separator layer implementations)
- src/repl.h (layer ownership in ik_repl_ctx_t)

### Pre-read Tests (patterns)
- tests/unit/layer/cake_test.c (layer cake rendering tests)
- tests/unit/layer/wrappers_test.c (wrapper layer tests if exist)

## Pre-conditions
- `make check` passes
- Layer system works (scrollback, separator, input, spinner layers exist)
- Completion data structures exist
- Layer cake can render multiple layers

## Task
Create completion layer for rendering tab completion suggestions below the input buffer:

**Display format**:
```
  command   description
```

**Layout**:
- Shown below input buffer layer (between input and bottom separator)
- Each candidate on one line with padding
- Current selection highlighted (ANSI reverse video)
- Maximum 10 lines (enforced by completion matching)
- Only visible when `repl->completion != NULL`

**Layer responsibilities**:
- `is_visible`: Return true if completion context exists
- `get_height`: Return number of candidates (0 if not visible)
- `render`: Render each candidate with description, highlight current selection

## TDD Cycle

### Red
1. Add declaration to `src/layer_wrappers.h`:
   ```c
   // Forward declaration
   typedef struct ik_completion_t ik_completion_t;

   // Create completion layer (shows command completions)
   // completion_ptr must remain valid for layer lifetime
   // command_descriptions_ptr provides descriptions for display
   ik_layer_t *ik_completion_layer_create(TALLOC_CTX *ctx,
                                           const char *name,
                                           ik_completion_t **completion_ptr);
   ```

2. Create `tests/unit/layer/completion_layer_test.c`:
   ```c
   START_TEST(test_completion_layer_visibility)
   {
       ik_completion_t *comp = NULL;
       ik_layer_t *layer = ik_completion_layer_create(NULL, "completion", &comp);

       // Not visible when completion is NULL
       ck_assert(!layer->is_visible(layer));

       // Create completion
       comp = ik_completion_create_for_commands(NULL, "/m");
       ck_assert(layer->is_visible(layer));

       talloc_free(comp);
       talloc_free(layer);
   }
   END_TEST

   START_TEST(test_completion_layer_height)
   {
       ik_completion_t *comp = ik_completion_create_for_commands(NULL, "/m");
       ik_layer_t *layer = ik_completion_layer_create(NULL, "completion", &comp);

       // Height = number of candidates
       size_t height = layer->get_height(layer, 80);
       ck_assert_int_eq(height, comp->count);

       talloc_free(comp);
       talloc_free(layer);
   }
   END_TEST

   START_TEST(test_completion_layer_render)
   {
       // Create completion with known commands
       ik_completion_t *comp = ik_completion_create_for_commands(NULL, "/m");
       ik_layer_t *layer = ik_completion_layer_create(NULL, "completion", &comp);

       ik_output_buffer_t *buf = ik_output_buffer_create(NULL, 1024);
       layer->render(layer, buf, 80, 0, comp->count);

       // Verify output contains command names
       ck_assert_ptr_nonnull(strstr(buf->data, "mark"));
       ck_assert_ptr_nonnull(strstr(buf->data, "model"));

       // Verify first entry is highlighted (ANSI reverse video)
       ck_assert_ptr_nonnull(strstr(buf->data, "\x1b[7m"));  // Reverse video

       talloc_free(buf);
       talloc_free(comp);
       talloc_free(layer);
   }
   END_TEST

   START_TEST(test_completion_layer_selection_highlight)
   {
       ik_completion_t *comp = ik_completion_create_for_commands(NULL, "/m");
       comp->current = 1;  // Select second entry

       ik_layer_t *layer = ik_completion_layer_create(NULL, "completion", &comp);
       ik_output_buffer_t *buf = ik_output_buffer_create(NULL, 1024);
       layer->render(layer, buf, 80, 0, comp->count);

       // Verify "model" is highlighted, not "mark"
       // This is complex to test precisely - may need to parse output

       talloc_free(buf);
       talloc_free(comp);
       talloc_free(layer);
   }
   END_TEST
   ```

3. Run `make check` - expect test failures

### Green
1. Implement in `src/layer_wrappers.c`:
   ```c
   // Completion layer data
   typedef struct {
       ik_completion_t **completion_ptr;  // Raw pointer to completion
   } completion_layer_data_t;

   static bool completion_is_visible(const ik_layer_t *layer) {
       completion_layer_data_t *data = (completion_layer_data_t *)layer->data;
       return data->completion_ptr != NULL && *data->completion_ptr != NULL;
   }

   static size_t completion_get_height(const ik_layer_t *layer, size_t width) {
       (void)width;  // Unused
       completion_layer_data_t *data = (completion_layer_data_t *)layer->data;
       if (!completion_is_visible(layer)) {
           return 0;
       }
       return (*data->completion_ptr)->count;
   }

   static void completion_render(const ik_layer_t *layer,
                                  ik_output_buffer_t *output,
                                  size_t width,
                                  size_t start_row,
                                  size_t row_count) {
       (void)width;
       (void)start_row;
       (void)row_count;

       completion_layer_data_t *data = (completion_layer_data_t *)layer->data;
       ik_completion_t *comp = *data->completion_ptr;
       if (comp == NULL) return;

       // Get command descriptions
       size_t cmd_count;
       const ik_command_t *commands = ik_cmd_get_all(&cmd_count);

       for (size_t i = 0; i < comp->count; i++) {
           const char *cmd_name = comp->candidates[i];
           const char *description = "";

           // Find description
           for (size_t j = 0; j < cmd_count; j++) {
               if (strcmp(commands[j].name, cmd_name) == 0) {
                   description = commands[j].description;
                   break;
               }
           }

           // Highlight current selection
           if (i == comp->current) {
               ik_output_buffer_append(output, "\x1b[7m", 4);  // Reverse video
           }

           // Format: "  command   description"
           char line[256];
           snprintf(line, sizeof(line), "  %-10s %s", cmd_name, description);
           ik_output_buffer_append(output, line, strlen(line));

           if (i == comp->current) {
               ik_output_buffer_append(output, "\x1b[0m", 4);  // Reset
           }

           ik_output_buffer_append(output, "\r\n", 2);
       }
   }

   ik_layer_t *ik_completion_layer_create(TALLOC_CTX *ctx,
                                           const char *name,
                                           ik_completion_t **completion_ptr) {
       completion_layer_data_t *data = talloc(ctx, completion_layer_data_t);
       if (data == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       data->completion_ptr = completion_ptr;

       return ik_layer_create(ctx, name, data,
                             completion_is_visible,
                             completion_get_height,
                             completion_render);
   }
   ```

2. Add completion layer to REPL context in `src/repl.h`:
   ```c
   typedef struct ik_repl_ctx_t {
       // ... existing layers ...
       ik_layer_t *completion_layer;  // Completion suggestions layer

       // Completion state
       ik_completion_t *completion;    // Current completion context (NULL if inactive)
   } ik_repl_ctx_t;
   ```

3. Initialize layer in `src/repl.c` (ik_repl_init):
   ```c
   repl->completion = NULL;
   repl->completion_layer = ik_completion_layer_create(repl, "completion",
                                                        &repl->completion);
   TRY(ik_layer_cake_add_layer(repl->layer_cake, repl->completion_layer));
   ```

4. Run `make check` - expect pass

### Refactor
1. Consider column width calculations for proper alignment
2. Ensure ANSI codes don't break layout
3. Test with long command names and descriptions
4. Verify layer ordering (completion should be after input, before bottom)
5. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- Completion layer exists and renders correctly
- Current selection is highlighted with reverse video
- Command descriptions are shown
- Layer is only visible when completion context exists
- Height calculation matches number of candidates
- Layer is integrated into layer cake
- 100% test coverage maintained
