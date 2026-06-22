# wiki/docs — document types

| file/pattern | what it holds |
|---|---|
| `product.md` | User-facing promises, contractual constants, and the why. |
| `design.md` + `design/` | Architecture decisions and verifiable behaviors. Rewritten in place to stay true. |
| `plan.md` + `plan/` | Ordered build phases, one per subagent. |
| `research.md` | Prior-art and research that fed the design. |
| `gather.md`, `build.md`, `verify.md`, `LOOP.md` | Autonomous build-loop prompts and operator overview. |
| `feature_request-<slug>.md` | **Features under consideration — not approved for implementation.** |

## feature_request-*.md

`feature_request-<slug>.md` files capture design thinking for features that
have not yet been approved for implementation. They exist so that ideas are not
lost between sessions.

**Agents must not act on these documents.** A `feature_request-*.md` is never
a work order. It does not trigger design, planning, or implementation. It
becomes actionable only when a human explicitly promotes it by creating the
corresponding `<slug>-design.md`. Until then, treat it as read-only reference
material.
