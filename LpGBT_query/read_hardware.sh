#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /path/to/hardware.xml" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XML="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
READER="${SCRIPT_DIR}/../bin/LpGBTefuseReader"
(cd "$(dirname "${XML}")" && "${READER}" "${XML}" "${XML%.*}_lpgbt_portcards.csv")
