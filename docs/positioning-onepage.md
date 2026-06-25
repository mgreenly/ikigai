# Ikigenba — One-Page Positioning

> OpenClaw and Hermes build employees. Ikigenba builds the company.

Ikigenba is not an AI agent. It is the governed business substrate that capable
models need once they are good enough to do real work: persistent records,
business workflows, fixed context, identity, grants, audit, events, backup,
restore, and deployment.

OpenClaw and Hermes are agent runtimes. They wrap models with memory, skills,
channels, scheduling, learning loops, user modeling, and tool execution. That is
useful, but it is also the part of the stack most exposed to model progress.
OpenAI, Anthropic, and frontier clients will keep absorbing more planning,
memory, tool use, coding ability, and personal context. The better the model
gets, the less durable value there is in building another mind around it.

Ikigenba bets on the other side. A smarter model does not make a CRM obsolete. It
does not make a ledger, knowledge base, file mirror, event history, OAuth grant,
audit trail, rollback path, or backup less important. It makes those things more
valuable because the model can finally use them well.

## The Position

Ikigenba is the tool-and-record layer for frontier models running a small
business.

It is one dashboard plus small domain services, deployed as one isolated box per
customer and exposed primarily through MCP. The dashboard owns login, OAuth,
grants, service inventory, install guidance, and operations. Each service owns
one domain, one SQLite database, one loopback server, one MCP surface, and the
same lifecycle verbs: `serve`, `version`, `manifest`, `migrate`, `backup`, and
`restore`.

Services publish facts to an outbox and consume each other's `/feed` streams
over loopback SSE. The result is not a chat app, not a persona, and not a skill
marketplace. It is a governed place where model-driven work persists.

## The Contrast

**OpenClaw** gives a model hands across a user's devices, channels, files, and
tools. Its power is breadth and immediacy.

**Hermes** gives a model a persistent personal learning loop: memory, skill
creation, self-improvement, user modeling, scheduled work, and messaging
surfaces.

**Ikigenba** gives a capable model business infrastructure: systems of record,
fixed context, governed tools, service events, identity, audit, versioned
releases, backup, restore, and rollback.

The point is not that OpenClaw or Hermes are bad. The point is that they are
building in the layer most likely to be compressed by frontier-model progress.
Ikigenba builds in the layer that model progress makes more useful.

## The Wedge

> The governed tool-and-record layer that lets frontier models run a small
> business safely.

Start with the records small businesses already need: CRM, ledger, wiki, files,
email, notifications, scheduled jobs, prompt runs, and scripts. Make them
agent-native through MCP, governed by the dashboard, connected by events, and
operated as a recoverable appliance.

The buyer does not need another AI personality. They need somewhere for capable
models to do real work without every action becoming an ungoverned one-off.

## Internal Rule

Ask this for every feature:

> Does a smarter model make this unnecessary?

If yes, keep it thin. If no, make it durable.

Be the world, not the mind.
