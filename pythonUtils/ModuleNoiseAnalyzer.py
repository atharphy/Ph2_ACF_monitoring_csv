#!/usr/bin/env python3
"""
Analyze the newest 2S/PS ROOT file under a directory, or a specific one you pass.
Produces two text files next to the input .root:
  - <base>_<type>_Full_Channel_Summary.txt
  - <base>_<type>_Problem_Channel_Summary.txt
Also echoes the "Problem Channel Summary" to the terminal

Usage:
    python3 ModuleNoiseAnalyzer.py
    python3 ModuleNoiseAnalyzer.py -s /path/to/search/dir
    python3 ModuleNoiseAnalyzer.py -f /path/to/file.root
"""

import argparse
import math
import os
import sys
from typing import Iterable, List, Tuple
from ROOT import TFile, TH1  

SIGMA_THRESHOLD = 5
TOTAL_SENSOR_STRIPS_2S = 1016
HEADER_WIDTH = 60
N_PS_STRIPS_PER_SIDE = 120 * 8  

def open_summary_files(root_path: str, module_type: str):
    """Open the full/problem summary files for writing and return handles+paths."""
    base = os.path.splitext(os.path.basename(root_path))[0]
    out_dir = os.path.dirname(os.path.abspath(root_path))
    full_path = os.path.join(out_dir, f"{base}_{module_type}_Full_Channel_Summary.txt")
    prob_path = os.path.join(out_dir, f"{base}_{module_type}_Problem_Channel_Summary.txt")
    return open(full_path, "w"), open(prob_path, "w"), full_path, prob_path


def write_lines(file_handles: Iterable, lines: Iterable[str]):
    """Write lines (with trailing newline) to each handle in file_handles."""
    for fh in file_handles:
        for line in lines:
            fh.write(line + "\n")


def find_histogram(directory, name_substr: str):
    """Return the first TH1 in a directory whose key name contains name_substr, else None."""
    for key in directory.GetListOfKeys():
        name = key.GetName()
        if name_substr in name:
            obj = directory.Get(name)
            if isinstance(obj, TH1):
                return obj
    return None


def compute_hist_stats(hist: TH1):
    n = hist.GetNbinsX()
    data = [(hist.GetBinContent(i), hist.GetBinError(i)) for i in range(1, n + 1)]
    mean = sum(v for v, _ in data) / n if n else 0.0
    var = sum((v - mean) ** 2 for v, _ in data) / n if n else 0.0
    return mean, math.sqrt(var), data


def echo_problem_summary(path: str, prefix: str = "=== Problem Channel Summary ==="):
    print(f"\n{prefix}")
    with open(path, "r") as fh:
            content = fh.read()
            print(content if content.strip() else "  (file is empty)", end="")

def map_2s_strip_id(bin_index: int, is_top: bool, is_right: bool):
    if is_top and is_right:
        gid = TOTAL_SENSOR_STRIPS_2S - bin_index + 1
    
    elif is_top and not is_right:
        gid = TOTAL_SENSOR_STRIPS_2S - bin_index + 1 + TOTAL_SENSOR_STRIPS_2S
    
    elif not is_top and is_right:
        gid = bin_index + TOTAL_SENSOR_STRIPS_2S
    
    else:
        gid = bin_index

    return ("Top" if is_top else "Bottom", "Right" if is_right else "Left", gid)


def map_ps_strip_id_zero_based(chan_zero_based, is_right_side):
    base = N_PS_STRIPS_PER_SIDE - chan_zero_based
    return base if is_right_side else base + N_PS_STRIPS_PER_SIDE


def analyze_2S_file(root_path: str):
    full_fh, prob_fh, full_path, prob_path = open_summary_files(root_path, "2S")
    write_lines([full_fh, prob_fh], [f"Input File: {os.path.abspath(root_path)}", "Detected file type: 2S module"])

    root_file = TFile.Open(root_path)
    if not root_file or root_file.IsZombie():
        raise SystemExit(f"ERROR: cannot open '{root_path}'")
    
    detector_dir = root_file.GetDirectory("Detector")
    if not detector_dir:
        raise SystemExit("ERROR: 'Detector' directory not found")

    for board_key in detector_dir.GetListOfKeys():
        board_name = board_key.GetName()
        if not board_name.startswith("Board_"):
            continue
        write_lines([full_fh, prob_fh], [f"{(' Board: ' + board_name + ' '):-^{HEADER_WIDTH}}"])
        board_dir = detector_dir.GetDirectory(board_name)

        for og_key in board_dir.GetListOfKeys():
            og_name = og_key.GetName()
            if not og_name.startswith("OpticalGroup_"):
                continue
            write_lines([full_fh, prob_fh], [f"{(' Optical Group: ' + og_name + ' '):-^{HEADER_WIDTH}}", ""])
            og_dir = board_dir.GetDirectory(og_name)

            for hybrid_key in og_dir.GetListOfKeys():
                hybrid_name = hybrid_key.GetName()
                if not hybrid_name.startswith("Hybrid_"):
                    continue

                hybrid_index = int(hybrid_name.split("_")[1])
                is_right = (hybrid_index % 2 == 0)
                hybrid_dir = og_dir.GetDirectory(hybrid_name)

                top_hist = find_histogram(hybrid_dir, "StripChannelNoiseTop")
                bot_hist = find_histogram(hybrid_dir, "StripChannelNoiseBottom")
                if not top_hist or not bot_hist:
                    continue

                mean_top, sig_top, top_data = compute_hist_stats(top_hist)
                mean_bot, sig_bot, bot_data = compute_hist_stats(bot_hist)

                # Full summary (2S)
                write_lines([full_fh], ["", f"{(' Hybrid: ' + hybrid_name + ' '):-^{HEADER_WIDTH}}", ""])
                for side_flag, (mean_val, sigma_val, data) in (
                    (True, (mean_top, sig_top, top_data)),
                    (False, (mean_bot, sig_bot, bot_data)),
                ):
                    side_label = "Top" if side_flag else "Bottom"
                    write_lines(
                        [full_fh],
                        [f"{side_label} Sensor: avg={mean_val:.3f}, σ={sigma_val:.3f}",
                         "  chanID   noise    err   stripID (global)"]
                    )
                    for chan_idx, (val, err) in enumerate(data, 1):  # chan_idx is 1-based
                        _, _, strip_id = map_2s_strip_id(chan_idx, side_flag, is_right)
                        write_lines([full_fh], [f"  {chan_idx:6d}  {val:7.3f}  {err:7.3f}   {strip_id:6d}"])

                # Problem summary (2S)
                write_lines([prob_fh], ["", f"{(' Hybrid: ' + hybrid_name + ' '):-^{HEADER_WIDTH}}", ""])
                
                disconnected, noisy = [], []
                
                def check_side(is_top, data, mean, sigma):
                    for chan_idx, (val, _) in enumerate(data, 1):
                        sensor_pos, _, strip_id = map_2s_strip_id(chan_idx, is_top, is_right)
                        line = f"{sensor_pos} sensor - stripID={strip_id} Channel={chan_idx}"
                        if val < mean - SIGMA_THRESHOLD * sigma:
                            disconnected.append(line)

                        elif val > mean + SIGMA_THRESHOLD * sigma:
                            noisy.append(line)
                
                check_side(True,  top_data, mean_top, sig_top)
                check_side(False, bot_data, mean_bot, sig_bot)
                
                write_lines(
                    [prob_fh],
                    [f"Disconnected channels (< {SIGMA_THRESHOLD}σ):"]
                    + (["  " + x for x in disconnected] or ["  None"])
                )
                write_lines(
                    [prob_fh],
                    ["", f"Noisy channels (> {SIGMA_THRESHOLD}σ):"]
                    + (["  " + x for x in noisy] or ["  None"])
                )
                write_lines([prob_fh], ["=" * HEADER_WIDTH])
                

    root_file.Close()
    full_fh.close()
    prob_fh.close()

    print(f"\nFull Channel Summary:    {full_path}")
    print(f"Problem Channel Summary: {prob_path}")
    echo_problem_summary(prob_path)


def analyze_PS_file(root_path: str):
    full_fh, prob_fh, full_path, prob_path = open_summary_files(root_path, "PS")
    write_lines([full_fh, prob_fh], [f"Input File: {os.path.abspath(root_path)}", "Detected file type: PS module"])

    root_file = TFile.Open(root_path)
    if not root_file or root_file.IsZombie():
        raise SystemExit(f"ERROR: cannot open '{root_path}'")

    detector_dir = root_file.GetDirectory("Detector")
    if not detector_dir:
        raise SystemExit("ERROR: 'Detector' directory not found")

    for board_key in detector_dir.GetListOfKeys():
        board_name = board_key.GetName()
        if not board_name.startswith("Board_"):
            continue
        write_lines([full_fh, prob_fh], [f"{(' Board: ' + board_name + ' '):-^{HEADER_WIDTH}}"])
        board_dir = detector_dir.GetDirectory(board_name)

        for og_key in board_dir.GetListOfKeys():
            og_name = og_key.GetName()
            if not og_name.startswith("OpticalGroup_"):
                continue
            write_lines([full_fh, prob_fh], [f"{(' Optical Group: ' + og_name + ' '):-^{HEADER_WIDTH}}", ""])
            og_dir = board_dir.GetDirectory(og_name)

            for hybrid_key in og_dir.GetListOfKeys():
                hybrid_name = hybrid_key.GetName()
                if not hybrid_name.startswith("Hybrid_"):
                    continue

                hybrid_index = int(hybrid_name.split("_")[1])
                is_right = (hybrid_index % 2 == 0)
                hybrid_dir = og_dir.GetDirectory(hybrid_name)

                strip_hist = find_histogram(hybrid_dir, "StripChannelNoise")
                pixel_hist = find_histogram(hybrid_dir, "PixelChannelNoise")
                if not strip_hist or not pixel_hist:
                    continue

                strip_mean, strip_sigma, strip_data = compute_hist_stats(strip_hist)
                pixel_mean, pixel_sigma, pixel_data = compute_hist_stats(pixel_hist)

                # Full summary header (PS)
                write_lines([full_fh], ["", f"{(' Hybrid: ' + hybrid_name + ' '):-^{HEADER_WIDTH}}", ""])
                write_lines([full_fh], [f"PS Strips: avg={strip_mean:.3f}, σ={strip_sigma:.3f}", "  chanID   noise    err   stripID"])
                
                for idx, (val, err) in enumerate(strip_data, 1):
                    chan0 = int(strip_hist.GetXaxis().GetBinCenter(idx))
                    strip_id = map_ps_strip_id_zero_based(chan0, is_right)
                    write_lines([full_fh], [f"  {chan0:6d}  {val:7.3f}  {err:7.3f}   {strip_id:6d}"])

                write_lines([full_fh], [""])

                # PS Pixels (full)
                write_lines([full_fh], [f"PS Pixels: avg={pixel_mean:.3f}, σ={pixel_sigma:.3f}", "  chan   noise    err"])

                for idx, (val, err) in enumerate(pixel_data, 1):
                    chan = int(pixel_hist.GetXaxis().GetBinCenter(idx))
                    write_lines([full_fh], [f"  {chan:5d}  {val:7.3f}  {err:7.3f}"])

                write_lines([full_fh], [""])

                # Problem summary (PS)
                write_lines(
                    [prob_fh],
                    ["",
                     f"{(' Hybrid: ' + hybrid_name + ' '):-^{HEADER_WIDTH}}",
                     "",
                     f"Average PS-Strip Noise: {strip_mean:.3f}",
                     f"Average PS-Pixel Noise: {pixel_mean:.3f}",
                     f"Hybrid {hybrid_index} Noise Deviations (±{SIGMA_THRESHOLD}σ):",
                     ""]
                )

                # PS strips outliers
                strip_outliers = [
                    (idx, val, err)
                    for idx, (val, err) in enumerate(strip_data, 1)
                    if abs(val - strip_mean) > SIGMA_THRESHOLD * strip_sigma
                ]
                write_lines([prob_fh], ["PS Strip problem channels:"])
                if strip_outliers:
                    for idx, val, err in strip_outliers:
                        chan0 = int(strip_hist.GetXaxis().GetBinCenter(idx))
                        strip_id = map_ps_strip_id_zero_based(chan0, is_right)
                        write_lines([prob_fh], [f"  StripID={strip_id}  Channel={chan0}   noise={val:.3f}   err={err:.3f}"])
                else:
                    write_lines([prob_fh], ["  None"])

                # PS pixels outliers
                pixel_outliers = [
                    (idx, val, err)
                    for idx, (val, err) in enumerate(pixel_data, 1)
                    if abs(val - pixel_mean) > SIGMA_THRESHOLD * pixel_sigma
                ]

                write_lines([prob_fh], ["PS Pixel problem channels:"])
                if pixel_outliers:
                    for idx, val, err in pixel_outliers:
                        chan = int(pixel_hist.GetXaxis().GetBinCenter(idx))
                        write_lines([prob_fh], [f"  Channel={chan}   noise={val:.3f}   err={err:.3f}"])
                else:
                    write_lines([prob_fh], ["  None"])

                # Pattern-Match details (SSA↔MPA↔CIC)
                write_lines([prob_fh], ["Pattern-Match Details:"])
                cluster_wirebond_map = {
                    "0": {"port": 0, "wirebonds": (38, 39)},
                    "1": {"port": 1, "wirebonds": (41, 42)},
                    "2": {"port": 2, "wirebonds": (44, 45)},
                    "3": {"port": 3, "wirebonds": (47, 48)},
                    "4": {"port": 4, "wirebonds": (50, 51)},
                    "5": {"port": 5, "wirebonds": (53, 54)},
                    "6": {"port": 6, "wirebonds": (56, 57)},
                    "7": {"port": 7, "wirebonds": (59, 60)},
                }

                for substr, descr in (("SSAtoMPA_PatternMatchingErrorRate", "SSA -> MPA"),
                                      ("MPAtoCIC_PatternMatchingErrorRate", "MPA -> CIC")):
                    hist2d = find_histogram(hybrid_dir, substr)
                    if not hist2d or hist2d.GetDimension() != 2:
                        continue

                    any_written = False
                    nx, ny = hist2d.GetNbinsX(), hist2d.GetNbinsY()
                    xax, yax = hist2d.GetXaxis(), hist2d.GetYaxis()

                    for xb in range(1, nx + 1):
                        for yb in range(1, ny + 1):
                            err_rate = hist2d.GetBinContent(xb, yb)
                            if err_rate <= 0.5:
                                continue

                            mpa_id = int(xax.GetBinCenter(xb))
                            y_label = yax.GetBinLabel(yb)
                            write_lines([prob_fh], [f"  {descr}:", f"      MPA ID {mpa_id},", f"          {y_label}, Error Rate={err_rate:.3f}"])
                            any_written = True

                            if substr.startswith("SSAtoMPA"):
                                if y_label == "L1":
                                    write_lines([prob_fh], ["              Check port 8"])
                                elif y_label.startswith("Cluster"):
                                    cl = y_label[len("Cluster"):]
                                    info = cluster_wirebond_map.get(cl)
                                    if info:
                                        port = info["port"]
                                        wb1, wb2 = info["wirebonds"]
                                        write_lines([prob_fh], [f"              Check Input Port p{port}",
                                                                f"                  Wirebonds #{wb1} and #{wb2}"])
                                    else:
                                        write_lines([prob_fh], [f"              Unknown cluster {cl}"])
                            else:
                                write_lines([prob_fh], [f"              Check wirebonds #72–#88 on MPA ID {mpa_id}"])

                    if not any_written:
                        write_lines([prob_fh], [f"  {descr}: None"])
                    write_lines([prob_fh], ["=" * HEADER_WIDTH])

    root_file.Close()
    full_fh.close()
    prob_fh.close()

    print(f"\nPS Full Channel Summary:    {full_path}")
    print(f"PS Problem Channel Summary: {prob_path}")
    echo_problem_summary(prob_path)



def parse_args():
    p = argparse.ArgumentParser(description="Analyze the newest 2S/PS ROOT file under a directory, or use the one you pass explicitly.")
    p.add_argument("-d", "-s", "-dir", "-search", "--search-dir", default=".", help="Directory to search for 2S/PS ROOT files when none is given. Will auto-detect the newest file in the search dir (default: current directory)")
    p.add_argument("-f", "-file", "--rootfile", nargs="?", help="Path to an explicit .root file. Use if you want to override auto-detection feature.")
    return p.parse_args()


def autodetect_root_file(search_dir: str) -> str:
    """
    Find the most-recent .root file under search_dir. Module type is determined from Info/Module_ID metadata.
    """
    candidates: List[str] = []

    for dirpath, _, files in os.walk(search_dir):
        for f in files:
            if f.endswith(".root"):
                candidates.append(os.path.join(dirpath, f))

    if not candidates:
        raise SystemExit(f"ERROR: no .root files found under '{search_dir}'.")
    
    return max(candidates, key=os.path.getmtime)


def detect_module_type_from_metadata(root_path: str) -> str:
    """
    Open the ROOT file and inspect Info/Module_ID metadata to determine if this is a 2S or PS module.
    """
    root_file = TFile.Open(root_path)
    if not root_file or root_file.IsZombie():
        raise SystemExit(f"ERROR: cannot open '{root_path}'")

    info_dir = root_file.Get("Info")
    if not info_dir:
        root_file.Close()
        raise SystemExit("ERROR: 'Info' directory not found in ROOT file")

    module_id_obj = info_dir.Get("Module_ID")
    if not module_id_obj:
        root_file.Close()
        raise SystemExit("ERROR: 'Info/Module_ID' object not found in ROOT file")

    module_id = str(module_id_obj.GetString()).strip()
    root_file.Close()

    if not module_id:
        raise SystemExit("ERROR: Info/Module_ID is empty. No metadata found here. Check your ROOT file.")
    
    module_type = module_id.split("_", 1)[0].upper()

    if module_type not in ("2S", "PS"):
        raise SystemExit(
            f"ERROR: unknown module type '{module_type}' from Info/Module_ID = '{module_id}' "
            "(expected something starting with '2S_' or 'PS_')"
        )

    return module_type


def main():
    args = parse_args()
    input_path = args.rootfile or autodetect_root_file(args.search_dir)
    module_type = detect_module_type_from_metadata(input_path)

    if module_type == "2S":
        analyze_2S_file(input_path)
    elif module_type == "PS":
        analyze_PS_file(input_path)
    else:
        raise SystemExit(
            f"ERROR: unsupported module type '{module_type}' in '{input_path}' "
            "(expected '2S' or 'PS')."
        )
if __name__ == "__main__":
    main()