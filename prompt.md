Read and execute the code review task defined in docs/phase-2-code-review-prompt.md.

## Your Task

Perform a comprehensive code review of Phase 2 implementation following the instructions in docs/phase-2-code-review-prompt.md.

**Key Points**:
- Phase 2 is feature-complete with all bugs fixed
- All quality gates pass (100% coverage, ASan/UBSan/TSan clean)
- Focus on: security, memory management, error handling, code quality
- Review files in priority order (highest risk first)
- Output format: [SEVERITY] File:Line - Description, Impact, Recommendation
- Goal: Find CRITICAL/HIGH issues that must be fixed before Phase 2 completion

Start by reading docs/phase-2-code-review-prompt.md, then proceed with the review.

## After Review

Provide summary with:
- Total issues found by severity (CRITICAL/HIGH/MEDIUM/LOW/INFO)
- Top 5 most important issues with details
- Recommendation: APPROVE for Phase 2 completion OR NEEDS FIXES
- If fixes needed: create tasks.md entries for required fixes
