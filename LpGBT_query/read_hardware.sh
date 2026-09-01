#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /path/to/hardware.xml" >&2
    exit 1
fi

XML="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
READER="${PH2ACF_BASE_DIR:?Source setup.sh first}/bin/LpGBTefuseReader"
"${READER}" "${XML}" "${XML%.*}_lpgbt_portcards.csv"
