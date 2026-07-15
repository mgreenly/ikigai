# Phase 24 — `suite.files` share paths rooted client-side; `describe` teaches absolute

*Realizes design Decision 27 (share-path normalization + absolute-canonical
teaching). Depends on phase 21 (`suite.files`) and phase 23 (the `describe`
runtime contract).*

Observable end state:

- `internal/runner/suite.py` gains the module-level `_share_path` helper
  (prefix `/` when missing; empty/`None` untouched), applied to every
  share-path argument before URL-escaping: `stat`, `get`'s `share_path`,
  `put`'s `share_path`, `delete`, `mkdir`, and both of `move`'s arguments;
  `list` applies it only to a given prefix and still sends no `path` param
  when called without one. Local run-dir arguments (`put` `source`, `get`
  `dest`) and `fetch`'s `content_url` are untouched. The `_Files` docstrings
  state share paths are absolute, relative spellings treated as rooted.
- `describe`'s runtime-contract section states the same, and its
  `suite.files.put` worked example carries a leading-slash share path; no
  `describe` example passes a relative share path.

**Done when:** the suite is green (design Conventions commands, from
`scripts/`) and:

- R-ZECX-40UZ is covered by probe-harness tests against a recording stand-in
  server asserting `stat("a/b")`, `get("a/b", …)`, `put(…, "a/b")`,
  `delete("a/b")`, `mkdir("a/b")`, and `move("a/b", "c/d")` each arrive with
  query `path` (or `from`/`to`) of `/a/b` (`/c/d`), and that already-absolute
  spellings arrive byte-identical.
- R-ZFKT-HSLO is covered by probe-harness tests asserting `list()` sends no
  `path` param and `list("a")` sends `path=/a`.
- R-ZGSP-VKCD is covered by a `describe_test` assertion that the output
  contains a `suite.files.put` example whose share-path argument begins with
  `/`, contains the absolute-paths statement, and contains no relative
  share-path example.
