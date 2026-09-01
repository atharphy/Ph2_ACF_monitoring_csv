#!/usr/bin/env python3

import argparse
import csv
import os
import re
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

from rhapi import RhApi


DCA_URL = "https://cmsdca.cern.ch/trk_rhapi"
DCA_DATABASE = "trker_cmsr"
OUTPUT_SUFFIX = ".csv"
MODULE_OUTPUT = "serial_number"

MODULE_OUTPUT_OPTIONS = {
    "serial_number": ("parent_serial_number", "module"),
    "name_label": ("parent_name_label", "NAME"),
    "component": ("parent_component", "KIND_OF_PART"),
}


def tag_name(element):
    return element.tag.rsplit("}", 1)[-1]


def read_xml_chips(xml_path):
    chips = []

    def visit(element, location):
        current = dict(location)
        name = tag_name(element)

        if name == "BeBoard":
            current["board"] = element.get("Id", "")
        elif name == "OpticalGroup":
            current["optical"] = element.get("Id", "")
        elif name == "Hybrid":
            current["hybrid"] = element.get("Id", "")

        efuse = element.get("eFuseCode")
        if efuse is not None and efuse.strip():
            chips.append(
                {
                    "board": current.get("board", ""),
                    "optical": current.get("optical", ""),
                    "hybrid": current.get("hybrid", ""),
                    "chip": element.get("Id", ""),
                    "efuse": efuse.strip(),
                }
            )

        for child in element:
            visit(child, current)

    visit(ET.parse(xml_path).getroot(), {})
    return chips


def sql_literal(value):
    return "'" + value.replace("'", "''") + "'"


def module_output_config():
    try:
        return MODULE_OUTPUT_OPTIONS[MODULE_OUTPUT]
    except KeyError as error:
        choices = ", ".join(sorted(MODULE_OUTPUT_OPTIONS))
        raise RuntimeError(
            f"unsupported MODULE_OUTPUT {MODULE_OUTPUT!r}; choose one of: {choices}"
        ) from error


def build_query(efuses, database):
    values = ", ".join(sql_literal(value) for value in sorted(set(efuses)))
    database_field, _ = module_output_config()
    return f"""
SELECT DISTINCT
       chip.serial_number AS efuse_code,
       assembled.{database_field} AS module_value
  FROM {database}.parts chip
  JOIN {database}.trkr_relationships_v bare
    ON bare.child_name_label = chip.name_label
   AND bare.child_component = 'CROC Chip'
  JOIN {database}.trkr_relationships_v assembled
    ON assembled.child_name_label = bare.parent_name_label
   AND assembled.child_component = bare.parent_component
 WHERE chip.serial_number IN ({values})
   AND assembled.{database_field} IS NOT NULL
 ORDER BY chip.serial_number
""".strip()


def normalized_key(key):
    name = str(key)
    if not name.isupper():
        name = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name)
    return re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").lower()


def query_modules(url, database, auth, efuses, verbose):
    query = build_query(efuses, database)
    original_directory = Path.cwd()

    try:
        with tempfile.TemporaryDirectory(prefix="py4dbupload-", dir="/tmp") as workdir:
            os.chdir(workdir)
            api = RhApi(url, debug=verbose, sso=auth, save_password=False)
            rows = api.json2(query).get("data", [])
    finally:
        os.chdir(original_directory)

    modules = {}
    for row in rows:
        values = {normalized_key(key): value for key, value in row.items()}
        efuse = str(values.get("efuse_code", ""))
        module = values.get("module_value")
        if efuse and module not in (None, ""):
            modules.setdefault(efuse, set()).add(str(module))

    ambiguous = {efuse: values for efuse, values in modules.items() if len(values) > 1}
    if ambiguous:
        details = ", ".join(
            f"{efuse} -> {sorted(values)}" for efuse, values in sorted(ambiguous.items())
        )
        raise RuntimeError(f"multiple assembled modules found: {details}")

    return {efuse: next(iter(values)) for efuse, values in modules.items()}


def write_csv(output_path, chips, modules):
    _, output_column = module_output_config()
    columns = ["board", "optical", "hybrid", "chip", "efuse", output_column]
    with output_path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=columns)
        writer.writeheader()
        for chip in chips:
            writer.writerow(
                {**chip, output_column: modules.get(chip["efuse"], "")}
            )


def main():
    parser = argparse.ArgumentParser(
        description="Map Ph2_ACF XML eFuse codes to assembled-module information."
    )
    parser.add_argument("xml", type=Path, help="Ph2_ACF hardware XML file")
    parser.add_argument("--output", type=Path, help="output CSV; defaults beside the XML")
    parser.add_argument("--url", default=DCA_URL)
    parser.add_argument("--database", default=DCA_DATABASE)
    parser.add_argument("--auth", choices=("login", "krb"), default="login")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    xml_path = args.xml.expanduser().resolve()
    if not xml_path.is_file():
        parser.error(f"XML file does not exist: {xml_path}")

    output_path = (
        args.output.expanduser().resolve()
        if args.output
        else xml_path.with_name(xml_path.stem + OUTPUT_SUFFIX)
    )

    chips = read_xml_chips(xml_path)
    if not chips:
        parser.error(f"no eFuseCode attributes found in {xml_path}")

    modules = query_modules(
        args.url,
        args.database,
        args.auth,
        [chip["efuse"] for chip in chips],
        args.verbose,
    )
    write_csv(output_path, chips, modules)

    missing = sorted({chip["efuse"] for chip in chips if chip["efuse"] not in modules})
    print(f"Wrote {len(chips)} chip mapping(s) to {output_path}")
    if missing:
        print(
            "WARNING: no assembled module found for eFuse code(s): " + ", ".join(missing),
            file=sys.stderr,
        )
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
