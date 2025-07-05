#!/usr/bin/env bash
cd "$(dirname "$0")"
 [[ -f h-config.sh ]] && source ./h-config.sh
exec ./pepepow-miner "$@"
