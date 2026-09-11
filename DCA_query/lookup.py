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


def run_query(chip_efuses, lpgbt_efuses):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from rhapi import RhApi

    chip_efuses = {value for value in chip_efuses if value not in ("", "-1")}
    lpgbt_efuses = {value for value in lpgbt_efuses if value not in ("", "-1")}
    candidates = set()
    for value in lpgbt_efuses:
        number = int(value, 16)
        candidates.update((value.upper(), value[2:].upper(), str(number)))

    module_query = f"""
SELECT DISTINCT
       'module' AS mapping_type,
       chip.serial_number AS efuse,
       module.name_label AS value
FROM {DB}.parts chip
JOIN {DB}.trkr_relationships_v chip_to_bare
  ON chip_to_bare.child_id = chip.id
JOIN {DB}.parts bare
  ON bare.id = chip_to_bare.parent_id
JOIN {DB}.trkr_relationships_v bare_to_module
  ON bare_to_module.child_id = bare.id
JOIN {DB}.parts module
  ON module.id = bare_to_module.parent_id
WHERE TRIM(chip.serial_number) IN ({sql_values(chip_efuses)})
  AND LOWER(module.kind_of_part) LIKE '%module%'
  AND module.name_label IS NOT NULL
"""

    portcard_query = f"""
SELECT DISTINCT 'portcard' AS mapping_type, chip.serial_number AS efuse, portcard.serial_number AS value
FROM {DB}.parts chip
JOIN {DB}.trkr_relationships_v relation ON relation.child_id = chip.id
JOIN {DB}.parts portcard ON portcard.id = relation.parent_id
WHERE UPPER(TRIM(chip.serial_number)) IN ({sql_values(candidates)})
  AND LOWER(chip.kind_of_part) LIKE '%lpgbt%'
  AND LOWER(portcard.kind_of_part) LIKE '%portcard%'
  AND portcard.serial_number IS NOT NULL
"""
    current = Path.cwd()
    try:
        with tempfile.TemporaryDirectory(prefix="dca-combined-", dir="/tmp") as directory:
            os.chdir(directory)
            api = RhApi(URL, sso="login", save_password=False)
            rows = []
            for name, values, query in (("module", chip_efuses, module_query), ("portcard", candidates, portcard_query)):
                if not values:
                    continue
                try:
                    rows.extend(api.json2(query).get("data", []))
                except Exception as error:
                    print(f"Warning: DCA {name} lookup failed: {error}", file=sys.stderr)
            return rows
    finally:
        os.chdir(current)


def split_rows(rows):
    modules, portcards = {}, {}
    for row in rows:
        row = {str(key).replace("_", "").lower(): value for key, value in row.items()}
        if row.get("efuse") in (None, "") or row.get("value") in (None, ""):
            continue
        efuse, value = str(row["efuse"]), str(row["value"])
        if str(row.get("mappingtype", "")).lower() == "module":
            modules[efuse] = value
        else:
            base = 0 if efuse.lower().startswith("0x") else 16 if any(c in "abcdefABCDEF" for c in efuse) else 10
            portcards[f"0x{int(efuse, base):08X}"] = value
    return modules, portcards


def fill_missing(entries, values, efuse_field, name, fallback):
    values["-1"] = "-1"
    for efuse in sorted({entry[efuse_field] for entry in entries if entry[efuse_field] not in ("", "-1") and entry[efuse_field] not in values}):
        value = fallback
        if not value and sys.stdin.isatty():
            value = input(f"No {name} found for eFuse {efuse}. Enter {name} (-1 to leave empty): ").strip()
        values[efuse] = value or "-1"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--dummy-module-id", default="")
    parser.add_argument("--dummy-portcard-id", default="")
    parser.add_argument("--no-dca", action="store_true")
    args = parser.parse_args()

    with args.input.open(newline="", encoding="utf-8") as stream:
        entries = list(csv.DictReader(stream))
    if not entries:
        raise RuntimeError("hardware reader returned no entries")

    modules, portcards = {}, {}
    if not args.no_dca:
        try:
            rows = run_query([entry["chip_efuse"] for entry in entries], [entry["lpgbt_efuse"] for entry in entries])
            modules, portcards = split_rows(rows)
        except Exception as error:
            print(f"Warning: DCA lookup failed: {error}", file=sys.stderr)

    fill_missing(entries, modules, "chip_efuse", "module ID", args.dummy_module_id)
    fill_missing(entries, portcards, "lpgbt_efuse", "portcard ID", args.dummy_portcard_id)

    fields = ["board", "optical", "portcard_id", "lpgbt_efuse", "hybrid", "chip", "module_id", "chip_efuse"]
    with args.output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for entry in entries:
            entry["module_id"] = modules.get(entry["chip_efuse"], "-1")
            entry["portcard_id"] = portcards.get(entry["lpgbt_efuse"], "-1")
            writer.writerow(entry)
    print(f"Wrote {len(entries)} mapping(s) to {args.output}")


if __name__ == "__main__":
    main()
