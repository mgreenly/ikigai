# ikigai

[![CI](https://github.com/mgreenly/ikigai/actions/workflows/ci.yml/badge.svg)](https://github.com/mgreenly/ikigai/actions/workflows/ci.yml)


A experiment to build a Linux focused, multi-model, coding agent with RAG accessible permanent memory.

Why?  Because I want to and no one can stop me!

## Building

**Platform**: Debian 13 (Trixie) - may not work on other distributions.

**Clone and build**:
```bash
git clone https://github.com/mgreenly/ikigai.git
cd ikigai
make
```

**Install**:
```bash
make install PREFIX=$HOME/.local
```

**Uninstall**:
```bash
make uninstall PREFIX=$HOME/.local
```

**Distro-specific builds**: For brave users, see `distro/` directory for alternative build configurations.

## Documentation

See [docs/](docs/) for design and implementation details.
