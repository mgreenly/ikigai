#!/usr/bin/env bash
set -euo pipefail

pkill -f 'bin/ikigai' 2>/dev/null || true
pkill -f 'bin/mock-provider' 2>/dev/null || true

rm -f run/*.sock
