# Phase 5 — GitHub-fact intake

*Realizes design Decision 3 (webhooks consumer, dispatch table, loop
suppression, double-trigger guard). Depends on Phase 4.*

`internal/repos/intake.go`: the consumer handler decoding the webhooks
`received` payload (base64 body + `headers` allowlist), the
`x-github-event` → handler dispatch table with its single v1 row (`issues`),
the ordered gates (malformed → `ErrSkip`; unknown event → advance; bot
sender per `REPOS_BOT_LOGIN` → advance; non-`labeled`/non-`execute` /
closed issue → advance; active session for the issue → advance), and the
happy path: `EnsureRepo` from the delivery's repository facts +
session enqueue with `owner_email` from the hook owner. The
`appkit.Consumer` declaration subscribes source `webhooks`, filter
`webhooks:received/<REPOS_GITHUB_HOOK>`. Tests drive recorded GitHub
delivery fixtures through the handler over real SQLite with the Phase 3/4
seams stubbed at their interfaces.

**Done when:** R-EQ4C-D8F5, R-ERC8-R05U, R-ESK5-4RWJ, R-ETS1-IJN8,
R-EUZX-WBDX, and R-EW7U-A34M are each covered by a clearly-named test, and
the suite is green per design Conventions.
