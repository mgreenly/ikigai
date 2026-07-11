# Phase 14 — rename the bearer tier's prompts-named plumbing to scripts

*Realizes design Decision 16 (scripts-named identity plumbing). Depends on
Phase 13 (both phases edit `scripts/etc/nginx.conf`; sequencing them keeps each
diff single-purpose).*

Renames the copy-pasted prompts-named identifiers in `scripts/etc/nginx.conf` to
their scripts-owned equivalents: `$prompts_owner` → `$scripts_owner`,
`$prompts_client` → `$scripts_client` (in both `auth_request_set` and
`proxy_set_header`), and `@prompts_authn_500` → `@scripts_authn_500` (in both
the `error_page 500 =` reference and the `location` definition), including any
comment mentions. Behavior is identical; this is naming hygiene inside the
shared apex server block (nginx tolerates the duplicate named location today —
see D16). No Go code changes beyond the content-assertion tests.

**Done when:** the suite is green (per design *Conventions*) and each id below is
covered by a clearly-named test reading `scripts/etc/nginx.conf` from disk:

- R-4EOV-BQWQ — the bearer prefix `location /srv/scripts/` sets and forwards
  identity through `$scripts_owner`/`$scripts_client`, its 429 re-emit is
  `error_page 500 = @scripts_authn_500;`, and `location @scripts_authn_500` is
  defined.
- R-4FWR-PINF — the string `prompts` does not occur anywhere in
  `scripts/etc/nginx.conf`.
