#!/usr/bin/env python3

import csv
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "DCA_query"))
from rhapi import RhApi

DCA_URL = "https://cmsdca.cern.ch/trk_rhapi"
DCA_DATABASE = "trker_cmsr"


def normalize_efuse(value):
    value = str(value).strip()
    if value.lower().startswith("0x"):
        number = int(value, 16)
    elif any(character in "abcdefABCDEF" for character in value):
        number = int(value, 16)
    else:
        number = int(value, 10)
    return f"0x{number:08X}"


def make_query(efuses):
    candidates = set()
    for value in efuses:
        canonical = normalize_efuse(value)
        number = int(canonical, 16)
        candidates.update((canonical, canonical[2:], str(number)))
    values = ", ".join("'" + value.replace("'", "''") + "'" for value in sorted(candidates))
    return f"""
SELECT DISTINCT lpgbt.serial_number AS lpgbt_efuse,
       relation.parent_serial_number AS portcard_id
FROM {DCA_DATABASE}.parts lpgbt
JOIN {DCA_DATABASE}.trkr_relationships_v relation
  ON relation.child_name_label = lpgbt.name_label
WHERE lpgbt.serial_number IN ({values})
  AND LOWER(relation.child_component) LIKE '%lpgbt%'
  AND LOWER(relation.parent_component) LIKE '%portcard%'
  AND relation.parent_serial_number IS NOT NULL
ORDER BY lpgbt.serial_number
""".strip()


def lookup(efuses):
    current = Path.cwd()
    try:
        with tempfile.TemporaryDirectory(prefix="dca-lpgbt-", dir="/tmp") as directory:
            os.chdir(directory)
            rows = RhApi(DCA_URL, sso="login", save_password=False).json2(make_query(efuses)).get("data", [])
    finally:
        os.chdir(current)

    result = {}
    for row in rows:
        row = {str(key).lower(): value for key, value in row.items()}
        efuse, portcard = normalize_efuse(row.get("lpgbt_efuse", "")), row.get("portcard_id")
        if efuse and portcard not in (None, ""):
            if efuse in result and result[efuse] != str(portcard):
                raise RuntimeError(f"multiple portcards found for lpGBT eFuse {efuse}")
            result[efuse] = str(portcard)
    return result


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: lookup_lpgbt_portcards.py input.csv output.csv")
    source, output = map(Path, sys.argv[1:])
    with source.open(newline="", encoding="utf-8") as stream:
        entries = list(csv.DictReader(stream))
    required = {"board", "optical", "lpgbt_efuse", "portcard_id"}
    if not entries or not required.issubset(entries[0]):
        raise RuntimeError("input must contain board,optical,lpgbt_efuse,portcard_id")
    entries = [{key: value.strip() for key, value in entry.items()} for entry in entries]
    for entry in entries:
        entry["lpgbt_efuse"] = normalize_efuse(entry["lpgbt_efuse"])
    lookup_error = None
    try:
        portcards = lookup([entry["lpgbt_efuse"] for entry in entries])
    except Exception as error:
        portcards = {}
        lookup_error = error

    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=["board", "optical", "lpgbt_efuse", "portcard_id"])
        writer.writeheader()
        for entry in entries:
            entry["portcard_id"] = portcards.get(entry["lpgbt_efuse"], "")
            writer.writerow(entry)

    missing = sorted({entry["lpgbt_efuse"] for entry in entries if entry["lpgbt_efuse"] not in portcards})
    print(f"Wrote {len(entries)} lpGBT mapping(s) to {output}")
    if lookup_error is not None:
        print(f"Warning: DCA lookup failed; portcard_id was left empty: {lookup_error}", file=sys.stderr)
        return 0
    if missing:
        print("Missing DCA portcard for lpGBT eFuse: " + ", ".join(missing))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
