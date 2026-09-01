#!/usr/bin/env python3

import csv
import os
import sys
import tempfile
from pathlib import Path

from rhapi import RhApi


DCA_URL = "https://cmsdca.cern.ch/trk_rhapi"
DCA_DATABASE = "trker_cmsr"
MODULE_OUTPUT = "serial_number"
MODULE_FIELDS = {
    "serial_number": ("parent_serial_number", "module"),
    "name_label": ("parent_name_label", "NAME"),
    "component": ("parent_component", "KIND_OF_PART"),
}


def query(efuses):
    field, _ = MODULE_FIELDS[MODULE_OUTPUT]
    values = ", ".join("'" + value.replace("'", "''") + "'" for value in sorted(set(efuses)))
    return f"""
SELECT DISTINCT chip.serial_number AS efuse_code, assembled.{field} AS module_value
FROM {DCA_DATABASE}.parts chip
JOIN {DCA_DATABASE}.trkr_relationships_v bare
  ON bare.child_name_label = chip.name_label AND bare.child_component = 'CROC Chip'
JOIN {DCA_DATABASE}.trkr_relationships_v assembled
  ON assembled.child_name_label = bare.parent_name_label AND assembled.child_component = bare.parent_component
WHERE chip.serial_number IN ({values}) AND assembled.{field} IS NOT NULL
ORDER BY chip.serial_number
""".strip()


def lookup(efuses):
    current = Path.cwd()
    try:
        with tempfile.TemporaryDirectory(prefix="dca-query-", dir="/tmp") as directory:
            os.chdir(directory)
            rows = RhApi(DCA_URL, sso="login", save_password=False).json2(query(efuses)).get("data", [])
    finally:
        os.chdir(current)

    modules = {}
    for row in rows:
        row = {str(key).lower(): value for key, value in row.items()}
        efuse, module = str(row.get("efuse_code", "")), row.get("module_value")
        if efuse and module not in (None, ""):
            if efuse in modules and modules[efuse] != str(module):
                raise RuntimeError(f"multiple modules found for eFuse {efuse}")
            modules[efuse] = str(module)
    return modules


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: lookup_hardware_efuse_modules.py input.csv output.csv")
    source, output = map(Path, sys.argv[1:])
    with source.open(newline="", encoding="utf-8") as stream:
        chips = list(csv.DictReader(stream))
    required = {"board", "optical", "hybrid", "chip", "efuse"}
    if not chips or not required.issubset(chips[0]):
        raise RuntimeError("input must contain board,optical,hybrid,chip,efuse")
    chips = [{key: value.strip() for key, value in chip.items()} for chip in chips]

    modules = lookup([chip["efuse"] for chip in chips])
    column = MODULE_FIELDS[MODULE_OUTPUT][1]
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=["board", "optical", "hybrid", "chip", "efuse", column], extrasaction="ignore")
        writer.writeheader()
        for chip in chips:
            chip[column] = modules.get(chip["efuse"], "")
            writer.writerow(chip)

    missing = sorted({chip["efuse"] for chip in chips if chip["efuse"] not in modules})
    print(f"Wrote {len(chips)} chip mapping(s) to {output}")
    if missing:
        print("Missing DCA module for eFuse: " + ", ".join(missing))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
