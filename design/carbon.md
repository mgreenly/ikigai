# Carbon — Design System Rules

**Carbon** is a monochrome system built on neutral grays, signalled by a single
blue highlight and a red reserved strictly for errors. When building any UI in
this project, follow these rules and consume the tokens from
[`tokens.css`](./tokens.css).

## Core rules

- **Blue (`#2563EB`) is the only signal color.** Use it for highlights, links,
  focus rings, primary actions, and selected states — never decoratively.
- **Red (`#DC2626`) is reserved for errors and destructive actions only.** Never
  for emphasis.
- **Everything else is neutral gray.** Color is the exception, not the rule —
  bias to monochrome.
- **Spacing is a 4px grid.** Never use arbitrary values; use the `--space-*`
  tokens.
- **Stacking is named, never hand-tuned.** Use the `--z-*` tokens.
- **Focus state is always a 3px `--color-accent-weak` ring**; active darkens the
  fill ~10% and nudges 1px down.
- **Consume semantic tokens, not raw ramps.** Components reference
  `--color-*`, `--space-*`, etc. — not `--neutral-600` directly. Re-point the
  semantic aliases (only those) to theme or add dark mode later.
- **Radius is tight.** `--radius` (4px) is the default; corners stay sharp.
- **Prefer hairline borders over shadow.** Use `--color-border` for separation;
  shadows are subtle and reserved for genuine elevation.

## Typography

- **Pairing:** Space Grotesk (display), IBM Plex Sans (body), IBM Plex Mono
  (labels/code/data).
- Letter-spacing is `-.02em` on sizes above 28px (Display, H1, H2); `0` below.

| Token        | Size / line-height / weight | Family  |
| ------------ | --------------------------- | ------- |
| Display      | 56px / 1.04 / 700           | display |
| Heading 1    | 40px / 1.1 / 600            | display |
| Heading 2    | 30px / 1.16 / 600           | display |
| Heading 3    | 22px / 1.25 / 600           | display |
| Body Large   | 18px / 1.6 / 400            | body    |
| Body         | 15px / 1.6 / 400            | body    |
| Small        | 13px / 1.5 / 400            | body    |
| Mono · Label | 12px / 1.4 / 500            | mono    |

## Iconography

- Outline only, single **1.5px** stroke, 24px grid, round caps.
- Never mix filled and outline styles.
- Icon sizes: **14 / 18 / 24px** (`--icon-sm` / `--icon-md` / `--icon-lg`).

## Control sizing

- Button / input heights: **sm 30px · md 38px · lg 46px**
  (`--control-h-sm` / `-md` / `-lg`).
- Avatar sizes: **24 / 36 / 48px** (`--avatar-sm` / `-md` / `-lg`).
- Minimum touch target: **44px** (`--touch-target-min`).

## Layout

- Max content width **1120px**, **12** columns, **24px** column gap, **32px**
  gutters (`--layout-*`).
- Breakpoints: **sm 640 · md 768 · lg 1024 · xl 1280** (px). Use directly in
  `@media` — CSS variables can't be used inside media queries.

## Elevation & motion

- Elevation maps to the shadow scale: **0** flat (none) · **1** raised
  (`--shadow-sm`) · **2** overlay (`--shadow-md`) · **3** floating
  (`--shadow-lg`) · **4** top (`--shadow-xl`).
- Default transition: `--duration-base` (200ms) with `--ease-standard`. Use
  `--ease-entrance` for elements appearing, `--ease-exit` for leaving,
  `--ease-spring` for playful emphasis.

## Token reference

All values live in [`tokens.css`](./tokens.css) as CSS custom properties,
organized as **primitives** (ramps + scales) → **semantic aliases** → consumed
by components. See that file for the complete, authoritative list.
