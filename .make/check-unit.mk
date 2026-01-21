# check-unit: Run unit tests and parse XML results
# Tests output XML to reports/check/, which is parsed for structured output

.PHONY: check-unit

check-unit:
ifdef FILE
	@# Single test mode - run one test binary and show detailed per-assertion results
	@mkdir -p reports/check/$$(dirname $(FILE) | sed 's|^$(BUILDDIR)/tests/||')
	@xml_path=$$(echo $(FILE) | sed 's|^$(BUILDDIR)/tests/|reports/check/|').xml; \
	if [ ! -x "$(FILE)" ]; then \
		echo "🔴 $(FILE): binary not found (run make check-link first)"; \
		exit 1; \
	fi; \
	$(FILE) >/dev/null 2>&1 || true; \
	if [ ! -f "$$xml_path" ]; then \
		echo "🔴 $(FILE): no XML output generated"; \
		exit 1; \
	fi; \
	.make/parse-check-xml.sh "$$xml_path"; \
	if grep -q 'result="failure"' "$$xml_path"; then \
		exit 1; \
	fi
else
	@# Bulk mode - run all unit tests in parallel, one line per binary
	@# Phase 1: Ensure binaries are built (continue even if some fail)
	@$(MAKE) -s check-link >/dev/null 2>&1 || true
	@# Phase 2: Create output directories
	@mkdir -p reports/check
	@find tests/unit -type d | sed 's|^tests/|reports/check/|' | xargs mkdir -p 2>/dev/null || true
	@# Phase 3: Run all tests in parallel (each writes XML, suppress console output)
	@echo $(UNIT_TEST_BINARIES) | tr ' ' '\n' | xargs -P$(MAKE_JOBS) -I{} sh -c '{} >/dev/null 2>&1 || true'
	@# Phase 4: Check each binary's XML for pass/fail, one line per binary
	@passed=0; failed=0; \
	for bin in $(UNIT_TEST_BINARIES); do \
		xml=$$(echo $$bin | sed 's|^$(BUILDDIR)/tests/|reports/check/|').xml; \
		if [ ! -f "$$xml" ]; then \
			echo "🔴 $$bin"; \
			failed=$$((failed + 1)); \
		elif grep -q 'result="failure"' "$$xml"; then \
			echo "🔴 $$bin"; \
			failed=$$((failed + 1)); \
		else \
			echo "🟢 $$bin"; \
			passed=$$((passed + 1)); \
		fi; \
	done; \
	total=$$((passed + failed)); \
	if [ $$failed -eq 0 ]; then \
		echo "✅ All $$total test binaries passed"; \
	else \
		echo "❌ $$failed/$$total test binaries failed"; \
		exit 1; \
	fi
endif
