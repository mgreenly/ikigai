# check-complexity: Verify cyclomatic complexity and nesting depth are under threshold

.PHONY: check-complexity

COMPLEXITY_THRESHOLD ?= 15
NESTING_DEPTH_THRESHOLD ?= 5

check-complexity:
ifdef FILE
	@output=$$(complexity --threshold=$(COMPLEXITY_THRESHOLD) --no-header --scores $(FILE) 2>&1); \
	has_complexity=$$(echo "$$output" | grep -E "^\s+[0-9]+" | grep -v "No procedures" || true); \
	has_nesting=$$(echo "$$output" | grep -E "nesting depth reached level [$(shell echo $$(($(NESTING_DEPTH_THRESHOLD) + 1)))-9]" || true); \
	if [ -z "$$has_complexity" ] && [ -z "$$has_nesting" ]; then \
		echo "🟢 $(FILE)"; \
	else \
		if [ -n "$$has_complexity" ]; then \
			echo "$$has_complexity" | while read score lnct nclns rest; do \
				file_line=$$(echo "$$rest" | sed 's/: .*//' | tr '()' ':' | sed 's/:$$//'); \
				func=$$(echo "$$rest" | sed 's/.*: //'); \
				echo "🔴 $$file_line: $$func (complexity $$score)"; \
			done; \
		fi; \
		if [ -n "$$has_nesting" ]; then \
			echo "$$has_nesting" | while read line; do \
				level=$$(echo "$$line" | grep -oE "level [0-9]+" | grep -oE "[0-9]+"); \
				echo "🔴 $(FILE): nesting depth $$level (max $(NESTING_DEPTH_THRESHOLD))"; \
			done; \
		fi; \
		exit 1; \
	fi
else ifdef RAW
	@# RAW mode - show full complexity output for failures
	@failed=0; \
	for file in $(SRC_FILES) $(TEST_FILES); do \
		output=$$(complexity --threshold=$(COMPLEXITY_THRESHOLD) --no-header --scores "$$file" 2>&1); \
		has_complexity=$$(echo "$$output" | grep -E "^\s+[0-9]+" | grep -v "No procedures" || true); \
		has_nesting=$$(echo "$$output" | grep -E "nesting depth reached level [6-9]" || true); \
		if [ -n "$$has_complexity" ] || [ -n "$$has_nesting" ]; then \
			echo "=== $$file ==="; \
			echo "$$output"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	if [ $$failed -gt 0 ]; then \
		exit 1; \
	fi
else
	@# Bulk mode - check all source files
	@passed=0; failed=0; \
	for file in $(SRC_FILES) $(TEST_FILES); do \
		output=$$(complexity --threshold=$(COMPLEXITY_THRESHOLD) --no-header --scores "$$file" 2>&1); \
		has_complexity=$$(echo "$$output" | grep -E "^\s+[0-9]+" | grep -v "No procedures" || true); \
		has_nesting=$$(echo "$$output" | grep -E "nesting depth reached level [6-9]" || true); \
		if [ -z "$$has_complexity" ] && [ -z "$$has_nesting" ]; then \
			echo "🟢 $$file"; \
			passed=$$((passed + 1)); \
		else \
			echo "🔴 $$file"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	total=$$((passed + failed)); \
	if [ $$failed -eq 0 ]; then \
		echo "✅ All $$total files under complexity/nesting threshold"; \
	else \
		echo "❌ $$failed/$$total files exceed complexity/nesting threshold"; \
		exit 1; \
	fi
endif
