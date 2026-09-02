#!/usr/bin/env python3

import argparse
import csv
import os
import sys
import tempfile
from pathlib import Path

URL = "https://cmsdca.cern.ch/trk_rhapi"
DB = "trker_cmsr"


def sql_values(values):
    return ", ".join("'" + value.replace("'", "''") + "'" for value in sorted(set(values)))


def run_query(query):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from rhapi import RhApi

    current = Path.cwd()
    try:
        with tempfile.TemporaryDirectory(prefix="dca-combined-", dir="/tmp") as directory:
            os.chdir(directory)
            return RhApi(URL, sso="login", save_password=False).json2(query).get("data", [])
    finally:
        os.chdir(current)


def module_lookup(efuses):
    query = f"""
SELECT DISTINCT chip.serial_number AS efuse, assembled.parent_serial_number AS value
FROM {DB}.parts chip
JOIN {DB}.trkr_relationships_v bare ON bare.child_name_label = chip.name_label AND bare.child_component = 'CROC Chip'
JOIN {DB}.trkr_relationships_v assembled ON assembled.child_name_label = bare.parent_name_label AND assembled.child_component = bare.parent_component
WHERE chip.serial_number IN ({sql_values(efuses)}) AND assembled.parent_serial_number IS NOT NULL
"""
    return rows_to_map(run_query(query))


def portcard_lookup(efuses):
    candidates = set()
    for value in efuses:
        number = int(value, 16)
        candidates.update((value, value[2:], str(number)))
    query = f"""
SELECT DISTINCT chip.serial_number AS efuse, relation.parent_serial_number AS value
FROM {DB}.parts chip
JOIN {DB}.trkr_relationships_v relation ON relation.child_name_label = chip.name_label
WHERE chip.serial_number IN ({sql_values(candidates)})
  AND LOWER(relation.child_component) LIKE '%lpgbt%'
  AND LOWER(relation.parent_component) LIKE '%portcard%'
  AND relation.parent_serial_number IS NOT NULL
"""
    result = {}
    for key, value in rows_to_map(run_query(query)).items():
        result[f"0x{int(key, 0 if key.lower().startswith('0x') else 16 if any(c in 'abcdefABCDEF' for c in key) else 10):08X}"] = value
    return result


def rows_to_map(rows):
    result = {}
    for row in rows:
        row = {str(key).lower(): value for key, value in row.items()}
        if row.get("efuse") not in (None, "") and row.get("value") not in (None, ""):
            result[str(row["efuse"])] = str(row["value"])
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--dummy-module-id", default="")
    parser.add_argument("--dummy-portcard-id", default="")
    args = parser.parse_args()

    with args.input.open(newline="", encoding="utf-8") as stream:
        entries = list(csv.DictReader(stream))
    if not entries:
        raise RuntimeError("hardware reader returned no entries")

    modules, portcards = {}, {}
    for name, lookup, values in (
        ("module", module_lookup, [entry["chip_efuse"] for entry in entries]),
        ("portcard", portcard_lookup, [entry["lpgbt_efuse"] for entry in entries]),
    ):
        try:
            result = lookup(values)
            if name == "module":
                modules = result
            else:
                portcards = result
        except Exception as error:
            print(f"Warning: DCA {name} lookup failed: {error}", file=sys.stderr)

    fields = ["board", "optical", "portcard_id", "lpgbt_efuse", "hybrid", "chip", "module_id", "chip_efuse"]
    with args.output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for entry in entries:
            entry["module_id"] = modules.get(entry["chip_efuse"], args.dummy_module_id)
            entry["portcard_id"] = portcards.get(entry["lpgbt_efuse"], args.dummy_portcard_id)
            writer.writerow(entry)
    print(f"Wrote {len(entries)} mapping(s) to {args.output}")


if __name__ == "__main__":
    main()
