#!/usr/bin/env bash

set -euo pipefail

PYTHON_BIN="${PYTHON_BIN:-python3}"
command -v "${PYTHON_BIN}" >/dev/null 2>&1 || { echo "ERROR: ${PYTHON_BIN} is not available." >&2; exit 1; }
"${PYTHON_BIN}" -m pip install --user requests beautifulsoup4 ilock
