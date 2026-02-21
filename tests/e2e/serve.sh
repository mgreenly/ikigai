#!/usr/bin/env bash
set -euo pipefail

ANTHROPIC_BASE_URL=http://127.0.0.1:9100 \
GOOGLE_BASE_URL=http://127.0.0.1:9100 \
OPENAI_BASE_URL=http://127.0.0.1:9100 \
bin/ikigai
