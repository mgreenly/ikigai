# check-unit: Run unit tests and parse XML results
# Tests output XML to reports/check/, which is parsed for structured output

.PHONY: check-unit

check-unit:
ifdef FILE
	@# Single test mode - run one test binary and show detailed results
	@mkdir -p reports/check/$$(dirname $(FILE) | sed 's|^build/tests/||')
	@xml_path=$$(echo $(FILE) | sed 's|^build/tests/|reports/check/|').xml; \
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
	@# Bulk mode - run all unit tests in parallel
	@# Phase 1: Ensure binaries are built (continue even if some fail)
	@$(MAKE) -s check-link >/dev/null 2>&1 || true
	@# Phase 2: Create output directories
	@mkdir -p reports/check
	@find tests/unit -type d | sed 's|^tests/|reports/check/|' | xargs mkdir -p 2>/dev/null || true
	@# Phase 3: Run all tests in parallel (each writes XML, suppress console output)
	@echo $(UNIT_TEST_BINARIES) | tr ' ' '\n' | xargs -P$(MAKE_JOBS) -I{} sh -c '{} >/dev/null 2>&1 || true'
	@# Phase 4: Parse all XML files and report results
	@passed=0; failed=0; \
	for xml in $$(find reports/check/unit -name '*.xml' 2>/dev/null | sort); do \
		.make/parse-check-xml.sh "$$xml"; \
		fc=$$(grep -c 'result="failure"' "$$xml" 2>/dev/null) || fc=0; \
		pc=$$(grep -c 'result="success"' "$$xml" 2>/dev/null) || pc=0; \
		failed=$$((failed + fc)); \
		passed=$$((passed + pc)); \
	done; \
	total=$$((passed + failed)); \
	if [ $$failed -eq 0 ]; then \
		echo "✅ All $$total tests passed"; \
	else \
		echo "❌ $$failed/$$total tests failed"; \
		exit 1; \
	fi
endif
