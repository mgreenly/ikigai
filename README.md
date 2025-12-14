# ikigai

[![CI](https://github.com/mgreenly/ikigai/actions/workflows/ci.yml/badge.svg)](https://github.com/mgreenly/ikigai/actions/workflows/ci.yml)


An experiment to build a Linux focused, terminal based coding agent with hierarchical sub-agents, RAG accessible permanent memory, progressive tool discovery and a dynamic sliding context window.

Why?  Because I want to and no one can stop me!

See [project/](project/README.md) for design and implementation details.

## Install

**Platform**: The build system runs natively on Debian 13 (Trixie), other distributions may have library dependency issues.  If you're brave you can check out the distros/ folder for distro specific builds but it's to early in the project's life-cycle for me to officially support those.

**Clone**:
```text
git clone https://github.com/mgreenly/ikigai.git
cd ikigai
git checkout rel-02
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

Right or wrong Google gives this definition of **Ikigai**

> The intersection of what you love, what you are good at, what the world needs, and what you can be paid for.

This project really is all of that for me. I'm doing it for myself but it brings together so many things I'm interested in; Linux, C, Agentic Coding, etc...  The Linux eco system wouldn't be hurt by having a good native multi-model coding agent that was natively packaged in distros. I may not get paid directly by anyone for a 'C' based 'Linux' coding agent but the deeper understanding I'll gain of Agents, RAG, MCP is valuable to me from a career perspective.  For me it covers all the bases, it is ikigai.

