#!/usr/bin/env python3
"""
Usage:
  python3 update_v6_17_and_fit.py  --data-dir ./DataDir [--dry-run]

"""

import argparse
import os
import re
import shutil
import sys
import math

# Import ROOT for optical-group detection and fitting
try:
	import ROOT
except Exception as e:
	print(f"Error: PyROOT not available: {e}")
	print("Please ensure ROOT with Python bindings is installed and configured (PyROOT).")
	sys.exit(1)

ROOT.gErrorIgnoreLevel = ROOT.kError 
ROOT.gROOT.SetBatch(True)

COLUMNS = 120

# ------------------------ Utilities ------------------------

def find_version_candidates(data_dir: str, version: str):
	pattern = re.compile(rf"^(?P<stem>.+?){re.escape(version)}(?P<ext>\.root)?$")
	for entry in os.scandir(data_dir):
		if not entry.is_file():
			continue
		name = entry.name
		m = pattern.match(name)
		if m:
			yield entry.path, m.group("stem"), (m.group("ext") or "")


# ------------------------ Fitting ------------------------

def check_fit(root_path: str) -> None:
	# Open the file
	f = ROOT.TFile.Open(root_path, "UPDATE")

	# Navigate from top directory
	detector_dir = f.Get("Detector")
	board_dir = detector_dir.Get("Board_0")

	# Now loop dynamically over optical groups
	for og_key in board_dir.GetListOfKeys():
		og_dir = board_dir.Get(og_key.GetName())  # OpticalGroup_X

		if not isinstance(og_dir, ROOT.TDirectory):
			continue

		# Loop over hybrids
		for hyb_key in og_dir.GetListOfKeys():
			hyb_dir = og_dir.Get(hyb_key.GetName())  # Hybrid_X

			if not isinstance(hyb_dir, ROOT.TDirectory):
				continue
			   
			# Loop over Chips
			for chip_key in hyb_dir.GetListOfKeys():
				chip_dir = hyb_dir.Get(chip_key.GetName())  # SSA_X or MPA_X
				#print(" chip_dir ",chip_dir)
				if not isinstance(chip_dir, ROOT.TDirectory):
					continue
 
				# Inside Channel directory
				channel_dir = chip_dir.Get("Channel")
				if not channel_dir:
					continue

				# Loop over histograms in Channel
				hist_names = [k.GetName() for k in channel_dir.GetListOfKeys() if isinstance(channel_dir.Get(k.GetName()), ROOT.TH1)]

				for name in hist_names:
					
					hist = channel_dir.Get(name)
					if "SCurve" not in name:
						continue
					



					# Delete any existing fit objects for this channel
					fit_name = f"SCurveFit"
					fit = hist.GetFunction(fit_name)
					if not fit:
						print("No fit saved in histogram for file "+root_path+" scruve "+hist.GetName())
						with open(root_path.replace(".root","_old.txt"), "a") as textfile:
							textfile.write(hist.GetName()+" \n")
					else:
						chi2 = fit.GetChisquare()
						ndf = fit.GetNDF()
						prob = fit.GetProb()

						# print("chi2/ndf =", chi2, "/", ndf, "prob =", prob)

						if prob < 1e-3 or ndf == 0:
							# print("Likely a bad fit for file "+root_path+" scruve "+hist.GetName())
							with open(root_path.replace(".root","_old.txt"), "a") as textfile:
								textfile.write(hist.GetName()+" \n")

	f.Close()
	print(f"Finished with file: {root_path}")
	print("  - Updated per-channel S-curve fits in-place (SSA+MPA only)")
	print("  - Replaced noise and noise related histograms")
	print("  - All objects written to existing directories")


# ------------------------ Main ------------------------

def main() -> None:
	parser = argparse.ArgumentParser(description="Copy v6-16 files to v6-17 and update S-curve fits in-place for PS files (SSA + MPA), without altering file structure.")
	parser.add_argument("--data-dir", required=True, help="Directory to scan for files")
	parser.add_argument("--dry-run", action="store_true", help="Only print actions without performing them")
	args = parser.parse_args()

	data_dir = os.path.abspath(args.data_dir)
	if not os.path.isdir(data_dir):
		print(f"Error: data directory not found: {data_dir}")
		sys.exit(1)

	found_any = False
	for src_path, stem, ext in find_version_candidates(data_dir, "v6-16"):
		found_any = True
		
		base = os.path.basename(src_path)
		if (base.startswith("PS") or base.startswith("2S")) and not args.dry_run:
			
			# For PS, also update S-curve fits in-place
			if base.startswith("PS"):
				print(f"Updating S-curve fits for: {src_path}")
				check_fit(src_path)
		elif (base.startswith("PS") or base.startswith("2S")) and args.dry_run:
			print(f"[DRY-RUN] Would swap EyeOpening hist names/titles in: {src_path}")
			if base.startswith("PS"):
				print(f"[DRY-RUN] Would update S-curve fits in-place for: {src_path} (auto OG={og})")

	if not found_any:
		print(f"No files ending with 'v6-16' found in {data_dir}")
	else:
		print("Done.")


if __name__ == "__main__":
	main()
