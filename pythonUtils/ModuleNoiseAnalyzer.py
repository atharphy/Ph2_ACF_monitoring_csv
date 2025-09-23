#!/usr/bin/env python3

"""
Usage: 
    python3 ModuleNoiseAnalyzer.py
    python3 ModuleNoiseAnalyzer.py -s </path/to/Specific/Root/File/Directory>

Analyze the newest 2S/PS ROOT file under a directory, or use the one you pass explicitly. 
Outputs two summary text files in the same directory as the input ROOT file: Problem and Full Channel Summaries.
"""
import sys
import os
import math
import argparse
from ROOT import TFile, TH1

SIGMA_THRESHOLD      = 5
TOTAL_SENSOR_STRIPS  = 1016
HEADER_WIDTH         = 60

def open_summary_files(root_path, module_type):
    base_name               = os.path.splitext(os.path.basename(root_path))[0]
    out_dir                 = os.path.dirname(os.path.abspath(root_path))
    full_summary_filename   = os.path.join(out_dir, f"{base_name}_{module_type}_Full_Channel_Summary.txt")
    problem_summary_filename= os.path.join(out_dir, f"{base_name}_{module_type}_Problem_Channel_Summary.txt")
    full_handle             = open(full_summary_filename, "w")
    problem_handle          = open(problem_summary_filename, "w")
    return full_handle, problem_handle, full_summary_filename, problem_summary_filename

def write_lines(file_handles, lines):
    for fh in file_handles:
        for line in lines:
            fh.write(line + "\n")

def find_histogram(directory, name_substr):
    for key in directory.GetListOfKeys():
        hist_name = key.GetName()
        if name_substr in hist_name:
            hist_obj = directory.Get(hist_name)
            if isinstance(hist_obj, TH1):
                return hist_obj
    return None

def format_strip_mapping(bin_index, is_top, is_right):
    """
    Map a half-sensor channel (bin_index) to its global strip ID.
    """
    if is_top and is_right:
        global_id = TOTAL_SENSOR_STRIPS - bin_index + 1
    elif is_top and not is_right:
        global_id = TOTAL_SENSOR_STRIPS - bin_index + 1 + TOTAL_SENSOR_STRIPS
    elif not is_top and is_right:
        global_id = bin_index + TOTAL_SENSOR_STRIPS
    else:
        global_id = bin_index

    sensor_position = "Top"    if is_top   else "Bottom"
    hybrid_side     = "Right"  if is_right else "Left"
    return sensor_position, hybrid_side, global_id

def compute_hist_stats(hist):
    """
    Compute mean, standard deviation, and a list of (value, error) for each bin in the given TH1.
    """
    num_bins     = hist.GetNbinsX()
    bin_data     = [(hist.GetBinContent(i), hist.GetBinError(i))
                    for i in range(1, num_bins+1)]
    mean_noise   = sum(val for val, _ in bin_data) / num_bins
    sigma_noise  = math.sqrt(
        sum((val - mean_noise)**2 for val, _ in bin_data) / num_bins
    )
    return mean_noise, sigma_noise, bin_data

def analyze_2S_file(root_path):
    """
    Analyze a 2S-module ROOT file and produce two summary text files.
    """
    full_handle, problem_handle, full_filename, problem_filename = \
        open_summary_files(root_path, "2S")

    write_lines(
        [full_handle, problem_handle],
        [f"Input File: {os.path.abspath(root_path)}",
         "Detected file type: 2S module"]
    )

    root_file = TFile.Open(root_path)
    if not root_file or root_file.IsZombie():
        sys.exit(f"ERROR: cannot open '{root_path}'")
    detector_dir = root_file.GetDirectory("Detector")
    if not detector_dir:
        sys.exit("ERROR: 'Detector' directory not found")

    for board_key in detector_dir.GetListOfKeys():
        board_name = board_key.GetName()
        if not board_name.startswith("Board_"):
            continue

        write_lines(
            [full_handle, problem_handle],
            [f"{(' Board: ' + board_name + ' '):-^{HEADER_WIDTH}}"]
        )
        board_dir = detector_dir.GetDirectory(board_name)

        for og_key in board_dir.GetListOfKeys():
            og_name = og_key.GetName()
            if not og_name.startswith("OpticalGroup_"):
                continue

            write_lines(
                [full_handle, problem_handle],
                [f"{(' Optical Group: ' + og_name + ' '):-^{HEADER_WIDTH}}", ""]
            )
            og_dir = board_dir.GetDirectory(og_name)

            for hybrid_key in og_dir.GetListOfKeys():
                hybrid_name = hybrid_key.GetName()
                if not hybrid_name.startswith("Hybrid_"):
                    continue

                hybrid_index    = int(hybrid_name.split("_")[1])
                is_right_side   = (hybrid_index % 2 == 0)
                hybrid_dir      = og_dir.GetDirectory(hybrid_name)

                top_noise_hist    = find_histogram(hybrid_dir, "StripChannelNoiseTop")
                bottom_noise_hist = find_histogram(hybrid_dir, "StripChannelNoiseBottom")
                if not top_noise_hist or not bottom_noise_hist:
                    continue

                mean_top, sigma_top, top_bin_data    = compute_hist_stats(top_noise_hist)
                mean_bot, sigma_bot, bot_bin_data    = compute_hist_stats(bottom_noise_hist)

                # Full summary 
                write_lines(
                    [full_handle],
                    ["",
                     f"{(' Hybrid: ' + hybrid_name + ' '):-^{HEADER_WIDTH}}",
                     ""]
                )
                for side_flag, (mean_val, sigma_val, bin_data) in (
                    (True,  (mean_top, sigma_top, top_bin_data)),
                    (False, (mean_bot, sigma_bot, bot_bin_data))
                ):
                    side_label = "Top" if side_flag else "Bottom"
                    write_lines(
                        [full_handle],
                        [f"{side_label} Sensor: avg={mean_val:.3f}, σ={sigma_val:.3f}",
                         "  chan   noise    err   global_strip_id"]
                    )
                    for chan_idx, (val, err) in enumerate(bin_data, 1):
                        _, _, strip_id = format_strip_mapping(chan_idx, side_flag, is_right_side)
                        write_lines(
                            [full_handle],
                            [f"  {chan_idx:3d}   {val:7.3f}  {err:7.3f}   {strip_id:4d}"]
                        )

                write_lines(
                    [problem_handle],
                    ["",
                     f"{(' Hybrid: ' + hybrid_name + ' '):-^{HEADER_WIDTH}}",
                     ""]
                )

                disconnected = []
                noisy        = []

                # Identify disconnected/noisy on Top side
                for chan_idx, (val, _) in enumerate(top_bin_data, 1):
                    sensor_pos, _, strip_id = format_strip_mapping(chan_idx, True, is_right_side)
                    if   val < mean_top - SIGMA_THRESHOLD*sigma_top:
                        disconnected.append(f"{sensor_pos} sensor - strip #{strip_id}")
                    elif val > mean_top + SIGMA_THRESHOLD*sigma_top:
                        noisy.append(f"{sensor_pos} sensor - strip #{strip_id}")

                # Identify disconnected/noisy on Bottom side
                for chan_idx, (val, _) in enumerate(bot_bin_data, 1):
                    sensor_pos, _, strip_id = format_strip_mapping(chan_idx, False, is_right_side)
                    if   val < mean_bot - SIGMA_THRESHOLD*sigma_bot:
                        disconnected.append(f"{sensor_pos} sensor - strip #{strip_id}")
                    elif val > mean_bot + SIGMA_THRESHOLD*sigma_bot:
                        noisy.append(f"{sensor_pos} sensor - strip #{strip_id}")

                # Write out disconnected channels
                write_lines(
                    [problem_handle],
                    [f"Disconnected channels (< {SIGMA_THRESHOLD}σ):"]
                )
                if disconnected:
                    for entry in disconnected:
                        write_lines([problem_handle], [f"  {entry}"])
                else:
                    write_lines([problem_handle], ["  None"])

                # Write out noisy channels
                write_lines(
                    [problem_handle],
                    ["",
                     f"Noisy channels (> {SIGMA_THRESHOLD}σ):"]
                )
                if noisy:
                    for entry in noisy:
                        write_lines([problem_handle], [f"  {entry}"])
                else:
                    write_lines([problem_handle], ["  None"])

                write_lines([problem_handle], ["=" * HEADER_WIDTH])

    root_file.Close()
    full_handle.close()
    problem_handle.close()
    print(f"\nFull Channel Summary:    {full_filename}")
    print(f"Problem Channel Summary: {problem_filename}")

def analyze_PS_file(root_path):
    """Analyze a PS-module ROOT file and produce two summary text files."""
    full_handle, problem_handle, full_filename, problem_filename = \
        open_summary_files(root_path, "PS")

    write_lines(
        [full_handle, problem_handle],
        [f"Input File: {os.path.abspath(root_path)}",
         "Detected file type: PS module"]
    )

    root_file = TFile.Open(root_path)
    if not root_file or root_file.IsZombie():
        sys.exit(f"ERROR: cannot open '{root_path}'")
    detector_dir = root_file.GetDirectory("Detector")
    if not detector_dir:
        sys.exit("ERROR: 'Detector' directory not found")

    for board_key in detector_dir.GetListOfKeys():
        board_name = board_key.GetName()
        if not board_name.startswith("Board_"):
            continue

        write_lines(
            [full_handle, problem_handle],
            [f"{(' Board: ' + board_name + ' '):-^{HEADER_WIDTH}}"]
        )
        board_dir = detector_dir.GetDirectory(board_name)

        for og_key in board_dir.GetListOfKeys():
            og_name = og_key.GetName()
            if not og_name.startswith("OpticalGroup_"):
                continue

            write_lines(
                [full_handle, problem_handle],
                [f"{(' Optical Group: ' + og_name + ' '):-^{HEADER_WIDTH}}", ""]
            )
            og_dir = board_dir.GetDirectory(og_name)

            for hybrid_key in og_dir.GetListOfKeys():
                hybrid_name = hybrid_key.GetName()
                if not hybrid_name.startswith("Hybrid_"):
                    continue

                hybrid_index  = int(hybrid_name.split("_")[1])
                is_right_side = (hybrid_index % 2 == 0)
                hybrid_dir    = og_dir.GetDirectory(hybrid_name)

                strip_noise_hist  = find_histogram(hybrid_dir, "StripChannelNoise")
                pixel_noise_hist  = find_histogram(hybrid_dir, "PixelChannelNoise")
                if not strip_noise_hist or not pixel_noise_hist:
                    continue

                strip_mean, strip_sigma, strip_data = compute_hist_stats(strip_noise_hist)
                pixel_mean, pixel_sigma, pixel_data = compute_hist_stats(pixel_noise_hist)

                #PS Header Full summary
                write_lines(
                    [full_handle],
                    ["",
                     f"{(' Hybrid: ' + hybrid_name + ' '):-^{HEADER_WIDTH}}",
                     ""]
                )
                
                # PS Strips Full
                write_lines(
                    [full_handle],
                    [f"PS Strips: avg={strip_mean:.3f}, σ={strip_sigma:.3f}",
                     "  chan   noise    err"]
                )
                for idx, (val, err) in enumerate(strip_data, 1):
                    chan = int(strip_noise_hist.GetXaxis().GetBinCenter(idx))
                    write_lines([full_handle], [f"  {chan:5d}  {val:7.3f}  {err:7.3f}"])
                write_lines([full_handle], [""])
                
                # PS Pixels Full
                write_lines(
                    [full_handle],
                    [f"PS Pixels: avg={pixel_mean:.3f}, σ={pixel_sigma:.3f}",
                     "  chan   noise    err"]
                )
                for idx, (val, err) in enumerate(pixel_data, 1):
                    chan = int(pixel_noise_hist.GetXaxis().GetBinCenter(idx))
                    write_lines([full_handle], [f"  {chan:5d}  {val:7.3f}  {err:7.3f}"])
                write_lines([full_handle], [""])

   
                write_lines(
                    [problem_handle],
                    ["",
                     f"{(' Hybrid: ' + hybrid_name + ' '):-^{HEADER_WIDTH}}",
                     "",
                     f"Average PS-Strip Noise: {strip_mean:.3f}",
                     f"Average PS-Pixel Noise: {pixel_mean:.3f}",
                     f"Hybrid {hybrid_index} Noise Deviations (±{SIGMA_THRESHOLD}σ):",
                     ""]
                )

                # PS strip outliers
                strip_outliers = [
                    (idx, val, err)
                    for idx, (val, err) in enumerate(strip_data, 1)
                    if abs(val - strip_mean) > SIGMA_THRESHOLD * strip_sigma
                ]
                write_lines([problem_handle], ["PS Strip problem channels:"])
                if strip_outliers:
                    for idx, val, err in strip_outliers:
                        bin_center = strip_noise_hist.GetXaxis().GetBinCenter(idx)
                        write_lines(
                            [problem_handle],
                            [f"  Channel={bin_center}   noise={val:.3f}   err={err:.3f}"]
                        )
                else:
                    write_lines([problem_handle], ["  None"])

                # PS pixel outliers
                pixel_outliers = [
                    (idx, val, err)
                    for idx, (val, err) in enumerate(pixel_data, 1)
                    if abs(val - pixel_mean) > SIGMA_THRESHOLD * pixel_sigma
                ]
                write_lines([problem_handle], ["PS Pixel problem channels:"])
                if pixel_outliers:
                    for idx, val, err in pixel_outliers:
                        bin_center = pixel_noise_hist.GetXaxis().GetBinCenter(idx)
                        write_lines(
                            [problem_handle],
                            [f"  Channel={bin_center}   noise={val:.3f}   err={err:.3f}"]
                        )
                else:
                    write_lines([problem_handle], ["  None"])

                # Pattern-Match Details using cluster wirebond mapping
                write_lines([problem_handle], ["Pattern-Match Details:"])
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
                for substr, descr in (
                    ("SSAtoMPA_PatternMatchingErrorRate", "SSA→MPA"),
                    ("MPAtoCIC_PatternMatchingErrorRate", "MPA→CIC"),
                ):
                    two_d_hist = find_histogram(hybrid_dir, substr)
                    if not two_d_hist or two_d_hist.GetDimension() != 2:
                        continue

                    any_written = False
                    nx = two_d_hist.GetNbinsX()
                    ny = two_d_hist.GetNbinsY()
                    x_axis = two_d_hist.GetXaxis()
                    y_axis = two_d_hist.GetYaxis()

                    for x_bin in range(1, nx+1):
                        for y_bin in range(1, ny+1):
                            error_rate = two_d_hist.GetBinContent(x_bin, y_bin)
                            if error_rate <= 0.5:
                                continue
                            mpa_id        = int(x_axis.GetBinCenter(x_bin))
                            y_label       = y_axis.GetBinLabel(y_bin)
                            write_lines(
                                [problem_handle],
                                [f"  {descr}:",
                                 f"      MPA ID {mpa_id},",
                                 f"          {y_label}, Error Rate={error_rate:.3f}"]
                            )
                            any_written = True

                            if substr.startswith("SSAtoMPA"):
                                if y_label == "L1":
                                    write_lines([problem_handle], ["              Check port 8"])
                                elif y_label.startswith("Cluster"):
                                    cl = y_label[len("Cluster"):]
                                    info = cluster_wirebond_map.get(cl)
                                    if info:
                                        port = info["port"]
                                        wb1, wb2 = info["wirebonds"]
                                        write_lines(
                                            [problem_handle],
                                            [f"              Check Input Port p{port}",
                                             f"                  Wirebonds #{wb1} and #{wb2}"]
                                        )
                                    else:
                                        write_lines([problem_handle], [f"              Unknown cluster {cl}"])
                            else:  # MPAtoCIC
                                write_lines([problem_handle], ["              Check wirebonds #72–#88 on MPA ID {mpa_id}"])

                    if not any_written:
                        write_lines([problem_handle], [f"  {descr}: None"])
                    write_lines([problem_handle], ["=" * HEADER_WIDTH])

    root_file.Close()
    full_handle.close()
    problem_handle.close()
    print(f"\nPS Full Channel Summary:    {full_filename}")
    print(f"PS Problem Channel Summary: {problem_filename}")

def main():
    parser = argparse.ArgumentParser(
        description="Analyze the newest 2S/PS ROOT file under a directory, or use the one you pass explicitly."
    )
    parser.add_argument(
        "-file", "--rootfile", nargs="?",
        help="Path to a .root file (if you want to override auto-detection)."
    )
    parser.add_argument(
        "-s", "-dir","-search","--search-dir", default=".",
        help="Directory to search for 2S/PS ROOT files when none is given. (default: current directory)"
    )
    args = parser.parse_args()

    if args.rootfile:
        input_path = args.rootfile
    else:
        # walk the search directory looking for 2S/PS ROOT files
        candidates = []
        for dirpath, _, files in os.walk(args.search_dir):
            for f in files:
                if f.endswith(".root") and (f.startswith("2S") or f.startswith("PS")):
                    candidates.append(os.path.join(dirpath, f))

        if not candidates:
            sys.exit(f"ERROR: no 2S/PS .root files found under '{args.search_dir}'")
        # pick the most recently modified one
        input_path = max(candidates, key=os.path.getmtime)
        print(f"Auto‑detected ROOT file: {input_path}")

    base = os.path.basename(input_path)
    if "2S" in base:
        analyze_2S_file(input_path)
    elif "PS" in base:
        analyze_PS_file(input_path)
    else:
        sys.exit(f"ERROR: cannot determine module type from '{base}'")

if __name__ == "__main__":
    main()
