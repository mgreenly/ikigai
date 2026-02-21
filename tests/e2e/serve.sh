#!/usr/bin/env bash
set -euo pipefail

export ANTHROPIC_BASE_URL=http://127.0.0.1:9100
export GOOGLE_BASE_URL=http://127.0.0.1:9100
export OPENAI_BASE_URL=http://127.0.0.1:9100

bin/mock-provider &
MOCK_PID=$!
echo "mock-provider started (PID $MOCK_PID)"

bin/ikigai --headless &
IKIGAI_PID=$!
echo "ikigai started (PID $IKIGAI_PID)"

trap 'kill -INT $MOCK_PID 2>/dev/null; kill -INT $IKIGAI_PID 2>/dev/null; wait $MOCK_PID $IKIGAI_PID 2>/dev/null; exit 0' INT

wait
