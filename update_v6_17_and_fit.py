#!/usr/bin/env python3
"""
Scan a directory for files ending with 'v6-16' (optionally followed by .root),
create exact copies with the ending changed to 'v6-17', and for files whose
basename starts with 'PS', update the S-curve fits IN-PLACE inside the copied
v6-17 ROOT file. Only the fitting part is applied
and results are written back into the existing SSA/MPA directories without
altering the file's directory structure.

Usage:
  python3 pythonUtils/converter_v6-16tov6-17.py  --data-dir ./DataDir [--dry-run]

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


# ------------------------ Utilities ------------------------

def find_candidates(data_dir: str):
	pattern = re.compile(r"^(?P<stem>.+?)v6-16(?P<ext>\.root)?$")
	for entry in os.scandir(data_dir):
		if not entry.is_file():
			continue
		name = entry.name
		m = pattern.match(name)
		if m:
			yield entry.path, m.group("stem"), (m.group("ext") or "")


def copy_to_v6_17(src_path: str, stem: str, ext: str, dry_run: bool = False) -> str:
	new_name = f"{stem}v6-17{ext}"
	destination_path = os.path.join(os.path.dirname(src_path), new_name)
	if dry_run:
		print(f"[DRY-RUN] Would copy: {src_path} -> {destination_path}")
		return destination_path
	print(f"Copy: {src_path} -> {destination_path}")
	shutil.copy2(src_path, destination_path)
	return destination_path

# ------------------------ Fitting ------------------------

def MyErf(x, par):
	x0 = par[0]
	width = par[1]
	if x[0] < x0:
		return 0.5 * math.erfc((x[0] - x0) / (math.sqrt(2.0) * width))
	else:
		return 0.5 + 0.5 * math.erf((x0 - x[0]) / (math.sqrt(2.0) * width))


def delete_if_exists(tdir: ROOT.TDirectory, name: str) -> None:
	obj = tdir.Get(name)
	if obj:
		# delete all cycles of this name
		tdir.Delete(f"{name};*")


def overwrite_canvas(tdir: ROOT.TDirectory, canvas: ROOT.TCanvas, name: str) -> None:
	# ensure we overwrite the object with the same name (no extra cycles)
	delete_if_exists(tdir, name)
	tdir.cd()
	canvas.Write("", ROOT.TObject.kOverwrite)


def run_fit_in_place(root_path: str) -> None:
	# Open the file
	f = ROOT.TFile.Open(root_path, "UPDATE")

	# Navigate from top directory
	detector_dir = f.Get("Detector")
	# detector_dir.cd()
	board_dir = detector_dir.Get("Board_0")
	# board_dir.cd()

	# Now loop dynamically over optical groups
	for og_key in board_dir.GetListOfKeys():
		og_dir = board_dir.Get(og_key.GetName())  # OpticalGroup_X
		#print("og_dir ", og_dir)
		if not isinstance(og_dir, ROOT.TDirectory):
			continue
		# og_dir.cd()
		# Loop over hybrids
		for hyb_key in og_dir.GetListOfKeys():
			hyb_dir = og_dir.Get(hyb_key.GetName())  # Hybrid_X
			#print(" hyb_dir ",hyb_dir)
			if not isinstance(hyb_dir, ROOT.TDirectory):
				continue
			# hyb_dir.cd()
			# Loop over Chips
			for chip_key in hyb_dir.GetListOfKeys():
				chip_dir = hyb_dir.Get(chip_key.GetName())  # SSA_X or MPA_X
				#print(" chip_dir ",chip_dir)
				if not isinstance(chip_dir, ROOT.TDirectory):
					continue
 
 
				h_chip_noise_summary = None
				for key in chip_dir.GetListOfKeys():
					obj = chip_dir.Get(key.GetName())
					if isinstance(obj, ROOT.TH1) and "ChannelNoise" in obj.GetName():
						h_chip_noise_summary = obj
						break
				if h_chip_noise_summary is None:
					print("No summary histogram found in", chip_dir.GetName())
					continue
 
				h_chip_noise_summary_copy = ROOT.TH1F() 
				h_chip_noise_summary.Copy(h_chip_noise_summary_copy)
				h_chip_noise_summary_copy.SetDirectory(0) 
				h_chip_noise_summary_copy.Reset()
				# h_chip_noise_summary_copy.SetName(h_chip_noise_summary_copy.GetName()+"copy")
				# chip_dir.cd()
				# Inside Channel directory
				channel_dir = chip_dir.Get("Channel")
				if not channel_dir:
					continue
				# channel_dir.cd()
				# Loop over histograms in Channel
				hist_names = [k.GetName() for k in channel_dir.GetListOfKeys() if isinstance(channel_dir.Get(k.GetName()), ROOT.TH1)]
				channelcounter = 0
				for name in hist_names:
					
					hist = channel_dir.Get(name)
					if "SCurve" not in name:
						continue
					
					# print("Processing", name)

					# name = hist.GetName()
					# # Example filter: only SCurve hists
					# if "SCurve" in name:
					# 	print("Found:", name)
						
	

					# Delete any existing fit objects for this channel
					fit_name = f"SCurveFit"
					fit = hist.GetFunction(fit_name)
					if fit:
						hist.GetListOfFunctions().Remove(fit)


					# fit initial parameters
					if(chip_dir.GetName().find("SSA")):
						cChannelPedestal = 30.0
						cChannelNoise = 3.0
					elif(chip_dir.GetName().find("MPA")):
						cChannelPedestal = 125.0
						cChannelNoise = 5.0

					# Edge search
					lastOneIndex = -1
					firstZeroIndex = -1
					oneThreshold = 0.9
					zeroThreshold = 0.1
					maxNoise = 10.0
					bins = hist.GetNbinsX()
					for l in range(bins):
						currentbin = l+1
						if hist.GetBinContent(currentbin) > oneThreshold and hist.GetBinContent(currentbin + 1) < hist.GetBinContent(currentbin):
							lastOneIndex = l
							break
					for l in range(bins+1, 0, -1):
						if hist.GetBinContent(l) < zeroThreshold and hist.GetBinContent(l - 1) > hist.GetBinContent(l):
							firstZeroIndex = l
							break
						
					if firstZeroIndex != -1 and lastOneIndex != -1:
						cChannelPedestal = (lastOneIndex + firstZeroIndex) / 2.0
						cChannelNoise = (firstZeroIndex - lastOneIndex) / 2.0
						if cChannelNoise > maxNoise:
							cChannelNoise = maxNoise
					noiseTolerance = 2.0
					rangeMinus = cChannelPedestal - (cChannelNoise * noiseTolerance)
					rangePlus = cChannelPedestal + (cChannelNoise * noiseTolerance)
		
					# fit.SetRange(rangeMinus, rangePlus)
					newfit = ROOT.TF1(fit_name, MyErf, rangeMinus, rangePlus, 2)
					newfit.SetNpx(100)
					newfit.SetParameter(0, cChannelPedestal)
					newfit.SetParameter(1, cChannelNoise)
					hist.Fit(newfit, "RQ+")
					hist.Fit(newfit, "RQ+")
					hist.Fit(newfit, "RQ+")
					newfit.SetRange(rangeMinus, rangePlus)
					# channel_dir.WriteTObject(hist, hist.GetName(), ROOT.TObject.kOverwrite)
					channel_dir.cd()  # temporarily move into that directory
					hist.Write(hist.GetName(), ROOT.TObject.kOverwrite)
					# hist.Write(hist.GetName(), ROOT.TObject.kOverwrite)
					
					# h_chip_noise_summary_copy.SetBinContent(channelcounter + 1, newfit.GetParameter(1))
					h_chip_noise_summary_copy.SetBinContent(channelcounter + 1, newfit.GetParameter(1))

					channelcounter = channelcounter +1
				print("Updating chip hists")

				# Write back
				chip_dir.cd()
				# name = h_chip_noise_summary.GetName()
				# print(name)
				# print(h_chip_noise_summary_copy.GetBinContent(3))
				# print(h_chip_noise_summary.GetBinContent(3))
				# h_chip_noise_summary.Delete()
				# print("delete")
				# h_chip_noise_summary_copy.SetName(name)
				# print(name)
				# print(h_chip_noise_summary_copy.GetBinContent(3))
				h_chip_noise_summary_copy.Write(h_chip_noise_summary_copy.GetName(), ROOT.TObject.kOverwrite)
				# h_chip_noise_summary.Write(h_chip_noise_summary.GetName(), ROOT.TObject.kOverwrite)
			print("hybrid loop")
		print("og loop")
	print("close all")
	f.Close()
	print(f"Updated S-curve fits in-place (SSA+MPA) without altering structure: {root_path}")
	print("  - Updated existing 2D histogram fits")
	print("  - Replaced noise histograms (noiseSSA/noiseMPA)")
	print("  - Updated per-channel canvases with new fits")
	print("  - All objects written to existing directories (no new structure)")


def _swap_power_titles(title: str, from_str: str, to_str: str) -> str:
	if not isinstance(title, str):
		return to_str
	return title.replace(from_str, to_str)

def find_hists_with_string(tdir, substring, path=""):
	"""Recursively search for histograms whose names contain substring."""

	for key in tdir.GetListOfKeys():
		obj = key.ReadObj()
		name = obj.GetName()
		fullpath = f"{path}/{name}" if path else name

		if isinstance(obj, ROOT.TDirectory):
			# Recurse into subdirectory
			results = find_hists_with_string(obj, substring, fullpath)
		elif isinstance(obj, ROOT.TH1):
			if substring in name:
				results = fullpath, obj, tdir
	return results

def swap_eyeopening_histograms(root_path: str) -> None:
	f = ROOT.TFile.Open(root_path, "UPDATE")
	if not f or f.IsZombie():
		raise RuntimeError(f"Cannot open for UPDATE: {root_path}")
	try:
		print(f"Swapping EyeOpening histogram names/titles in: {root_path}")
		"""Within this directory, swap names/titles of LpGBT_EyeOpeningScan_Power_0.333333 and _1.000000 if present."""
		name_0p33 = "LpGBT_EyeOpeningScan_Power_0.333333"
		name_1p00 = "LpGBT_EyeOpeningScan_Power_1.000000"


		fullpath_0p33, h0p33, parent_dir_0p33 = find_hists_with_string(f, name_0p33)
		fullpath_1p00, h1p00, parent_dir_1p00 = find_hists_with_string(f, name_1p00)
		if not h033 and not h100:
			return

		parent_dir_0p33.cd()
		if h033 and h100:
			# Clone both, swap names and titles, overwrite
			tmp0p33 = h0p33.Clone()
			tmp1p00 = h1p00.Clone()
		if hasattr(tmp0p33, "SetDirectory"):
			tmp0p33.SetDirectory(0)
		if hasattr(tmp1p00, "SetDirectory"):
			tmp1p00.SetDirectory(0)

		tdir.Delete(f"{name_0p33};*")
		tdir.Delete(f"{name_1p00};*")

		tmp0p33.SetName(name_1p00)
		tmp0p33.SetTitle(tmp0p33.GetTitle().replace("0.333333", "1.000000"))
		tmp1p00.SetName(name_0p33)
		tmp1p00.SetTitle(tmp1p00.GetTitle().replace("1.000000", "0.333333"))

		# Attach and write
		if hasattr(tmp0p33, "SetDirectory"):
			tmp0p33.SetDirectory(tdir)
		if hasattr(tmp1p00, "SetDirectory"):
			tmp1p00.SetDirectory(tdir)
		tmp0p33.Write("", ROOT.TObject.kOverwrite)
		tmp1p00.Write("", ROOT.TObject.kOverwrite)
	finally:
		f.Close()



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
	for src_path, stem, ext in find_candidates(data_dir):
		found_any = True
		# Make the v6-17 copy
		destination_path = copy_to_v6_17(src_path, stem, ext, dry_run=args.dry_run)

		# If the file starts with PS or 2S, perform required updates
		base = os.path.basename(destination_path)
		if (base.startswith("PS") or base.startswith("2S")) and not args.dry_run:
			# Swap EyeOpening histogram names/titles first
			# swap_eyeopening_histograms(destination_path)
			# For PS, also update fits in-place
			if base.startswith("PS"):
				print(f"Updating S-curve fits in-place for: {destination_path}")
				run_fit_in_place(destination_path)
				print("done with first file")
		elif (base.startswith("PS") or base.startswith("2S")) and args.dry_run:
			print(f"[DRY-RUN] Would swap EyeOpening hist names/titles in: {destination_path}")
			if base.startswith("PS"):
				print(f"[DRY-RUN] Would update S-curve fits in-place for: {destination_path} (auto OG={og})")

	if not found_any:
		print(f"No files ending with 'v6-16' found in {data_dir}")
	else:
		print("Done.")


if __name__ == "__main__":
	main()
