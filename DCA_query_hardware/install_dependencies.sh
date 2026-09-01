#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="${PYTHON_BIN:-python3}"

if ! command -v "${PYTHON_BIN}" >/dev/null 2>&1; then
    echo "ERROR: ${PYTHON_BIN} is not available." >&2
    exit 1
fi

"${PYTHON_BIN}" -m pip install --user requests beautifulsoup4 ilock

echo "Dependencies installed for the current user."
echo "Run: ${SCRIPT_DIR}/run_lookup.sh /path/to/hardware.xml"
