# dashboard — Product (web pages restructure)

**Authority: intent.** This document owns *why* this change exists, *for whom*,
what is in and out of scope, and what we **promise** the user — in outcome terms
only. Mechanism, route tables, template structure, redirect codes, and test
assertions live in `project/design/README.md`. Where the two touch observable
behavior, product states the *promise* and design states the *exact, checkable
proof*. This product doc scopes the dashboard's web-surface reshape: splitting the
single hybrid apex page into purpose-built pages, enriching the logged-out login
page with a brief, diminished explanation of the **ikigenba** name, and adding an
owner-only **telemetry** page that graphs the box's resource health over the last
day. It does not re-state the dashboard's whole product (identity, OAuth AS, push,
inventory) — only the web surface this change reshapes.

## Problem

Today the dashboard serves **one** hybrid page at `/`. Logged out, it is a
sign-in wall. Logged in, that same page tries to be everything at once: it is the
**home** a returning owner lands on (the "connect your agent" install snippet and
the list of the box's MCP services) **and** the **account-management console**
(create/revoke personal access tokens, view/revoke the OAuth grants other agents
hold). Two unrelated jobs share one scroll.

That conflation hurts both jobs. The owner who just wants to **get connected** —
paste the install line, see what services exist, click through to one — wades
past token tables that are irrelevant to that task. The owner who wants to
**manage their account** — rotate a leaked token, revoke a client's grant — hunts
for those controls below the install instructions on a page that re-renders its
"welcome, connect your agent" framing every visit. The home page and the settings
page want different framing, different prominence, and different return cadence,
and one page can give neither.

The suite is also moving so that **every service serves its own landing page** at
`/srv/<svc>/`. The dashboard's service list still shows each service only as a row
of raw MCP text for hand-wiring — there is nowhere to *click through* to a service
now that each has a real human page.

## Purpose

Separate **"get connected"** from **"manage my account"** by giving each its own
page, and turn the service list into real navigation:

- The **landing page** (logged-in `/`) becomes a clean **home**: who you are, how
  to connect an agent, and a directory of the box's services you can **click into**
  (each name links to that service's own landing page). It is the first thing a
  returning owner sees and it stays focused on connecting and navigating.
- A new **profile page** (`/profile`) becomes the **account console**: personal
  access tokens (create / revoke) and OAuth grants (view / revoke) — the security
  controls that were buried on the home page — gathered in one deliberate place
  you go to when you mean to manage access, reached by clicking your email.
- The **login page** (logged-out `/`) stays functionally just sign-in, but now
  also tells a first-time visitor what **ikigenba** means — a quiet, diminished
  colophon beneath the control-plane tagline and the sign-in button. It orients;
  it adds no new control.
- A new **telemetry page** (`/telemetry`) becomes the owner's **at-a-glance box
  health view**: how much memory and disk the box has free, and how much memory
  and disk each service is consuming, each drawn as a graph of the **last 24
  hours**. It is reached from a tile on the landing page and is for watching the
  box, not managing it — it carries no controls, only graphs.

The dashboard is the **apex** app, so unlike the other services this page is not a
generic name+version card — the dashboard's logged-in landing **is** its web home.
This change is bespoke to the dashboard; the rest of the suite gets the uniform
v1 landing.

## Users

- **The owner, in a browser.** Signs in, lands on the home page, connects an agent
  by pasting one line, and clicks a service name to open that service's page. When
  they mean to manage access, they click their email to reach the profile page and
  there create or revoke a token, or revoke a client's grant. The two intents now
  live on two pages, each framed for its job.

## Scope

This change does exactly this and only this:

- **Four pages, two of them new.** Keep the logged-out `/` login page as the
  sign-in page — its only function — and add to it a **diminished name-origin
  colophon** explaining the ikigenba name, below the tagline and the sign-in
  button. Keep the logged-in `/` as the **home/landing** page. Add a **profile**
  page and a **telemetry** page, each at its own new session-gated route.
- **Telemetry = watch the box.** The telemetry page graphs, over the **last 24
  hours**, the box's **free memory** and **free disk**, and **each service's
  memory usage and disk usage**. It refreshes about once a minute while open, is
  reached from a tile on the landing page, and is gated to the signed-in owner. It
  shows the most recent day only and starts empty after a restart; it carries no
  controls.
- **Landing = connect + navigate.** The logged-in home keeps the **install
  instructions** and the **service list**, shows the owner's email at the top, and
  carries **sign-out**. Each service in the list is a **link** to that service's
  own page at `/srv/<svc>/`. The owner's email is a **link** to the profile page.
- **Profile = manage access.** The profile page holds the **personal-access-token**
  management (create, list, revoke) and the **OAuth grant** management (view live,
  revoke) that used to sit on the home page. It is reached by clicking the email on
  the landing page and is gated to the signed-in owner.
- **Token management leaves the landing.** Personal access tokens and OAuth grants
  no longer appear on the logged-in home page — they live only on the profile page.
- **Doc truth follows the code.** The dashboard's standing "keep the apex `/` a
  single hybrid page, do not split it into a separate IAM console" rule is now
  **false** and is purged, replaced by the three-page truth.

It deliberately does **nothing else** — in particular it does not: add new
account-management capabilities (the PAT and grant features are **moved, not
changed**); change how login, OAuth, push, or inventory work; add per-resource
authorization to the profile or telemetry page beyond "signed-in owner";
introduce new MCP verbs; give the telemetry page any control (it only shows
graphs); persist telemetry history across restarts or alert on it; or give the
dashboard a generic name+version landing card (its home page is its landing).

## Contractual constants

Promised values the design must honor verbatim and never re-declare:

- **The apex serves these human pages:** the **login** page (logged-out `/`), the
  **landing/home** page (logged-in `/`), the **profile** page (session-gated
  route), and the **telemetry** page (session-gated route). The profile and
  telemetry pages are the two session-gated routes added by this change.
- **The telemetry page graphs the last 24 hours of box health and nothing else.**
  It shows free memory, free disk, and per-service memory and disk usage; it is
  gated to the signed-in owner, refreshes about once a minute, and carries no
  controls. Its history is the most recent day and does not survive a restart.
- **The login page keeps its control-plane tagline and sign-in control verbatim,**
  and carries the name-origin colophon **only** in its logged-out form — the
  colophon never appears on the logged-in landing/home page, and adds no new
  control.
- **The profile page is gated to a signed-in owner.** A visitor without a live
  session never sees profile content.
- **Personal-access-token and OAuth-grant management live only on the profile
  page** after this change — never on the landing/home page.
- **Each service name on the landing links to that service's own page at
  `/srv/<svc>/`** — the human landing page, not the raw MCP resource URL.

## What we promise (user-facing behavior)

- **Logged out, `/` is just sign-in** — and tells you what the name means. The
  control-plane tagline and the "Sign in with Google" button are exactly as
  before; below them sits a quiet, diminished explanation of the **ikigenba** name
  (its two Japanese roots and what the word means together). No new control.
- **Logged in, `/` is a focused home.** It shows who you are, how to connect an
  agent (the same paste-one-line install instructions), and the box's services —
  and nothing about token administration.
- **You navigate to a service by clicking its name.** Each service in the home
  list links to that service's own page.
- **You manage access on a page you choose to visit.** Clicking your email opens
  the profile page; there — and only there — you create and revoke personal access
  tokens and view and revoke the OAuth grants your connected clients hold.
- **The profile page is yours alone.** Reaching it requires a live session; signed
  out, you are sent back to the login page rather than shown account controls.
- **Sign-out stays on the home page** where you land, not hidden on a settings
  screen.
- **No capability is lost in the move.** Every token and grant action that worked
  on the old hybrid page works on the profile page, identically — only its
  location changed.
- **You can watch the box's health on the telemetry page.** From the landing you
  open a telemetry page that graphs, over the last 24 hours, how much memory and
  disk the box has free and how much memory and disk each service is using. The
  graphs advance about once a minute while the page is open. Like the profile
  page, it is yours alone — signed out, you are sent back to the login page. It
  shows only the most recent day and begins empty after a restart.

## Success criteria (outcomes)

Each is a result the owner can confirm against the running dashboard:

- Visiting `/` while signed out shows the sign-in page — the control-plane
  tagline, the "Sign in with Google" button, and beneath them a diminished
  explanation of the ikigenba name — and no account controls.
- The name-origin explanation is visible only signed out; once signed in, the
  home page shows no such colophon.
- Visiting `/` while signed in shows the home page: my email, the connect-your-agent
  install instructions, and the list of services — with **no** token or grant
  controls on it.
- Clicking a service's name in the home list opens that service's own page at
  `/srv/<svc>/`.
- Clicking my email on the home page opens the profile page.
- The profile page shows my personal access tokens and lets me create and revoke
  them, and shows my OAuth grants and lets me revoke them — the same actions that
  used to be on the home page.
- Visiting the profile route while signed out does not reveal account controls; I
  am returned to the login page.
- Sign-out is available from the home page.
- The home page shows a tile that opens the telemetry page.
- Visiting `/telemetry` while signed in shows graphs of the box's free memory and
  free disk over the last 24 hours, and of each service's memory and disk usage
  over the last 24 hours; the graphs advance about once a minute while the page
  stays open.
- Visiting the telemetry route while signed out does not reveal the graphs; I am
  returned to the login page.
- After this change the dashboard's docs describe the login, landing, profile, and
  telemetry pages, and no longer claim the apex is a single hybrid page that must
  not be split or capped at three pages.
