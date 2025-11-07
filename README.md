# ikigai

[![CI](https://github.com/mgreenly/ikigai/actions/workflows/ci.yml/badge.svg)](https://github.com/mgreenly/ikigai/actions/workflows/ci.yml)


A experiment to build a Linux focused, multi-model, coding agent with RAG accessible permanent memory.

Why?  Because I want to and no one can stop me!

See [docs/](docs/) for design and implementation details.

## Install

**Platform**: The build system runs natively on Debian 13 (Trixie), other distributions may have dependency issues.  If you're brave you can check out the distros/ folder for distro specific builds but it's to early in the project's life-cycle for me to officially support those yet.

**Clone**:
```bash
git clone https://github.com/mgreenly/ikigai.git && cd ikigai
```

**Install**:
```bash
make install PREFIX=$HOME/.local
```

**Uninstall**:
```bash
make uninstall PREFIX=$HOME/.local
```
