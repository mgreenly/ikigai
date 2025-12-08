# Task: Context-Aware Argument Completion

## Target
Feature: Tab Completion - Argument Completion

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/patterns/strategy.md

### Pre-read Docs
- docs/backlog/readline-features.md (argument completion specification)

### Pre-read Source (patterns)
- src/completion.h (completion data structures)
- src/completion.c (command matching logic)
- src/commands.h (command registry)
- src/config.h (model configuration for /model completion)
- src/marks.h (marks for /rewind completion)

### Pre-read Tests (patterns)
- tests/unit/completion/matching_test.c (completion matching tests)

## Pre-conditions
- `make check` passes
- Command completion works
- Basic completion navigation works
- Command registry provides command list
- Marks system exists (for /rewind)

## Task
Implement context-aware argument completion for specific commands:

**Commands with argument completion**:
- `/model` - Complete from available model names (from config or registry)
- `/rewind` - Complete from existing mark labels
- `/debug` - Complete from ["on", "off"]

**Commands without argument completion**:
- `/mark`, `/system`, `/help`, `/clear` - No completion (freeform or no args)

**Detection logic**:
- Parse input to extract command and argument parts
- If input is `/cmd `, trigger argument completion for that command
- Match argument prefix against available values
- Format and display same as command completion

## TDD Cycle

### Red
1. Add function declarations to `src/completion.h`:
   ```c
   // Create completion for command arguments
   // Returns NULL if command has no argument completion or no matches
   ik_completion_t *ik_completion_create_for_arguments(TALLOC_CTX *ctx,
                                                        const char *command,
                                                        const char *arg_prefix,
                                                        ik_repl_ctx_t *repl);
   ```

2. Extend `tests/unit/completion/matching_test.c`:
   ```c
   START_TEST(test_completion_model_arguments)
   {
       // Setup: Config with models or mock model list
       // Input: "/model g"
       // Expect: Completions for models starting with 'g'
       ik_completion_t *comp = ik_completion_create_for_arguments(
           NULL, "model", "g", repl);
       ck_assert_ptr_nonnull(comp);
       // Verify model names in candidates
       talloc_free(comp);
   }
   END_TEST

   START_TEST(test_completion_rewind_arguments)
   {
       // Setup: Create marks "checkpoint1", "checkpoint2"
       // Input: "/rewind check"
       // Expect: Both checkpoints match
       ik_completion_t *comp = ik_completion_create_for_arguments(
           NULL, "rewind", "check", repl);
       ck_assert_ptr_nonnull(comp);
       ck_assert_int_eq(comp->count, 2);
       talloc_free(comp);
   }
   END_TEST

   START_TEST(test_completion_debug_arguments)
   {
       // Input: "/debug o"
       // Expect: ["on", "off"]
       ik_completion_t *comp = ik_completion_create_for_arguments(
           NULL, "debug", "o", repl);
       ck_assert_ptr_nonnull(comp);
       ck_assert_int_eq(comp->count, 2);
       ck_assert_str_eq(comp->candidates[0], "off");
       ck_assert_str_eq(comp->candidates[1], "on");
       talloc_free(comp);
   }
   END_TEST

   START_TEST(test_completion_no_args_for_mark)
   {
       // Input: "/mark lab"
       // Expect: NULL (mark takes freeform label)
       ik_completion_t *comp = ik_completion_create_for_arguments(
           NULL, "mark", "lab", repl);
       ck_assert_ptr_null(comp);
   }
   END_TEST

   START_TEST(test_completion_empty_arg_prefix)
   {
       // Input: "/debug " (space after command, no arg text)
       // Expect: All debug options ["off", "on"]
       ik_completion_t *comp = ik_completion_create_for_arguments(
           NULL, "debug", "", repl);
       ck_assert_ptr_nonnull(comp);
       ck_assert_int_eq(comp->count, 2);
       talloc_free(comp);
   }
   END_TEST
   ```

3. Run `make check` - expect test failures

### Green
1. Implement in `src/completion.c`:
   ```c
   ik_completion_t *ik_completion_create_for_arguments(TALLOC_CTX *ctx,
                                                        const char *command,
                                                        const char *arg_prefix,
                                                        ik_repl_ctx_t *repl) {
       assert(command != NULL);
       assert(arg_prefix != NULL);
       assert(repl != NULL);

       const char **options = NULL;
       size_t option_count = 0;

       // Determine which arguments to complete based on command
       if (strcmp(command, "debug") == 0) {
           static const char *debug_opts[] = {"off", "on"};
           options = debug_opts;
           option_count = 2;
       } else if (strcmp(command, "model") == 0) {
           // TODO: Get from config or model registry
           // For now, hardcode common models
           static const char *models[] = {
               "gpt-3.5-turbo",
               "gpt-4",
               "gpt-4-turbo",
               "claude-3-opus",
               "claude-3-sonnet"
           };
           options = models;
           option_count = 5;
       } else if (strcmp(command, "rewind") == 0) {
           // Get mark labels from REPL context
           if (repl->mark_count == 0) {
               return NULL;  // No marks to complete
           }
           // Build temporary array of mark labels
           char **labels = talloc_array(NULL, char *, repl->mark_count);
           if (labels == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

           for (size_t i = 0; i < repl->mark_count; i++) {
               labels[i] = repl->marks[i]->label;
           }
           options = (const char **)labels;
           option_count = repl->mark_count;
       } else {
           // No argument completion for this command
           return NULL;
       }

       // Match options against prefix
       size_t prefix_len = strlen(arg_prefix);
       size_t match_count = 0;
       for (size_t i = 0; i < option_count; i++) {
           if (strncmp(options[i], arg_prefix, prefix_len) == 0) {
               match_count++;
           }
       }

       if (match_count == 0) {
           if (strcmp(command, "rewind") == 0) {
               talloc_free((void *)options);  // Free temporary array
           }
           return NULL;
       }

       // Create completion context
       ik_completion_t *comp = talloc_zero(ctx, ik_completion_t);
       if (comp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       comp->prefix = talloc_asprintf(comp, "/%s %s", command, arg_prefix);
       if (comp->prefix == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       comp->count = match_count > 10 ? 10 : match_count;
       comp->candidates = talloc_array(comp, char *, comp->count);
       if (comp->candidates == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Collect matches
       size_t idx = 0;
       for (size_t i = 0; i < option_count && idx < comp->count; i++) {
           if (strncmp(options[i], arg_prefix, prefix_len) == 0) {
               comp->candidates[idx] = talloc_strdup(comp->candidates, options[i]);
               if (comp->candidates[idx] == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
               idx++;
           }
       }

       if (strcmp(command, "rewind") == 0) {
           talloc_free((void *)options);  // Free temporary array
       }

       comp->current = 0;
       return comp;
   }
   ```

2. Modify TAB handler in `src/repl_actions.c` to detect arguments:
   ```c
   res_t handle_tab(ik_repl_ctx_t *repl) {
       size_t len;
       const char *text = ik_input_buffer_get_text(repl->input_buffer, &len);

       if (repl->completion != NULL) {
           // Accept logic (same as before)
           // ... existing code ...
       }

       // No completion active - try to create one
       if (len == 0 || text[0] != '/') {
           return OK(NULL);
       }

       // Parse input: "/command argument"
       const char *space = strchr(text + 1, ' ');
       if (space != NULL) {
           // Has space - might be argument completion
           size_t cmd_len = space - (text + 1);
           char *command = talloc_strndup(repl, text + 1, cmd_len);
           const char *arg_prefix = space + 1;

           repl->completion = ik_completion_create_for_arguments(
               repl, command, arg_prefix, repl);
           talloc_free(command);
       } else {
           // No space - command completion
           repl->completion = ik_completion_create_for_commands(repl, text);
       }

       return OK(NULL);
   }
   ```

3. Run `make check` - expect pass

### Refactor
1. Consider moving model list to config or separate registry
2. Sort mark labels alphabetically for consistency
3. Handle edge cases: multiple spaces, trailing spaces
4. Extract argument provider logic to separate functions (strategy pattern)
5. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- `/model` completion works (lists available models)
- `/rewind` completion works (lists mark labels)
- `/debug` completion works (shows "on", "off")
- Commands without argument completion return NULL
- Empty argument prefix shows all options
- Prefix matching is case-sensitive
- Maximum 10 results enforced
- 100% test coverage maintained
