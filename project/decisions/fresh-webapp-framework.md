# Why Fresh for Webapp Framework?

**Decision**: Use Fresh (Deno's native web framework) for all Ikigai webapps.

**Rationale**:
- **Deno alignment**: Same runtime as agents - one toolchain, one mental model
- **No build step**: Runs TypeScript directly, matching agent deployment model
- **Lightweight**: Islands architecture ships minimal JavaScript by default
- **TypeScript-first**: Designed for TypeScript from the ground up
- **File-based routing**: Convention over configuration, easy to understand
- **Preact foundation**: Battle-tested rendering with React-compatible ecosystem

**Alternatives Considered**:
- **SolidJS**: Excellent TypeScript support, fastest benchmarks, but requires build step and doesn't align with Deno runtime
- **Svelte 5**: Great DX and compiles away framework, but requires build step and separate toolchain
- **Preact standalone**: Lighter than React but would require assembling routing, SSR, etc. manually
- **Next.js/React**: Most popular but heavy, Node-centric, TypeScript bolted on rather than native

**Trade-offs**:
- **Pro**: Full Deno alignment (agents and webapps share runtime)
- **Pro**: No build step simplifies deployment (git clone and run)
- **Pro**: Islands architecture = fast by default
- **Pro**: TypeScript-first design
- **Con**: Smaller ecosystem than React/Vue
- **Con**: Preact has minor React API differences
- **Con**: Less community resources and tutorials

**Webapp Structure**:
```
webapp/
├── manifest.json       # Ikigai metadata
├── deno.json          # Fresh config
├── deno.lock
├── fresh.gen.ts       # Auto-generated
├── routes/            # File-based routing
│   ├── index.tsx
│   ├── _app.tsx       # Layout
│   └── api/           # API routes
├── islands/           # Interactive components
└── static/            # Assets
```

**Migration Path**: If Fresh proves insufficient, SolidJS or Svelte are viable alternatives. The `@ikigai/platform` package abstracts backend communication, so framework changes only affect UI code.
