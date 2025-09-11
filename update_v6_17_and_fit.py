#!/usr/bin/env python3
"""
Scan a directory for files ending with 'v6-16' (optionally followed by .root),
create exact copies with the ending changed to 'v6-17', and for files whose
basename starts with 'PS', update the S-curve fits IN-PLACE inside the copied
v6-17 ROOT file. Only the fitting part is applied
and results are written back into the existing SSA/MPA directories without
altering the file's directory structure.

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


# def copy_to_v6_17(src_path: str, stem: str, ext: str, dry_run: bool = False) -> str:
# 	new_name = f"{stem}v6-17{ext}"
# 	destination_path = os.path.join(os.path.dirname(src_path), new_name)
# 	if dry_run:
# 		print(f"[DRY-RUN] Would copy: {src_path} -> {destination_path}")
# 		return destination_path
# 	print(f"Copy: {src_path} -> {destination_path}")
# 	shutil.copy2(src_path, destination_path)
# 	return destination_path

def copy_file_version(src_path: str, stem: str, vupdate: str, ext: str, perdirectory: bool  = False, dry_run: bool = False) -> str:
	new_name = f"{stem}{vupdate}{ext}"
	destination_path = os.path.join(os.path.dirname(src_path), new_name)
	if dry_run:
		print(f"[DRY-RUN] Would copy: {src_path} -> {destination_path}")
		return destination_path
	print(f"Copy: {src_path} -> {destination_path}")
	if not perdirectory: shutil.copy2(src_path, destination_path)
	else:
		#FIXMEEEEE
		f_in = ROOT.TFile.Open(src_path)
		f_out = ROOT.TFile.Open(destination_path, "RECREATE")
		for key in f_in.GetListOfKeys():
			obj = key.ReadObj()
			if obj:
				f_out.cd()   # ensure writing in root dir
				obj.Write()

		f_out.Close()
		f_in.Close()
		shutil.os.remove(src_path)
	return destination_path

# ------------------------ Fitting ------------------------

def MyErf(x, par):
	x0 = par[0]
	width = par[1]
	if x[0] < x0:
		return 0.5 * math.erfc((x[0] - x0) / (math.sqrt(2.0) * width))
	else:
		return 0.5 + 0.5 * math.erf((x0 - x[0]) / (math.sqrt(2.0) * width))


def find_and_copy_hist1D(hist_name, location, hist_type):
	hist = None
	for key in location.GetListOfKeys():
		obj = location.Get(key.GetName())
		if isinstance(obj, ROOT.TH1) and hist_name in obj.GetName():
			hist = obj
			break
	if hist is None:
		print("No "+hist_name+" histogram found in", location.GetName())
		sys.exit()
 
	hist_copy = hist_type 
	hist.Copy(hist_copy)
	hist_copy.SetDirectory(0) 
	hist_copy.Reset()
	return hist_copy, hist

def find_hist1D(hist_name, location):
	hist = None
	for key in location.GetListOfKeys():
		obj = location.Get(key.GetName())
		if isinstance(obj, ROOT.TH1) and hist_name in obj.GetName():
			hist = obj
			break
	if hist is None:
		print("No "+hist_name+" histogram found in", location.GetName())
		sys.exit()

	return  hist

def find_and_copy_hist2D(hist_name, location, hist_type):
	hist = None
	for key in location.GetListOfKeys():
		obj = location.Get(key.GetName())
		if isinstance(obj, ROOT.TH2) and hist_name in obj.GetName():
			hist = obj
			break
	if hist is None:
		print("No "+hist_name+" histogram found in", location.GetName())
		sys.exit()
	hist_copy = hist_type
	hist.Copy(hist_copy)
	hist_copy.SetDirectory(0) 
	hist_copy.Reset()
	return hist_copy

def linearizeRowAndColumns(row, col):
	return col + row * COLUMNS

def run_fit_in_place(root_path: str) -> None:
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
			
			h_hybrid_strip_channel_noise_summary_copy, dummy = find_and_copy_hist1D("StripChannelNoise", hyb_dir, ROOT.TH1F())
			# h_hybrid_pixel_channel_noise_summary_copy, h_hybrid_pixel_channel_noise_summary = find_and_copy_hist1D("PixelChannelNoise", hyb_dir, ROOT.TH1F())
			h_hybrid_pixel_channel_noise_summary = find_hist1D("PixelChannelNoise", hyb_dir)
			h_hybrid_strip_noise_distribution_summary_copy, dummy = find_and_copy_hist1D("StripNoiseDistribution", hyb_dir, ROOT.TH1F())
			h_hybrid_pixel_noise_distribution_summary_copy, dummy = find_and_copy_hist1D("PixelNoiseDistribution", hyb_dir, ROOT.TH1F())
			h_hybrid_pixel_channel_noise_summary.Reset()
   
			# Loop over Chips
			for chip_key in hyb_dir.GetListOfKeys():
				chip_dir = hyb_dir.Get(chip_key.GetName())  # SSA_X or MPA_X
				#print(" chip_dir ",chip_dir)
				if not isinstance(chip_dir, ROOT.TDirectory):
					continue
 
				h_chip_channel_noise_summary_copy, h_chip_channel_noise_summary = find_and_copy_hist1D("ChannelNoise", chip_dir, ROOT.TH1F())
				h_chip_noise_distribution_summary_copy, dummy = find_and_copy_hist1D("NoiseDistribution", chip_dir, ROOT.TH1F())
				if "MPA" in chip_dir.GetName(): h_chip_2D_channel_noise_summary_copy = find_and_copy_hist2D("2DChannelNoise", chip_dir, ROOT.TH2F())

				h_chip_channel_pulseheight_summary_copy, h_chip_channel_pulseheight_summary = find_and_copy_hist1D("ChannelPulseHeight", chip_dir, ROOT.TH1F())
				h_chip_pulseheight_distribution_summary_copy, dummy = find_and_copy_hist1D("PulseHeightDistribution", chip_dir, ROOT.TH1F())


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
					
					# print("Processing", name)
					m = re.search(r"Row\((\d+)\)_Col\((\d+)\)", name)
					if m:
						row = int(m.group(1))
						col = int(m.group(2))
						# print("Row:", row, "Col:", col)

					selectPix = (hyb_dir.GetName() == "Hybrid_9" and chip_dir.GetName() == "MPA_8" and  row == 0 and col == 9)

					# Delete any existing fit objects for this channel
					fit_name = f"SCurveFit"
					fit = hist.GetFunction(fit_name)
					if fit:
						hist.GetListOfFunctions().Remove(fit)
						fit.Delete()

					# fit initial parameters
					# cChannelPedestal = h_chip_channel_pulseheight_summary.GetBinContent(col +1, row + 1)
					# cChannelNoise = h_chip_channel_noise_summary.GetBinContent(col +1, row + 1)
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
					maxNoise = 15.0
					noiseTolerance = 2.0
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
						if(selectPix): print("firstZeroIndex != -1 and lastOneIndex != -1, hybrid ", hyb_dir, " chip ", chip_dir, " row ", row, " col ", col)

						cChannelPedestal = (lastOneIndex + firstZeroIndex) / 2.0
						cChannelNoise = (firstZeroIndex - lastOneIndex) / 2.0
						if cChannelNoise > maxNoise:
							cChannelNoise = maxNoise
							
						rangeMinus = cChannelPedestal - (cChannelNoise * noiseTolerance)
						rangePlus = cChannelPedestal + (cChannelNoise * noiseTolerance)
					elif lastOneIndex == -1 and firstZeroIndex != -1:
						lastOneIndex = hist.GetMaximumBin() # bin with highest content
						cChannelPedestal = (lastOneIndex + firstZeroIndex) / 2.0
						cChannelNoise = (firstZeroIndex - lastOneIndex) / 2.0
						if cChannelNoise > maxNoise:
							cChannelNoise = maxNoise
							
						rangeMinus = cChannelPedestal - (cChannelNoise * noiseTolerance)
						rangePlus = cChannelPedestal + (cChannelNoise * noiseTolerance)
						if(selectPix): print("firstZeroIndex != -1 and lastOneIndex == -1, hybrid ", hyb_dir, " chip ", chip_dir, " row ", row, " col ", col)
						if(selectPix): print(" range ", rangeMinus, " ", rangePlus, " zero ",firstZeroIndex, " one ",lastOneIndex, " pedestal ", cChannelPedestal, " noise ", cChannelNoise)   

					elif firstZeroIndex == -1 and lastOneIndex != -1:
						if(selectPix): print("firstZeroIndex == -1 and lastOneIndex != -1, hybrid ", hyb_dir, " chip ", chip_dir, " row ", row, " col ", col)
	
						firstZeroIndex = hist.GetMinimumBin() # bin with lowest contentfirstZeroIndex == -1:
						cChannelPedestal = (lastOneIndex + firstZeroIndex) / 2.0
						cChannelNoise = (firstZeroIndex - lastOneIndex) / 2.0
						if cChannelNoise > maxNoise:
							cChannelNoise = maxNoise
						rangeMinus = cChannelPedestal - (cChannelNoise * noiseTolerance)
						rangePlus = cChannelPedestal + (cChannelNoise * noiseTolerance) 
						if(selectPix): print(" range ", rangeMinus, " ", rangePlus, " zero ",firstZeroIndex, " one ",lastOneIndex)   

					elif firstZeroIndex == -1 and lastOneIndex == -1:
						if(selectPix): print("firstZeroIndex == -1 and lastOneIndex == -1, hybrid ", hyb_dir, " chip ", chip_dir, " row ", row, " col ", col)

						firstZeroIndex = hist.GetMinimumBin()
						lastOneIndex = hist.GetMaximumBin()
						cChannelPedestal = (lastOneIndex + firstZeroIndex) / 2.0
						cChannelNoise = (firstZeroIndex - lastOneIndex) / 2.0
						if cChannelNoise > maxNoise:
							cChannelNoise = maxNoise
						rangeMinus = cChannelPedestal - (cChannelNoise * noiseTolerance)
						rangePlus = cChannelPedestal + (cChannelNoise * noiseTolerance)
						if(selectPix):print(" range ", rangeMinus, " ", rangePlus, " zero ",firstZeroIndex, " one ",lastOneIndex)   

					if(selectPix): print(" hybrid ", hyb_dir, " chip ", chip_dir, " row ", row, " col ", col)
					if(selectPix): print(" range ", rangeMinus, " ", rangePlus, " zero ",firstZeroIndex, " one ",lastOneIndex)   
					# fit.SetRange(rangeMinus, rangePlus)
					newfit = ROOT.TF1(fit_name, MyErf, rangeMinus, rangePlus, 2)
					newfit.SetNpx(100)
					newfit.SetParameter(0, cChannelPedestal)
					newfit.SetParameter(1, cChannelNoise)
					newfit.SetParLimits(1, 1, maxNoise*noiseTolerance)
     
					if hist.GetMean() != 0:

						hist.Fit(newfit, "RQ+")
						hist.Fit(newfit, "RQ+")
						result = hist.Fit(newfit, "SRQ+")
						
						# if not result:
						# 	print(" not result ", hist.GetName())
						# 	if(selectPix): print("bad fit hybrid ", hyb_dir, " chip ", chip_dir)
						# 	with open(root_path.replace(".root","_Irene.txt"), "a") as textfile:
						# 		textfile.write(hist.GetName()+" \n")
						# else:
						if int(result) != 0: # or not result.IsValid():
							# print("bad fit ", hist.GetName())
							if(selectPix): print("bad fit hybrid ", hyb_dir, " chip ", chip_dir)
							with open(root_path.replace(".root","_Irene.txt"), "a") as textfile:
								textfile.write(hist.GetName()+" \n")
     
					newfit.SetRange(rangeMinus, rangePlus)
					# channel_dir.WriteTObject(hist, hist.GetName(), ROOT.TObject.kOverwrite)
					channel_dir.cd()  # temporarily move into that directory
					newfit.Write()
					# hist.Write(hist.GetName(), ROOT.TObject.kOverwrite)
					# hist.Write(hist.GetName(), ROOT.TObject.kOverwrite)
					noise = newfit.GetParameter(1)
					noise_error = newfit.GetParError(1)
					pulseheight = newfit.GetParameter(0)
					pulseheight_error = newfit.GetParError(0)
					# Chip summary hists
					h_chip_channel_noise_summary_copy.SetBinContent(linearizeRowAndColumns(row, col) + 1, noise)
					h_chip_channel_noise_summary_copy.SetBinError(linearizeRowAndColumns(row, col) + 1, noise_error)
	 
					h_chip_noise_distribution_summary_copy.Fill(noise)
	 
					h_chip_channel_pulseheight_summary_copy.SetBinContent(linearizeRowAndColumns(row, col) + 1, pulseheight)
					h_chip_channel_pulseheight_summary_copy.SetBinError(linearizeRowAndColumns(row, col) + 1, pulseheight_error)

					h_chip_pulseheight_distribution_summary_copy.Fill(pulseheight)
	 
					if "MPA" in chip_dir.GetName(): 
		 				h_chip_2D_channel_noise_summary_copy.SetBinContent(col +1, row + 1, noise)
		 				h_chip_2D_channel_noise_summary_copy.SetBinError(col +1, row + 1, noise_error)
	 
					# Hybrid summary hists
					if "SSA" in chip_dir.GetName():
						theBin = linearizeRowAndColumns(row, col) + COLUMNS * int(chip_dir.GetName().split("_")[-1])
						h_hybrid_strip_channel_noise_summary_copy.SetBinContent(theBin, noise)
						h_hybrid_strip_channel_noise_summary_copy.SetBinError(theBin, noise_error)
						h_hybrid_strip_noise_distribution_summary_copy.Fill(noise)
					if "MPA" in chip_dir.GetName(): 
						theBin = linearizeRowAndColumns(row, col) + COLUMNS * 16 * (int(chip_dir.GetName().split("_")[-1]) - 8)
						# h_hybrid_pixel_channel_noise_summary_copy.SetBinContent(theBin, noise)
						# h_hybrid_pixel_channel_noise_summary_copy.SetBinError(theBin, noise_error)
						h_hybrid_pixel_channel_noise_summary.SetBinContent(theBin, noise)
						h_hybrid_pixel_channel_noise_summary.SetBinError(theBin, noise_error)
						h_hybrid_pixel_noise_distribution_summary_copy.Fill(noise)

				# Write back
				chip_dir.cd()

				h_chip_channel_noise_summary_copy.Write(h_chip_channel_noise_summary_copy.GetName(), ROOT.TObject.kOverwrite)
				h_chip_noise_distribution_summary_copy.Write(h_chip_noise_distribution_summary_copy.GetName(), ROOT.TObject.kOverwrite)
				h_chip_channel_pulseheight_summary_copy.Write(h_chip_channel_pulseheight_summary_copy.GetName(), ROOT.TObject.kOverwrite)
				h_chip_pulseheight_distribution_summary_copy.Write(h_chip_pulseheight_distribution_summary_copy.GetName(), ROOT.TObject.kOverwrite)
				if "MPA" in chip_dir.GetName(): h_chip_2D_channel_noise_summary_copy.Write(h_chip_2D_channel_noise_summary_copy.GetName(), ROOT.TObject.kOverwrite)

			hyb_dir.cd()
			h_hybrid_strip_channel_noise_summary_copy.Write(h_hybrid_strip_channel_noise_summary_copy.GetName(), ROOT.TObject.kOverwrite)
			# h_hybrid_pixel_channel_noise_summary_copy.Write(h_hybrid_pixel_channel_noise_summary_copy.GetName(), ROOT.TObject.kOverwrite)
			h_hybrid_pixel_channel_noise_summary.Write(h_hybrid_pixel_channel_noise_summary.GetName(), ROOT.TObject.kOverwrite)
			h_hybrid_strip_noise_distribution_summary_copy.Write(h_hybrid_strip_noise_distribution_summary_copy.GetName(), ROOT.TObject.kOverwrite)
			h_hybrid_pixel_noise_distribution_summary_copy.Write(h_hybrid_pixel_noise_distribution_summary_copy.GetName(), ROOT.TObject.kOverwrite)

	f.Close()
	print(f"Finished with file: {root_path}")
	print("  - Updated per-channel S-curve fits in-place (SSA+MPA only)")
	print("  - Replaced noise and noise related histograms")
	print("  - All objects written to existing directories")


def swap_eyeopening_histograms(root_path: str) -> None:
	f = ROOT.TFile.Open(root_path, "UPDATE")
	# Navigate from top directory to the board directory where these hists are stored
	detector_dir = f.Get("Detector")
	board_dir = detector_dir.Get("Board_0")

	# Now loop dynamically over optical groups
	for og_key in board_dir.GetListOfKeys():
		og_dir = board_dir.Get(og_key.GetName())  # OpticalGroup_X
		if not isinstance(og_dir, ROOT.TDirectory):
			continue
		print(f"Swapping EyeOpening histogram names/titles in: {root_path}")
		"""Within this directory, swap names/titles of LpGBT_EyeOpeningScan_Power_0.333333 and _1.000000 if present."""
		name_0p33 = "LpGBT_EyeOpeningScan_Power_0.333333"
		name_1p00 = "LpGBT_EyeOpeningScan_Power_1.000000"
		hist = None
		for key in og_dir.GetListOfKeys():
			obj = og_dir.Get(key.GetName())
			
			if isinstance(obj, ROOT.TH2) and name_0p33 in obj.GetName():
				hist = obj
				break
		if hist is None:
			print("No "+name_0p33+" histogram found in", og_dir.GetName())
			sys.exit()

		h0p33_copy = hist.Clone(hist.GetName().replace("0.333333", "1.000000"))

		for key in og_dir.GetListOfKeys():
			obj = og_dir.Get(key.GetName())
   
			if isinstance(obj, ROOT.TH2) and name_1p00 in obj.GetName():
				hist = obj
				break
		if hist is None:
			print("No "+name_1p00+" histogram found in", og_dir.GetName())
			sys.exit()
   
		h1p00_copy = hist.Clone(hist.GetName().replace("1.000000", "0.333333"))

		
		
		h0p33_copy.SetTitle(h0p33_copy.GetTitle().replace("0.333333", "1.000000"))
		
		h1p00_copy.SetTitle(h1p00_copy.GetTitle().replace("1.000000", "0.333333"))
		# Write
		og_dir.cd()
		h0p33_copy.Write(h0p33_copy.GetName(), ROOT.TObject.kOverwrite)
		h1p00_copy.Write(h1p00_copy.GetName(), ROOT.TObject.kOverwrite)

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
	for src_path, stem, ext in find_version_candidates(data_dir, "v6-16"):
		found_any = True
		# Make the v6-17 temp copy
		destination_path = copy_file_version(src_path, stem, "v6-17temp", ext, dry_run=args.dry_run)

		# If the file starts with PS or 2S, perform required updates
		base = os.path.basename(destination_path)
		if (base.startswith("PS") or base.startswith("2S")) and not args.dry_run:
			# Swap EyeOpening histogram names/titles first
			swap_eyeopening_histograms(destination_path)
			# For PS, also update S-curve fits in-place
			if base.startswith("PS"):
				print(f"Updating S-curve fits for: {destination_path}")
				run_fit_in_place(destination_path)
		elif (base.startswith("PS") or base.startswith("2S")) and args.dry_run:
			print(f"[DRY-RUN] Would swap EyeOpening hist names/titles in: {destination_path}")
			if base.startswith("PS"):
				print(f"[DRY-RUN] Would update S-curve fits in-place for: {destination_path} (auto OG={og})")

	for src_path, stem, ext in find_version_candidates(data_dir, "v6-17temp"):
			found_any = True
			# Make the v6-17 copy
			destination_path = copy_file_version(src_path, stem, "v6-17", ext, perdirectory=False, dry_run=args.dry_run)




	if not found_any:
		print(f"No files ending with 'v6-16' found in {data_dir}")
	else:
		print("Done.")


if __name__ == "__main__":
	main()
