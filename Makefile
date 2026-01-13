PACKAGE = ikigai
VERSION = $(shell grep '\#define IK_VERSION "' src/version.h | cut -d'"' -f2)

PREFIX ?= /usr/local
bindir ?= $(PREFIX)/bin
libexecdir ?= $(PREFIX)/libexec
datadir ?= $(PREFIX)/share

# Special handling for sysconfdir and configdir based on PREFIX
HOME_DIR := $(shell echo $$HOME)

# Detect install type
IS_OPT_INSTALL := $(if $(filter /opt%,$(PREFIX)),yes,no)
IS_USER_INSTALL := $(if $(findstring /home/,$(PREFIX)),yes,no)

ifeq ($(PREFIX),/usr)
    # /usr uses /etc not /usr/etc
    sysconfdir ?= /etc
    configdir = /etc/ikigai
else ifeq ($(IS_OPT_INSTALL),yes)
    # PREFIX is /opt/* - use PREFIX/etc not PREFIX/etc/ikigai
    sysconfdir ?= $(PREFIX)/etc
    configdir = $(PREFIX)/etc
else ifeq ($(IS_USER_INSTALL),yes)
    # PREFIX is under /home/* - use XDG variables or defaults
    XDG_CONFIG_HOME ?= $(shell echo $${XDG_CONFIG_HOME:-$(HOME_DIR)/.config})
    XDG_DATA_HOME ?= $(shell echo $${XDG_DATA_HOME:-$(HOME_DIR)/.local/share})
    sysconfdir ?= $(XDG_CONFIG_HOME)
    configdir = $(XDG_CONFIG_HOME)/ikigai
    user_datadir = $(XDG_DATA_HOME)/ikigai
else
    # Default: /usr/local and others use PREFIX/etc/ikigai
    sysconfdir ?= $(PREFIX)/etc
    configdir = $(sysconfdir)/ikigai
endif

CC = gcc

# Distribution list (default: all distros/*/ directories)
DISTROS ?= $(notdir $(patsubst %/,%,$(wildcard distros/*/)))

# Warning flags (always enabled)
WARNING_FLAGS = -Wall -Wextra -Wshadow \
  -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings \
  -Wformat=2 -Wconversion -Wcast-qual -Wundef \
  -Wdate-time -Winit-self -Wstrict-overflow=2 \
  -Wimplicit-fallthrough -Walloca -Wvla \
  -Wnull-dereference -Wdouble-promotion -Werror

# Security hardening flags (stack protector works at all optimization levels)
SECURITY_FLAGS = -fstack-protector-strong

# Dependency generation flags
DEP_FLAGS = -MMD -MP

# Debug build flags
DEBUG_FLAGS = -O0 -g3 -fno-omit-frame-pointer -DDEBUG

# Sanitizer flags (for sanitize build)
ifeq ($(SKIP_SIGNAL_TESTS),1)
  SANITIZE_FLAGS = -fsanitize=address,undefined -DSKIP_SIGNAL_TESTS
else
  SANITIZE_FLAGS = -fsanitize=address,undefined
endif

# Thread sanitizer flags (for tsan build)
ifeq ($(SKIP_SIGNAL_TESTS),1)
  TSAN_FLAGS = -fsanitize=thread -DSKIP_SIGNAL_TESTS
else
  TSAN_FLAGS = -fsanitize=thread
endif

# Valgrind build flags (optimized for backtraces)
ifeq ($(SKIP_SIGNAL_TESTS),1)
  VALGRIND_FLAGS = -O0 -g3 -fno-omit-frame-pointer -DDEBUG -DSKIP_SIGNAL_TESTS
else
  VALGRIND_FLAGS = -O0 -g3 -fno-omit-frame-pointer -DDEBUG
endif

# Release build flags (_FORTIFY_SOURCE requires optimization)
RELEASE_FLAGS = -O2 -g -DNDEBUG -D_FORTIFY_SOURCE=2

# Base flags (always present)
BASE_FLAGS = -std=c17 -fPIC -D_GNU_SOURCE -Isrc -I/usr/include/postgresql

# Build type selection (debug, release, sanitize, tsan, or valgrind)
BUILD ?= debug

# Parallelization settings
MAKE_JOBS ?= $(shell expr $$(nproc) / 4)
PARALLEL ?= 0

# Build directory (can be overridden for parallel builds)
BUILDDIR ?= build

# Report subdirectory (derived from BUILDDIR: build->check, build-X->X)
ifeq ($(BUILDDIR),build)
  REPORT_SUBDIR = check
else
  REPORT_SUBDIR = $(patsubst build-%,%,$(BUILDDIR))
endif

ifeq ($(BUILD),release)
  CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(RELEASE_FLAGS)
else ifeq ($(BUILD),sanitize)
  CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(DEBUG_FLAGS) $(SANITIZE_FLAGS)
  LDFLAGS = -fsanitize=address,undefined
else ifeq ($(BUILD),tsan)
  CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(DEBUG_FLAGS) $(TSAN_FLAGS)
  LDFLAGS = -fsanitize=thread
else ifeq ($(BUILD),valgrind)
  CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(VALGRIND_FLAGS)
else
  CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(DEBUG_FLAGS)
endif

# Allow LDFLAGS override if not set by BUILD type
LDFLAGS ?=

CLIENT_LIBS ?= -ltalloc -luuid -lb64 -lpthread -lutf8proc -lcurl -lpq -lxkbcommon
CLIENT_STATIC_LIBS ?=

COMPLEXITY_THRESHOLD = 15
NESTING_DEPTH_THRESHOLD = 5
LINE_LENGTH = 120
# CRITICAL: 16KB limit for all non-vendor files - DO NOT MODIFY THIS VALUE
# Files exceeding 16KB must be refactored into smaller modules
MAX_FILE_BYTES = 16384

# Coverage settings
# LCOV 2.0-1 supports exclusion markers (LCOV_EXCL_BR_LINE, LCOV_EXCL_START/STOP)
# in both static and non-static functions. Investigation confirmed exclusion markers
# work reliably in static functions (see rel-06/docs/lcov-static-fn-findings.md).
# When using exclusion markers: avoid mentioning marker keywords in nearby comments
# to prevent LCOV comment parsing issues.
COVERAGE_DIR = reports/coverage
COVERAGE_CFLAGS = -O0 -fprofile-arcs -ftest-coverage
COVERAGE_LDFLAGS = --coverage
COVERAGE_THRESHOLD = 100
LCOV_EXCL_COVERAGE = 3465

CLIENT_SOURCES = src/client.c src/error.c src/logger.c src/debug_log.c src/config.c src/credentials.c src/paths.c src/wrapper_talloc.c src/wrapper_json.c src/wrapper_curl.c src/wrapper_postgres.c src/wrapper_pthread.c src/wrapper_posix.c src/wrapper_stdlib.c src/wrapper_internal.c src/file_utils.c src/array.c src/byte_array.c src/line_array.c src/terminal.c src/input.c src/input_escape.c src/input_xkb.c src/scroll_detector.c src/input_buffer/core.c src/input_buffer/multiline.c src/input_buffer/cursor.c src/input_buffer/layout.c src/render.c src/render_cursor.c src/repl.c src/repl_agent_mgmt.c src/repl_navigation.c src/repl_event_handlers.c src/repl_tool_completion.c src/repl_init.c src/repl_viewport.c src/repl_actions.c src/repl_actions_completion.c src/repl_actions_history.c src/repl_actions_viewport.c src/repl_actions_llm.c src/repl_callbacks.c src/repl_tool.c src/signal_handler.c src/format.c src/fzy_wrapper.c src/pp_helpers.c src/input_buffer/pp.c src/input_buffer/cursor_pp.c src/scrollback.c src/scrollback_layout.c src/scrollback_render.c src/scrollback_utils.c src/panic.c src/json_allocator.c src/vendor/yyjson/yyjson.c src/vendor/fzy/match.c src/layer.c src/layer_separator.c src/layer_scrollback.c src/layer_input.c src/layer_spinner.c src/layer_completion.c src/commands.c src/commands_basic.c src/commands_model.c src/commands_fork.c src/commands_fork_args.c src/commands_fork_helpers.c src/commands_kill.c src/commands_agent_list.c src/commands_mail.c src/commands_mail_helpers.c src/commands_mark.c src/commands_tool.c src/marks.c src/history.c src/history_io.c src/completion.c src/debug_pipe.c src/db/connection.c src/db/migration.c src/db/pg_result.c src/db/session.c src/db/message.c src/db/replay.c src/db/agent.c src/db/agent_row.c src/db/agent_zero.c src/db/agent_replay.c src/db/mail.c src/repl/agent_restore.c src/repl/agent_restore_replay.c src/event_render.c src/tool_arg_parser.c src/tool_call.c src/tool_registry.c src/tool_discovery.c src/tool_external.c src/tool_wrapper.c src/msg.c src/message.c src/ansi.c src/shared.c src/agent.c src/agent_messages.c src/agent_state.c src/agent_provider.c src/uuid.c src/mail/msg.c src/providers/provider.c src/providers/factory.c src/providers/stubs.c src/providers/request.c src/providers/request_tools.c src/providers/response.c src/providers/common/error_utils.c src/providers/common/http_multi.c src/providers/common/http_multi_info.c src/providers/common/sse_parser.c src/providers/openai/serialize.c src/providers/openai/openai.c src/providers/openai/openai_handlers.c src/providers/openai/reasoning.c src/providers/openai/error.c src/providers/openai/request_chat.c src/providers/openai/request_responses.c src/providers/openai/response_chat.c src/providers/openai/response_responses.c src/providers/openai/streaming_chat.c src/providers/openai/streaming_chat_delta.c src/providers/openai/streaming_responses.c src/providers/openai/streaming_responses_events.c src/providers/anthropic/anthropic.c src/providers/anthropic/thinking.c src/providers/anthropic/error.c src/providers/anthropic/request.c src/providers/anthropic/request_serialize.c src/providers/anthropic/response.c src/providers/anthropic/response_helpers.c src/providers/anthropic/streaming.c src/providers/anthropic/streaming_events.c src/providers/google/google.c src/providers/google/thinking.c src/providers/google/error.c src/providers/google/request.c src/providers/google/request_helpers.c src/providers/google/response.c src/providers/google/response_utils.c src/providers/google/response_error.c src/providers/google/streaming.c src/providers/google/streaming_helpers.c
CLIENT_OBJ = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(CLIENT_SOURCES))
CLIENT_TARGET = bin/ikigai

UNIT_TEST_SOURCES = $(wildcard tests/unit/*/*_test.c) $(wildcard tests/unit/*/*/*_test.c)
UNIT_TEST_TARGETS = $(patsubst tests/unit/%.c,$(BUILDDIR)/tests/unit/%,$(UNIT_TEST_SOURCES))

INTEGRATION_TEST_SOURCES = $(wildcard tests/integration/*_test.c)
INTEGRATION_TEST_TARGETS = $(patsubst tests/integration/%_test.c,$(BUILDDIR)/tests/integration/%_test,$(INTEGRATION_TEST_SOURCES))

DB_INTEGRATION_TEST_SOURCES = $(wildcard tests/integration/db/*_test.c)
DB_INTEGRATION_TEST_TARGETS = $(patsubst tests/integration/db/%_test.c,$(BUILDDIR)/tests/integration/db/%_test,$(DB_INTEGRATION_TEST_SOURCES))

TEST_TARGETS = $(UNIT_TEST_TARGETS) $(INTEGRATION_TEST_TARGETS) $(DB_INTEGRATION_TEST_TARGETS)

# Single test filtering: make check TEST=config_test
ifdef TEST
  FILTERED_TEST = $(filter %/$(TEST) %/$(TEST)_test,$(TEST_TARGETS))
  ifeq ($(FILTERED_TEST),)
    $(error No test matching '$(TEST)' found in TEST_TARGETS)
  endif
endif

MODULE_SOURCES = src/error.c src/logger.c src/config.c src/credentials.c src/paths.c src/debug_log.c src/wrapper_talloc.c src/wrapper_json.c src/wrapper_curl.c src/wrapper_postgres.c src/wrapper_pthread.c src/wrapper_posix.c src/wrapper_stdlib.c src/wrapper_internal.c src/file_utils.c src/array.c src/byte_array.c src/line_array.c src/terminal.c src/input.c src/input_escape.c src/input_xkb.c src/scroll_detector.c src/input_buffer/core.c src/input_buffer/multiline.c src/input_buffer/cursor.c src/input_buffer/layout.c src/repl.c src/repl_agent_mgmt.c src/repl_navigation.c src/repl_event_handlers.c src/repl_tool_completion.c src/repl_init.c src/repl_viewport.c src/repl_actions.c src/repl_actions_completion.c src/repl_actions_history.c src/repl_actions_viewport.c src/repl_actions_llm.c src/repl_callbacks.c src/repl_tool.c src/signal_handler.c src/render.c src/render_cursor.c src/format.c src/fzy_wrapper.c src/pp_helpers.c src/input_buffer/pp.c src/input_buffer/cursor_pp.c src/scrollback.c src/scrollback_layout.c src/scrollback_render.c src/scrollback_utils.c src/panic.c src/json_allocator.c src/vendor/yyjson/yyjson.c src/vendor/fzy/match.c src/layer.c src/layer_separator.c src/layer_scrollback.c src/layer_input.c src/layer_spinner.c src/layer_completion.c src/commands.c src/commands_basic.c src/commands_model.c src/commands_fork.c src/commands_fork_args.c src/commands_fork_helpers.c src/commands_kill.c src/commands_agent_list.c src/commands_mail.c src/commands_mail_helpers.c src/commands_mark.c src/commands_tool.c src/marks.c src/history.c src/history_io.c src/completion.c src/debug_pipe.c src/db/connection.c src/db/migration.c src/db/pg_result.c src/db/session.c src/db/message.c src/db/replay.c src/db/agent.c src/db/agent_row.c src/db/agent_zero.c src/db/agent_replay.c src/db/mail.c src/repl/agent_restore.c src/repl/agent_restore_replay.c src/event_render.c src/tool_arg_parser.c src/tool_call.c src/tool_registry.c src/tool_discovery.c src/tool_external.c src/tool_wrapper.c src/msg.c src/message.c src/ansi.c src/shared.c src/agent.c src/agent_messages.c src/agent_state.c src/agent_provider.c src/uuid.c src/mail/msg.c src/providers/provider.c src/providers/factory.c src/providers/stubs.c src/providers/request.c src/providers/request_tools.c src/providers/response.c src/providers/common/error_utils.c src/providers/common/http_multi.c src/providers/common/http_multi_info.c src/providers/common/sse_parser.c src/providers/openai/serialize.c src/providers/openai/openai.c src/providers/openai/openai_handlers.c src/providers/openai/reasoning.c src/providers/openai/error.c src/providers/openai/request_chat.c src/providers/openai/request_responses.c src/providers/openai/response_chat.c src/providers/openai/response_responses.c src/providers/openai/streaming_chat.c src/providers/openai/streaming_chat_delta.c src/providers/openai/streaming_responses.c src/providers/openai/streaming_responses_events.c src/providers/anthropic/anthropic.c src/providers/anthropic/thinking.c src/providers/anthropic/error.c src/providers/anthropic/request.c src/providers/anthropic/request_serialize.c src/providers/anthropic/response.c src/providers/anthropic/response_helpers.c src/providers/anthropic/streaming.c src/providers/anthropic/streaming_events.c src/providers/google/google.c src/providers/google/thinking.c src/providers/google/error.c src/providers/google/request.c src/providers/google/request_helpers.c src/providers/google/response.c src/providers/google/response_utils.c src/providers/google/response_error.c src/providers/google/streaming.c src/providers/google/streaming_helpers.c
MODULE_OBJ = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(MODULE_SOURCES))

# Module objects without DB (for some tests) - keeps connection for repl_init.c
MODULE_SOURCES_NO_DB = src/error.c src/logger.c src/config.c src/credentials.c src/paths.c src/debug_log.c src/wrapper_talloc.c src/wrapper_json.c src/wrapper_curl.c src/wrapper_postgres.c src/wrapper_pthread.c src/wrapper_posix.c src/wrapper_stdlib.c src/wrapper_internal.c src/file_utils.c src/array.c src/byte_array.c src/line_array.c src/terminal.c src/input.c src/input_escape.c src/input_xkb.c src/scroll_detector.c src/input_buffer/core.c src/input_buffer/multiline.c src/input_buffer/cursor.c src/input_buffer/layout.c src/repl.c src/repl_agent_mgmt.c src/repl_navigation.c src/repl_event_handlers.c src/repl_tool_completion.c src/repl_init.c src/repl_viewport.c src/repl_actions.c src/repl_actions_completion.c src/repl_actions_history.c src/repl_actions_viewport.c src/repl_actions_llm.c src/repl_callbacks.c src/repl_tool.c src/signal_handler.c src/render.c src/render_cursor.c src/format.c src/fzy_wrapper.c src/pp_helpers.c src/input_buffer/pp.c src/input_buffer/cursor_pp.c src/scrollback.c src/scrollback_layout.c src/scrollback_render.c src/scrollback_utils.c src/panic.c src/json_allocator.c src/vendor/yyjson/yyjson.c src/vendor/fzy/match.c src/layer.c src/layer_separator.c src/layer_scrollback.c src/layer_input.c src/layer_spinner.c src/layer_completion.c src/commands.c src/commands_basic.c src/commands_model.c src/commands_fork.c src/commands_fork_args.c src/commands_fork_helpers.c src/commands_kill.c src/commands_agent_list.c src/commands_mail.c src/commands_mail_helpers.c src/commands_mark.c src/commands_tool.c src/marks.c src/history.c src/history_io.c src/completion.c src/debug_pipe.c src/db/connection.c src/db/migration.c src/db/pg_result.c src/db/agent.c src/db/agent_row.c src/db/agent_zero.c src/db/agent_replay.c src/db/mail.c src/repl/agent_restore.c src/repl/agent_restore_replay.c src/event_render.c src/tool_arg_parser.c src/tool_call.c src/tool_registry.c src/tool_discovery.c src/tool_external.c src/tool_wrapper.c src/msg.c src/message.c src/ansi.c src/shared.c src/agent.c src/agent_messages.c src/agent_state.c src/agent_provider.c src/uuid.c src/mail/msg.c src/providers/provider.c src/providers/factory.c src/providers/stubs.c src/providers/request.c src/providers/request_tools.c src/providers/response.c src/providers/common/error_utils.c src/providers/common/http_multi.c src/providers/common/http_multi_info.c src/providers/common/sse_parser.c src/providers/openai/serialize.c src/providers/openai/openai.c src/providers/openai/openai_handlers.c src/providers/openai/reasoning.c src/providers/openai/error.c src/providers/openai/request_chat.c src/providers/openai/request_responses.c src/providers/openai/response_chat.c src/providers/openai/response_responses.c src/providers/openai/streaming_chat.c src/providers/openai/streaming_chat_delta.c src/providers/openai/streaming_responses.c src/providers/openai/streaming_responses_events.c src/providers/anthropic/anthropic.c src/providers/anthropic/thinking.c src/providers/anthropic/error.c src/providers/anthropic/request.c src/providers/anthropic/request_serialize.c src/providers/anthropic/response.c src/providers/anthropic/response_helpers.c src/providers/anthropic/streaming.c src/providers/anthropic/streaming_events.c src/providers/google/google.c src/providers/google/thinking.c src/providers/google/error.c src/providers/google/request.c src/providers/google/request_helpers.c src/providers/google/response.c src/providers/google/response_utils.c src/providers/google/response_error.c src/providers/google/streaming.c src/providers/google/streaming_helpers.c
MODULE_OBJ_NO_DB = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(MODULE_SOURCES_NO_DB))

# Module objects excluding db/agent.c (for repl_init_db_test mocking)
MODULE_OBJ_NO_DB_AGENT = $(filter-out $(BUILDDIR)/db/agent.o $(BUILDDIR)/db/agent_row.o $(BUILDDIR)/db/agent_zero.o,$(MODULE_OBJ_NO_DB))

# Common sources for tool binaries (in libexec/ikigai/)
TOOL_COMMON_SRCS = src/error.c src/panic.c src/wrapper_talloc.c src/logger.c src/json_allocator.c src/vendor/yyjson/yyjson.c

# Test utilities (linked with all tests)
TEST_UTILS_OBJ = $(BUILDDIR)/tests/test_utils.o
TEST_CONTEXTS_OBJ = $(BUILDDIR)/tests/helpers/test_contexts.o
VCR_OBJ = $(BUILDDIR)/tests/helpers/vcr.o
VCR_STUBS_OBJ = $(BUILDDIR)/tests/helpers/vcr_stubs.o
REPL_RUN_COMMON_OBJ = $(BUILDDIR)/tests/unit/repl/repl_run_common.o
REPL_STREAMING_COMMON_OBJ = $(BUILDDIR)/tests/unit/repl/repl_streaming_test_common.o
EQUIVALENCE_FIXTURES_OBJ = $(BUILDDIR)/tests/unit/providers/openai/equivalence_fixtures.o
EQUIVALENCE_COMPARE_OBJ = $(BUILDDIR)/tests/unit/providers/openai/equivalence_compare_basic.o $(BUILDDIR)/tests/unit/providers/openai/equivalence_compare_complex.o
REQUEST_RESPONSES_TEST_HELPERS_OBJ = $(BUILDDIR)/tests/unit/providers/openai/request_responses_test_helpers.o
REQUEST_CHAT_COVERAGE_HELPERS_OBJ = $(BUILDDIR)/tests/unit/providers/openai/request_chat_coverage_helpers.o

.PHONY: all release clean install uninstall check check-unit check-integration check-bash-tool build-tests verify-mocks verify-mocks-anthropic verify-mocks-google verify-mocks-all verify-credentials check-sanitize check-valgrind check-helgrind check-tsan check-dynamic dist fmt lint check-complexity filesize cloc ci install-deps check-coverage help tags distro-check distro-images distro-images-clean distro-clean distro-package clean-test-runs vcr-record-openai vcr-record-anthropic vcr-record-google vcr-record-all bash_tool $(UNIT_TEST_RUNS) $(INTEGRATION_TEST_RUNS)

# Prevent Make from deleting intermediate files (needed for coverage .gcno files)
.SECONDARY:

all: $(CLIENT_TARGET)

release:
	@$(MAKE) clean
	@$(MAKE) all BUILD=release

$(CLIENT_TARGET): $(CLIENT_OBJ) $(VCR_STUBS_OBJ) | bin
	@$(CC) $(LDFLAGS) -o $@ $^ -Wl,-Bstatic $(CLIENT_STATIC_LIBS) -Wl,-Bdynamic $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Vendor files compile with relaxed warnings (no -Werror, disable conversion warnings)
$(BUILDDIR)/vendor/%.o: src/vendor/%.c | $(BUILDDIR)
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -Wno-error -Wno-conversion -Wno-sign-conversion -Wno-double-promotion -Wno-missing-prototypes -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/%_test.o: tests/unit/%_test.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/%_test: $(BUILDDIR)/tests/unit/%_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Terminal PTY test helper compilation - all terminal_pty_* tests require -lutil for openpty()
TERMINAL_PTY_HELPERS_OBJ = $(BUILDDIR)/tests/unit/terminal/terminal_pty_helpers.o

$(BUILDDIR)/tests/unit/terminal/terminal_pty_helpers.o: tests/unit/terminal/terminal_pty_helpers.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/terminal/terminal_pty_test: $(BUILDDIR)/tests/unit/terminal/terminal_pty_test.o $(TERMINAL_PTY_HELPERS_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -lutil $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/terminal/terminal_pty_enable_basic_test: $(BUILDDIR)/tests/unit/terminal/terminal_pty_enable_basic_test.o $(TERMINAL_PTY_HELPERS_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -lutil $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/terminal/terminal_pty_enable_edge_test: $(BUILDDIR)/tests/unit/terminal/terminal_pty_enable_edge_test.o $(TERMINAL_PTY_HELPERS_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -lutil $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/terminal/terminal_pty_probe_test: $(BUILDDIR)/tests/unit/terminal/terminal_pty_probe_test.o $(TERMINAL_PTY_HELPERS_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -lutil $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special case: http_multi_coverage_test includes http_multi.c directly
# so it must exclude http_multi.o from MODULE_OBJ to avoid duplicate symbols
$(BUILDDIR)/tests/unit/providers/common/http_multi_coverage_test: $(BUILDDIR)/tests/unit/providers/common/http_multi_coverage_test.o $(filter-out $(BUILDDIR)/providers/common/http_multi.o,$(MODULE_OBJ)) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special case: http_multi_info_test includes http_multi_info.c directly
# so it must exclude http_multi_info.o from MODULE_OBJ to avoid duplicate symbols
$(BUILDDIR)/tests/unit/providers/common/http_multi_info_test: $(BUILDDIR)/tests/unit/providers/common/http_multi_info_test.o $(filter-out $(BUILDDIR)/providers/common/http_multi_info.o,$(MODULE_OBJ)) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special case: cmd_fork_coverage_test requires its own mocks
$(BUILDDIR)/tests/unit/commands/cmd_fork_coverage_test_mocks.o: tests/unit/commands/cmd_fork_coverage_test_mocks.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/commands/cmd_fork_coverage_test: $(BUILDDIR)/tests/unit/commands/cmd_fork_coverage_test.o $(BUILDDIR)/tests/unit/commands/cmd_fork_coverage_test_mocks.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Note: Provider factory test no longer needs separate stubs since stubs.c
# is now part of MODULE_OBJ and will be replaced when actual providers are implemented

# OpenAI serialize test helper compilation
OPENAI_SERIALIZE_HELPERS_SRC = $(wildcard tests/unit/providers/openai/helpers/*.c)
OPENAI_SERIALIZE_HELPERS_OBJ = $(patsubst tests/unit/providers/openai/helpers/%.c,$(BUILDDIR)/tests/unit/providers/openai/helpers/%.o,$(OPENAI_SERIALIZE_HELPERS_SRC))

$(BUILDDIR)/tests/unit/providers/openai/helpers/%.o: tests/unit/providers/openai/helpers/%.c | $(BUILDDIR)/tests/unit/providers/openai/helpers
	@$(CC) $(CFLAGS) -I tests/unit/providers/openai -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/openai/helpers:
	@mkdir -p $@

$(BUILDDIR)/tests/unit/providers/openai/openai_serialize_test: $(BUILDDIR)/tests/unit/providers/openai/openai_serialize_test.o $(OPENAI_SERIALIZE_HELPERS_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Message test helper compilation
MESSAGE_TEST_HELPERS_OBJ = $(BUILDDIR)/tests/unit/message/message_tool_call_helpers.o $(BUILDDIR)/tests/unit/message/message_tool_result_helpers.o $(BUILDDIR)/tests/unit/message/message_thinking_helpers.o

$(BUILDDIR)/tests/unit/message/message_tool_call_helpers.o: tests/unit/message/message_tool_call_helpers.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/message/message_tool_result_helpers.o: tests/unit/message/message_tool_result_helpers.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/message/message_thinking_helpers.o: tests/unit/message/message_thinking_helpers.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/message/message_from_db_test: $(BUILDDIR)/tests/unit/message/message_from_db_test.o $(MESSAGE_TEST_HELPERS_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Agent restore test helper compilation
AGENT_RESTORE_TEST_HELPERS_OBJ = $(BUILDDIR)/tests/unit/repl/agent_restore_test_helpers.o

$(BUILDDIR)/tests/unit/repl/agent_restore_test_helpers.o: tests/unit/repl/agent_restore_test_helpers.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/repl/agent_restore_test: $(BUILDDIR)/tests/unit/repl/agent_restore_test.o $(AGENT_RESTORE_TEST_HELPERS_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Anthropic request serialize test helper compilation
ANTHROPIC_SERIALIZE_HELPERS_OBJ = $(BUILDDIR)/tests/unit/providers/anthropic/content_block_serialize_helpers.o $(BUILDDIR)/tests/unit/providers/anthropic/message_serialize_helpers.o

$(BUILDDIR)/tests/unit/providers/anthropic/content_block_serialize_helpers.o: tests/unit/providers/anthropic/content_block_serialize_helpers.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/anthropic/message_serialize_helpers.o: tests/unit/providers/anthropic/message_serialize_helpers.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/anthropic/request_serialize_success_test: $(BUILDDIR)/tests/unit/providers/anthropic/request_serialize_success_test.o $(ANTHROPIC_SERIALIZE_HELPERS_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/integration/%_test.o: tests/integration/%_test.c | $(BUILDDIR)/tests/integration
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/integration/%_test: $(BUILDDIR)/tests/integration/%_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) | $(BUILDDIR)/tests/integration
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Mock verification helper compilation
$(BUILDDIR)/tests/integration/google_mock_verification_helpers.o: tests/integration/google_mock_verification_helpers.c tests/integration/google_mock_verification_helpers.h | $(BUILDDIR)/tests/integration
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/integration/google_mock_verification_test: $(BUILDDIR)/tests/integration/google_mock_verification_test.o $(BUILDDIR)/tests/integration/google_mock_verification_helpers.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) | $(BUILDDIR)/tests/integration
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/integration/anthropic_mock_verification_helpers.o: tests/integration/anthropic_mock_verification_helpers.c tests/integration/anthropic_mock_verification_helpers.h | $(BUILDDIR)/tests/integration
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/integration/anthropic_mock_verification_test: $(BUILDDIR)/tests/integration/anthropic_mock_verification_test.o $(BUILDDIR)/tests/integration/anthropic_mock_verification_helpers.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) | $(BUILDDIR)/tests/integration
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# DB integration test compilation
$(BUILDDIR)/tests/integration/db/%_test.o: tests/integration/db/%_test.c | $(BUILDDIR)/tests/integration/db
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/integration/db/%_test: $(BUILDDIR)/tests/integration/db/%_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) | $(BUILDDIR)/tests/integration/db
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/test_utils.o: tests/test_utils.c tests/test_utils.h | $(BUILDDIR)/tests
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/helpers/test_contexts.o: tests/helpers/test_contexts.c tests/helpers/test_contexts.h | $(BUILDDIR)/tests
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/helpers/vcr.o: tests/helpers/vcr.c tests/helpers/vcr.h | $(BUILDDIR)/tests
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/helpers/vcr_stubs.o: tests/helpers/vcr_stubs.c | $(BUILDDIR)/tests
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/repl/repl_run_common.o: tests/unit/repl/repl_run_common.c tests/unit/repl/repl_run_common.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/repl/repl_streaming_test_common.o: tests/unit/repl/repl_streaming_test_common.c tests/unit/repl/repl_streaming_test_common.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

# Equivalence test support objects
$(BUILDDIR)/tests/unit/providers/openai/equivalence_fixtures.o: tests/unit/providers/openai/equivalence_fixtures.c tests/unit/providers/openai/equivalence_fixtures.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/openai/equivalence_compare_basic.o: tests/unit/providers/openai/equivalence_compare_basic.c tests/unit/providers/openai/equivalence_compare.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/openai/equivalence_compare_complex.o: tests/unit/providers/openai/equivalence_compare_complex.c tests/unit/providers/openai/equivalence_compare.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

# Special rule for equivalence_test that needs fixtures and compare objects
$(BUILDDIR)/tests/unit/providers/openai/equivalence_test: $(BUILDDIR)/tests/unit/providers/openai/equivalence_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(EQUIVALENCE_FIXTURES_OBJ) $(EQUIVALENCE_COMPARE_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Request responses test helper compilation
$(BUILDDIR)/tests/unit/providers/openai/request_responses_test_helpers.o: tests/unit/providers/openai/request_responses_test_helpers.c tests/unit/providers/openai/request_responses_test_helpers.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

# Request chat coverage test helper compilation
$(BUILDDIR)/tests/unit/providers/openai/request_chat_coverage_helpers.o: tests/unit/providers/openai/request_chat_coverage_helpers.c tests/unit/providers/openai/request_chat_coverage_helpers.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $< && echo "🔨 $@" || (echo "🔴 $@" && exit 1)

# Special rules for request_responses tests that need the test helpers
$(BUILDDIR)/tests/unit/providers/openai/request_responses_coverage_test: $(BUILDDIR)/tests/unit/providers/openai/request_responses_coverage_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REQUEST_RESPONSES_TEST_HELPERS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/openai/request_responses_content_test: $(BUILDDIR)/tests/unit/providers/openai/request_responses_content_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REQUEST_RESPONSES_TEST_HELPERS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/openai/request_responses_tool_errors_test: $(BUILDDIR)/tests/unit/providers/openai/request_responses_tool_errors_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REQUEST_RESPONSES_TEST_HELPERS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/openai/request_responses_coverage1_test: $(BUILDDIR)/tests/unit/providers/openai/request_responses_coverage1_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REQUEST_RESPONSES_TEST_HELPERS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/openai/request_responses_coverage2_test: $(BUILDDIR)/tests/unit/providers/openai/request_responses_coverage2_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REQUEST_RESPONSES_TEST_HELPERS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/openai/request_chat_coverage_test: $(BUILDDIR)/tests/unit/providers/openai/request_chat_coverage_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REQUEST_CHAT_COVERAGE_HELPERS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rules for repl_run tests that need the run common object
$(BUILDDIR)/tests/unit/repl/repl_run_basic_test: $(BUILDDIR)/tests/unit/repl/repl_run_basic_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REPL_RUN_COMMON_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/repl/repl_run_io_error_test: $(BUILDDIR)/tests/unit/repl/repl_run_io_error_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REPL_RUN_COMMON_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/repl/repl_run_curl_error_test: $(BUILDDIR)/tests/unit/repl/repl_run_curl_error_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REPL_RUN_COMMON_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/repl/repl_run_render_misc_test: $(BUILDDIR)/tests/unit/repl/repl_run_render_misc_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REPL_RUN_COMMON_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for repl_agent_lookup_test that needs test_contexts
$(BUILDDIR)/tests/unit/repl/repl_agent_lookup_test: $(BUILDDIR)/tests/unit/repl/repl_agent_lookup_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(TEST_CONTEXTS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rules for repl streaming/completion tests that need the streaming common object
$(BUILDDIR)/tests/unit/repl/repl_streaming_test: $(BUILDDIR)/tests/unit/repl/repl_streaming_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REPL_STREAMING_COMMON_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/repl/repl_completion_test: $(BUILDDIR)/tests/unit/repl/repl_completion_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REPL_STREAMING_COMMON_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/repl/handle_request_error_test: $(BUILDDIR)/tests/unit/repl/handle_request_error_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REPL_STREAMING_COMMON_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/repl/handle_request_success_advanced_test: $(BUILDDIR)/tests/unit/repl/handle_request_success_advanced_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REPL_STREAMING_COMMON_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/repl/repl_streaming_basic_test: $(BUILDDIR)/tests/unit/repl/repl_streaming_basic_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REPL_STREAMING_COMMON_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/repl/repl_streaming_advanced_test: $(BUILDDIR)/tests/unit/repl/repl_streaming_advanced_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(REPL_STREAMING_COMMON_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for repl_actions_db_error_test (uses mocks, excludes DB modules)
$(BUILDDIR)/tests/unit/repl/repl_actions_db_error_test: $(BUILDDIR)/tests/unit/repl/repl_actions_db_error_test.o $(MODULE_OBJ_NO_DB) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for repl_actions_db_advanced_test (uses mocks, excludes DB modules)
$(BUILDDIR)/tests/unit/repl/repl_actions_db_advanced_test: $(BUILDDIR)/tests/unit/repl/repl_actions_db_advanced_test.o $(MODULE_OBJ_NO_DB) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for repl_actions_db_basic_test (uses mocks, excludes DB modules)
$(BUILDDIR)/tests/unit/repl/repl_actions_db_basic_test: $(BUILDDIR)/tests/unit/repl/repl_actions_db_basic_test.o $(MODULE_OBJ_NO_DB) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for repl_actions_llm_basic_test (uses mocks, excludes DB modules, agent_provider, request_tools)
$(BUILDDIR)/tests/unit/repl/repl_actions_llm_basic_test: $(BUILDDIR)/tests/unit/repl/repl_actions_llm_basic_test.o $(filter-out $(BUILDDIR)/agent_provider.o $(BUILDDIR)/providers/request_tools.o,$(MODULE_OBJ_NO_DB)) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) -lgcov && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for repl_actions_llm_errors_test (uses mocks, excludes DB modules, agent_provider, request_tools)
$(BUILDDIR)/tests/unit/repl/repl_actions_llm_errors_test: $(BUILDDIR)/tests/unit/repl/repl_actions_llm_errors_test.o $(filter-out $(BUILDDIR)/agent_provider.o $(BUILDDIR)/providers/request_tools.o,$(MODULE_OBJ_NO_DB)) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) -lgcov && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for repl_init_db_test (uses mocks, excludes src/db/agent.c and src/repl/agent_restore.c)
$(BUILDDIR)/tests/unit/repl/repl_init_db_test: $(BUILDDIR)/tests/unit/repl/repl_init_db_test.o $(filter-out $(BUILDDIR)/repl/agent_restore.o,$(MODULE_OBJ_NO_DB_AGENT)) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for repl_session_test (uses mocks, excludes src/db/agent.c and src/repl/agent_restore.c)
$(BUILDDIR)/tests/unit/repl/repl_session_test: $(BUILDDIR)/tests/unit/repl/repl_session_test.o $(filter-out $(BUILDDIR)/repl/agent_restore.o,$(MODULE_OBJ_NO_DB_AGENT)) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for test_contexts_test (needs test_contexts helper)
$(BUILDDIR)/tests/unit/helpers/test_contexts_test: $(BUILDDIR)/tests/unit/helpers/test_contexts_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ) $(TEST_CONTEXTS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for vcr_test (needs vcr helper, no module objects needed)
$(BUILDDIR)/tests/unit/helpers/vcr_test: $(BUILDDIR)/tests/unit/helpers/vcr_test.o $(VCR_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ -lcheck -lm -lsubunit && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for vcr_advanced_test (needs vcr helper, no module objects needed)
$(BUILDDIR)/tests/unit/helpers/vcr_advanced_test: $(BUILDDIR)/tests/unit/helpers/vcr_advanced_test.o $(VCR_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ -lcheck -lm -lsubunit && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rule for vcr_mock_integration_test (needs vcr helper and module objects)
$(BUILDDIR)/tests/unit/helpers/vcr_mock_integration_test: $(BUILDDIR)/tests/unit/helpers/vcr_mock_integration_test.o $(VCR_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rules for anthropic streaming tests (need vcr helper for fixture playback)
$(BUILDDIR)/tests/unit/providers/anthropic/anthropic_streaming_async_test: $(BUILDDIR)/tests/unit/providers/anthropic/anthropic_streaming_async_test.o $(VCR_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/anthropic/anthropic_streaming_basic_test: $(BUILDDIR)/tests/unit/providers/anthropic/anthropic_streaming_basic_test.o $(VCR_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/anthropic/anthropic_streaming_advanced_test: $(BUILDDIR)/tests/unit/providers/anthropic/anthropic_streaming_advanced_test.o $(VCR_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/anthropic/anthropic_streaming_unit_test: $(BUILDDIR)/tests/unit/providers/anthropic/anthropic_streaming_unit_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rules for google streaming tests (need vcr helper for fixture playback)
$(BUILDDIR)/tests/unit/providers/google/google_streaming_async_test: $(BUILDDIR)/tests/unit/providers/google/google_streaming_async_test.o $(VCR_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/google/google_streaming_content_test: $(BUILDDIR)/tests/unit/providers/google/google_streaming_content_test.o $(VCR_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/google/google_streaming_advanced_test: $(BUILDDIR)/tests/unit/providers/google/google_streaming_advanced_test.o $(VCR_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR)/tests/unit/providers/google/google_vtable_test: $(BUILDDIR)/tests/unit/providers/google/google_vtable_test.o $(VCR_OBJ) $(MODULE_OBJ) $(TEST_UTILS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# Special rules for openai streaming tests (parser test doesn't need VCR, vtable test doesn't either)
# Both use internal context, not VCR fixtures

# Special rule for repl_full_viewport_test
$(BUILDDIR)/tests/unit/repl/repl_full_viewport_test: $(BUILDDIR)/tests/unit/repl/repl_full_viewport_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS_OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit $(CLIENT_LIBS) && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

bin:
	@mkdir -p bin && echo "📁 bin"

libexec/ikigai:
	@mkdir -p libexec/ikigai && echo "📁 libexec/ikigai"

bash_tool: libexec/ikigai/bash_tool

libexec/ikigai/bash_tool: src/tools/bash/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -ltalloc && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR) && echo "📁 $(BUILDDIR)"

$(BUILDDIR)/tests: | $(BUILDDIR)
	@mkdir -p $(BUILDDIR)/tests && echo "📁 $(BUILDDIR)/tests"

$(BUILDDIR)/tests/unit: | $(BUILDDIR)/tests
	@mkdir -p $(BUILDDIR)/tests/unit && echo "📁 $(BUILDDIR)/tests/unit"

$(BUILDDIR)/tests/integration: | $(BUILDDIR)/tests
	@mkdir -p $(BUILDDIR)/tests/integration && echo "📁 $(BUILDDIR)/tests/integration"

$(BUILDDIR)/tests/integration/db: | $(BUILDDIR)/tests/integration
	@mkdir -p $(BUILDDIR)/tests/integration/db && echo "📁 $(BUILDDIR)/tests/integration/db"

# Include dependency files (auto-generated by -MMD -MP)
# The '-' prefix means don't error if files don't exist yet
-include $(wildcard $(BUILDDIR)/*.d)
-include $(wildcard $(BUILDDIR)/tests/*.d)
-include $(wildcard $(BUILDDIR)/tests/unit/*/*.d)
-include $(wildcard $(BUILDDIR)/tests/integration/*.d)
-include $(wildcard $(BUILDDIR)/tests/integration/db/*.d)

clean:
	rm -rf build build-* bin libexec $(COVERAGE_DIR) coverage_html reports
	rm -rf distros/dist distros/*/build 2>/dev/null || true
	find . -name "*.gcda" -o -name "*.gcno" -o -name "*.gcov" -delete 2>/dev/null || true
	find src tests -name "*.d" -delete 2>/dev/null || true
	rm -f core.* vgcore.* tags 2>/dev/null || true
	rm -f *.LOG 2>/dev/null || true

install: all
	# Create directories
	install -d $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(libexecdir)/ikigai
	install -d $(DESTDIR)$(configdir)
ifeq ($(IS_USER_INSTALL),yes)
	install -d $(DESTDIR)$(user_datadir)
else
	install -d $(DESTDIR)$(datadir)/ikigai
endif
	# Install actual binary to libexec
	install -m 755 $(CLIENT_TARGET) $(DESTDIR)$(libexecdir)/ikigai/ikigai
	# Generate and install wrapper script to bin
	printf '#!/bin/sh\n' > $(DESTDIR)$(bindir)/ikigai
	printf 'IKIGAI_BIN_DIR=%s\n' "$(bindir)" >> $(DESTDIR)$(bindir)/ikigai
	printf 'IKIGAI_CONFIG_DIR=%s\n' "$(configdir)" >> $(DESTDIR)$(bindir)/ikigai
ifeq ($(IS_USER_INSTALL),yes)
	printf 'IKIGAI_DATA_DIR=%s\n' "$(user_datadir)" >> $(DESTDIR)$(bindir)/ikigai
else
	printf 'IKIGAI_DATA_DIR=%s\n' "$(datadir)/ikigai" >> $(DESTDIR)$(bindir)/ikigai
endif
	printf 'IKIGAI_LIBEXEC_DIR=%s\n' "$(libexecdir)/ikigai" >> $(DESTDIR)$(bindir)/ikigai
	printf 'export IKIGAI_BIN_DIR IKIGAI_CONFIG_DIR IKIGAI_DATA_DIR IKIGAI_LIBEXEC_DIR\n' >> $(DESTDIR)$(bindir)/ikigai
	printf 'exec %s/ikigai/ikigai "$$@"\n' "$(libexecdir)" >> $(DESTDIR)$(bindir)/ikigai
	chmod 755 $(DESTDIR)$(bindir)/ikigai
	# Install config files
ifeq ($(FORCE),1)
	install -m 644 etc/ikigai/config.json $(DESTDIR)$(configdir)/config.json
	install -m 644 etc/ikigai/credentials.example.json $(DESTDIR)$(configdir)/credentials.example.json
else
	test -f $(DESTDIR)$(configdir)/config.json || install -m 644 etc/ikigai/config.json $(DESTDIR)$(configdir)/config.json
	test -f $(DESTDIR)$(configdir)/credentials.example.json || install -m 644 etc/ikigai/credentials.example.json $(DESTDIR)$(configdir)/credentials.example.json
endif
	# Install database migrations
ifeq ($(IS_USER_INSTALL),yes)
	install -d $(DESTDIR)$(user_datadir)/migrations
	install -m 644 share/ikigai/migrations/*.sql $(DESTDIR)$(user_datadir)/migrations/
else
	install -d $(DESTDIR)$(datadir)/ikigai/migrations
	install -m 644 share/ikigai/migrations/*.sql $(DESTDIR)$(datadir)/ikigai/migrations/
endif

uninstall:
	rm -f $(DESTDIR)$(bindir)/ikigai
	rm -f $(DESTDIR)$(libexecdir)/ikigai/ikigai
	rmdir $(DESTDIR)$(libexecdir)/ikigai 2>/dev/null || true
	rmdir $(DESTDIR)$(libexecdir) 2>/dev/null || true
ifeq ($(PURGE),1)
	rm -f $(DESTDIR)$(configdir)/config.json
	rm -f $(DESTDIR)$(configdir)/credentials.json
	rm -f $(DESTDIR)$(configdir)/credentials.example.json
endif
	rmdir $(DESTDIR)$(configdir) 2>/dev/null || true
ifeq ($(IS_USER_INSTALL),yes)
	rmdir $(DESTDIR)$(user_datadir) 2>/dev/null || true
else
	rmdir $(DESTDIR)$(datadir)/ikigai 2>/dev/null || true
endif

# Individual test run targets (enables parallel execution)
# Usage: make -j8 check (runs tests in parallel)
# Speedup: ~7.75x faster on typical systems with clean build
UNIT_TEST_RUNS = $(UNIT_TEST_TARGETS:%=%.run)
INTEGRATION_TEST_RUNS = $(INTEGRATION_TEST_TARGETS:%=%.run)
DB_INTEGRATION_TEST_RUNS = $(DB_INTEGRATION_TEST_TARGETS:%=%.run)

# Pattern rule to run a test with XML report output
# Reports are written to reports/check/ mirroring the test structure
# Test stdout/stderr redirected to /dev/null - results are in XML
%.run: %
	@mkdir -p $(dir $(patsubst $(BUILDDIR)/tests/%,reports/$(REPORT_SUBDIR)/%,$<))
ifeq ($(BUILD),sanitize)
	@LSAN_OPTIONS=suppressions=.suppressions/lsan.supp CK_XML_LOG_FILE_NAME=$(patsubst $(BUILDDIR)/tests/%,reports/$(REPORT_SUBDIR)/%,$<).xml $< >/dev/null 2>&1 && echo "🟢 $<" || (echo "🔴 $<" && exit 1)
else ifeq ($(BUILD),tsan)
	@TSAN_OPTIONS=suppressions=.suppressions/tsan.supp CK_FORK=no CK_XML_LOG_FILE_NAME=$(patsubst $(BUILDDIR)/tests/%,reports/$(REPORT_SUBDIR)/%,$<).xml $< >/dev/null 2>&1 && echo "🟢 $<" || (echo "🔴 $<" && exit 1)
else ifeq ($(BUILD),valgrind)
	@CK_FORK=no CK_TIMEOUT_MULTIPLIER=10 CK_XML_LOG_FILE_NAME=$(patsubst $(BUILDDIR)/tests/%,reports/$(REPORT_SUBDIR)/%,$<).xml $< >/dev/null 2>&1 && echo "🟢 $<" || (echo "🔴 $<" && exit 1)
else
	@CK_XML_LOG_FILE_NAME=$(patsubst $(BUILDDIR)/tests/%,reports/$(REPORT_SUBDIR)/%,$<).xml $< >/dev/null 2>&1 && echo "🟢 $<" || (echo "🔴 $<" && exit 1)
endif

check:
ifdef TEST
	@$(MAKE) $(FILTERED_TEST)
	@for test in $(FILTERED_TEST); do \
		report_path=$$(echo "$$test" | sed 's|$(BUILDDIR)/tests/|reports/$(REPORT_SUBDIR)/|').xml; \
		mkdir -p $$(dirname "$$report_path"); \
		CK_XML_LOG_FILE_NAME="$$report_path" $$test >/dev/null 2>&1 && echo "🟢 $$test" || (echo "🔴 $$test" && exit 1); \
	done
else
	@$(MAKE) -j$(MAKE_JOBS) check-unit check-integration
	@echo "All tests passed!"
endif

# Build test binaries without running them
build-tests: $(TEST_TARGETS)
	@echo "Test binaries built successfully!"

# Parallel-safe test execution using Make's -j flag
# Each test creates a .run target that depends on the test binary
# This allows Make to build and run tests in parallel when -j is used
check-unit: $(UNIT_TEST_RUNS)
	@echo "Unit tests passed!"

check-integration: $(INTEGRATION_TEST_RUNS) $(DB_INTEGRATION_TEST_RUNS)
	@echo "Integration tests passed!"

check-bash-tool: bash_tool
	@tests/integration/bash_tool_test.sh

# Verify mock fixtures against real OpenAI API
# Reads OPENAI_API_KEY from ~/.config/ikigai/credentials.json if not set in environment
# Only runs the verification test, which checks if fixtures match real API responses
verify-mocks: $(BUILDDIR)/tests/integration/openai_mock_verification_test
	@API_KEY="$$OPENAI_API_KEY"; \
	if [ -z "$$API_KEY" ]; then \
		CONFIG_FILE="$$HOME/.config/ikigai/credentials.json"; \
		if [ -f "$$CONFIG_FILE" ]; then \
			API_KEY=$$(jq -r '.openai.api_key // empty' "$$CONFIG_FILE"); \
		fi; \
	fi; \
	if [ -z "$$API_KEY" ]; then \
		echo "Error: OPENAI_API_KEY not found"; \
		echo "Set OPENAI_API_KEY env var or add openai.api_key to ~/.config/ikigai/credentials.json"; \
		exit 1; \
	fi; \
	echo "Running mock verification tests against real OpenAI API..."; \
	echo "Note: This will make real API calls and incur costs."; \
	VERIFY_MOCKS=1 OPENAI_API_KEY="$$API_KEY" $(BUILDDIR)/tests/integration/openai_mock_verification_test; \
	echo "Mock verification passed!"

# Verify mock fixtures against real Anthropic API
verify-mocks-anthropic: $(BUILDDIR)/tests/integration/anthropic_mock_verification_test
	@API_KEY="$$ANTHROPIC_API_KEY"; \
	if [ -z "$$API_KEY" ]; then \
		CONFIG_FILE="$$HOME/.config/ikigai/credentials.json"; \
		if [ -f "$$CONFIG_FILE" ]; then \
			API_KEY=$$(jq -r '.anthropic.api_key // empty' "$$CONFIG_FILE"); \
		fi; \
	fi; \
	if [ -z "$$API_KEY" ]; then \
		echo "Error: ANTHROPIC_API_KEY not found"; \
		echo "Set ANTHROPIC_API_KEY env var or add anthropic.api_key to ~/.config/ikigai/credentials.json"; \
		exit 1; \
	fi; \
	echo "Running Anthropic mock verification tests..."; \
	echo "Note: This will make real API calls and incur costs."; \
	VERIFY_MOCKS=1 ANTHROPIC_API_KEY="$$API_KEY" $(BUILDDIR)/tests/integration/anthropic_mock_verification_test; \
	echo "Anthropic mock verification passed!"

# Verify mock fixtures against real Google API
verify-mocks-google: $(BUILDDIR)/tests/integration/google_mock_verification_test
	@API_KEY="$$GOOGLE_API_KEY"; \
	if [ -z "$$API_KEY" ]; then \
		CONFIG_FILE="$$HOME/.config/ikigai/credentials.json"; \
		if [ -f "$$CONFIG_FILE" ]; then \
			API_KEY=$$(jq -r '.google.api_key // empty' "$$CONFIG_FILE"); \
		fi; \
	fi; \
	if [ -z "$$API_KEY" ]; then \
		echo "Error: GOOGLE_API_KEY not found"; \
		echo "Set GOOGLE_API_KEY env var or add google.api_key to ~/.config/ikigai/credentials.json"; \
		exit 1; \
	fi; \
	echo "Running Google mock verification tests..."; \
	echo "Note: This will make real API calls and incur costs."; \
	VERIFY_MOCKS=1 GOOGLE_API_KEY="$$API_KEY" $(BUILDDIR)/tests/integration/google_mock_verification_test; \
	echo "Google mock verification passed!"

# Verify all provider mocks
verify-mocks-all: verify-mocks verify-mocks-anthropic verify-mocks-google
	@echo "All provider mock verifications passed!"

# Validate API credentials in ~/.config/ikigai/credentials.json
# Tests each provider's API key without exposing the key values
verify-credentials:
	@CREDS="$$HOME/.config/ikigai/credentials.json"; \
	if [ ! -f "$$CREDS" ]; then \
		echo "Error: $$CREDS not found"; \
		exit 1; \
	fi; \
	FAILED=0; \
	echo "Validating API credentials..."; \
	echo ""; \
	echo -n "  OpenAI:    "; \
	KEY=$$(jq -r '.openai.api_key // empty' "$$CREDS"); \
	if [ -z "$$KEY" ]; then \
		echo "SKIP (no key)"; \
	else \
		HTTP_CODE=$$(curl -s -o /dev/null -w "%{http_code}" \
			-H "Authorization: Bearer $$KEY" \
			https://api.openai.com/v1/models); \
		if [ "$$HTTP_CODE" = "200" ]; then \
			echo "OK"; \
		else \
			echo "FAILED (HTTP $$HTTP_CODE)"; \
			FAILED=1; \
		fi; \
	fi; \
	echo -n "  Anthropic: "; \
	KEY=$$(jq -r '.anthropic.api_key // empty' "$$CREDS"); \
	if [ -z "$$KEY" ]; then \
		echo "SKIP (no key)"; \
	else \
		HTTP_CODE=$$(curl -s -o /dev/null -w "%{http_code}" \
			-H "x-api-key: $$KEY" \
			-H "anthropic-version: 2023-06-01" \
			-H "content-type: application/json" \
			-d '{"model":"claude-3-haiku-20240307","max_tokens":1,"messages":[{"role":"user","content":"hi"}]}' \
			https://api.anthropic.com/v1/messages); \
		if [ "$$HTTP_CODE" = "200" ]; then \
			echo "OK"; \
		else \
			echo "FAILED (HTTP $$HTTP_CODE)"; \
			FAILED=1; \
		fi; \
	fi; \
	echo -n "  Google:    "; \
	KEY=$$(jq -r '.google.api_key // empty' "$$CREDS"); \
	if [ -z "$$KEY" ]; then \
		echo "SKIP (no key)"; \
	else \
		HTTP_CODE=$$(curl -s -o /dev/null -w "%{http_code}" \
			"https://generativelanguage.googleapis.com/v1beta/models?key=$$KEY"); \
		if [ "$$HTTP_CODE" = "200" ]; then \
			echo "OK"; \
		else \
			echo "FAILED (HTTP $$HTTP_CODE)"; \
			FAILED=1; \
		fi; \
	fi; \
	echo ""; \
	if [ "$$FAILED" = "1" ]; then \
		echo "Some credentials failed validation."; \
		exit 1; \
	fi; \
	echo "All credentials valid."

# VCR recording targets - re-record fixtures with real API calls
# Requires API keys via environment variables

vcr-record-openai:
	@echo "Recording OpenAI fixtures (requires OPENAI_API_KEY)..."
	@for test in $(BUILDDIR)/tests/unit/providers/openai/*_test; do \
		echo "Running $$test with VCR_RECORD=1..."; \
		VCR_RECORD=1 $$test || true; \
	done
	@echo "OpenAI fixtures recorded"

vcr-record-anthropic:
	@echo "Recording Anthropic fixtures (requires ANTHROPIC_API_KEY)..."
	@for test in $(BUILDDIR)/tests/unit/providers/anthropic/*_test; do \
		echo "Running $$test with VCR_RECORD=1..."; \
		VCR_RECORD=1 $$test || true; \
	done
	@echo "Anthropic fixtures recorded"

vcr-record-google:
	@echo "Recording Google fixtures (requires GOOGLE_API_KEY)..."
	@for test in $(BUILDDIR)/tests/unit/providers/google/*_test; do \
		echo "Running $$test with VCR_RECORD=1..."; \
		VCR_RECORD=1 $$test || true; \
	done
	@echo "Google fixtures recorded"

vcr-record-all: vcr-record-openai vcr-record-anthropic vcr-record-google
	@echo "All fixtures re-recorded"

# Clean up .run sentinel files
clean: clean-test-runs

.PHONY: clean-test-runs
clean-test-runs:
	@rm -f $(UNIT_TEST_RUNS) $(INTEGRATION_TEST_RUNS)

check-sanitize:
	@echo "Building with AddressSanitizer + UndefinedBehaviorSanitizer..."
	@rm -rf build-sanitize
	@mkdir -p build-sanitize/tests/unit build-sanitize/tests/integration
	@find tests/unit -type d | sed 's|tests/unit|build-sanitize/tests/unit|' | xargs mkdir -p
	@echo "Building test binaries in parallel..."
	@BUILD=sanitize BUILDDIR=build-sanitize SKIP_SIGNAL_TESTS=1 $(MAKE) -j$(MAKE_JOBS) build-tests
	@echo "Running tests in parallel..."
	@LSAN_OPTIONS=suppressions=.suppressions/lsan.supp BUILD=sanitize BUILDDIR=build-sanitize SKIP_SIGNAL_TESTS=1 $(MAKE) -j$(MAKE_JOBS) check-unit check-integration
	@echo "🟢 Sanitizer checks passed!"

check-valgrind:
	@echo "Building for Valgrind with enhanced debug info..."
	@rm -rf build-valgrind
	@mkdir -p build-valgrind/tests/unit build-valgrind/tests/integration
	@find tests/unit -type d | sed 's|tests/unit|build-valgrind/tests/unit|' | xargs mkdir -p
	@SKIP_SIGNAL_TESTS=1 $(MAKE) -j$(MAKE_JOBS) check BUILD=valgrind BUILDDIR=build-valgrind TEST_TARGETS_VAR=1
	@echo "Running tests under Valgrind Memcheck..."
	@ulimit -n 1024; \
	SUPP_FILE="$$(pwd)/.suppressions/valgrind.supp"; \
	if ! find build-valgrind/tests -type f -executable | sort | xargs -I {} -P $(MAKE_JOBS) sh -c \
		'REPORT_PATH=$$(echo "{}" | sed "s|build-valgrind/tests/|reports/valgrind/|").xml; \
		mkdir -p $$(dirname "$$REPORT_PATH"); \
		if CK_FORK=no CK_TIMEOUT_MULTIPLIER=10 valgrind --leak-check=full --show-leak-kinds=all \
		            --track-origins=yes --error-exitcode=1 \
		            --xml=yes --xml-file="$$REPORT_PATH" \
		            --gen-suppressions=no \
		            --suppressions='"$$SUPP_FILE"' \
		            ./{} >/dev/null 2>&1; then \
			echo "🟢 {}"; \
		else \
			echo "🔴 {}"; \
			exit 1; \
		fi'; then \
		echo "🔴 Valgrind checks failed"; \
		exit 1; \
	fi
	@total=$$(find build-valgrind/tests -type f -executable | wc -l); \
	echo "Valgrind: $$total passed, 0 failed"

check-helgrind:
	@echo "Building tests for Helgrind..."
	@rm -rf build-helgrind
	@BUILD=valgrind BUILDDIR=build-helgrind SKIP_SIGNAL_TESTS=1 $(MAKE) -j$(MAKE_JOBS) build-tests
	@echo "Running tests under Valgrind Helgrind..."
	@ulimit -n 1024; \
	if ! find build-helgrind/tests -type f -executable | sort | xargs -I {} -P $(MAKE_JOBS) sh -c \
		'REPORT_PATH=$$(echo "{}" | sed "s|build-helgrind/tests/|reports/helgrind/|").xml; \
		mkdir -p $$(dirname "$$REPORT_PATH"); \
		if CK_FORK=no CK_TIMEOUT_MULTIPLIER=10 valgrind --tool=helgrind --error-exitcode=1 \
		            --history-level=approx \
		            --xml=yes --xml-file="$$REPORT_PATH" \
		            --suppressions=$(CURDIR)/.suppressions/helgrind.supp \
		            ./{} >/dev/null 2>&1; then \
			echo "🟢 {}"; \
		else \
			echo "🔴 {}"; \
			exit 1; \
		fi'; then \
		echo "🔴 Helgrind checks failed"; \
		exit 1; \
	fi
	@total=$$(find build-helgrind/tests -type f -executable | wc -l); \
	echo "Helgrind: $$total passed, 0 failed"

check-tsan:
	@echo "Building with ThreadSanitizer..."
	@rm -rf build-tsan
	@mkdir -p build-tsan/tests/unit build-tsan/tests/integration
	@find tests/unit -type d | sed 's|tests/unit|build-tsan/tests/unit|' | xargs mkdir -p
	@echo "Building test binaries in parallel..."
	@BUILD=tsan BUILDDIR=build-tsan SKIP_SIGNAL_TESTS=1 $(MAKE) -j$(MAKE_JOBS) build-tests
	@echo "Running tests in parallel..."
	@BUILD=tsan BUILDDIR=build-tsan SKIP_SIGNAL_TESTS=1 $(MAKE) -j$(MAKE_JOBS) check-unit check-integration
	@echo "🟢 ThreadSanitizer checks passed!"

check-dynamic:
ifeq ($(PARALLEL),1)
	@echo "Running dynamic analysis checks in parallel..."
	@$(MAKE) -j4 check-sanitize check-valgrind check-helgrind check-tsan
else
	@echo "Running dynamic analysis checks sequentially..."
	@$(MAKE) check-sanitize
	@$(MAKE) check-valgrind
	@$(MAKE) check-helgrind
	@$(MAKE) check-tsan
endif
	@echo "🟢 All dynamic analysis checks passed!"

distro-images:
	@echo "Building Docker images for distributions: $(DISTROS)"
	@for distro in $(DISTROS); do \
		echo "Building ikigai-ci-$$distro..."; \
		docker build -f distros/$$distro/Dockerfile -t ikigai-ci-$$distro . || exit 1; \
	done
	@echo "🟢 All images built successfully!"

distro-images-clean:
	@echo "Removing Docker images for distributions: $(DISTROS)"
	@for distro in $(DISTROS); do \
		echo "Removing ikigai-ci-$$distro..."; \
		docker rmi ikigai-ci-$$distro 2>/dev/null || true; \
	done
	@echo "🟢 All images removed!"

distro-clean:
	@echo "Cleaning build artifacts using $(word 1,$(DISTROS)) Docker image..."
	@docker run --rm --user $$(id -u):$$(id -g) -v "$$(pwd)":/workspace ikigai-ci-$(word 1,$(DISTROS)) bash -c "make clean"
	@echo "🟢 Clean complete!"

distro-check:
	@echo "Testing on distributions: $(DISTROS)"
	@for distro in $(DISTROS); do \
		echo ""; \
		echo "=== Testing on $$distro ==="; \
		if [ -f distros/$$distro/docker-compose.yml ]; then \
			UID=$$(id -u) GID=$$(id -g) docker-compose -f distros/$$distro/docker-compose.yml up --build --abort-on-container-exit --exit-code-from test || exit 1; \
			UID=$$(id -u) GID=$$(id -g) docker-compose -f distros/$$distro/docker-compose.yml down -v; \
		else \
			docker build -f distros/$$distro/Dockerfile -t ikigai-ci-$$distro . || exit 1; \
			docker run --rm --user $$(id -u):$$(id -g) -v "$$(pwd)":/workspace ikigai-ci-$$distro bash -c "make ci" || exit 1; \
		fi; \
		echo "🟢 $$distro passed!"; \
	done
	@echo ""
	@echo "🟢 All distributions passed!"

distro-package:
	@echo "Building packages for distributions: $(DISTROS)"
	@$(MAKE) dist
	@for distro in $(DISTROS); do \
		echo ""; \
		echo "=== Building package for $$distro ==="; \
		docker build -f distros/$$distro/Dockerfile -t ikigai-ci-$$distro . || exit 1; \
		docker run --rm --user $$(id -u):$$(id -g) -v "$$(pwd)":/workspace ikigai-ci-$$distro bash -c "distros/$$distro/package.sh" || exit 1; \
		echo "🟢 $$distro package built!"; \
	done
	@echo ""
	@echo "🟢 All packages built!"
	@echo ""
	@ls -lh distros/dist/*.deb distros/dist/*.rpm 2>/dev/null || true

dist:
	@echo "Creating distribution tarball..."
	@mkdir -p distros/dist $(PACKAGE)-$(VERSION)
	@cp -r src tests Makefile README.md LICENSE $(PACKAGE)-$(VERSION)/
	@tar czf distros/dist/$(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)
	@rm -rf $(PACKAGE)-$(VERSION)
	@echo "Created distros/dist/$(PACKAGE)-$(VERSION).tar.gz"

fmt:
	@uncrustify -c .uncrustify.cfg --replace --no-backup src/*.c src/*.h
	@find tests/unit -name "*.c" -o -name "*.h" | xargs uncrustify -c .uncrustify.cfg --replace --no-backup
	@[ ! -d tests/integration ] || uncrustify -c .uncrustify.cfg --replace --no-backup tests/integration/*.c
	@[ ! -f tests/test_utils.c ] || uncrustify -c .uncrustify.cfg --replace --no-backup tests/test_utils.c tests/test_utils.h

check-complexity:
	@echo "Checking complexity in src/*.c..."
	@output=$$(complexity --threshold=$(COMPLEXITY_THRESHOLD) src/*.c 2>&1); \
	if echo "$$output" | grep -q "^Complexity Scores$$"; then \
		echo "🔴 Cyclomatic complexity exceeds threshold ($(COMPLEXITY_THRESHOLD))"; \
		echo "$$output"; \
		exit 1; \
	fi; \
	if echo "$$output" | grep -q "nesting depth reached level [6-9]"; then \
		echo "🔴 Nesting depth exceeds threshold ($(NESTING_DEPTH_THRESHOLD))"; \
		echo "$$output" | grep "nesting depth"; \
		exit 1; \
	fi
	@echo "Checking complexity in tests/unit/*/*.c..."
	@output=$$(find tests/unit -name "*.c" -exec complexity --threshold=$(COMPLEXITY_THRESHOLD) {} \; 2>&1); \
	if echo "$$output" | grep -q "^Complexity Scores$$"; then \
		echo "🔴 Cyclomatic complexity exceeds threshold ($(COMPLEXITY_THRESHOLD))"; \
		echo "$$output"; \
		exit 1; \
	fi; \
	if echo "$$output" | grep -q "nesting depth reached level [6-9]"; then \
		echo "🔴 Nesting depth exceeds threshold ($(NESTING_DEPTH_THRESHOLD))"; \
		echo "$$output" | grep "nesting depth"; \
		exit 1; \
	fi
	@echo "Checking complexity in tests/integration/*.c..."
	@if [ -d tests/integration ]; then \
		output=$$(complexity --threshold=$(COMPLEXITY_THRESHOLD) tests/integration/*.c 2>&1); \
		if echo "$$output" | grep -q "^Complexity Scores$$"; then \
			echo "🔴 Cyclomatic complexity exceeds threshold ($(COMPLEXITY_THRESHOLD))"; \
			echo "$$output"; \
			exit 1; \
		fi; \
		if echo "$$output" | grep -q "nesting depth reached level [6-9]"; then \
			echo "🔴 Nesting depth exceeds threshold ($(NESTING_DEPTH_THRESHOLD))"; \
			echo "$$output" | grep "nesting depth"; \
			exit 1; \
		fi; \
	fi
	@echo "🟢 All complexity checks passed"

check-filesize:
	@echo "Checking file sizes (max: $(MAX_FILE_BYTES) bytes)..."
	@failed=0; \
	for file in $$(find src -name "*.c" -o -name "*.h" | grep -v vendor); do \
		bytes=$$(wc -c < "$$file"); \
		if [ $$bytes -gt $(MAX_FILE_BYTES) ]; then \
			echo "🔴 $$file: $$bytes bytes (exceeds $(MAX_FILE_BYTES))"; \
			failed=1; \
		fi; \
	done; \
	for file in $$(find tests/unit -name "*.c"); do \
		bytes=$$(wc -c < "$$file"); \
		if [ $$bytes -gt $(MAX_FILE_BYTES) ]; then \
			echo "🔴 $$file: $$bytes bytes (exceeds $(MAX_FILE_BYTES))"; \
			failed=1; \
		fi; \
	done; \
	for file in tests/integration/*.c; do \
		[ -f "$$file" ] || continue; \
		bytes=$$(wc -c < "$$file"); \
		if [ $$bytes -gt $(MAX_FILE_BYTES) ]; then \
			echo "🔴 $$file: $$bytes bytes (exceeds $(MAX_FILE_BYTES))"; \
			failed=1; \
		fi; \
	done; \
	for file in project/*.md project/*/*.md; do \
		[ -f "$$file" ] || continue; \
		case "$$file" in project/backlog/*) continue ;; esac; \
		bytes=$$(wc -c < "$$file"); \
		if [ $$bytes -gt $(MAX_FILE_BYTES) ]; then \
			echo "🔴 $$file: $$bytes bytes (exceeds $(MAX_FILE_BYTES))"; \
			failed=1; \
		fi; \
	done; \
	if [ $$failed -eq 1 ]; then \
		echo "🔴 Some files exceed $(MAX_FILE_BYTES) byte limit"; \
		exit 1; \
	fi
	@echo "🟢 All file size checks passed"

lint: check-complexity check-filesize

cloc:
	@cloc src/ tests/ Makefile

tags:
	ctags -R src/

ci:
	@echo "Running CI checks..."
	@$(MAKE) filesize
	@$(MAKE) check-complexity
	@$(MAKE) check-coverage
	@$(MAKE) check-unit
	@$(MAKE) check-integration
	@$(MAKE) check-sanitize
	@$(MAKE) check-tsan
	@$(MAKE) check-valgrind
	@$(MAKE) check-helgrind
	@echo "🟢 All CI checks passed"

# Generate source file -> test mapping for targeted coverage runs
# Output: .claude/data/source_tests.json
coverage-map:
	@echo "Building test binaries with coverage instrumentation..."
	@$(MAKE) clean
	@mkdir -p build-coverage/tests/unit build-coverage/tests/integration
	@find tests/unit -type d | sed 's|tests/unit|build-coverage/tests/unit|' | xargs mkdir -p
	@find tests/integration -type d | sed 's|tests/integration|build-coverage/tests/integration|' | xargs mkdir -p
	@BUILDDIR=build-coverage $(MAKE) build-tests CFLAGS="$(CFLAGS) $(COVERAGE_CFLAGS)" LDFLAGS="$(LDFLAGS) $(COVERAGE_LDFLAGS)"
	@BUILDDIR=build-coverage .claude/scripts/generate-coverage-map.sh

check-coverage:
	@echo "Building with coverage instrumentation..."
	@$(MAKE) clean
	@find . -name "*.gcda" -o -name "*.gcno" -delete 2>/dev/null || true
	@mkdir -p build-coverage/tests/unit build-coverage/tests/integration
	@find tests/unit -type d | sed 's|tests/unit|build-coverage/tests/unit|' | xargs mkdir -p
	@BUILDDIR=build-coverage $(MAKE) -j$(MAKE_JOBS) check CFLAGS="$(CFLAGS) $(COVERAGE_CFLAGS)" LDFLAGS="$(LDFLAGS) $(COVERAGE_LDFLAGS)"
	@echo "🤔 Generating coverage report..."
ifdef TEST
	@for test in $(FILTERED_TEST); do \
		report_base=$$(echo "$$test" | sed 's|build[^/]*/tests/|reports/coverage/|'); \
		mkdir -p $$(dirname "$$report_base"); \
		lcov --capture --directory . --output-file "$${report_base}.coverage.info" --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative --rc lcov_branch_coverage=1 --quiet >/dev/null 2>&1; \
		lcov --extract "$${report_base}.coverage.info" '*/src/*' --output-file "$${report_base}.coverage.info" --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative --quiet >/dev/null 2>&1; \
		lcov --remove "$${report_base}.coverage.info" '*/src/vendor/*' --output-file "$${report_base}.coverage.info" --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative --quiet >/dev/null 2>&1; \
		echo "=== Coverage by File ===" > "$${report_base}.coverage.txt"; \
		{ lcov --list "$${report_base}.coverage.info" --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative 2>&1; } | grep -v "^Message summary" | grep -v "messages were reported" | grep -v "warning messages" | grep -v "ignore messages" | grep -v "coverpoints" | grep -v "instances" | grep -v "deprecated:" | grep -v "inconsistent:" | grep -v "negative:" | grep -v "region:" | grep -v "branch_region:" >> "$${report_base}.coverage.txt"; \
		echo "" >> "$${report_base}.coverage.txt"; \
		echo "=== Coverage Summary ===" >> "$${report_base}.coverage.txt"; \
		{ lcov --summary "$${report_base}.coverage.info" --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative 2>&1; } | grep -v "^Message summary" | grep -v "messages were reported" | grep -v "Filter" >> "$${report_base}.coverage.txt"; \
		cat "$${report_base}.coverage.txt"; \
		echo ""; \
		echo "Coverage report: $${report_base}.coverage.txt"; \
	done
else
	@mkdir -p $(COVERAGE_DIR)
	@lcov --capture --directory . --output-file $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative --rc lcov_branch_coverage=1 --quiet >/dev/null 2>&1
	@lcov --extract $(COVERAGE_DIR)/coverage.info '*/src/*' --output-file $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative --quiet >/dev/null 2>&1
	@lcov --remove $(COVERAGE_DIR)/coverage.info '*/src/vendor/*' --output-file $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative --quiet >/dev/null 2>&1
	@echo "=== Coverage by File ===" > $(COVERAGE_DIR)/summary.txt
	@{ lcov --list $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative 2>&1; } | grep -v "^Message summary" | grep -v "messages were reported" | grep -v "warning messages" | grep -v "ignore messages" | grep -v "coverpoints" | grep -v "instances" | grep -v "deprecated:" | grep -v "inconsistent:" | grep -v "negative:" | grep -v "region:" | grep -v "branch_region:" >> $(COVERAGE_DIR)/summary.txt
	@echo "" >> $(COVERAGE_DIR)/summary.txt
	@echo "=== Coverage Summary ===" >> $(COVERAGE_DIR)/summary.txt
	@{ lcov --summary $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative 2>&1; } | grep -v "^Message summary" | grep -v "messages were reported" | grep -v "Filter" >> $(COVERAGE_DIR)/summary.txt
	@echo "" >> $(COVERAGE_DIR)/summary.txt
	@cat $(COVERAGE_DIR)/summary.txt
	@echo ""
	@echo "Checking coverage thresholds (lines, functions, branches: $(COVERAGE_THRESHOLD)%)..."
	@LINE_COV=$$(lcov --summary $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative 2>&1 | grep "lines\.\.*:" | grep -oE "[0-9]+\.[0-9]+%" | head -1 | tr -d '%'); \
	FUNC_COV=$$(lcov --summary $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative 2>&1 | grep "functions\.\.*:" | grep -oE "[0-9]+\.[0-9]+%" | head -1 | tr -d '%'); \
	BRANCH_COV=$$(lcov --summary $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative 2>&1 | grep "branches\.\.*:" | grep -oE "[0-9]+\.[0-9]+%" | head -1 | tr -d '%'); \
	echo "  Lines: $${LINE_COV}%, Functions: $${FUNC_COV}%, Branches: $${BRANCH_COV}%"; \
	if [ "$$(echo "$$LINE_COV >= $(COVERAGE_THRESHOLD)" | bc)" -eq 1 ] && \
	   [ "$$(echo "$$FUNC_COV >= $(COVERAGE_THRESHOLD)" | bc)" -eq 1 ] && \
	   [ "$$(echo "$$BRANCH_COV >= $(COVERAGE_THRESHOLD)" | bc)" -eq 1 ]; then \
		echo "🟢 All coverage thresholds met ($(COVERAGE_THRESHOLD)%)"; \
	else \
		echo "🔴 Coverage below $(COVERAGE_THRESHOLD)% threshold"; \
		exit 1; \
	fi
endif

# Default package manager and package list (Debian)
PKG_UPDATE ?= apt-get update
PKG_INSTALL ?= apt-get install -y
PACKAGES ?= \
	build-essential \
	uncrustify \
	complexity \
	check \
	libtalloc-dev \
	libulfius-dev \
	libcurl4-gnutls-dev \
	uuid-dev \
	libb64-dev \
	libutf8proc-dev \
	lcov \
	valgrind

# Include distro-specific overrides if specified
ifdef DISTRO
-include mk/$(DISTRO).mk
endif

install-deps:
	@echo "Installing build dependencies..."
	sudo $(PKG_UPDATE)
	sudo $(PKG_INSTALL) $(PACKAGES)

help:
	@echo "Available targets:"
	@echo "  all             - Build ikigai (default: BUILD=debug)"
	@echo "  release         - Build in release mode (shortcut for BUILD=release)"
	@echo "  clean           - Remove all built files, coverage data, and reports"
	@echo "  install         - Install binary to $(prefix)"
	@echo "  uninstall       - Remove installed files"
	@echo ""
	@echo "Testing targets:"
	@echo "  check           - Build and run all tests (use TEST=name for single test)"
	@echo "  check-unit      - Build and run only unit tests"
	@echo "  check-integration - Build and run only integration tests"
	@echo "  check-bash-tool - Build and test bash_tool integration"
	@echo "  verify-mocks    - Verify OpenAI mock fixtures (uses credentials.json or OPENAI_API_KEY)"
	@echo "  verify-mocks-anthropic - Verify Anthropic mock fixtures (uses credentials.json or ANTHROPIC_API_KEY)"
	@echo "  verify-mocks-google - Verify Google mock fixtures (uses credentials.json or GOOGLE_API_KEY)"
	@echo "  verify-mocks-all - Verify all provider mock fixtures"
	@echo "  verify-credentials - Validate API keys in ~/.config/ikigai/credentials.json"
	@echo "  check-sanitize  - Run all tests with AddressSanitizer + UBSanitizer"
	@echo "  check-valgrind  - Run all tests under Valgrind Memcheck"
	@echo "  check-helgrind  - Run all tests under Valgrind Helgrind (thread errors)"
	@echo "  check-tsan      - Run all tests with ThreadSanitizer"
	@echo "  check-dynamic   - Run all dynamic analysis (all of the above)"
	@echo ""
	@echo "Quality assurance:"
	@echo "  check-coverage  - Generate text-based coverage report (requires lcov)"
	@echo "  lint            - Run all lint checks (complexity + filesize)"
	@echo "  check-complexity - Check code complexity (threshold: $(COMPLEXITY_THRESHOLD))"
	@echo "  filesize        - Check file sizes (max: $(MAX_FILE_BYTES) bytes)"
	@echo "  ci              - Run all CI checks (lint + check-coverage + dynamic + release)"
	@echo ""
	@echo "Distribution targets:"
	@echo "  distro-images       - Build all Docker images (distros/*/Dockerfile)"
	@echo "  distro-images-clean - Remove all Docker images"
	@echo "  distro-clean        - Clean build artifacts using first Docker image"
	@echo "  distro-check        - Run CI on all supported distributions (via Docker)"
	@echo "  distro-package      - Build distribution packages (.deb, .rpm) in Docker"
	@echo ""
	@echo "Other targets:"
	@echo "  dist            - Create source distribution"
	@echo "  fmt             - Format source code with uncrustify (K&R style, line length: $(LINE_LENGTH))"
	@echo "  cloc            - Count lines of code in src/, tests/, and Makefile"
	@echo "  tags            - Generate ctags index for src/ directory"
	@echo "  install-deps    - Install build dependencies (Debian/Ubuntu)"
	@echo ""
	@echo "Build modes (use with any target):"
	@echo "  BUILD=debug    - Debug: -O0 -g3 -DDEBUG (default)"
	@echo "  BUILD=release  - Release: -O2 -g -DNDEBUG -Werror"
	@echo "  BUILD=sanitize - Sanitize: debug + ASan + UBSan"
	@echo "  BUILD=tsan     - TSan: debug + ThreadSanitizer"
	@echo "  BUILD=valgrind - Valgrind: enhanced backtraces"
	@echo ""
	@echo "Examples:"
	@echo "  make all BUILD=release"
	@echo "  make check BUILD=sanitize"
	@echo "  make prefix=/usr install"
	@echo "  make lint COMPLEXITY_THRESHOLD=10"
	@echo "  make fmt LINE_LENGTH=100"
	@echo "  make coverage COVERAGE_LINE_THRESHOLD=90"
	@echo "  make distro-images DISTROS=\"debian fedora\""
	@echo "  make distro-check DISTROS=\"debian\""
