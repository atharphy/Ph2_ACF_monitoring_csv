#!/usr/bin/env python3

import csv
import sys
from pathlib import Path


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: set_dummy_portcard.py mapping.csv portcard_id")

    path, portcard = Path(sys.argv[1]), sys.argv[2].strip()
    if not portcard:
        raise SystemExit("portcard_id must not be empty")

    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        rows, fields = list(reader), reader.fieldnames
    if not fields or "portcard_id" not in fields:
        raise RuntimeError("CSV does not contain portcard_id")

    updated = 0
    for row in rows:
        if not row["portcard_id"].strip():
            row["portcard_id"] = portcard
            updated += 1

    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
    temporary.replace(path)
    print(f"Set portcard_id={portcard} for {updated} row(s) in {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
