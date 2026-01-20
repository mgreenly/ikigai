# check-link: Link all binaries (main, tools, tests)
# This target links all binaries in the project

.PHONY: check-link

# Pattern rule: link main binary
bin/ikigai: $(IKIGAI_OBJECTS)
	@mkdir -p $(dir $@)
	@if $(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) 2>&1; then \
		echo "🟢 $@"; \
	else \
		echo "🔴 $@"; \
		exit 1; \
	fi

# Pattern rule: link tool binaries
# Each tool links with: tool-specific objects + all src objects + vendor (over-link, gc-sections strips unused)
libexec/ikigai/%-tool: $(BUILDDIR)/tools/%/main.o $(MODULE_OBJ)
	@mkdir -p $(dir $@)
	@tool_objs=$$(find $(BUILDDIR)/tools/$* -name '*.o' 2>/dev/null | tr '\n' ' '); \
	if $(CC) $(LDFLAGS) -o $@ $$tool_objs $(MODULE_OBJ) $(LDLIBS) 2>&1; then \
		echo "🟢 $@"; \
	else \
		echo "🔴 $@"; \
		exit 1; \
	fi

# Pattern rule: link unit test binaries
# Each test links with: test .o + module objects + test helpers + VCR stubs
$(BUILDDIR)/tests/unit/%_test: $(BUILDDIR)/tests/unit/%_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS)
	@mkdir -p $(dir $@)
	@if $(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) 2>&1; then \
		echo "🟢 $@"; \
	else \
		echo "🔴 $@"; \
		exit 1; \
	fi

# Pattern rule: link integration test binaries
$(BUILDDIR)/tests/integration/%_test: $(BUILDDIR)/tests/integration/%_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(VCR_STUBS)
	@mkdir -p $(dir $@)
	@if $(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) 2>&1; then \
		echo "🟢 $@"; \
	else \
		echo "🔴 $@"; \
		exit 1; \
	fi

# check-link: Link all binaries
check-link:
ifdef FILE
	@echo "⚠️  FILE= mode not supported for check-link (skipped)"
else
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
