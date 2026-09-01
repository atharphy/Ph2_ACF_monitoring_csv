#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /path/to/hardware.xml" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XML="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
READER="${PH2ACF_BASE_DIR:?Source setup.sh first}/bin/LpGBTefuseReader"
OUTPUT="${XML%.*}_lpgbt_portcards.csv"
RAW="$(mktemp "${TMPDIR:-/tmp}/lpgbt-efuse.XXXXXX.csv")"
trap 'rm -f "${RAW}"' EXIT

"${READER}" "${XML}" "${RAW}"
python3 "${SCRIPT_DIR}/lookup_lpgbt_portcards.py" "${RAW}" "${OUTPUT}"
