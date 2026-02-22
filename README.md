[![CI](https://github.com/mgreenly/ikigai/actions/workflows/ci.yml/badge.svg)](https://github.com/mgreenly/ikigai/actions/workflows/ci.yml)

⚠️ This is an early stage experiment. Stay away, you absolutely should not try to use this, it's not ready!

# ikigai

An experiment in agentic orchestration. A platform for living agents that coordinate through hierarchical sub-agents, persist state to permanent memory, and manage long-term context using a dynamic sliding window paired with historical summaries and embedding-searchable topical history.

Why? Because I want to and no one can stop me!

See [docs/README.md](docs/README.md) for user documentation.

See [project/README.md](project/README.md) for design and implementation details.

## Install

**Platform**: The build system runs natively on Debian 13 (Trixie), other distributions may have library dependency issues.  If you're brave you can check out the distros/ folder for distro specific builds but it's too early in the project's life-cycle for me to officially support those.

**Clone**:
```text
git clone https://github.com/mgreenly/ikigai.git
cd ikigai
git checkout rel-12
```

**Install**:
```text
make install BUILD=release PREFIX=$HOME/.local
```

**Uninstall**:
```text
make uninstall PREFIX=$HOME/.local
```
## The Story

Right or wrong, Google gives this definition of **Ikigai**:

> The intersection of what you love, what you are good at, what the world needs, and what you can be paid for.

This project really is all of that for me. I'm doing it for myself but it brings together so many things I'm interested in: Linux, C, agent systems, process models, distributed coordination. The Linux ecosystem wouldn't be hurt by having a native agent platform that treats agents like first-class processes. I may not get paid directly by anyone for a C-based agent platform, but the deeper understanding I'll gain of multi-agent systems, memory architectures, and process coordination is valuable to me from a career perspective. For me it covers all the bases, it is ikigai.

