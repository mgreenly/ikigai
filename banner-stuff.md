# Banner Layer Cursor Bug Fix

## Problem
When scrollback fills enough that scrolling is enabled:
1. Cursor renders one line below where it should be
2. Banner disappears when partially scrolled out of view

## Root Causes

### 1. Banner layer ignored partial rendering parameters
`banner_render()` was ignoring `start_row` and `row_count` parameters, always rendering all 6 rows even when only part of the banner was in the viewport.

### 2. Document height calculation was wrong
`calculate_document_height()` didn't properly account for:
- Spinner layer (1 row when visible)
- Status layer (2 rows, was counted as 1)

### 3. Input buffer position calculation missed spinner
`input_buffer_start_doc_row` didn't account for the spinner layer position in the layer stack.

## Changes

### src/layer_banner.c
- Added conditional rendering for each row: `if (N >= start_row && N < end_row)`
- Each of the 6 banner rows now only renders when within the visible viewport range
- Fixed tagline text to "Agentic Orchestration" (was "AI Orchestration")
- Added debug logging for start_row/row_count

### src/repl_viewport.c
- Fixed `calculate_document_height()` to properly sum all layer heights:
  - banner (6 when visible)
  - scrollback (variable)
  - spinner (1 when visible)
  - separator (1)
  - input (1+ when visible)
  - completion (variable)
  - status (2 when visible)
- Fixed `separator_row` calculation to include spinner_rows
- Fixed `input_buffer_start_doc_row` to be `separator_row + 1`
- Added debug logging for viewport calculations and cursor position

### src/layer.c
- Added debug logging to `ik_layer_cake_render()` showing:
  - Viewport parameters
  - Each layer's rendering parameters or skip reason

### src/repl_event_handlers.c
- Removed noisy debug output for terminal reads

### tests/unit/repl/repl_cursor_position_basic_test.c
- Set `banner_visible = false` (test doesn't include banner layer)
- Updated test expectations for corrected document model

## Test Status
14 viewport-related tests fail due to changed document model. Tests were written assuming:
- Status layer = 1 row (actually 2)
- No spinner layer in calculation

Tests need expectations updated to match correct document model.

## Debug Output
Build with `make BUILD=debug` and check `IKIGAI_DEBUG.LOG` for:
- `banner_render: start_row=X, row_count=Y, end_row=Z`
- `layer_cake_render: viewport_row=X, viewport_height=Y`
- `layer[N] 'name': current_row=X, layer_height=Y, start_row=Z, row_count=W`
- `render_frame: banner_visible=X, document_height=Y, terminal_rows=Z, first_visible_row=W`
- `cursor: input_buffer_start_row=X, final_cursor_row=Y, final_cursor_col=Z`
