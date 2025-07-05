#!/usr/bin/env bash
cd "$(dirname "$0")"
if ./pepepow-miner --status > stats.json 2>/dev/null; then
  cat stats.json
  exit 0
fi
# fallback
echo '{"ver":"1.0","hs":[0],"temp":[0],"fan":[0]}'
