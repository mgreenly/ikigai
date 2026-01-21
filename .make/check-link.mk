# check-link: Link all binaries (main, tools, tests)
# Uses .d files to discover helper and mock dependencies automatically

.PHONY: check-link

# Dependency extraction function (used in link rules)
# Reads the .d file for a test and extracts linkable dependencies:
#   - *_helper.o files (test helpers)
#   - *_mock.o files (mock implementations)
#   - helpers/*_test.o files (test suite helpers in helpers/ subdirs)
# Usage: $(call deps_from_d_file,path/to/test.d)
define deps_from_d_file_script
grep -oE '[^ \\:]*(_helper|_mock)\.h|[^ \\:]*helpers/[^ \\:]*_test\.h' $(1) 2>/dev/null | sort -u | while read p; do \
  while echo "$$p" | grep -q '[^/]*/\.\./'; do p=$$(echo "$$p" | sed 's|[^/]*/\.\./||'); done; \
  echo "$(BUILDDIR)/$${p%.h}.o"; \
done
endef

# =============================================================================
# Main binary
# =============================================================================

bin/ikigai: $(IKIGAI_OBJECTS)
	@mkdir -p $(dir $@)
	@if $(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) 2>&1; then \
		echo "🟢 $@"; \
	else \
		echo "🔴 $@"; \
		exit 1; \
	fi

# =============================================================================
# Tool binaries
# =============================================================================

# Secondary expansion needed to convert hyphens in binary names back to underscores for directory lookup
# e.g., libexec/ikigai/file-read-tool needs to find build/tools/file_read/main.o
.SECONDEXPANSION:

# Each tool links with: tool-specific objects + all src objects + vendor
# Over-link strategy: gc-sections strips unused symbols
libexec/ikigai/%-tool: $$(BUILDDIR)/tools/$$(subst -,_,$$*)/main.o $(MODULE_OBJ) $(VCR_STUBS)
	@mkdir -p $(dir $@)
	@tool_dir=$$(echo "$*" | tr '-' '_'); \
	tool_objs=$$(find $(BUILDDIR)/tools/$$tool_dir -name '*.o' 2>/dev/null | tr '\n' ' '); \
	if $(CC) $(LDFLAGS) -o $@ $$tool_objs $(MODULE_OBJ) $(VCR_STUBS) $(LDLIBS) 2>&1; then \
		echo "🟢 $@"; \
	else \
		echo "🔴 $@"; \
		exit 1; \
	fi

# =============================================================================
# Test binaries - automatic helper/mock discovery from .d files
# =============================================================================

# Pattern rule for unit tests
# Helpers and mocks are discovered from the .d file generated during compilation
# VCR_STUBS provides weak symbol fallbacks for tests that don't use VCR directly
# TOOL_LIB_OBJECTS included for *_direct_test files that test tool functionality
# Mocks linked BEFORE MODULE_OBJ with --allow-multiple-definition so mocks override real implementations
$(BUILDDIR)/tests/unit/%_test: $(BUILDDIR)/tests/unit/%_test.o $(MODULE_OBJ) $(TOOL_LIB_OBJECTS) $(VCR_STUBS)
	@mkdir -p $(dir $@)
	@deps=$$($(call deps_from_d_file_script,$(<:.o=.d))); \
	if $(CC) $(LDFLAGS) -Wl,--allow-multiple-definition -o $@ $< $$deps $(MODULE_OBJ) $(TOOL_LIB_OBJECTS) $(VCR_STUBS) -lcheck -lm -lsubunit $(LDLIBS) 2>&1; then \
		echo "🟢 $@"; \
	else \
		echo "🔴 $@"; \
		exit 1; \
	fi

# Pattern rule for integration tests
# Same mock-first linking as unit tests
$(BUILDDIR)/tests/integration/%_test: $(BUILDDIR)/tests/integration/%_test.o $(MODULE_OBJ) $(VCR_STUBS)
	@mkdir -p $(dir $@)
	@deps=$$($(call deps_from_d_file_script,$(<:.o=.d))); \
	if $(CC) $(LDFLAGS) -Wl,--allow-multiple-definition -o $@ $< $$deps $(MODULE_OBJ) $(VCR_STUBS) -lcheck -lm -lsubunit $(LDLIBS) 2>&1; then \
		echo "🟢 $@"; \
	else \
		echo "🔴 $@"; \
		exit 1; \
	fi

# =============================================================================
# check-link target
# =============================================================================

check-link:
ifdef FILE
	@rm -f $(FILE) 2>/dev/null; \
	output=$$($(MAKE) -s $(FILE) 2>&1); \
	status=$$?; \
	if [ $$status -eq 0 ]; then \
		echo "🟢 $(FILE)"; \
	else \
		echo "$$output" | grep -E "undefined reference|multiple definition|cannot find" | \
			sed "s|$$(pwd)/||g" | while read line; do \
			echo "🔴 $$line"; \
		done; \
	fi; \
	exit $$status
else
	@# Phase 1: Compile all objects in parallel
	@$(MAKE) -k -j$(MAKE_JOBS) $(ALL_OBJECTS) 2>&1 | grep -E "^(🟢|🔴)" || true
	@# Phase 2: Link all binaries in parallel
	@$(MAKE) -k -j$(MAKE_JOBS) $(ALL_BINARIES) 2>&1 | grep -E "^(🟢|🔴)" || true; \
	failed=0; \
	for bin in $(ALL_BINARIES); do \
		[ ! -f "$$bin" ] && failed=$$((failed + 1)); \
	done; \
	if [ $$failed -eq 0 ]; then \
		echo "✅ All binaries linked"; \
	else \
		echo "❌ $$failed binaries failed to link"; \
		exit 1; \
	fi
endif
