# Ikigenba — Positioning

**The line:**

> OpenClaw and Hermes build employees. Ikigenba is the company they work at.

## The paragraphs

> **Ikigenba isn't an AI agent — it's the company an agent works at.** The last
> eighteen months taught the industry one lesson over and over: wait, and the
> model improves so much it no longer needs the crutches you built for it. The
> personal-agent tools — OpenClaw, Hermes — are made almost entirely of those
> crutches: bolted-on memory, self-improving skill libraries, user modeling, a
> persona that rewrites itself. That's the part of the stack with the shortest
> shelf life, because it's exactly what the next model ships natively. Ikigenba
> bets the other way. It is *only* tools for an agent to use — the durable half
> of the stack that a smarter model makes *more* valuable, not less: the systems
> of record (CRM, ledger, a knowledge wiki), the identity and access control,
> the event plane that lets one action ripple through a business, and the
> deployment discipline to run it all. Bring whatever agent you want; Ikigenba
> is where its work persists after it logs off.

> It's **deployment-target-first**: an opinionated structure where every
> capability is a small, uniform service with the same MCP footprint, the same
> operational verbs, its own database, and a built-in pub/sub event plane, so
> services react to each other instead of being orchestrated. It's **multi-user
> and governed from day one** — real identity, an audit trail, one trust
> boundary — built for a business or organization, not a single power user. And
> it's **delivered, not assembled**: one command stands up a complete, isolated
> appliance per customer in the cloud, operated as a fleet. A company doesn't get
> smarter by being clever about its employees' brains — it gives them an office,
> systems, a badge, and records, and the better the employee, the more that's
> worth. That's the bet: the employees keep improving, and you don't want to be
> the one selling last year's employee.

## What's different (the five that land hardest)

- **A toolset, not an agent.** They *are* the autonomous actor; Ikigenba is the
  governed place the actor works. As agents get commoditized, the backend is
  where durable value lives.
- **Built from what the model *can't* absorb.** Tools, state, identity, the
  event plane — the world, not the mind. No memory crutches or self-modifying
  skills with a depreciation fuse on them.
- **Multi-user and governed from the foundation.** One identity authority,
  per-client tokens, audit trail, one isolated box per customer — a market a
  single-user personal agent architecturally cannot enter.
- **No marketplace to poison, no persona to drift.** Capability is reviewed,
  versioned, rollback-able code — the answer to the security reckoning the
  personal-agent free-for-all is walking into.
- **Delivered turnkey, operated as a fleet.** One command brings a whole
  governed appliance into existence; self-hosting without the self-management.

## Against each

> **vs. OpenClaw** — "OpenClaw is a personal agent you babysit, extended through
> a skill marketplace that became its biggest security liability. Ikigenba is the
> multi-user backend an agent operates — nothing to poison, nothing to
> self-modify, one isolated governed appliance per customer."

> **vs. Hermes** — "Hermes seals its learning loop inside a single-user persona.
> Ikigenba keeps the durable half in the open — knowledge you own and can cite,
> tools you can govern — and adds the identity, audit, and per-customer isolation
> a business actually needs."

## The internal principle (for every future feature)

> Run the blade across it: *does a smarter model make this unnecessary?* If yes,
> it's a crutch — build it thin enough to throw away, because the labs are coming
> for it. If no, it's the world — and that's the only thing worth owning. **Be
> the world, not the mind.**

---

## Background — the two comparison points

**OpenClaw** (Peter Steinberger; started Nov 2025 as Clawd → Moltbot → OpenClaw;
~300k+ GitHub stars, now under an OpenClaw Foundation with OpenAI sponsorship). A
self-hosted *personal AI assistant* that runs on your own devices and answers you
on the channels you already use (WhatsApp, Telegram, voice, a live Canvas). Skills
come from a marketplace, **ClawHub** — which became its biggest liability
(Bitdefender found ~900 malicious packages, ~20% of the registry; widespread
shadow-IT installs on corporate machines; a maintainer warned it's "too dangerous
if you can't use a command line"; some governments restricted it).

**Hermes Agent** (Nous Research, MIT, Feb 2026). A persistent, self-hosted
*personal agent* that lives on a $5 VPS and reaches you across 16–20+ messaging
platforms with one identity. Its differentiators are a closed learning loop:
persistent cross-session memory, autonomous skill creation, self-improving skills,
user modeling — and, because Nous trains models, it doubles as an RL-trajectory /
training-data generator.

**The thing to notice:** both *are* the agent. They're single-user personal
agents, channel/chat-centric, with the value baked into a self-modifying persona.
That's the axis Ikigenba refuses — it's the governed backend the agent connects to,
not the agent.

## The strategic thesis (why this position is defensible)

The category is splitting in two:

- **The agent *is* the product** — intelligence, memory, skills, identity fused
  into one self-modifying process you run and babysit. OpenClaw and Hermes are
  both here.
- **The agent is interchangeable; the durable value is the governed substrate it
  operates.** Ikigenba is here, nearly alone.

Value accrues to the second side because the agent layer is being commoditized
fast — every model release and harness update absorbs last year's differentiators
(memory, skills, planning, user modeling). A product that *is* the agent stands on
a melting iceberg; a product that is the substrate the agent operates rides the
rising tide. **Better agents are a tailwind for Ikigenba, not a threat.**

The metaphor carries the bet: a company doesn't get smarter by being clever about
its employees' brains — it provides the office, systems, badge, and records, and
the smarter the employee the more that's worth. The company outlives any single
employee; agents are swappable, the data/identity/processes/box persist. And it
refuses the "smartest agent" frame entirely — Ikigenba is not on that axis.

### Strengths / moats

1. **Agent-neutrality (Switzerland).** Any agent plugs in; model progress is pure
   tailwind.
2. **Multi-user + governance as a foundational invariant** — not retrofittable
   into a single-memory persona.
3. **Ownership of the system of record** — agents are transient, the data is
   sticky.
4. **The event plane as an automation fabric** — replayable, auditable; not a
   chat loop.
5. **Uniformity → safe extensibility + fleet operability** — extensible *and*
   governed, the synthesis the marketplace model can't reach.
6. **A governed learning loop** — knowledge you can cite, skills you can version;
   the only acceptable form for a business.

### Honest weaknesses (to stay credible)

- **Time-to-wow** — they're "talk to it from your phone in 10 minutes"; the
  payoff here is "connect your agent to a governed backend," and the tooling is
  still immature.
- **No intelligence of its own** — betting on borrowed brains (correct, but means
  no standalone AI "wow"; sell it as neutrality/durability).
- **No community flywheel** — deliberately forgoing viral consumer growth for
  governance; a slower, quieter curve.
- **Single-box / scheduled-downtime bet** caps the top end — fine for SMB, don't
  position into territory it can't serve.

### The wedge

"The governed backend that lets an AI agent run a small business — safely." Land
with the systems of record an SMB already runs on (CRM, ledger, wiki), made
agent-native and governed, delivered as a per-client appliance you operate. Time
the message to the agent-security reckoning: as orgs realize personal-agent sprawl
is ungovernable, Ikigenba is the governed alternative.

**The one risk to respect:** a foundation lab decides to build the governed
backend too. Defense — this is boring, heavy, vertical, enterprise-ops work
(fleet provisioning, per-client isolation, domain services, deploy/rollback
discipline) that horizontal-intelligence labs don't want to run. They sell the
brain; Ikigenba runs the workplace.
