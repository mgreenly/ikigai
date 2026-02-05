# check-coverage: Verify code coverage meets threshold
# Requires lcov. Builds with coverage instrumentation, runs tests, checks per-file coverage.

.PHONY: check-coverage coverage-build

# Internal target: build and run tests with coverage instrumentation
# Note: FILE= clears any FILE variable to ensure bulk mode for sub-makes
coverage-build:
	@find build-coverage -name "*.gcda" -delete 2>/dev/null || true
	@mkdir -p build-coverage/tests/unit build-coverage/tests/integration
	@find tests/unit -type d | sed 's|tests/unit|build-coverage/tests/unit|' | xargs mkdir -p 2>/dev/null || true
	@find tests/integration -type d | sed 's|tests/integration|build-coverage/tests/integration|' | xargs mkdir -p 2>/dev/null || true
ifdef RAW
	IKIGAI_DEV=1 BUILDDIR=build-coverage $(MAKE) -k -j$(MAKE_JOBS) check-compile FILE= CFLAGS="$(CFLAGS) $(COVERAGE_FLAGS)"
	IKIGAI_DEV=1 BUILDDIR=build-coverage $(MAKE) -k -j$(MAKE_JOBS) check-link FILE= LDFLAGS="$(LDFLAGS) $(COVERAGE_LDFLAGS)" LDLIBS="$(LDLIBS) -lgcov"
	IKIGAI_DEV=1 BUILDDIR=build-coverage $(MAKE) check-unit FILE=
	IKIGAI_DEV=1 BUILDDIR=build-coverage $(MAKE) check-integration FILE=
else
	@IKIGAI_DEV=1 BUILDDIR=build-coverage $(MAKE) -k -j$(MAKE_JOBS) check-compile FILE= CFLAGS="$(CFLAGS) $(COVERAGE_FLAGS)" >/dev/null 2>&1
	@IKIGAI_DEV=1 BUILDDIR=build-coverage $(MAKE) -k -j$(MAKE_JOBS) check-link FILE= LDFLAGS="$(LDFLAGS) $(COVERAGE_LDFLAGS)" LDLIBS="$(LDLIBS) -lgcov" >/dev/null 2>&1
	@IKIGAI_DEV=1 BUILDDIR=build-coverage $(MAKE) -s check-unit FILE= >/dev/null 2>&1
	@IKIGAI_DEV=1 BUILDDIR=build-coverage $(MAKE) -s check-integration FILE= >/dev/null 2>&1
endif

# Common lcov flags (ignore gcov errors from stale files)
LCOV_FLAGS = --rc branch_coverage=1 --ignore-errors inconsistent,deprecated,negative,gcov

check-coverage:
ifdef RAW
	@# RAW mode - show full build/test/lcov output
	$(MAKE) coverage-build
	@mkdir -p $(COVERAGE_DIR)
	lcov --capture --directory . --output-file $(COVERAGE_DIR)/coverage.info \
		$(LCOV_FLAGS) --rc lcov_branch_coverage=1
	lcov --extract $(COVERAGE_DIR)/coverage.info '*/apps/ikigai/*' '*/shared/*' \
		--output-file $(COVERAGE_DIR)/coverage.info $(LCOV_FLAGS)
	lcov --remove $(COVERAGE_DIR)/coverage.info '*/tests/*' \
		--output-file $(COVERAGE_DIR)/coverage.info $(LCOV_FLAGS)
	lcov --list $(COVERAGE_DIR)/coverage.info --list-full-path $(LCOV_FLAGS)
	@# Check for failures
	@lcov --list $(COVERAGE_DIR)/coverage.info --list-full-path $(LCOV_FLAGS) 2>&1 \
		| grep -E '\.c\s*\|' > $(COVERAGE_DIR)/files.txt
	@failed=0; \
	while IFS= read -r line; do \
		file=$$(echo "$$line" | awk -F'|' '{gsub(/^[ \t]+|[ \t]+$$/, "", $$1); print $$1}' | sed 's|^$(CURDIR)/||'); \
		LINE_PCT=$$(echo "$$line" | awk -F'|' '{if($$2 ~ /-/) print "100.0"; else {split($$2,a,"%"); gsub(/[^0-9.]/,"",a[1]); print a[1]}}'); \
		FUNC_PCT=$$(echo "$$line" | awk -F'|' '{if($$3 ~ /-/) print "100.0"; else {split($$3,a,"%"); gsub(/[^0-9.]/,"",a[1]); print a[1]}}'); \
		BRANCH_PCT=$$(echo "$$line" | awk -F'|' '{if($$4 ~ /-/) print "100.0"; else {split($$4,a,"%"); gsub(/[^0-9.]/,"",a[1]); print a[1]}}'); \
		LINE_FAIL=$$(echo "$$LINE_PCT < $(COVERAGE_THRESHOLD)" | bc); \
		FUNC_FAIL=$$(echo "$$FUNC_PCT < $(COVERAGE_THRESHOLD)" | bc); \
		BRANCH_FAIL=$$(echo "$$BRANCH_PCT < $(COVERAGE_THRESHOLD)" | bc); \
		if [ "$$LINE_FAIL" -eq 1 ] || [ "$$FUNC_FAIL" -eq 1 ] || [ "$$BRANCH_FAIL" -eq 1 ]; then \
			echo "FAIL $$file: Lines=$${LINE_PCT}%, Functions=$${FUNC_PCT}%, Branches=$${BRANCH_PCT}%"; \
			failed=$$((failed + 1)); \
		fi; \
	done < $(COVERAGE_DIR)/files.txt; \
	if [ $$failed -gt 0 ]; then \
		exit 1; \
	fi
else
	@# Phase 1: Build and run tests with coverage instrumentation
	@$(MAKE) -s coverage-build 2>&1 || { echo "🔴 preexisting errors"; exit 1; }
	@# Phase 2: Capture and filter coverage data
	@mkdir -p $(COVERAGE_DIR)
	@lcov --capture --directory . --output-file $(COVERAGE_DIR)/coverage.info \
		$(LCOV_FLAGS) --rc lcov_branch_coverage=1 --quiet >/dev/null 2>&1
	@lcov --extract $(COVERAGE_DIR)/coverage.info '*/apps/ikigai/*' '*/shared/*' \
		--output-file $(COVERAGE_DIR)/coverage.info $(LCOV_FLAGS) --quiet >/dev/null 2>&1
	@lcov --remove $(COVERAGE_DIR)/coverage.info '*/tests/*' \
		--output-file $(COVERAGE_DIR)/coverage.info $(LCOV_FLAGS) --quiet >/dev/null 2>&1
	@lcov --list $(COVERAGE_DIR)/coverage.info --list-full-path $(LCOV_FLAGS) 2>&1 \
		| grep -E '\.c\s*\|' > $(COVERAGE_DIR)/files.txt
	@# Phase 3: Output results
ifdef FILE
	@# Single file mode - show detailed coverage
	@line=$$(grep -F "$(FILE)" $(COVERAGE_DIR)/files.txt | head -1); \
	if [ -z "$$line" ]; then \
		echo "🔴 $(FILE): no coverage data"; \
		exit 1; \
	fi; \
	LINE_PCT=$$(echo "$$line" | awk -F'|' '{if($$2 ~ /-/) print "100.0"; else {split($$2,a,"%"); gsub(/[^0-9.]/,"",a[1]); print a[1]}}'); \
	FUNC_PCT=$$(echo "$$line" | awk -F'|' '{if($$3 ~ /-/) print "100.0"; else {split($$3,a,"%"); gsub(/[^0-9.]/,"",a[1]); print a[1]}}'); \
	BRANCH_PCT=$$(echo "$$line" | awk -F'|' '{if($$4 ~ /-/) print "100.0"; else {split($$4,a,"%"); gsub(/[^0-9.]/,"",a[1]); print a[1]}}'); \
	LINE_FAIL=$$(echo "$$LINE_PCT < $(COVERAGE_THRESHOLD)" | bc); \
	FUNC_FAIL=$$(echo "$$FUNC_PCT < $(COVERAGE_THRESHOLD)" | bc); \
	BRANCH_FAIL=$$(echo "$$BRANCH_PCT < $(COVERAGE_THRESHOLD)" | bc); \
	if [ "$$LINE_FAIL" -eq 1 ] || [ "$$FUNC_FAIL" -eq 1 ] || [ "$$BRANCH_FAIL" -eq 1 ]; then \
		echo "🔴 $(FILE): Lines=$${LINE_PCT}%, Functions=$${FUNC_PCT}%, Branches=$${BRANCH_PCT}%"; \
		exit 1; \
	else \
		echo "🟢 $(FILE): Lines=$${LINE_PCT}%, Functions=$${FUNC_PCT}%, Branches=$${BRANCH_PCT}%"; \
	fi
else
	@# Bulk mode - one line per file
	@rm -f $(COVERAGE_DIR)/failed.txt; \
	passed=0; failed=0; \
	while IFS= read -r line; do \
		file=$$(echo "$$line" | awk -F'|' '{gsub(/^[ \t]+|[ \t]+$$/, "", $$1); print $$1}' | sed 's|^$(CURDIR)/||'); \
		LINE_PCT=$$(echo "$$line" | awk -F'|' '{if($$2 ~ /-/) print "100.0"; else {split($$2,a,"%"); gsub(/[^0-9.]/,"",a[1]); print a[1]}}'); \
		FUNC_PCT=$$(echo "$$line" | awk -F'|' '{if($$3 ~ /-/) print "100.0"; else {split($$3,a,"%"); gsub(/[^0-9.]/,"",a[1]); print a[1]}}'); \
		BRANCH_PCT=$$(echo "$$line" | awk -F'|' '{if($$4 ~ /-/) print "100.0"; else {split($$4,a,"%"); gsub(/[^0-9.]/,"",a[1]); print a[1]}}'); \
		LINE_FAIL=$$(echo "$$LINE_PCT < $(COVERAGE_THRESHOLD)" | bc); \
		FUNC_FAIL=$$(echo "$$FUNC_PCT < $(COVERAGE_THRESHOLD)" | bc); \
		BRANCH_FAIL=$$(echo "$$BRANCH_PCT < $(COVERAGE_THRESHOLD)" | bc); \
		if [ "$$LINE_FAIL" -eq 1 ] || [ "$$FUNC_FAIL" -eq 1 ] || [ "$$BRANCH_FAIL" -eq 1 ]; then \
			echo "🔴 $$file"; \
			echo "$$file" >> $(COVERAGE_DIR)/failed.txt; \
			failed=$$((failed + 1)); \
		else \
			echo "🟢 $$file"; \
			passed=$$((passed + 1)); \
		fi; \
	done < $(COVERAGE_DIR)/files.txt; \
	total=$$((passed + failed)); \
	if [ $$failed -eq 0 ]; then \
		echo "✅ All $$total files meet $(COVERAGE_THRESHOLD)% threshold"; \
	else \
		echo "❌ $$failed/$$total files below $(COVERAGE_THRESHOLD)% threshold"; \
		exit 1; \
	fi
endif
endif
