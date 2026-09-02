#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 hardware.xml [--dummy-module-id ID] [--dummy-portcard-id ID]" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XML="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
RAW="$(mktemp "${TMPDIR:-/tmp}/combined-efuse.XXXXXX.csv")"
trap 'rm -f "${RAW}"' EXIT

(cd "$(dirname "${XML}")" && "${SCRIPT_DIR}/../bin/CombinedEfuseReader" "${XML}" "${RAW}")
python3 "${SCRIPT_DIR}/lookup.py" "${RAW}" "${XML%.*}_hardware_mapping.csv" "${@:2}"
