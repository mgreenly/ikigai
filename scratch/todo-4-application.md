# TODO: Application Layer Alignment

Plan docs: `scratch/plan/04-application/`
Status: 3 items to resolve

---

## 4.1 Missing: Model Switch Rejection During Active Request

**Priority:** High (affects correctness)

- **Issue:** Plan specifies rejecting `/model` during streaming but task lacks implementation detail
- **Plan ref:** 04-application/commands.md - Model switch rejection
- **Action:** Add active request check to `model-command.md`

**Implementation needed:**
```c
// Check agent->curl_still_running > 0 before processing
if (agent->curl_still_running > 0) {
    // Display error: "Cannot switch model while request in progress"
    return;
}
```

**Task to update:**
- [ ] `model-command.md` - add active request check section

---

## 4.2 Missing: No Model Configured Error Handler

**Priority:** Critical (blocks implementation)

- **Issue:** Plan specifies error when sending message without model configured, but no task owns this
- **Plan ref:** 04-application/commands.md - No model configured
- **Action:** Assign to `repl-provider-routing.md` or create dedicated task

**Expected behavior:**
1. User sends message but no model configured
2. Display error: "No model configured. Use /model to select one."
3. Return to input prompt
4. Do NOT save message to database
5. Do NOT send to provider

**Resolution options:**
- [ ] Add to `repl-provider-routing.md` - check before starting request
- [ ] Or create `repl-validation.md` task for pre-send checks

---

## 4.3 Inconsistency: Thinking Level Names

**Priority:** Low (documentation)

- **Issue:** Plan uses 4 levels (none/low/med/high) but task hints at 5 (includes "extended")
- **Plan ref:** 04-application/commands.md vs model-command.md
- **Action:** Align naming across plan and tasks

**Investigation needed:**
- [ ] Read `model-command.md` - check actual thinking levels
- [ ] Check if "extended" is real or documentation error
- [ ] Update plan or task to match

**If 4 levels (plan is correct):**
- none, low, med, high

**If 5 levels (task is correct):**
- none, low, med, high, extended

---

## Completion Checklist

- [ ] 4.1 resolved
- [ ] 4.2 resolved
- [ ] 4.3 resolved
